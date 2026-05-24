#ifndef ESPNOW_KEYFOB_H
#define ESPNOW_KEYFOB_H

#include <stdint.h>
#include <stdbool.h>

// ─── Main controller MAC address ────────────────────
#define MAIN_CTRL_MAC {0xb0, 0xcb, 0xd8, 0xe2, 0x19, 0xdc}

// ─── Command definitions ─────────────────────────────
typedef enum {
    CMD_LOCK   = 0x01,
    CMD_UNLOCK = 0x02,
    CMD_PANIC  = 0x03,
} keyfob_cmd_t;

// ─── Message structure ───────────────────────────────
typedef struct {
    keyfob_cmd_t command;
    uint8_t      checksum;
} espnow_message_t;

// ─── Function prototypes ────────────────────────────
void espnow_keyfob_init(void);
bool espnow_keyfob_send(keyfob_cmd_t cmd);

#endif // ESPNOW_KEYFOB_H