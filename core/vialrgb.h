#pragma once

// Per-key lighting from Vial (its "Lighting" tab): the app paints each key with a colour of its own and this keeps them, so the backlight can be
// a picture instead of an effect. The protocol is in docs/vial.md; the colours live in the keymap (core/keymap.c) so they survive power-off,
// and the effect that shows them is FX_VIAL (core/led_effect.h).
//
// Only two of Vial's effects are offered, "off" and "direct": the other ids belong to QMK's rgb_matrix, whose effects do not exist here (this
// keyboard has its own, on the Fn keys). Vial asks which ids the firmware has (get_supported) and shows only those, so the app tells the truth.

#include "matrix.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

#define VIALRGB_LEDS (MATRIX_ROWS * MATRIX_COLS)

// One LED per cell of the key matrix, in the order row * MATRIX_COLS + col: a cell that is not a key is reported to the app as such, and Vial
// does not paint it.
#define VIALRGB_EFFECT_OFF    0u
#define VIALRGB_EFFECT_DIRECT 1u

void vialrgb_init(void);

// The colour of a key as the app painted it, 0-255 per channel. False when it was never painted (the key stays dark).
bool vialrgb_colour(uint8_t row, uint8_t col, uint8_t out[3]);

// The protocol, on VIA's lighting commands (core/via.c routes them): data[1] is the sub-command from docs/vial.md.
void vialrgb_get_value(XDATA uint8_t *data);
void vialrgb_set_value(XDATA uint8_t *data);
void vialrgb_save(void);

// Provided by the board (or a test): write the keymap down now, because the app asked for it
void vialrgb_hook_save(void);
