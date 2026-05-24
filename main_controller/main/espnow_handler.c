#include "espnow_handler.h"
#include "gpio_handler.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ESPNOW";

// ─── System state ───────────────────────────────────
static bool s_is_armed  = false;
static bool s_is_killed = false;

// ─── Receive callback ───────────────────────────────
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len)
{
    if (len != sizeof(espnow_message_t)) {
        ESP_LOGW(TAG, "Received invalid message length: %d", len);
        return;
    }

    espnow_message_t *msg = (espnow_message_t *)data;

    // Validate checksum
    uint8_t expected = msg->command ^ 0xAA;
    if (msg->checksum != expected) {
        ESP_LOGW(TAG, "Checksum mismatch — ignoring message");
        return;
    }

    switch (msg->command) {

        case CMD_LOCK:
            ESP_LOGI(TAG, "CMD_LOCK received — arming system");
            s_is_armed  = true;
            s_is_killed = true;
            set_ign_kill(true);
            set_siren(false);
            break;

        case CMD_UNLOCK:
            ESP_LOGI(TAG, "CMD_UNLOCK received — disarming system");
            s_is_armed  = false;
            s_is_killed = false;
            set_ign_kill(false);
            set_siren(false);
            break;

        case CMD_PANIC:
            ESP_LOGI(TAG, "CMD_PANIC received — triggering siren");
            set_siren(true);
            break;

        default:
            ESP_LOGW(TAG, "Unknown command: 0x%02X", msg->command);
            break;
    }
}

// ─── Send callback ──────────────────────────────────
static void espnow_send_cb(const esp_now_send_info_t *tx_info,
                           esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS) {
        ESP_LOGW(TAG, "Send failed to keyfob");
    }
}

// ─── Initialise ESP-NOW ─────────────────────────────
void espnow_handler_init(void)
{
    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Init WiFi in station mode — required for ESP-NOW
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Init ESP-NOW
    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);
    esp_now_register_send_cb(espnow_send_cb);

    // Register keyfob as a peer
    esp_now_peer_info_t peer = {
        .channel = 0,
        .ifidx   = WIFI_IF_STA,
        .encrypt = false,
    };
    uint8_t keyfob_mac[] = KEYFOB_MAC;
    memcpy(peer.peer_addr, keyfob_mac, 6);
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "ESP-NOW initialised");
}

// ─── Send armed/killed status back to keyfob ────────
void espnow_send_status(bool is_armed, bool is_killed)
{
    espnow_message_t msg = {
        .command  = is_armed ? CMD_LOCK : CMD_UNLOCK,
        .checksum = (is_armed ? CMD_LOCK : CMD_UNLOCK) ^ 0xAA,
    };
    uint8_t keyfob_mac[] = KEYFOB_MAC;
    esp_now_send(keyfob_mac, (uint8_t *)&msg, sizeof(msg));
}