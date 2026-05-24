#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "button_handler.h"
#include "led_handler.h"
#include "espnow_keyfob.h"

static const char *TAG = "KEYFOB_MAIN";

// ─── Button task ─────────────────────────────────────
static void button_task(void *pvParameters)
{
    // Start disarmed — green LED
    led_show_disarmed();

    while (1) {
        button_event_t event = button_get_event();

        switch (event) {

            case BTN_LOCK:
                ESP_LOGI(TAG, "Sending LOCK command");
                led_show_sending();
                if (espnow_keyfob_send(CMD_LOCK)) {
                    led_show_armed();
                } else {
                    // Flash red fast to show failure
                    for (int i = 0; i < 3; i++) {
                        led_set_red(true);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        led_set_red(false);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
                break;

            case BTN_UNLOCK:
                ESP_LOGI(TAG, "Sending UNLOCK command");
                led_show_sending();
                if (espnow_keyfob_send(CMD_UNLOCK)) {
                    led_show_disarmed();
                } else {
                    for (int i = 0; i < 3; i++) {
                        led_set_red(true);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        led_set_red(false);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
                break;

            case BTN_PANIC:
                ESP_LOGI(TAG, "Sending PANIC command");
                led_show_panic();
                espnow_keyfob_send(CMD_PANIC);
                break;

            case BTN_NONE:
            default:
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Jeep Keyfob booting...");

    button_handler_init();
    led_handler_init();
    espnow_keyfob_init();

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Keyfob running");
}