/*
 * main.c - Jeep Main Controller orchestration
 *
 * Power model (confirmed):
 *   - Runs entirely from the aux AGM battery.
 *   - Default parked state = light sleep while recently active (radio
 *     listenable for instant ESP-NOW lock/unlock), dropping to deep sleep
 *     after ACTIVE_WINDOW_MS of no activity.
 *   - Wakes from deep sleep on: hood (IO27), shock (IO26), or timer
 *     (scheduled MQTT check-in / charge poll).
 *   - On any wake: read sensors + battery + vehicle voltage, run charge
 *     control, and if armed+triggered -> alarm + alert + GPS burst.
 *
 * Wake-on-keyfob caveat: a deep-sleeping ESP32 cannot hear ESP-NOW. The
 * tiered model keeps the radio alive during the active window so fob
 * presses are instant right after any activity; during long deep-sleep
 * stretches the fob is serviced at the next timer wake. For a security
 * fob this is the right tradeoff between instant response and months of
 * battery life.
 */
#include "config.h"
#include "system_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/rtc_io.h"

static const char *TAG = "main";

/* Global state instance */
system_state_t g_state = {
    .arm_state = STATE_DISARMED,
    .last_seq  = 0xFF,
};

/* from other modules */
void gpio_handler_init(void);
bool gpio_hood_triggered(void);
bool gpio_shock_triggered(void);
void espnow_handler_init(void);
void sim7600_init_uart(void);
bool sim7600_report_cycle(bool is_alert, const char *reason);
void sim7600_power_off(void);

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* Configure deep-sleep wake sources: sensors (ext1, low level) + timer. */
static void arm_deep_sleep_wakeups(uint64_t timer_s)
{
    const uint64_t mask = (1ULL << PIN_HOOD_SW) | (1ULL << PIN_SHOCK_SW);
    /* Sensors idle HIGH, trigger LOW -> wake when ANY goes low. */
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ALL_LOW);
    if (timer_s) esp_sleep_enable_timer_wakeup(timer_s * 1000000ULL);
}

/* Decide check-in cadence based on arm/battery state. */
static uint64_t checkin_period_s(void)
{
    if (g_state.batt_critical) return 0;                 /* timer off: only sensor wakes */
    if (g_state.arm_state != STATE_DISARMED) return CHECKIN_INTERVAL_ARMED_S;
    return CHECKIN_INTERVAL_S;
}

/* Handle whatever woke us / whatever happened this active window. */
static void service_once(void)
{
    /* Always: refresh analog state, run charge control + battery eval. */
    sys_update_charge_control();
    sys_update_battery_status();

    bool prev_present = g_state.vehicle_present;

    /* Sensor checks (only meaningful when armed). */
    if (g_state.arm_state != STATE_DISARMED) {
        if (gpio_hood_triggered()) {
            sys_trigger_alarm("hood opened");
            sim7600_report_cycle(true, "hood opened");
        } else if (gpio_shock_triggered()) {
            sys_trigger_alarm("impact detected");
            sim7600_report_cycle(true, "impact detected");
        }
    }

    /* Power-cut detection: vehicle sense lost while we thought present. */
    if (prev_present && !g_state.vehicle_present &&
        g_state.arm_state != STATE_DISARMED) {
        sys_trigger_alarm("vehicle power cut");
        sim7600_report_cycle(true, "vehicle power cut");
    }
}

/* Scheduled check-in: bring up modem, publish status + GPS, tear down. */
static void do_checkin(void)
{
    ESP_LOGI(TAG, "scheduled check-in");
    sim7600_report_cycle(false, NULL);
    sim7600_power_off();
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Wi-Fi/ESP-NOW base (STA, no association). */
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());
    /* Modem-sleep lets the radio idle between beacons during light sleep. */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    gpio_handler_init();
    sim7600_init_uart();
    espnow_handler_init();

    /* Determine why we woke. */
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGW(TAG, "woke on sensor");
        service_once();
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(TAG, "woke on timer");
        service_once();
        if (!g_state.batt_critical) do_checkin();
        break;
    default:
        ESP_LOGI(TAG, "cold boot");
        /* On cold boot, default DISARMED -> allow vehicle to run. */
        sys_set_armed(false);
        service_once();
        break;
    }

    /* --- Active window: stay light-sleep-listenable for ESP-NOW --- */
    g_state.last_activity_ms = now_ms();
    gpio_set_level(PIN_LED_SYS, LED_ON);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_LED_SYS, LED_OFF);

    while (now_ms() - g_state.last_activity_ms < ACTIVE_WINDOW_MS) {
        /* Light sleep: CPU halts, RAM + radio retained, ESP-NOW callback
         * fires on packet RX (which refreshes last_activity_ms). */
        esp_sleep_enable_timer_wakeup(1 * 1000000ULL);  /* 1s tick */
        esp_light_sleep_start();

        /* periodic housekeeping while in the active window */
        sys_update_charge_control();
        if (g_state.arm_state != STATE_DISARMED) {
            if (gpio_hood_triggered())  sys_trigger_alarm("hood opened");
            if (gpio_shock_triggered()) sys_trigger_alarm("impact detected");
        }
    }

    /* --- Drop to deep sleep --- */
    sys_update_battery_status();
    sim7600_power_off();
    uint64_t period = checkin_period_s();
    ESP_LOGI(TAG, "deep sleep; next timer %llus (arm=%d batt=%.2fV)",
             period, g_state.arm_state, g_state.batt_voltage);
    arm_deep_sleep_wakeups(period);
    esp_deep_sleep_start();
    /* never returns */
}