#pragma once

// The keymap held in RAM: KEYMAP_LAYERS layers of MATRIX_ROWS x MATRIX_COLS keycodes, plus the two rotation
// keycodes of the encoder on each layer. It starts from the compiled-in default; later phases load and save it
// through the profile store.

#include "matrix.h"
#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_LAYERS 8
#define ENCODER_COUNT 1

// The two layers of the Mac side of the Win/Mac switch (board/os_switch.c): KEYMAP_MAC_LAYER is the base one (a copy of the base layer
// with Alt and GUI swapped) and KEYMAP_MAC_FN_LAYER is its own Fn layer, so the switch picks between two complete keyboards, each with a
// Fn layer, instead of just swapping two keys. Both are compiled in by the board's keymap_default() and editable in Vial like any other
// layer. Switching changes which layer the lookups fall back to; it does not touch what is stored.
#define KEYMAP_MAC_LAYER    7
#define KEYMAP_MAC_FN_LAYER 6

// Provided by the board (or a test): the compiled-in keymap. A layer it does not define answers KC_TRANSPARENT.
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col);
uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise);

// Loads the default into RAM.
void keymap_init(void);

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col);
uint16_t keymap_get_encoder(uint8_t layer, uint8_t encoder, bool clockwise);

// False for a position that does not exist.
bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);
bool keymap_set_encoder(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode);

// Counts the edits, wrapping at 256: a reader compares it to know when to look again.
uint8_t keymap_version(void);

// ---- tap dances: a tap / hold / double tap on one key, edited from Vial's Tap Dance tab ----
#define KEYMAP_TAP_DANCE_COUNT 8

// The five words of a tap dance
#define TD_ON_TAP        0 // a tap
#define TD_ON_HOLD       1 // held past the tapping term (empty: the "on tap" action is held instead)
#define TD_ON_DOUBLE_TAP 2 // a second tap within the term
#define TD_ON_TAP_HOLD   3 // tap then hold: stored for Vial, not acted on
#define TD_TERM          4 // tapping term in ms for this dance (0 = the general one)
#define TD_FIELDS        5

// Provided by the board (or a test): the compiled-in tap dances.
uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field);

uint16_t keymap_tap_dance_get(uint8_t index, uint8_t field);
bool     keymap_tap_dance_set(uint8_t index, uint8_t field, uint16_t value);

// ---- combos: keys pressed together give one other key, edited from Vial's Combos tab ----
// The wire format of Vial's combo entry (see docs/vial.md): up to four input keycodes and one output, in words. A combo with an empty output
// (or no inputs) is not used. The matching engine is in combo.c.
#define KEYMAP_COMBO_COUNT 8
#define COMBO_INPUTS       4
#define COMBO_FIELDS       (COMBO_INPUTS + 1) // input 0-3, then the output

// Provided by the board (or a test): the compiled-in combos (none).
uint16_t keymap_default_combo(uint8_t index, uint8_t field);

uint16_t keymap_combo_get(uint8_t index, uint8_t field);
bool     keymap_combo_set(uint8_t index, uint8_t field, uint16_t value);

// ---- settings that belong to a profile, as words ----
#define KEYMAP_CFG_COUNT 5
#define CFG_LED_EFFECT   0
#define CFG_LED_BRIGHT   1
#define CFG_LED_SPEED    2
#define CFG_TAP_TERM     3 // ms, 0 = KEYMAP_TAPPING_TERM_DEFAULT
#define CFG_LED_COLOR    4 // colour of the plain (solid) effect: 0 white, 1-16 the colour wheel in steps of 16

// Provided by the board (or a test): the compiled-in value of a setting.
uint16_t keymap_default_cfg(uint8_t field);

uint16_t keymap_cfg_get(uint8_t field);
bool     keymap_cfg_set(uint8_t field, uint16_t value);

// The general tapping term (ms) of tap-or-hold keys: how long a key is held to count as a hold.
#define KEYMAP_TAPPING_TERM_DEFAULT 200u
uint16_t keymap_tapping_term(void);
void     keymap_set_tapping_term(uint16_t ms);

// Back to the compiled-in keys, encoders, tap dances and settings. The macros stay as they are.
void keymap_reset(void);

// ---- macros: the buffer Vial reads and writes ----
// Macros back to back, each ended by a 0 byte; the interpreter is in macro.h.
#define KEYMAP_MACRO_COUNT 16
#define KEYMAP_MACRO_BYTES 512

// A byte of the buffer. 0 past the end.
uint8_t keymap_macro_byte(uint16_t offset);

// Reads and writes past the end are cut short.
void keymap_macro_read(uint16_t offset, uint8_t size, uint8_t *dst);
void keymap_macro_write(uint16_t offset, uint8_t size, const uint8_t *src);

// Empties all macros.
void keymap_macro_clear(void);
