#include "keymap.h"
#include "keycodes.h"
#include "platform.h"

// The whole keymap is one run of words, in the order of the store's payload (see keymap.h): an offset is then simply an index, and the store
// loads and saves it word by word. The tables below are views of that run (the macro bytes are its words, low byte first).
static XDATA uint16_t words[KEYMAP_WORDS];

#define OFF_ENCODERS KEYMAP_KEY_WORDS
#define OFF_MACROS   ((uint16_t)(OFF_ENCODERS + KEYMAP_ENCODER_WORDS))
#define OFF_TD       ((uint16_t)(OFF_MACROS + KEYMAP_MACRO_WORDS))
#define OFF_COMBOS   ((uint16_t)(OFF_TD + KEYMAP_TD_WORDS))
#define OFF_KO       ((uint16_t)(OFF_COMBOS + KEYMAP_COMBO_WORDS))
#define OFF_CFG      ((uint16_t)(OFF_KO + KEYMAP_KO_WORDS))

// Element (i, j, k) of a table, as the word it is in the run
#define KEYW(l, r, c)   words[((uint16_t)(l) * MATRIX_ROWS + (r)) * MATRIX_COLS + (c)]
#define ENCW(l, e, d)   words[OFF_ENCODERS + ((uint16_t)(l) * ENCODER_COUNT + (e)) * 2 + (d)]
#define TDW(i, f)       words[OFF_TD + (uint16_t)(i) * TD_FIELDS + (f)]
#define COMBOW(i, f)    words[OFF_COMBOS + (uint16_t)(i) * COMBO_FIELDS + (f)]
#define KOW(i, f)       words[OFF_KO + (uint16_t)(i) * KO_FIELDS + (f)]
#define CFGW(f)         words[OFF_CFG + (f)]
#define macros          ((XDATA uint8_t *)(words + OFF_MACROS))

static uint8_t  version;

// The words edited since the store last looked (see keymap.h).
static uint16_t changed[KEYMAP_CHANGE_MAX];
static uint8_t  changed_count;
static bool     changed_overflow;

static void note(uint16_t offset)
{
    for (uint8_t i = 0; i < changed_count; i++) {
        if (changed[i] == offset) {
            return; // once is enough: the store reads the value when it writes
        }
    }

    if (changed_count < KEYMAP_CHANGE_MAX) {
        changed[changed_count++] = offset;
    } else {
        changed_overflow = true;
    }
}

static void load_defaults(void)
{
    for (uint8_t layer = 0; layer < KEYMAP_LAYERS; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                KEYW(layer, row, col) = keymap_default(layer, row, col);
            }
        }

        for (uint8_t enc = 0; enc < ENCODER_COUNT; enc++) {
            ENCW(layer, enc, 0) = keymap_default_encoder(layer, enc, false);
            ENCW(layer, enc, 1) = keymap_default_encoder(layer, enc, true);
        }
    }

    for (uint8_t index = 0; index < KEYMAP_TAP_DANCE_COUNT; index++) {
        for (uint8_t field = 0; field < TD_FIELDS; field++) {
            TDW(index, field) = keymap_default_tap_dance(index, field);
        }
    }

    for (uint8_t index = 0; index < KEYMAP_COMBO_COUNT; index++) {
        for (uint8_t field = 0; field < COMBO_FIELDS; field++) {
            COMBOW(index, field) = keymap_default_combo(index, field);
        }
    }

    for (uint8_t index = 0; index < KEYMAP_KEY_OVERRIDE_COUNT; index++) {
        for (uint8_t field = 0; field < KO_FIELDS; field++) {
            KOW(index, field) = keymap_default_override(index, field);
        }
    }

    for (uint8_t field = 0; field < KEYMAP_CFG_COUNT; field++) {
        CFGW(field) = keymap_default_cfg(field);
    }
}

void keymap_init(void)
{
    load_defaults();
    changed_overflow = true;

    for (uint16_t i = 0; i < KEYMAP_MACRO_BYTES; i++) {
        macros[i] = 0;
    }

    version++;
}

void keymap_reset(void)
{
    load_defaults();
    changed_overflow = true;
    version++;
}

uint16_t keymap_get(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }

    return KEYW(layer, row, col);
}

uint16_t keymap_get_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    if (layer >= KEYMAP_LAYERS || encoder >= ENCODER_COUNT) {
        return KC_NO;
    }

    return ENCW(layer, encoder, clockwise ? 1 : 0);
}

