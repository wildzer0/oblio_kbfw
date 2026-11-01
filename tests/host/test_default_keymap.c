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

    CHECK(!keymap_set(KEYMAP_LAYERS, 0, 0, KC_A)); // a position that does not exist
    CHECK(keymap_set(3, 0, 0, KC_A) && keymap_get(3, 0, 0) == KC_A);

    return tests_result();
}
