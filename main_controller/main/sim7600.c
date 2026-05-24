#include "sim7600.h"
#include "gpio_handler.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SIM7600";

// ─── Send AT command and wait for response ──────────
static bool at_send(const char *cmd, const char *expected,
                    uint32_t timeout_ms)
{
    char buf[512] = {0};

    uart_write_bytes(SIM_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(SIM_UART_NUM, "\r\n", 2);

    vTaskDelay(pdMS_TO_TICKS(timeout_ms));

    int len = uart_read_bytes(SIM_UART_NUM, buf,
                              sizeof(buf) - 1,
                              pdMS_TO_TICKS(100));
    if (len > 0) {
        buf[len] = '\0';
        ESP_LOGI(TAG, "AT response: %s", buf);
        if (strstr(buf, expected)) {
            return true;
        }
    }
    return false;
}

// ─── Init UART for SIM7600 ──────────────────────────
void sim7600_init(void)
{
    uart_config_t uart_config = {
        .baud_rate  = SIM_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(SIM_UART_NUM, SIM_UART_BUF, 0, 0, NULL, 0);
    uart_param_config(SIM_UART_NUM, &uart_config);
    uart_set_pin(SIM_UART_NUM,
                 SIM_UART_TX,
                 SIM_UART_RX,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

    ESP_LOGI(TAG, "UART initialised for SIM7600");
}

// ─── Power on SIM7600 ───────────────────────────────
bool sim7600_power_on(void)
{
    ESP_LOGI(TAG, "Powering on SIM7600...");

    // Pull PWRKEY low for 500ms to power on
    gpio_set_level(PIN_SIM_PWRKEY, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(PIN_SIM_PWRKEY, 0);

    // Wait for module to boot
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Test AT communication
    for (int i = 0; i < 5; i++) {
        if (at_send("AT", "OK", 1000)) {
            ESP_LOGI(TAG, "SIM7600 is online");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGE(TAG, "SIM7600 failed to respond");
    return false;
}

// ─── Get GPS coordinates ────────────────────────────
bool sim7600_get_gps(float *latitude, float *longitude)
{
    char buf[256] = {0};

    // Enable GPS
    at_send("AT+CGPS=1,1", "OK", 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Request GPS info
    uart_write_bytes(SIM_UART_NUM, "AT+CGPSINFO\r\n", 13);
    vTaskDelay(pdMS_TO_TICKS(2000));

    int len = uart_read_bytes(SIM_UART_NUM, buf,
                              sizeof(buf) - 1,
                              pdMS_TO_TICKS(100));
    if (len <= 0) {
        ESP_LOGW(TAG, "No GPS response");
        return false;
    }

    buf[len] = '\0';
    ESP_LOGI(TAG, "GPS raw: %s", buf);

    // Parse +CGPSINFO: lat,N/S,lon,E/W,...
    float lat_raw, lon_raw;
    char ns, ew;
    char *info = strstr(buf, "+CGPSINFO:");
    if (!info) return false;

    int parsed = sscanf(info, "+CGPSINFO: %f,%c,%f,%c",
                        &lat_raw, &ns, &lon_raw, &ew);
    if (parsed != 4) {
        ESP_LOGW(TAG, "GPS fix not available yet");
        return false;
    }

    // Convert DDMM.MMMM to decimal degrees
    int lat_deg = (int)(lat_raw / 100);
    float lat_min = lat_raw - (lat_deg * 100);
    *latitude = lat_deg + (lat_min / 60.0f);
    if (ns == 'S') *latitude = -*latitude;

    int lon_deg = (int)(lon_raw / 100);
    float lon_min = lon_raw - (lon_deg * 100);
    *longitude = lon_deg + (lon_min / 60.0f);
    if (ew == 'W') *longitude = -*longitude;

    ESP_LOGI(TAG, "GPS fix: %.6f, %.6f", *latitude, *longitude);
    return true;
}

// ─── Connect to MQTT broker via SSL ─────────────────
bool sim7600_mqtt_connect(void)
{
    ESP_LOGI(TAG, "Connecting to MQTT broker...");

    // Set APN for Hologram
    at_send("AT+CGDCONT=1,\"IP\",\"hologram\"", "OK", 2000);

    // Activate data connection
    at_send("AT+CGACT=1,1", "OK", 5000);

    // Set MQTT parameters
    at_send("AT+CMQTTSTART", "OK", 3000);

    char cmd[256];

    // Acquire client
    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTACCQ=0,\"jeep_esp32\",1");
    at_send(cmd, "OK", 2000);

    // Set SSL context
    at_send("AT+CMQTTSSLCFG=0,0", "OK", 1000);

    // Set credentials
    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTCONNPARA=0,\"%s\",\"%s\"",
             MQTT_USER, MQTT_PASS);
    at_send(cmd, "OK", 2000);

    // Connect to broker
    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTCONN=0,\"%s\",%d,60,1",
             MQTT_BROKER, MQTT_PORT);
    if (at_send(cmd, "+CMQTTCONN: 0,0", 10000)) {
        ESP_LOGI(TAG, "MQTT connected");
        return true;
    }

    ESP_LOGE(TAG, "MQTT connection failed");
    return false;
}

// ─── Publish to MQTT topic ──────────────────────────
bool sim7600_mqtt_publish(const char *topic, const char *payload)
{
    char cmd[256];

    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTTOPIC=0,%d", strlen(topic));
    at_send(cmd, ">", 1000);
    uart_write_bytes(SIM_UART_NUM, topic, strlen(topic));
    vTaskDelay(pdMS_TO_TICKS(500));

    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTPAYLOAD=0,%d", strlen(payload));
    at_send(cmd, ">", 1000);
    uart_write_bytes(SIM_UART_NUM, payload, strlen(payload));
    vTaskDelay(pdMS_TO_TICKS(500));

    if (at_send("AT+CMQTTPUB=0,1,60", "OK", 3000)) {
        ESP_LOGI(TAG, "Published to %s: %s", topic, payload);
        return true;
    }

    ESP_LOGE(TAG, "Publish failed");
    return false;
}

// ─── Subscribe to MQTT topic ────────────────────────
bool sim7600_mqtt_subscribe(const char *topic)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "AT+CMQTTSUB=0,\"%s\",1", topic);
    if (at_send(cmd, "OK", 3000)) {
        ESP_LOGI(TAG, "Subscribed to %s", topic);
        return true;
    }
    return false;
}

// ─── Main SIM7600 FreeRTOS task ─────────────────────
void sim7600_task(void *pvParameters)
{
    char buf[512] = {0};

    sim7600_power_on();
    sim7600_mqtt_connect();
    sim7600_mqtt_subscribe(TOPIC_GPS_REQ);

    ESP_LOGI(TAG, "Waiting for GPS requests...");

    while (1) {
        // Check for incoming MQTT messages
        int len = uart_read_bytes(SIM_UART_NUM, buf,
                                  sizeof(buf) - 1,
                                  pdMS_TO_TICKS(1000));
        if (len > 0) {
            buf[len] = '\0';

            // Check if it's a GPS request
            if (strstr(buf, TOPIC_GPS_REQ)) {
                ESP_LOGI(TAG, "GPS request received");

                float lat, lon;
                if (sim7600_get_gps(&lat, &lon)) {
                    char payload[64];
                    snprintf(payload, sizeof(payload),
                             "{\"lat\":%.6f,\"lon\":%.6f}",
                             lat, lon);
                    sim7600_mqtt_publish(TOPIC_GPS_DATA,
                                        payload);
                } else {
                    sim7600_mqtt_publish(TOPIC_GPS_DATA,
                                        "{\"error\":\"no_fix\"}");
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}