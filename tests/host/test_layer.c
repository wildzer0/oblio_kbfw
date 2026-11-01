#include "check.h"
#include "stubs.h"
#include "keycodes.h"
#include "keymap.h"
#include "layer.h"

// A small keymap of the test's own. Row 0:  col0 col1 col2 col3 col4
//   layer 0: A   MO(1) B   TG(2) OSL(1)
//   layer 1: X   TRNS  TRNS TRNS TRNS      (col1: transparent, falls through to MO(1) on layer 0)
//   layer 2: TRNS TRNS C    TRNS TRNS
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    static const uint16_t l0[5] = {KC_A, MO(1), KC_B, TG(2), OSL(1)};
    static const uint16_t l1[5] = {KC_X, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS};
    static const uint16_t l2[5] = {KC_TRNS, KC_TRNS, KC_C, KC_TRNS, KC_TRNS};

    if (row != 0 || col >= 5) {
        return layer == 0 ? KC_NO : KC_TRANSPARENT;
    }

    return layer == 0 ? l0[col] : layer == 1 ? l1[col] : layer == 2 ? l2[col] : KC_TRANSPARENT;
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)encoder;
    if (layer == 0) {
        return clockwise ? KC_VOLU : KC_VOLD;
    }
    return layer == 1 ? KC_TRNS : KC_TRNS;
}

