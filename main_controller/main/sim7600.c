/*
 * sim7600.c - SIM7600G-H power control, GPS fix, MQTT publish over 4G.
 *
 * This module owns the modem power rail (MP2307 EN) and the PWRKEY
 * sequence. AT-command exchange is intentionally minimal/illustrative;
 * fill in the broker credentials via NVS/menuconfig (see secrets note in
 * config.h) and expand the MQTT/AT flow to taste.
 */
#include "config.h"
#include "system_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "sim7600";
static bool s_modem_on = false;

#define UART_BUF 1024

void sim7600_init_uart(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(SIM_UART_PORT, UART_BUF * 2, 0, 0, NULL, 0);
    uart_param_config(SIM_UART_PORT, &cfg);
    uart_set_pin(SIM_UART_PORT, PIN_SIM_TX, PIN_SIM_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

/* Blocking AT send + wait for "OK"/"ERROR". Returns true on OK. */
static bool at_cmd(const char *cmd, int timeout_ms)
{
    uart_flush_input(SIM_UART_PORT);
    uart_write_bytes(SIM_UART_PORT, cmd, strlen(cmd));
    uart_write_bytes(SIM_UART_PORT, "\r\n", 2);

    char buf[UART_BUF];
    int total = 0;
    int waited = 0;
    while (waited < timeout_ms) {
        int n = uart_read_bytes(SIM_UART_PORT, (uint8_t *)buf + total,
                                sizeof(buf) - 1 - total, pdMS_TO_TICKS(100));
        if (n > 0) {
            total += n;
            buf[total] = '\0';
            if (strstr(buf, "OK"))    return true;
            if (strstr(buf, "ERROR")) return false;
        }
        waited += 100;
    }
    ESP_LOGW(TAG, "AT timeout: %s", cmd);
    return false;
}

/* PWRKEY power-up sequence per Waveshare doc:
 *   HIGH 500ms -> LOW >=1s -> release (return pin to input/high-Z). */
static void pwrkey_pulse(void)
{
    gpio_set_direction(PIN_SIM_PWRKEY, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_SIM_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(PWRKEY_HIGH_MS));
    gpio_set_level(PIN_SIM_PWRKEY, 0);
    vTaskDelay(pdMS_TO_TICKS(PWRKEY_LOW_MS));
    gpio_set_direction(PIN_SIM_PWRKEY, GPIO_MODE_INPUT);  /* release */
}

bool sim7600_power_on(void)
{
    if (s_modem_on) return true;

    gpio_set_level(PIN_SIM_PWR_EN, 1);     /* enable 3.8V rail            */
    vTaskDelay(pdMS_TO_TICKS(100));        /* let rail settle             */
    pwrkey_pulse();

    gpio_set_level(PIN_LED_CELL, LED_ON);

    /* Wait for the module to become AT-responsive (can take ~10s). */
    for (int i = 0; i < 30; i++) {
        if (at_cmd("AT", 500)) {
            s_modem_on = true;
            at_cmd("ATE0", 1000);          /* echo off                    */
            ESP_LOGI(TAG, "modem up");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGE(TAG, "modem failed to respond");
    gpio_set_level(PIN_LED_CELL, LED_OFF);
    return false;
}

void sim7600_power_off(void)
{
    if (!s_modem_on) { gpio_set_level(PIN_SIM_PWR_EN, 0); return; }
    at_cmd("AT+CPOF", 2000);               /* graceful shutdown           */
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(PIN_SIM_PWR_EN, 0);     /* kill rail -> ~0 draw        */
    gpio_set_level(PIN_LED_CELL, LED_OFF);
    s_modem_on = false;
    ESP_LOGI(TAG, "modem off");
}

/* Returns true and fills lat/lon on a valid fix. */
bool sim7600_get_gps(float *lat, float *lon)
{
    at_cmd("AT+CGPS=1", 1000);             /* ensure GPS engine on        */
    char buf[UART_BUF];
    for (int i = 0; i < 60; i++) {         /* up to ~60s for first fix    */
        uart_flush_input(SIM_UART_PORT);
        uart_write_bytes(SIM_UART_PORT, "AT+CGPSINFO\r\n", 13);
        int n = uart_read_bytes(SIM_UART_PORT, (uint8_t *)buf,
                                sizeof(buf) - 1, pdMS_TO_TICKS(1000));
        if (n > 0) {
            buf[n] = '\0';
            /* +CGPSINFO: lat,N/S,lon,E/W,date,utc,alt,speed,course */
            char *p = strstr(buf, "+CGPSINFO:");
            if (p && p[10] != ',' && p[11] != ',') {
                /* Parse NMEA ddmm.mmmm into decimal degrees. */
                float la, lo; char ns, ew;
                if (sscanf(p + 10, " %f,%c,%f,%c", &la, &ns, &lo, &ew) == 4) {
                    float lad = (int)(la / 100) + fmodf(la, 100.0f) / 60.0f;
                    float lod = (int)(lo / 100) + fmodf(lo, 100.0f) / 60.0f;
                    if (ns == 'S') lad = -lad;
                    if (ew == 'W') lod = -lod;
                    *lat = lad; *lon = lod;
                    return true;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return false;
}

/* --- MQTT (using the modem's built-in stack, CMQTT* commands) --- */
static bool mqtt_connect(void)
{
    /* TODO: set APN, start MQTT service, connect to HiveMQ over TLS.
     * Credentials come from NVS/menuconfig, not source. Sketch: */
    at_cmd("AT+CGDCONT=1,\"IP\",\"hologram\"", 2000);
    at_cmd("AT+CMQTTSTART", 5000);
    /* ... CMQTTACCQ / CMQTTSSLCFG / CMQTTCONNECT with broker creds ... */
    return true;
}

void sim7600_publish_status(const char *status)
{
    /* Best-effort: only meaningful when modem+MQTT are up. Caller is
     * responsible for power/connect lifecycle in the task below. */
    ESP_LOGI(TAG, "status -> %s", status);
    /* AT+CMQTTTOPIC / CMQTTPAYLOAD / CMQTTPUB ... topic MQTT_TOPIC_STATUS */
}

void sim7600_publish_gps(float lat, float lon)
{
    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"lat\":%.5f,\"lon\":%.5f}", lat, lon);
    ESP_LOGI(TAG, "gps -> %s", payload);
    /* publish to MQTT_TOPIC_GPS_DATA */
}

void sim7600_publish_alert(const char *reason)
{
    ESP_LOGW(TAG, "ALERT -> %s", reason);
    /* publish to MQTT_TOPIC_ALERT - highest priority message */
}

/* One full report cycle: power on, fix, publish, (optionally) stay for
 * a request window, then power off. Used by the check-in / alarm paths. */
bool sim7600_report_cycle(bool is_alert, const char *reason)
{
    if (!sim7600_power_on()) return false;
    mqtt_connect();

    if (is_alert) sim7600_publish_alert(reason);

    float lat, lon;
    if (sim7600_get_gps(&lat, &lon)) {
        sim7600_publish_gps(lat, lon);
    } else {
        ESP_LOGW(TAG, "no GPS fix this cycle");
    }
    return true;
}