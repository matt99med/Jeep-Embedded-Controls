#ifndef SIM7600_H
#define SIM7600_H

#include <stdbool.h>

// ─── UART config ────────────────────────────────────
#define SIM_UART_NUM     UART_NUM_2
#define SIM_UART_BAUD    115200
#define SIM_UART_TX      GPIO_NUM_16
#define SIM_UART_RX      GPIO_NUM_4
#define SIM_UART_BUF     2048

// ─── MQTT config ────────────────────────────────────
#define MQTT_BROKER      "b354244753214e4a8256c8d36e8a98ad.s1.eu.hivemq.cloud"
#define MQTT_PORT        8883
#define MQTT_USER        "jeep_controller"
#define MQTT_PASS        "1995Jeep"

// ─── MQTT topics ────────────────────────────────────
#define TOPIC_GPS_REQ    "jeep/gps/request"
#define TOPIC_GPS_DATA   "jeep/gps/data"
#define TOPIC_STATUS     "jeep/status"

// ─── Function prototypes ────────────────────────────
void sim7600_init(void);
bool sim7600_power_on(void);
bool sim7600_get_gps(float *latitude, float *longitude);
bool sim7600_mqtt_connect(void);
bool sim7600_mqtt_publish(const char *topic, const char *payload);
bool sim7600_mqtt_subscribe(const char *topic);
void sim7600_task(void *pvParameters);

#endif // SIM7600_H