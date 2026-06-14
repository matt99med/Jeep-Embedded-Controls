/*
 * espnow_keyfob.c - one-shot ESP-NOW command transmit to the controller.
 */
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "espnow_kf";
static const uint8_t s_peer[6] = CONTROLLER_MAC;
static SemaphoreHandle_t s_sent;
static volatile esp_now_send_status_t s_status;

static void on_sent(const uint8_t *mac, esp_now_send_status_t status)
{
    s_status = status;
    xSemaphoreGive(s_sent);
}

void espnow_kf_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_sent));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_peer, 6);
    peer.channel = 0;          /* use current channel */
    peer.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    s_sent = xSemaphoreCreateBinary();
}

/* Send a command, retrying until the send callback reports success. */
bool espnow_kf_send(uint8_t cmd, uint8_t seq)
{
    espnow_msg_t msg = { .cmd = cmd, .seq = seq };
    for (int i = 0; i < TX_RETRY_COUNT; i++) {
        if (esp_now_send(s_peer, (uint8_t *)&msg, sizeof(msg)) == ESP_OK) {
            if (xSemaphoreTake(s_sent, pdMS_TO_TICKS(200)) == pdTRUE &&
                s_status == ESP_NOW_SEND_SUCCESS) {
                ESP_LOGI(TAG, "cmd 0x%02x sent", cmd);
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    ESP_LOGW(TAG, "cmd 0x%02x failed after retries", cmd);
    return false;
}