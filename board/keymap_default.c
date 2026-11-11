#include "keymap.h"
#include "keycodes.h"
#include "b68_keys.h"
#include "platform.h"

// clang-format off

// The matrix cells were mapped on the board with the probe layout (docs/keyboards/yunzii-b68.md): 15 + 15 + 14 + 14 + 9
// cells over 16 columns and 5 rows, i.e. 66 keys and the push of the rotary encoder (c15r0). LAYOUT_B68 lists them
// left to right, top to bottom; the names are K<column>_<row>. The three keys under the encoder (c15r1..r3) are
// assumed to be Del, PgUp and PgDn.
#define LAYOUT_B68( \
    K00_0, K01_0, K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, K15_0, \
    K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, K15_1, \
    K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, K13_2, K15_2, \
    K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, K13_3, K14_3, K15_3, \
    K00_4, K01_4, K02_4, K05_4, K08_4, K09_4, K13_4, K14_4, K15_4 \
) { \
    { K00_0, K01_0, K02_0, K03_0, K04_0, K05_0, K06_0, K07_0, K08_0, K09_0, K10_0, K11_0, K12_0, K13_0, KC_NO, K15_0 }, \
    { K00_1, K01_1, K02_1, K03_1, K04_1, K05_1, K06_1, K07_1, K08_1, K09_1, K10_1, K11_1, K12_1, K13_1, KC_NO, K15_1 }, \
    { K00_2, K01_2, K02_2, K03_2, K04_2, K05_2, K06_2, K07_2, K08_2, K09_2, K10_2, K11_2, KC_NO, K13_2, KC_NO, K15_2 }, \
    { K00_3, K01_3, K02_3, K03_3, K04_3, K05_3, K06_3, K07_3, K08_3, K09_3, K10_3, KC_NO, KC_NO, K13_3, K14_3, K15_3 }, \
    { K00_4, K01_4, K02_4, KC_NO, KC_NO, K05_4, KC_NO, KC_NO, K08_4, K09_4, KC_NO, KC_NO, KC_NO, K13_4, K14_4, K15_4 } \
}

#define BASE 0
#define FN   1
#define ACC  2 // the accented letters: reached with a one-shot (OSL) from the key right of the space bar
#define DEFAULT_LAYERS 3

static const ROM uint16_t keymaps[DEFAULT_LAYERS][MATRIX_ROWS][MATRIX_COLS] = {
    // Base layer. The encoder push (top right) is tap dance 0: tap mute, hold play/pause, double tap next track.
    [BASE] = LAYOUT_B68(
        KC_ESC,  KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINS, KC_EQL, KC_BSPC, TD(0),
        KC_TAB,  KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSLS, KC_DEL,
        KC_CAPS, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_ENT, KC_PGUP,
        KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT, KC_UP, KC_PGDN,
        KC_LCTL, KC_LGUI, KC_LALT, KC_SPC, OSL(ACC), MO(FN), KC_LEFT, KC_DOWN, KC_RGHT
    ),
    // Fn layer: F1-F12 on the number row, lighting on Q..Y (effect -/+, brightness -/+, speed -/+), plain colour on U I,
    // sleep time on O P, Bluetooth slots on B N M, battery level on Backspace, media on the arrows.
    [FN] = LAYOUT_B68(
        KC_GRV,    KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, B68_BATTERY, KC_TRNS,
        B68_SLEEP, B68_FX_PREV, B68_FX_NEXT, B68_BRI_DN, B68_BRI_UP, B68_SPD_DN, B68_SPD_UP, B68_COL_DN, B68_COL_UP, B68_SLP_DN, B68_SLP_UP, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS,   B68_OS_LNX, KC_TRNS, KC_TRNS, KC_TRNS, B68_LK_1, B68_LK_2, B68_LK_3, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_VOLU, KC_TRNS,
        KC_TRNS,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_MPRV, KC_VOLD, KC_MNXT
    ),
    // Accented letters: one-shot from the key right of the space bar (OSL), so the layer lasts for ONE key. The six keys are on the vowels (a e r u i o of a
    // QWERTY board: e grave on E, e acute on R); everything else is transparent, so any other key is typed as usual and ends the layer.
    [ACC] = LAYOUT_B68(
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, B68_ACC_EG, B68_ACC_EA, KC_TRNS, KC_TRNS, B68_ACC_U, B68_ACC_I, B68_ACC_O, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, B68_ACC_A, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
    ),
};