bool keymap_set(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYERS || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }

    KEYW(layer, row, col) = keycode;
    note((uint16_t)(((uint16_t)layer * MATRIX_ROWS + row) * MATRIX_COLS + col));
    version++;
    return true;
}

bool keymap_set_encoder(uint8_t layer, uint8_t encoder, bool clockwise, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYERS || encoder >= ENCODER_COUNT) {
        return false;
    }

    ENCW(layer, encoder, clockwise ? 1 : 0) = keycode;
    note((uint16_t)(OFF_ENCODERS + ((uint16_t)layer * ENCODER_COUNT + encoder) * 2 + (clockwise ? 1 : 0)));
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

    return TDW(index, field);
}

bool keymap_tap_dance_set(uint8_t index, uint8_t field, uint16_t value)
{
    if (index >= KEYMAP_TAP_DANCE_COUNT || field >= TD_FIELDS) {
        return false;
    }

    TDW(index, field) = value;
    note((uint16_t)(OFF_TD + (uint16_t)index * TD_FIELDS + field));
    version++;
    return true;
}

uint16_t keymap_combo_get(uint8_t index, uint8_t field)
{
    if (index >= KEYMAP_COMBO_COUNT || field >= COMBO_FIELDS) {
        return 0;
    }

    return COMBOW(index, field);
}

bool keymap_combo_set(uint8_t index, uint8_t field, uint16_t value)
{
    if (index >= KEYMAP_COMBO_COUNT || field >= COMBO_FIELDS) {
        return false;
    }

    COMBOW(index, field) = value;
    note((uint16_t)(OFF_COMBOS + (uint16_t)index * COMBO_FIELDS + field));
    version++;
    return true;
}

uint16_t keymap_override_get(uint8_t index, uint8_t field)
{
    if (index >= KEYMAP_KEY_OVERRIDE_COUNT || field >= KO_FIELDS) {
        return 0;
    }

    return KOW(index, field);
}

bool keymap_override_set(uint8_t index, uint8_t field, uint16_t value)
{
    if (index >= KEYMAP_KEY_OVERRIDE_COUNT || field >= KO_FIELDS) {
        return false;
    }

    KOW(index, field) = value;
    note((uint16_t)(OFF_KO + (uint16_t)index * KO_FIELDS + field));
    version++;
    return true;
}

uint16_t keymap_cfg_get(uint8_t field)
{
    return (field < KEYMAP_CFG_COUNT) ? CFGW(field) : 0;
}

bool keymap_cfg_set(uint8_t field, uint16_t value)
{
    if (field >= KEYMAP_CFG_COUNT) {
        return false;
    }

    CFGW(field) = value;

    // The lighting (effect, brightness, speed, colour) is kept in the settings log, which is cheap to write (an append), and not in the keymap
    // store, whose save erases four sectors (~130 ms of dark backlight). So it is not an edit of the keymap: the version does not move.
    if (field != CFG_LED_EFFECT && field != CFG_LED_BRIGHT && field != CFG_LED_SPEED && field != CFG_LED_COLOR) {
        note((uint16_t)(OFF_CFG + field));
        version++;
    }

    return true;
}

uint16_t keymap_tapping_term(void)
{
    return CFGW(CFG_TAP_TERM) ? CFGW(CFG_TAP_TERM) : KEYMAP_TAPPING_TERM_DEFAULT;
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
            note((uint16_t)(OFF_MACROS + at / 2u));
        }
    }

    version++;
}

void keymap_macro_clear(void)
{
    for (uint16_t i = 0; i < KEYMAP_MACRO_BYTES; i++) {
        macros[i] = 0;
    }

    changed_overflow = true;
    version++;
}

// ---- words by offset, and the list of what changed ----
uint16_t keymap_word_get(uint16_t offset)
{
    return (offset < KEYMAP_WORDS) ? words[offset] : 0;
}

void keymap_word_set(uint16_t offset, uint16_t value)
{
    if (offset < KEYMAP_WORDS) {
        words[offset] = value;
    }
}

uint8_t keymap_changes(void)
{
    return changed_count;
}

uint16_t keymap_change_at(uint8_t index)
{
    return (index < changed_count) ? changed[index] : 0;
}

bool keymap_changes_overflow(void)
{
    return changed_overflow;
}

void keymap_changes_clear(void)
{
    changed_count    = 0;
    changed_overflow = false;
}

void keymap_changes_all(void)
{
    changed_overflow = true;
}

