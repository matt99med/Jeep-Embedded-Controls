/*
 * gpio_handler.c - GPIO init, ADC sensing, relay control, charge logic
 */
#include "config.h"
#include "system_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "gpio";

static adc_oneshot_unit_handle_t s_adc1;
static adc_cali_handle_t s_cali_veh;
static adc_cali_handle_t s_cali_batt;
static bool s_cali_ok_veh, s_cali_ok_batt;

/* ------------------------------------------------------------------ */
/* ADC                                                                 */
/* ------------------------------------------------------------------ */
static bool cali_init(adc_channel_t ch, adc_cali_handle_t *out)
{
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = ch,
        .atten    = ADC_ATTEN_DB_12,   /* full-scale ~3.1V at the pin     */
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    return adc_cali_create_scheme_curve_fitting(&cfg, out) == ESP_OK;
}

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc1));

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ADC_CH_VEH,  &ch_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, ADC_CH_BATT, &ch_cfg));

    s_cali_ok_veh  = cali_init(ADC_CH_VEH,  &s_cali_veh);
    s_cali_ok_batt = cali_init(ADC_CH_BATT, &s_cali_batt);
}

/* Returns rail voltage (post-divider math). Averages a few samples. */
static float read_rail(adc_channel_t ch, adc_cali_handle_t cali, bool cali_ok)
{
    const int N = 8;
    int acc = 0;
    for (int i = 0; i < N; i++) {
        int raw = 0;
        adc_oneshot_read(s_adc1, ch, &raw);
        acc += raw;
    }
    int raw_avg = acc / N;
    int mv = raw_avg;
    if (cali_ok) {
        adc_cali_raw_to_voltage(cali, raw_avg, &mv);
    } else {
        /* crude fallback: 12-bit, ~3100mV span */
        mv = (raw_avg * 3100) / 4095;
    }
    return MV_TO_RAIL(mv);
}

float gpio_read_vehicle_voltage(void)
{
    g_state.veh_voltage = read_rail(ADC_CH_VEH, s_cali_veh, s_cali_ok_veh);
    g_state.vehicle_present = (g_state.veh_voltage > VEH_PRESENT_V);
    return g_state.veh_voltage;
}

float gpio_read_battery_voltage(void)
{
    g_state.batt_voltage = read_rail(ADC_CH_BATT, s_cali_batt, s_cali_ok_batt);
    return g_state.batt_voltage;
}

/* ------------------------------------------------------------------ */
/* OUTPUTS                                                             */
/* ------------------------------------------------------------------ */
void gpio_handler_init(void)
{
    /* Outputs. Initialize to SAFE states BEFORE enabling as output:
     * KILL_COIL must come up immobilized (LOW); arming logic decides
     * later whether to enable run. SIREN off. Charge off. */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_KILL_COIL) | (1ULL << PIN_SIREN_COIL) |
                        (1ULL << PIN_CHG_EN)    | (1ULL << PIN_SIM_PWR_EN) |
                        (1ULL << PIN_SIM_PWRKEY)| (1ULL << PIN_SIM_DTR)    |
                        (1ULL << PIN_LED_SYS)   | (1ULL << PIN_LED_CELL)   |
                        (1ULL << PIN_LED_ARM)   | (1ULL << PIN_LED_FAULT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_set_level(PIN_KILL_COIL,  KILL_IMMOBILIZED);
    gpio_set_level(PIN_SIREN_COIL, RELAY_OFF);
    gpio_set_level(PIN_CHG_EN,     RELAY_OFF);
    gpio_set_level(PIN_SIM_PWR_EN, 0);
    gpio_set_level(PIN_SIM_PWRKEY, 0);
    gpio_set_level(PIN_SIM_DTR,    1);   /* DTR high = modem allowed awake */
    gpio_set_level(PIN_LED_SYS,    LED_OFF);
    gpio_set_level(PIN_LED_CELL,   LED_OFF);
    gpio_set_level(PIN_LED_ARM,    LED_OFF);
    gpio_set_level(PIN_LED_FAULT,  LED_OFF);

    /* Sensor inputs: external 10k pull-ups exist; enable internal too as
     * belt-and-suspenders. RTC-capable, so usable as deep-sleep wake. */
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_HOOD_SW) | (1ULL << PIN_SHOCK_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    adc_init();
    ESP_LOGI(TAG, "GPIO init complete; KILL=immobilized, SIREN=off");
}

