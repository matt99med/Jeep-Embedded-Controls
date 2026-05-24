#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// ─── Keyfob MAC address ─────────────────────────────
// Update this once you flash the keyfob and get its MAC
#define KEYFOB_MAC {0x1c, 0xdb, 0xd4, 0xe0, 0x86, 0xdc}

// ─── Command definitions ────────────────────────────
typedef enum {
    CMD_LOCK    = 0x01,  // Arm siren + engage kill switch
    CMD_UNLOCK  = 0x02,  // Disarm siren + release kill switch
    CMD_PANIC   = 0x03,  // Trigger siren immediately
} keyfob_cmd_t;

// ─── Message structure ──────────────────────────────
typedef struct {
    keyfob_cmd_t command;
    uint8_t      checksum;  // Simple XOR checksum for validation
} espnow_message_t;

// ─── Function prototypes ────────────────────────────
void espnow_handler_init(void);
void espnow_send_status(bool is_armed, bool is_killed);

#endif // ESPNOW_HANDLER_H