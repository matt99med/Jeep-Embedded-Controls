/*
 * config.h - Jeep Embedded Controls : Main Controller
 * Central hardware + behavior configuration.
 *
 * Target: ESP32-WROOM-32E  (ESP-IDF v6.x, FreeRTOS)
 *
 * All pin numbers verified against schematic
 * "Jeep_Embedded_Controls_Main" rev "Update".
 *
 * NOTE ON SAFETY-CRITICAL CONSTANTS:
 *   KILL relay is ENERGIZED-TO-RUN. Driving KILL_COIL HIGH energizes the
 *   relay coil and CLOSES the starter circuit (vehicle can start). Driving
 *   it LOW de-energizes -> vehicle immobilized. A dead controller therefore
 *   fails immobilized; the hidden mechanical BYPASS switch is the recovery
 *   path and is intentionally NOT wired to this board.
 */
#pragma once

#include "driver/gpio.h"

/* ------------------------------------------------------------------ */
/* GPIO ASSIGNMENTS  (from schematic)                                  */
/* ------------------------------------------------------------------ */

/* --- Analog sense (ADC1, input-only pins) --- */
#define PIN_VEH_SENSE      GPIO_NUM_36   /* SENSOR_VP - vehicle 12V divider */
#define PIN_BATT_SENSE     GPIO_NUM_39   /* SENSOR_VN - aux battery divider */
#define ADC_CH_VEH         ADC_CHANNEL_0 /* GPIO36 = ADC1_CH0 */
#define ADC_CH_BATT        ADC_CHANNEL_3 /* GPIO39 = ADC1_CH3 */

/* --- Charge control --- */
#define PIN_CHG_EN         GPIO_NUM_25   /* HIGH = close charge relay        */

/* --- SIM7600 (Waveshare carrier) --- */
#define PIN_SIM_PWR_EN     GPIO_NUM_13   /* HIGH = enable MP2307 3.8V rail   */
#define PIN_SIM_PWRKEY     GPIO_NUM_14   /* power-on toggle (see sequence)   */
#define PIN_SIM_DTR        GPIO_NUM_4    /* sleep/wake control (optional)    */
#define PIN_SIM_RX         GPIO_NUM_16   /* ESP RX  <- modem TXD             */
#define PIN_SIM_TX         GPIO_NUM_17   /* ESP TX  -> modem RXD             */
#define SIM_UART_PORT      UART_NUM_2

/* --- Sensor inputs (RTC-capable -> can wake from deep sleep) --- */
#define PIN_HOOD_SW        GPIO_NUM_27   /* idle HIGH, LOW = hood open       */
#define PIN_SHOCK_SW       GPIO_NUM_26   /* idle HIGH, LOW = impact pulse    */

/* --- Relay coil drivers (low-side N-FET; HIGH = energized) --- */
#define PIN_KILL_COIL      GPIO_NUM_32   /* HIGH = RUN-enabled, LOW = killed */
#define PIN_SIREN_COIL     GPIO_NUM_33   /* HIGH = siren on                  */

/* --- Status LEDs (HIGH = on; dark by default to protect sleep budget) --- */
#define PIN_LED_SYS        GPIO_NUM_19
#define PIN_LED_CELL       GPIO_NUM_18
#define PIN_LED_ARM        GPIO_NUM_21
#define PIN_LED_FAULT      GPIO_NUM_22

/* ------------------------------------------------------------------ */
/* LOGIC LEVELS                                                        */
/* ------------------------------------------------------------------ */
#define RELAY_ON           1
#define RELAY_OFF          0
#define KILL_RUN_ENABLED   1   /* energize coil -> car can start         */
#define KILL_IMMOBILIZED   0
#define SENSOR_IDLE        1   /* pulled high                            */
#define SENSOR_TRIGGERED   0   /* switch closed to ground                */
#define LED_ON             1
#define LED_OFF            0

/* ------------------------------------------------------------------ */
/* ADC SCALING  (divider = 220k / (1M + 220k) = 0.1803)                */
/*   Vpin = Vrail * 0.1803 ; Vrail = Vpin / 0.1803                      */
/* ------------------------------------------------------------------ */
#define DIV_RATIO          0.18033f
#define MV_TO_RAIL(mv)     ((float)(mv) / DIV_RATIO / 1000.0f)  /* -> volts */

/* ------------------------------------------------------------------ */
/* CHARGE CONTROL HYSTERESIS  (vehicle-sense, volts)                   */
/* ------------------------------------------------------------------ */
#define CHG_CLOSE_V        13.20f   /* alternator running -> start charge  */
#define CHG_OPEN_V         12.80f   /* engine off -> disconnect            */

/* ------------------------------------------------------------------ */
/* AUX BATTERY THRESHOLDS  (volts)                                     */
/* ------------------------------------------------------------------ */
#define BATT_LOW_V         12.00f   /* stretch reporting interval          */
#define BATT_CRIT_V        11.50f   /* "going dark" alert + deep sleep only*/
/* Hardware UVLO (LM5164 EN divider) cuts the rail ~11.5V regardless.  */

/* ------------------------------------------------------------------ */
/* VEHICLE-POWER-LOSS DETECTION                                        */
/*   Vehicle sense dropping below this while previously present = the  */
/*   main battery was cut. Highest-value theft signal in the system.   */
/* ------------------------------------------------------------------ */
#define VEH_PRESENT_V      6.00f

/* ------------------------------------------------------------------ */
/* SLEEP / TIMING                                                      */
/* ------------------------------------------------------------------ */
#define ACTIVE_WINDOW_MS       (90 * 1000)   /* stay light-sleep listenable */
#define CHECKIN_INTERVAL_S     (3 * 3600)    /* deep-sleep MQTT check-in     */
#define CHECKIN_INTERVAL_ARMED_S (30 * 60)   /* faster when armed            */
#define PWRKEY_HIGH_MS         500
#define PWRKEY_LOW_MS          1100
#define SENSOR_DEBOUNCE_MS     50

/* ------------------------------------------------------------------ */
/* SIM / MQTT                                                          */
/*   Secrets are NOT stored here. Provide them via menuconfig / NVS.   */
/*   Add a secrets.h (gitignored) or use idf.py menuconfig values.     */
/* ------------------------------------------------------------------ */
#define MQTT_TOPIC_GPS_REQ     "jeep/gps/request"
#define MQTT_TOPIC_GPS_DATA    "jeep/gps/data"
#define MQTT_TOPIC_STATUS      "jeep/status"
#define MQTT_TOPIC_ALERT       "jeep/alert"     /* power-cut / tamper        */

/* ------------------------------------------------------------------ */
/* ESP-NOW COMMAND PROTOCOL  (matches keyfob)                          */
/* ------------------------------------------------------------------ */
typedef enum {
    CMD_LOCK   = 0x01,   /* arm siren + immobilize (KILL_COIL LOW)      */
    CMD_UNLOCK = 0x02,   /* disarm + allow run (KILL_COIL HIGH)         */
} jeep_cmd_t;

/* keyfob -> controller payload */
typedef struct __attribute__((packed)) {
    uint8_t cmd;
    uint8_t seq;          /* rolling counter, replay sanity            */
} espnow_msg_t;