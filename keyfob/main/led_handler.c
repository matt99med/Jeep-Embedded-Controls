#include "led_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED";

void led_handler_init(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_RED)   |
                        (1ULL << PIN_LED_GREEN) |
                        (1ULL << PIN_LED_BLUE),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    led_all_off();
    ESP_LOGI(TAG, "LEDs initialised");
}

void led_set_red(bool state)   { gpio_set_level(PIN_LED_RED,   state); }
void led_set_green(bool state) { gpio_set_level(PIN_LED_GREEN, state); }
void led_set_blue(bool state)  { gpio_set_level(PIN_LED_BLUE,  state); }

void led_all_off(void)
{
    led_set_red(false);
    led_set_green(false);
    led_set_blue(false);
}

// ─── LED patterns for each state ────────────────────

// Armed — solid red
void led_show_armed(void)
{
    led_all_off();
    led_set_red(true);
}

// Disarmed — solid green
void led_show_disarmed(void)
{
    led_all_off();
    led_set_green(true);
}

// Panic — fast blue blink 3 times
void led_show_panic(void)
{
    led_all_off();
    for (int i = 0; i < 3; i++) {
        led_set_blue(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        led_set_blue(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Sending — quick white flash (all on briefly)
void led_show_sending(void)
{
    led_set_red(true);
    led_set_green(true);
    led_set_blue(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    led_all_off();
}