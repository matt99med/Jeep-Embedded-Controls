#include "espnow_keyfob.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ESPNOW_KEYFOB";

static void espnow_send_cb(const esp_now_send_info_t *tx_info,
                           esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Command sent successfully");
    } else {
        ESP_LOGW(TAG, "Command send failed");
    }
}

void espnow_keyfob_init(void)
{
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init WiFi in station mode
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Init ESP-NOW
    esp_now_init();
    esp_now_register_send_cb(espnow_send_cb);

    // Register main controller as peer
    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    uint8_t main_mac[] = MAIN_CTRL_MAC;
    memcpy(peer.peer_addr, main_mac, 6);
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "ESP-NOW keyfob initialised");
}

bool espnow_keyfob_send(keyfob_cmd_t cmd)
{
    espnow_message_t msg = {
        .command  = cmd,
        .checksum = cmd ^ 0xAA,
    };

    uint8_t main_mac[] = MAIN_CTRL_MAC;
    esp_err_t result = esp_now_send(main_mac,
                                    (uint8_t *)&msg,
                                    sizeof(msg));
    return result == ESP_OK;
}