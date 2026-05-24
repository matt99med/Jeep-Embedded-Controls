#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include "driver/gpio.h"
#include <stdbool.h>

// ─── LED GPIO pins ───────────────────────────────────
#define PIN_LED_RED      GPIO_NUM_3
#define PIN_LED_GREEN    GPIO_NUM_4
#define PIN_LED_BLUE     GPIO_NUM_5

// ─── Function prototypes ────────────────────────────
void led_handler_init(void);
void led_set_red(bool state);
void led_set_green(bool state);
void led_set_blue(bool state);
void led_all_off(void);
void led_show_armed(void);
void led_show_disarmed(void);
void led_show_panic(void);
void led_show_sending(void);

#endif // LED_HANDLER_H