// Encoder, {counter-clockwise, clockwise}: volume on the base layer, the Fn layer leaves it as it is.
static const ROM uint16_t encoder_keys[DEFAULT_LAYERS][2] = {
    [BASE] = {KC_VOLD, KC_VOLU},
    [FN]   = {KC_TRNS, KC_TRNS},
    [ACC]  = {KC_TRNS, KC_TRNS},
};

// The Win/Mac switch (board/os_switch.c) makes KEYMAP_MAC_LAYER the default layer while it is on "Mac". That layer is the base one with
// the two modifier keys swapped, which is what the stock firmware does with its second set of keymap tables: on a Mac the key where a
// Windows keyboard has the Windows key is the Command key, and the one where it has Alt is Option. Everything else is the base layer, so
// a fresh profile is complete on both sides of the switch and Vial edits the Mac layout like any other layer.
// LGUI <-> LALT and RGUI <-> RALT are one bit apart (0xE3/0xE2 and 0xE7/0xE6), so the swap is an exclusive-or of that bit
static uint16_t os_swap(uint16_t keycode)
{
    if (keycode == KC_LGUI || keycode == KC_LALT || keycode == KC_RGUI || keycode == KC_RALT) {
        return (uint16_t)(keycode ^ 1u);
    }

    return keycode;
}

uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    // The Mac side: the same keyboard with its own Fn layer, so everything on that side can be different (and is edited in Vial there).
    if (layer == KEYMAP_MAC_FN_LAYER) {
        return keymaps[FN][row][col];
    }

    if (layer == KEYMAP_MAC_LAYER) {
        const uint16_t base = keymaps[BASE][row][col];

        if (base == MO(FN)) {
            return MO(KEYMAP_MAC_FN_LAYER); // Fn goes to the Fn layer of this side
        }

        return os_swap(base);
    }

    return (layer < DEFAULT_LAYERS) ? keymaps[layer][row][col] : KC_TRANSPARENT;
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)encoder; // one encoder

    if (layer == KEYMAP_MAC_LAYER) {
        layer = BASE; // the knob does the same on both sides of the switch
    } else if (layer == KEYMAP_MAC_FN_LAYER) {
        layer = FN;
    }

    return (layer < DEFAULT_LAYERS) ? encoder_keys[layer][clockwise ? 1 : 0] : KC_TRANSPARENT;
}

// Tap dances, {on tap, on hold, on double tap, on tap-then-hold, tapping term (0 = the general one)}.
// TD(0) is the encoder push: tap = mute, hold = play/pause, double tap = next track.
uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field)
{
    static const ROM uint16_t td0[TD_FIELDS] = {KC_MUTE, KC_MPLY, KC_MNXT, 0, 0};

    return (index == 0) ? td0[field] : 0;
}

// Combos by default: none. Vial's Combos tab fills them in (core/combo.c matches them).
uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

// Key overrides by default: none (Vial's Key Overrides tab fills them in, core/override.c acts on them).
uint16_t keymap_default_override(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

// Settings of a profile: LED effect (0 = radial), brightness, speed, tapping term in ms, plain colour (0 = white)
uint16_t keymap_default_cfg(uint8_t field)
{
    static const ROM uint16_t cfg[KEYMAP_CFG_COUNT] = {0, 160, 4, 200, 0};

    return (field < KEYMAP_CFG_COUNT) ? cfg[field] : 0;
}