uint16_t keymap_default_tap_dance(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

uint16_t keymap_default_combo(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

static uint8_t mods;

void layer_hook_mods_add(uint8_t m) { mods |= m; }
void layer_hook_mods_del(uint8_t m) { mods &= (uint8_t)~m; }

static void reset(void)
{
    mods = 0;
    keymap_init();
    layer_init();
}

static void test_base_lookup(void)
{
    reset();
    CHECK(layer_lookup(0, 0) == KC_A);
    CHECK(layer_lookup(0, 9) == KC_NO);
    CHECK(layer_lookup(4, 4) == KC_NO);
}

static void test_momentary_layer_and_transparency(void)
{
    reset();
    CHECK(layer_process(layer_key_down(0, 1), true, 0)); // MO(1) down
    CHECK(layer_get_state() == 0x02);
    CHECK(layer_lookup(0, 0) == KC_X);
    CHECK(layer_lookup(0, 2) == KC_B); // transparent on layer 1: the default layer answers
    layer_process(layer_key_up(0, 1), false, 0);
    CHECK(layer_get_state() == 0);
    CHECK(layer_lookup(0, 0) == KC_A);
}

static void test_release_uses_the_press_keycode(void)
{
    reset();
    layer_process(MO(1), true, 0);
    CHECK(layer_key_down(0, 0) == KC_X);
    layer_process(MO(1), false, 0); // the layer goes away while the key is still down
    CHECK(layer_key_up(0, 0) == KC_X);
    CHECK(layer_key_up(0, 0) == KC_NO); // nothing left to release
}

static void test_toggle_to_and_default(void)
{
    reset();
    layer_process(TG(2), true, 0);
    CHECK(layer_lookup(0, 2) == KC_C);
    layer_process(TG(2), false, 0); // release does nothing
    CHECK(layer_lookup(0, 2) == KC_C);
    layer_process(TG(2), true, 0);
    CHECK(layer_lookup(0, 2) == KC_B);

    layer_process(MO(1), true, 0);
    layer_process(TO(2), true, 0); // TO leaves only that layer
    CHECK(layer_get_state() == 0x04);

    layer_process(DF(1), true, 0);
    CHECK(layer_get_default() == 1);
    layer_process(DF(9), true, 0); // no such layer
    CHECK(layer_get_default() == 1);
}

static void test_invalid_layer_is_ignored(void)
{
    reset();
    CHECK(layer_process(MO(20), true, 0));
    CHECK(layer_get_state() == 0);
    CHECK(!layer_process(KC_A, true, 0)); // a plain key is not ours
}

static void test_oneshot_layer(void)
{
    reset();
    layer_process(OSL(1), true, 0);
    layer_process(OSL(1), false, 0); // tapped
    CHECK(layer_oneshot_layer() == 1);
    CHECK(layer_lookup(0, 0) == KC_X);

    layer_key_pressed(); // the next key is the one it waited for
    CHECK(layer_get_state() == 0);
    CHECK(layer_oneshot_layer() == LAYER_NONE);

    // held with a key under it: lasts until the OSL key is let go
    layer_process(OSL(1), true, 0);
    layer_key_pressed();
    CHECK(layer_get_state() == 0x02);
    layer_key_pressed();
    CHECK(layer_get_state() == 0x02);
    layer_process(OSL(1), false, 0);
    CHECK(layer_get_state() == 0);
}

static void test_oneshot_mods(void)
{
    reset();

    // tapped: stays on for exactly one key
    layer_process(OSM(MOD_LSFT), true, 0);
    layer_process(OSM(MOD_LSFT), false, 0);
    CHECK(mods == 0x02);
    CHECK(layer_oneshot_mods() == 0x02);
    layer_key_pressed();
    CHECK(mods == 0x02);
    layer_key_released();
    CHECK(mods == 0);
    CHECK(layer_oneshot_mods() == 0);

    // held with a key under it: off when let go
    layer_process(OSM(MOD_LCTL), true, 0);
    layer_key_pressed();
    layer_key_released();
    CHECK(mods == 0x01);
    layer_process(OSM(MOD_LCTL), false, 0);
    CHECK(mods == 0);

    // right-hand modifiers: bit 4 moves them to the upper nibble
    layer_process(OSM(MOD_RALT), true, 0);
    CHECK(mods == 0x40);
    layer_process(OSM(MOD_RALT), false, 0);
    layer_key_pressed();
    layer_key_released();
    CHECK(mods == 0);
}

static void test_layer_mod(void)
{
    reset();
    layer_process(LM(1, MOD_LSFT), true, 0);
    CHECK(layer_get_state() == 0x02 && mods == 0x02);
    layer_process(LM(1, MOD_LSFT), false, 0);
    CHECK(layer_get_state() == 0 && mods == 0);
}

static void test_layer_tap_toggle(void)
{
    uint16_t t = 0;

    reset();

    for (unsigned i = 0; i < LAYER_TT_TAPS - 1; i++) { // four quick taps: momentary only
        layer_process(TT(1), true, t);
        CHECK(layer_get_state() == 0x02);
        t = (uint16_t)(t + 50);
        layer_process(TT(1), false, t);
        CHECK(layer_get_state() == 0);
        t = (uint16_t)(t + 50);
    }

    layer_process(TT(1), true, t);
    t = (uint16_t)(t + 50);
    layer_process(TT(1), false, t); // the fifth latches it
    CHECK(layer_get_state() == 0x02);

    for (unsigned i = 0; i < LAYER_TT_TAPS; i++) { // five more unlatch it
        t = (uint16_t)(t + 50);
        layer_process(TT(1), true, t);
        t = (uint16_t)(t + 50);
        layer_process(TT(1), false, t);
    }
    CHECK(layer_get_state() == 0);
}

static void test_tt_pause_restarts_the_count(void)
{
    uint16_t t = 0;

    reset();

    for (unsigned i = 0; i < LAYER_TT_TAPS; i++) {
        layer_process(TT(1), true, t);
        layer_process(TT(1), false, (uint16_t)(t + 20));
        t = (uint16_t)(t + 20 + LAYER_TT_TERM + 100); // too slow between taps
    }

    CHECK(layer_get_state() == 0);
}

static void test_encoder_lookup(void)
{
    reset();
    CHECK(layer_lookup_encoder(0, true) == KC_VOLU);
    CHECK(layer_lookup_encoder(0, false) == KC_VOLD);
    layer_process(MO(1), true, 0); // layer 1 is transparent for the encoder
    CHECK(layer_lookup_encoder(0, true) == KC_VOLU);
}

int main(void)
{
    test_base_lookup();
    test_momentary_layer_and_transparency();
    test_release_uses_the_press_keycode();
    test_toggle_to_and_default();
    test_invalid_layer_is_ignored();
    test_oneshot_layer();
    test_oneshot_mods();
    test_layer_mod();
    test_layer_tap_toggle();
    test_tt_pause_restarts_the_count();
    test_encoder_lookup();
    return tests_result();
}
