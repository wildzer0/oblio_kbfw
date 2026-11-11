#include "check.h"
#include "b68_keys.h"
#include "keycodes.h"
#include "keymap.h"

// The default keymap against the matrix measured on the board (docs/keyboards/yunzii-b68.md): 67 cells, no more.
static const uint8_t cells_per_row[MATRIX_ROWS] = {15, 15, 14, 14, 9};

int main(void)
{
    keymap_init();

    unsigned total = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        unsigned in_row = 0;

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (keymap_get(0, row, col) != KC_NO) {
                in_row++;
            }
        }

        CHECK(in_row == cells_per_row[row]);
        total += in_row;
    }

    CHECK(total == 67);

    CHECK(keymap_get(0, 0, 0) == KC_ESC);
    CHECK(keymap_get(0, 0, 15) == TD(0)); // the encoder push
    CHECK(keymap_get(0, 4, 9) == MO(1));  // Fn
    CHECK(keymap_get(1, 0, 1) == KC_F1);
    CHECK(keymap_get(1, 1, 2) == B68_FX_NEXT);
    CHECK(keymap_get(2, 0, 0) == KC_TRANSPARENT); // layers the default does not define
    CHECK(keymap_get_encoder(0, 0, true) == KC_VOLU);
    CHECK(keymap_get_encoder(0, 0, false) == KC_VOLD);

    // the Fn layer only sets keys that exist on the board
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (keymap_get(0, row, col) == KC_NO) {
                CHECK(keymap_get(1, row, col) == KC_NO);
            }
        }
    }

    // The Win/Mac switch makes KEYMAP_MAC_LAYER the default while it is on Mac: the base layer with the two modifier keys swapped and
    // nothing else different (board/os_switch.c, board/keymap_default.c)
    unsigned swapped = 0;
    unsigned fnkeys  = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            const uint16_t base = keymap_get(0, row, col);
            const uint16_t mac  = keymap_get(KEYMAP_MAC_LAYER, row, col);

            if (base == KC_LGUI || base == KC_LALT || base == KC_RGUI || base == KC_RALT) {
                CHECK(mac == (uint16_t)(base ^ 1u));
                swapped++;
            } else if (base == MO(1)) {
                CHECK(mac == MO(KEYMAP_MAC_FN_LAYER)); // the Mac side has its own Fn layer
                fnkeys++;
            } else {
                CHECK(mac == base);
            }
        }
    }

    CHECK(swapped == 2); // the bottom row is Ctrl Win Alt ... (the accents key) Fn: the two that are Win or Alt are swapped, Ctrl is not; the one right of the space bar is OSL(2)
    CHECK(fnkeys == 1);  // the Fn key is the only one that goes somewhere else
    // the Mac Fn layer starts as a copy of the Windows one and is edited in Vial from there
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            CHECK(keymap_get(KEYMAP_MAC_FN_LAYER, row, col) == keymap_get(1, row, col));
        }
    }
    CHECK(keymap_get(KEYMAP_MAC_FN_LAYER, 3, 5) == B68_LK_1); // Fn+B is still the Bluetooth key on that side
    CHECK(keymap_get(0, 4, 1) == KC_LGUI && keymap_get(KEYMAP_MAC_LAYER, 4, 1) == KC_LALT); // the key labelled Win sends Alt on a Mac
    CHECK(keymap_get(0, 4, 2) == KC_LALT && keymap_get(KEYMAP_MAC_LAYER, 4, 2) == KC_LGUI); // the one labelled Alt sends Gui/Cmd
    CHECK(keymap_get_encoder(KEYMAP_MAC_LAYER, 0, true) == KC_VOLU); // the knob is the same on both sides

    // The accented letters: a one-shot layer from the key right of the space bar (the same on both sides of the Win/Mac switch), six keys on the vowels
    CHECK(keymap_get(0, 4, 8) == OSL(2) && keymap_get(KEYMAP_MAC_LAYER, 4, 8) == OSL(2));
    CHECK(keymap_get(2, 1, 3) == B68_ACC_EG && keymap_get(2, 1, 4) == B68_ACC_EA && keymap_get(2, 1, 7) == B68_ACC_U);
    CHECK(keymap_get(2, 1, 8) == B68_ACC_I && keymap_get(2, 1, 9) == B68_ACC_O && keymap_get(2, 2, 1) == B68_ACC_A);
    CHECK(keymap_get(1, 3, 1) == B68_OS_LNX); // Fn+Z switches the accents to the Linux sequence
    unsigned accents = 0;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (keymap_get(2, row, col) != KC_TRNS && keymap_get(2, row, col) != KC_NO) { // (the cells with no key are KC_NO)
                accents++;
            }
        }
    }

    CHECK(accents == 6); // and nothing else on that layer: every other key is typed as usual

    CHECK(!keymap_set(KEYMAP_LAYERS, 0, 0, KC_A)); // a position that does not exist
    CHECK(keymap_set(3, 0, 0, KC_A) && keymap_get(3, 0, 0) == KC_A);

    return tests_result();
}
