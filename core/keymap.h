#pragma once

// The keymap held in RAM: KEYMAP_LAYERS layers of MATRIX_ROWS x MATRIX_COLS keycodes, plus the two rotation
// keycodes of the encoder on each layer. It starts from the compiled-in default; store.h loads and saves it.
// The cfg fields of the lighting are held here in RAM too, but settings.h saves them (see keymap_cfg_set).

#include "matrix.h"
#include <stdbool.h>
#include <stdint.h>

#define KEYMAP_LAYERS 6
#define ENCODER_COUNT 1

// The two layers of the Mac side of the Win/Mac switch (board/os_switch.c): KEYMAP_MAC_LAYER is the base one (a copy of the base layer
// with Alt and GUI swapped) and KEYMAP_MAC_FN_LAYER is its own Fn layer, so the switch picks between two complete keyboards, each with a
// Fn layer, instead of just swapping two keys. Both are compiled in by the board's keymap_default() and editable in Vial like any other
// layer. Switching changes which layer the lookups fall back to; it does not touch what is stored.
#define KEYMAP_MAC_LAYER    5
#define KEYMAP_MAC_FN_LAYER 4

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
#define KEYMAP_COMBO_COUNT 6
#define COMBO_INPUTS       4
#define COMBO_FIELDS       (COMBO_INPUTS + 1) // input 0-3, then the output

// Provided by the board (or a test): the compiled-in combos (none).
uint16_t keymap_default_combo(uint8_t index, uint8_t field);

uint16_t keymap_combo_get(uint8_t index, uint8_t field);
bool     keymap_combo_set(uint8_t index, uint8_t field, uint16_t value);

// ---- key overrides: a key with some modifiers held does something else, edited from Vial's Key Overrides tab ----
// Vial's entry is 10 bytes: trigger keycode, replacement keycode, the layers it applies on (0xFFFF = all), the modifiers it needs, the ones
// that must not be held, the ones to take away while it acts, and the option bits (bit 7 = enabled). Kept as five words.
#define KEYMAP_KEY_OVERRIDE_COUNT 4
#define KO_TRIGGER     0
#define KO_REPLACEMENT 1
#define KO_LAYERS      2
#define KO_MODS        3 // low byte: required, high byte: must-not-be-held
#define KO_ACTION      4 // low byte: suppressed, high byte: options
#define KO_FIELDS      5

#define KO_OPTIONS_ENABLED 0x80u

// Provided by the board (or a test): the compiled-in key overrides (none).
uint16_t keymap_default_override(uint8_t index, uint8_t field);

uint16_t keymap_override_get(uint8_t index, uint8_t field);
bool     keymap_override_set(uint8_t index, uint8_t field, uint16_t value);

// ---- settings that belong to a profile, as words ----
#define KEYMAP_CFG_COUNT 5
#define CFG_LED_EFFECT   0
#define CFG_LED_BRIGHT   1
#define CFG_LED_SPEED    2
#define CFG_TAP_TERM     3 // ms, 0 = KEYMAP_TAPPING_TERM_DEFAULT
#define CFG_LED_COLOR    4 // colour of the plain effect: the hue in the low byte, the saturation in the high one (0 = white)

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

// ---- the keymap as a run of 16-bit words, and the list of the words that changed ----
// The store keeps the keymap in flash as a run of words in this order: the keys (layer, row, column), the encoders (layer, encoder,
// direction), the macros (two bytes a word, little-endian), the tap dances (index, field), the combos, the key overrides, the settings.
// A word is named by its offset in that run. Every edit notes the offset it changed, so that the store can write down just those words
// (a record of the log) instead of the whole keymap (four sector erases, ~130 ms of dark backlight).
#define KEYMAP_KEY_WORDS     ((uint16_t)(KEYMAP_LAYERS * MATRIX_ROWS * MATRIX_COLS))
#define KEYMAP_ENCODER_WORDS ((uint16_t)(KEYMAP_LAYERS * ENCODER_COUNT * 2))
#define KEYMAP_MACRO_WORDS   ((uint16_t)(KEYMAP_MACRO_BYTES / 2))
#define KEYMAP_TD_WORDS      ((uint16_t)(KEYMAP_TAP_DANCE_COUNT * TD_FIELDS))
#define KEYMAP_COMBO_WORDS   ((uint16_t)(KEYMAP_COMBO_COUNT * COMBO_FIELDS))
#define KEYMAP_KO_WORDS      ((uint16_t)(KEYMAP_KEY_OVERRIDE_COUNT * KO_FIELDS))
#define KEYMAP_CFG_WORDS     ((uint16_t)KEYMAP_CFG_COUNT)
#define KEYMAP_WORDS \
    ((uint16_t)(KEYMAP_KEY_WORDS + KEYMAP_ENCODER_WORDS + KEYMAP_MACRO_WORDS + KEYMAP_TD_WORDS + KEYMAP_COMBO_WORDS + KEYMAP_KO_WORDS + KEYMAP_CFG_WORDS))

// The word at an offset, and a word put there directly (no version bump, no note: this is how the store loads). Offsets past the end read 0.
uint16_t keymap_word_get(uint16_t offset);
void     keymap_word_set(uint16_t offset, uint16_t value);

// The offsets edited since the list was cleared, each once. `keymap_changes_overflow()` is true when there were too many to list (or the
// whole keymap was replaced: reset, defaults, macros cleared): the store then writes everything.
#define KEYMAP_CHANGE_MAX 24
uint8_t  keymap_changes(void);
uint16_t keymap_change_at(uint8_t index);
bool     keymap_changes_overflow(void);
void     keymap_changes_clear(void);
void     keymap_changes_all(void); // makes the next save a whole one

