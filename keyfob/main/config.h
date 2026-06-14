/*
 * config.h - Jeep Embedded Controls : Keyfob
 * Target: ESP32-C3FH4X (ESP-IDF v6.x, FreeRTOS)
 *
 * Pins verified against schematic "Jeep_Embedded_KeyFob".
 *
 * Power model: the C3 sits in deep sleep until a button wakes it. LOCK
 * (GPIO4) and UNLOCK (GPIO5) are both in the GPIO0-5 deep-sleep wake
 * range. On wake: send the command via ESP-NOW, blink the status LED for
 * acknowledgement, then return to deep sleep. Average draw is dominated
 * by the AP2112K LDO quiescent (~55uA), giving very long standby on the
 * LiPo.
 */
#pragma once

#include "driver/gpio.h"

/* --- Buttons (active LOW, external behavior: press pulls to GND) --- */
#define PIN_BTN_LOCK    GPIO_NUM_4
#define PIN_BTN_UNLOCK  GPIO_NUM_5

/* --- Status LED (HIGH = on) --- */
#define PIN_LED_STATUS  GPIO_NUM_3
#define LED_ON   1
#define LED_OFF  0

/* Charge LED is hardwired off the MCP73831 STAT pin - not a GPIO. */

/* --- ESP-NOW command protocol (must match main controller) --- */
typedef enum {
    CMD_LOCK   = 0x01,
    CMD_UNLOCK = 0x02,
} jeep_cmd_t;

typedef struct __attribute__((packed)) {
    uint8_t cmd;
    uint8_t seq;
} espnow_msg_t;

/* Main controller's STA MAC - fill in after reading it from the
 * controller's boot log (espnow_handler_init prints it). */
#define CONTROLLER_MAC { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

/* Timing */
#define BTN_DEBOUNCE_MS   30
#define LED_ACK_MS        120
#define TX_RETRY_COUNT    3