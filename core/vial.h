#pragma once

// The Vial side of the raw HID protocol: commands behind the 0xFE prefix (keyboard id, definition, encoders, unlock, QMK
// settings, tap dances). The VIA commands are in via.h.

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#define VIAL_PROTOCOL_VERSION 6u

// Commands behind the 0xFE prefix
enum {
    vial_get_keyboard_id    = 0x00,
    vial_get_size           = 0x01,
    vial_get_def            = 0x02,
    vial_get_encoder        = 0x03,
    vial_set_encoder        = 0x04,
    vial_get_unlock_status  = 0x05,
    vial_unlock_start       = 0x06,
    vial_unlock_poll        = 0x07,
    vial_lock               = 0x08,
    vial_qmk_settings_query = 0x09,
    vial_qmk_settings_get   = 0x0A,
    vial_qmk_settings_set   = 0x0B,
    vial_qmk_settings_reset = 0x0C,
    vial_dynamic_entry_op   = 0x0D,
};

void vial_init(void); // locked, no unlock in progress

// A Vial request is in `data` (data[0] is the 0xFE prefix, data[1] the command); the reply replaces it.
void vial_receive(XDATA uint8_t *data, uint8_t length);

// Unlocked: the GUI may change macros, read the key matrix and jump to the bootloader.
bool vial_is_unlocked(void);

// While an unlock is in progress only the unlock questions are answered (see via.c).
bool vial_unlock_in_progress(void);

// The board's data, generated at build time by tools/vial_gen.py: the LZMA-compressed vial.json, the 8-byte keyboard id and
// the keys to hold to unlock, as (row, col) pairs (at most 14).
extern const ROM uint8_t vial_definition[];
extern const uint16_t    vial_definition_size; // under 64 KB: served in pages of 32 bytes
extern const ROM uint8_t vial_keyboard_uid[8];
extern const ROM uint8_t vial_unlock_keys[][2];
extern const uint8_t     vial_unlock_key_count;
