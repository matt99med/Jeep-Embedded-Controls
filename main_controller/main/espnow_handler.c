/*
 * espnow_handler.c - receive lock/unlock from the keyfob
 */
#include "config.h"
#include "system_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "espnow";

extern void sim7600_publish_status(const char *status);

static void on_recv(const esp_now_recv_info_t *info,
                    const uint8_t *data, int len)
{
    if (len != sizeof(espnow_msg_t)) {
        ESP_LOGW(TAG, "bad len %d", len);
        return;
    }
    const espnow_msg_t *msg = (const espnow_msg_t *)data;

    /* trivial replay guard: ignore a repeat of the exact last seq */
    if (msg->seq == g_state.last_seq) {
        ESP_LOGD(TAG, "dup seq %u ignored", msg->seq);
        return;
    }
    g_state.last_seq = msg->seq;
    g_state.last_activity_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    switch (msg->cmd) {
    case CMD_LOCK:
        sys_set_armed(true);
        sim7600_publish_status("armed");
        break;
    case CMD_UNLOCK:
        sys_clear_alarm();
        sys_set_armed(false);
        sim7600_publish_status("disarmed");
        break;
    default:
        ESP_LOGW(TAG, "unknown cmd 0x%02x", msg->cmd);
        break;
    }
}

void espnow_handler_init(void)
{
    /* Wi-Fi must be started (STA) for ESP-NOW; we do not associate. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "ESP-NOW ready, MAC %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    /* NOTE: pair this MAC into the keyfob's peer list. */
}