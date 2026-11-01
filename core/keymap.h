#pragma once

// The keymap held in RAM: KEYMAP_LAYERS layers of MATRIX_ROWS x MATRIX_COLS keycodes, plus the two rotation
// keycodes of the encoder on each layer. It starts from the compiled-in default; later phases load and save it
// through the profile store.

#include "matrix.h"
#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_LAYERS 8
#define ENCODER_COUNT 1

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

// The general tapping term (ms) of tap-or-hold keys: how long a key is held to count as a hold.
#define KEYMAP_TAPPING_TERM_DEFAULT 200u
uint16_t keymap_tapping_term(void);
void     keymap_set_tapping_term(uint16_t ms);

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
