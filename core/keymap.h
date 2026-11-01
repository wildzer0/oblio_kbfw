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
