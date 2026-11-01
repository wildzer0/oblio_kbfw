#pragma once

// The keyboard HUD protocol: lets a desktop overlay show the keyboard's live state (layers, pressed keys, modifiers, lock LEDs). It rides
// on the VIA / Vial raw HID interface as command 0xF0 and only ever answers requests: nothing is sent unasked and nothing is stored.
// The description is in docs/hud-protocol-v2.md; the B68 has 16 columns, so it announces protocol 2.
//
// Every packet is 32 bytes: [0xF0, command, status, sequence, args... / reply...]; multi-byte fields are little-endian.
//   0x01 GET_INFO    protocol, rows, columns, layers, capability bits, HUD_TOG keycode, keymap version, bytes per row
//   0x02 GET_STATE   layer mask, default layer, pressed keys (the first 8 columns), mods, one-shots, flags, toggle counter, keymap version
//   0x03 GET_KEYMAP  args: layer, offset (16 bit, row * columns + column), count (<= 12); reply: the keycodes
//   0x04 GET_MATRIX  (protocol 2) the whole pressed-key matrix, rows * ceil(columns / 8) bytes

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#define HUD_CMD 0xF0u

// The keycode of the HUD_TOG key (the first of the board's own keycodes); its presses are counted, not sent to the host.
#define HUD_TOGGLE_KEYCODE 0x7E00u

// Provided by the board (or a test): the LED report of the host (bit 0 num lock, bit 1 caps lock).
uint8_t hud_hook_host_leds(void);

// Handles a request in `data` (32 bytes) and turns it into the reply. False if `data` is not a HUD packet.
bool hud_receive(XDATA uint8_t *data, uint8_t length);

// The HUD_TOG key went down: the overlay sees the counter change.
void hud_toggle_pressed(void);
