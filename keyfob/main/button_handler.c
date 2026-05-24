#include "button_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUTTON";

void button_handler_init(void)
{
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_LOCK)   |
                        (1ULL << PIN_BTN_UNLOCK) |
                        (1ULL << PIN_BTN_PANIC),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);
    ESP_LOGI(TAG, "Buttons initialised");
}

// ─── Read buttons with debounce ──────────────────────
// Buttons are active LOW — pressed = 0
button_event_t button_get_event(void)
{
    if (gpio_get_level(PIN_BTN_LOCK) == 0) {
        vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));
        if (gpio_get_level(PIN_BTN_LOCK) == 0) {
            // Wait for release
            while (gpio_get_level(PIN_BTN_LOCK) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGI(TAG, "LOCK button pressed");
            return BTN_LOCK;
        }
    }

    if (gpio_get_level(PIN_BTN_UNLOCK) == 0) {
        vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));
        if (gpio_get_level(PIN_BTN_UNLOCK) == 0) {
            while (gpio_get_level(PIN_BTN_UNLOCK) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGI(TAG, "UNLOCK button pressed");
            return BTN_UNLOCK;
        }
    }

    if (gpio_get_level(PIN_BTN_PANIC) == 0) {
        vTaskDelay(pdMS_TO_TICKS(BTN_DEBOUNCE_MS));
        if (gpio_get_level(PIN_BTN_PANIC) == 0) {
            while (gpio_get_level(PIN_BTN_PANIC) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGI(TAG, "PANIC button pressed");
            return BTN_PANIC;
        }
    }

    return BTN_NONE;
}