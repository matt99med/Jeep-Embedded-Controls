#ifndef GPIO_HANDLER_H
#define GPIO_HANDLER_H

#include "driver/gpio.h"
#include <stdbool.h>

// ─── Output GPIO pins ───────────────────────────────
#define PIN_CHASE_LIGHT  GPIO_NUM_23
#define PIN_POD_LIGHT    GPIO_NUM_22
#define PIN_SPOT_LIGHT   GPIO_NUM_19
#define PIN_FOG_LIGHT    GPIO_NUM_18
#define PIN_LIGHTBAR     GPIO_NUM_5
#define PIN_IGN_KILL     GPIO_NUM_2
#define PIN_SIREN        GPIO_NUM_3
#define PIN_BLINK        GPIO_NUM_1

// ─── Input GPIO pins ────────────────────────────────
#define PIN_SW1          GPIO_NUM_36
#define PIN_SW2          GPIO_NUM_39
#define PIN_SW3          GPIO_NUM_34
#define PIN_SW4          GPIO_NUM_35
#define PIN_SW5          GPIO_NUM_32
#define PIN_HOOD_SW      GPIO_NUM_25

// ─── SIM7600 control pins ───────────────────────────
#define PIN_SIM_PWRKEY   GPIO_NUM_15
#define PIN_SIM_RESET    GPIO_NUM_17

// ─── GPS UART pins ──────────────────────────────────
#define PIN_GPS_TX       GPIO_NUM_16
#define PIN_GPS_RX       GPIO_NUM_4

// ─── Function prototypes ────────────────────────────
void gpio_handler_init(void);

// Outputs
void set_chase_light(bool state);
void set_pod_light(bool state);
void set_spot_light(bool state);
void set_fog_light(bool state);
void set_lightbar(bool state);
void set_ign_kill(bool state);
void set_siren(bool state);
void set_blink(bool state);

// Inputs
bool get_sw1(void);
bool get_sw2(void);
bool get_sw3(void);
bool get_sw4(void);
bool get_sw5(void);
bool get_hood_sw(void);

#endif // GPIO_HANDLER_H