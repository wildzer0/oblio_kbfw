#include "keymap.h"
#include "keycodes.h"

static uint16_t keys[KEYMAP_LAYERS][MATRIX_ROWS][MATRIX_COLS];
static uint16_t encoder_keys[KEYMAP_LAYERS][ENCODER_COUNT][2];
static uint16_t tap_dances[KEYMAP_TAP_DANCE_COUNT][TD_FIELDS];
static uint16_t cfg[KEYMAP_CFG_COUNT];
static uint8_t  macros[KEYMAP_MACRO_BYTES];
static uint8_t  version;

static void load_defaults(void)
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

    for (uint8_t index = 0; index < KEYMAP_TAP_DANCE_COUNT; index++) {
        for (uint8_t field = 0; field < TD_FIELDS; field++) {
            tap_dances[index][field] = keymap_default_tap_dance(index, field);
        }
    }

    for (uint8_t field = 0; field < KEYMAP_CFG_COUNT; field++) {
        cfg[field] = keymap_default_cfg(field);
    }
}

void keymap_init(void)
{
    load_defaults();

    for (uint16_t i = 0; i < KEYMAP_MACRO_BYTES; i++) {
        macros[i] = 0;
    }

    version++;
}

void keymap_reset(void)
{
    load_defaults();
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

uint16_t keymap_tap_dance_get(uint8_t index, uint8_t field)
{
    if (index >= KEYMAP_TAP_DANCE_COUNT || field >= TD_FIELDS) {
        return 0;
    }

    return tap_dances[index][field];
}

bool keymap_tap_dance_set(uint8_t index, uint8_t field, uint16_t value)
{
    if (index >= KEYMAP_TAP_DANCE_COUNT || field >= TD_FIELDS) {
        return false;
    }

    tap_dances[index][field] = value;
    version++;
    return true;
}

uint16_t keymap_cfg_get(uint8_t field)
{
    return (field < KEYMAP_CFG_COUNT) ? cfg[field] : 0;
}

bool keymap_cfg_set(uint8_t field, uint16_t value)
{
    if (field >= KEYMAP_CFG_COUNT) {
        return false;
    }

    cfg[field] = value;
    version++;
    return true;
}

uint16_t keymap_tapping_term(void)
{
    return cfg[CFG_TAP_TERM] ? cfg[CFG_TAP_TERM] : KEYMAP_TAPPING_TERM_DEFAULT;
}

void keymap_set_tapping_term(uint16_t ms)
{
    keymap_cfg_set(CFG_TAP_TERM, ms);
}

uint8_t keymap_macro_byte(uint16_t offset)
{
    return (offset < KEYMAP_MACRO_BYTES) ? macros[offset] : 0;
}

void keymap_macro_read(uint16_t offset, uint8_t size, uint8_t *dst)
{
    for (uint8_t i = 0; i < size; i++) {
        dst[i] = keymap_macro_byte((uint16_t)(offset + i));
    }
}

void keymap_macro_write(uint16_t offset, uint8_t size, const uint8_t *src)
{
    for (uint8_t i = 0; i < size; i++) {
        const uint16_t at = (uint16_t)(offset + i);

        if (at < KEYMAP_MACRO_BYTES) {
            macros[at] = src[i];
        }
    }

    version++;
}

void keymap_macro_clear(void)
{
    for (uint16_t i = 0; i < KEYMAP_MACRO_BYTES; i++) {
        macros[i] = 0;
    }

    version++;
}
