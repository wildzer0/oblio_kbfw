#include "keymap.h"
#include "keycodes.h"

static uint16_t keys[KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
static uint16_t encoder_keys[KEYMAP_LAYERS][ENCODER_COUNT][2];
static uint8_t  version;

void keymap_init(void)
{
    for (uint8_t layer = 0; layer < KEYMAP_LAYERS; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                keys[layer][row][col] = keymap_default(layer, row, col);
            }
        }

        for (uint8_t enc = 0; enc < ENCODER_COUNT; enc++) {
            encoder_keys[layer][enc][0] = keymap_default_encoder(layer, enc, false);
            encoder_keys[layer][enc][1] = keymap_default_encoder(layer, enc, true);
        }
    }

    version++;
}

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }

    return keys[layer][row][col];
}

uint16_t keymap_get_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    if (layer >= KEYMAP_LAYERS || encoder >= ENCODER_COUNT) {
        return KC_NO;
    }

    return encoder_keys[layer][encoder][clockwise ? 1 : 0];
}

bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }

    keys[layer][row][col] = keycode;
    version++;
    return true;
}

bool keymap_set_encoder(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYERS || encoder >= ENCODER_COUNT) {
        return false;
    }

    encoder_keys[layer][encoder][clockwise ? 1 : 0] = keycode;
    version++;
    return true;
}

uint8_t keymap_version(void)
{
    return version;
}
