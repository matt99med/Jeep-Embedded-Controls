#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "gpio_handler.h"
#include "espnow_handler.h"
#include "sim7600.h"

static const char *TAG = "MAIN";

// ─── Hood sensor task ────────────────────────────────
// Monitors hood switch — triggers siren if armed and hood opens
static void hood_monitor_task(void *pvParameters)
{
    bool armed       = false;
    bool hood_open   = false;
    bool last_hood   = false;

    while (1) {
        hood_open = get_hood_sw();

        // Hood opened while armed — trigger siren
        if (armed && hood_open && !last_hood) {
            ESP_LOGW(TAG, "Hood opened while armed — triggering siren");
            set_siren(true);
        }

        last_hood = hood_open;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ─── Light switch task ───────────────────────────────
// Reads physical switches and controls lights accordingly
static void light_switch_task(void *pvParameters)
{
    while (1) {
        set_lightbar(get_sw1());
        set_fog_light(get_sw2());
        set_spot_light(get_sw3());
        set_pod_light(get_sw4());
        set_chase_light(get_sw5());

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ─── Blink task ──────────────────────────────────────
// Heartbeat LED so you know the system is alive
static void blink_task(void *pvParameters)
{
    while (1) {
        set_blink(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_blink(false);
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

// ─── App main ────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "Jeep Controller booting...");

    // Init all GPIO first
    gpio_handler_init();
    ESP_LOGI(TAG, "GPIO initialised");

    // Init ESP-NOW for keyfob communication
    espnow_handler_init();
    ESP_LOGI(TAG, "ESP-NOW initialised");

    // Init SIM7600 UART
    sim7600_init();
    ESP_LOGI(TAG, "SIM7600 UART initialised");

    // Start FreeRTOS tasks
    xTaskCreate(blink_task,        "blink",        2048, NULL, 1, NULL);
    xTaskCreate(hood_monitor_task, "hood_monitor", 2048, NULL, 5, NULL);
    xTaskCreate(light_switch_task, "light_switch", 2048, NULL, 3, NULL);
    xTaskCreate(sim7600_task,      "sim7600",       8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "All tasks started — system running");
}