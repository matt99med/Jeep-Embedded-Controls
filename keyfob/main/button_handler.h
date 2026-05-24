#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "driver/gpio.h"
#include <stdbool.h>

// ─── Button GPIO pins ────────────────────────────────
#define PIN_BTN_LOCK     GPIO_NUM_0
#define PIN_BTN_UNLOCK   GPIO_NUM_1
#define PIN_BTN_PANIC    GPIO_NUM_2

// ─── Debounce time ───────────────────────────────────
#define BTN_DEBOUNCE_MS  50

// ─── Button states ───────────────────────────────────
typedef enum {
    BTN_NONE   = 0,
    BTN_LOCK   = 1,
    BTN_UNLOCK = 2,
    BTN_PANIC  = 3,
} button_event_t;

// ─── Function prototypes ────────────────────────────
void button_handler_init(void);
button_event_t button_get_event(void);

#endif // BUTTON_HANDLER_H