void gpio_set_led(gpio_num_t led, int on) { gpio_set_level(led, on); }

/* Debounced read: returns true if the sensor is in TRIGGERED state. */
static bool sensor_triggered(gpio_num_t pin)
{
    if (gpio_get_level(pin) != SENSOR_TRIGGERED) return false;
    vTaskDelay(pdMS_TO_TICKS(SENSOR_DEBOUNCE_MS));
    return (gpio_get_level(pin) == SENSOR_TRIGGERED);
}

bool gpio_hood_triggered(void)  { return sensor_triggered(PIN_HOOD_SW); }
bool gpio_shock_triggered(void) { return sensor_triggered(PIN_SHOCK_SW); }

/* ------------------------------------------------------------------ */
/* ARM / ALARM                                                         */
/* ------------------------------------------------------------------ */
void sys_set_armed(bool armed)
{
    if (armed) {
        g_state.arm_state = STATE_ARMED;
        gpio_set_level(PIN_KILL_COIL, KILL_IMMOBILIZED);  /* block start  */
        gpio_set_level(PIN_LED_ARM, LED_ON);
        ESP_LOGI(TAG, "ARMED - immobilized");
    } else {
        g_state.arm_state = STATE_DISARMED;
        gpio_set_level(PIN_SIREN_COIL, RELAY_OFF);
        gpio_set_level(PIN_KILL_COIL, KILL_RUN_ENABLED);  /* allow start  */
        gpio_set_level(PIN_LED_ARM, LED_OFF);
        ESP_LOGI(TAG, "DISARMED - run enabled");
    }
}

void sys_trigger_alarm(const char *reason)
{
    if (g_state.arm_state == STATE_DISARMED) return;   /* only when armed */
    g_state.arm_state = STATE_ALARM;
    gpio_set_level(PIN_SIREN_COIL, RELAY_ON);
    gpio_set_level(PIN_KILL_COIL, KILL_IMMOBILIZED);
    ESP_LOGW(TAG, "ALARM: %s", reason ? reason : "(unknown)");
    /* sim7600 layer will publish jeep/alert + a GPS burst */
}

void sys_clear_alarm(void)
{
    gpio_set_level(PIN_SIREN_COIL, RELAY_OFF);
    if (g_state.arm_state == STATE_ALARM)
        g_state.arm_state = STATE_ARMED;
}

/* ------------------------------------------------------------------ */
/* CHARGE CONTROL  (firmware comparator w/ hysteresis)                 */
/* ------------------------------------------------------------------ */
void sys_update_charge_control(void)
{
    float v = gpio_read_vehicle_voltage();
    if (!g_state.charging && v >= CHG_CLOSE_V) {
        gpio_set_level(PIN_CHG_EN, RELAY_ON);
        g_state.charging = true;
        gpio_set_level(PIN_LED_FAULT, LED_ON);  /* amber = charging        */
        ESP_LOGI(TAG, "charge relay CLOSED (%.2fV)", v);
    } else if (g_state.charging && v < CHG_OPEN_V) {
        gpio_set_level(PIN_CHG_EN, RELAY_OFF);
        g_state.charging = false;
        gpio_set_level(PIN_LED_FAULT, LED_OFF);
        ESP_LOGI(TAG, "charge relay OPEN (%.2fV)", v);
    }
}

void sys_update_battery_status(void)
{
    float v = gpio_read_battery_voltage();
    g_state.batt_low      = (v < BATT_LOW_V);
    g_state.batt_critical = (v < BATT_CRIT_V);
    if (g_state.batt_critical)
        ESP_LOGW(TAG, "battery CRITICAL %.2fV", v);
}