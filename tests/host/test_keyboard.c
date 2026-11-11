#include "check.h"
#include "stubs.h"
#include "encoder.h"
#include "keyboard.h"
#include "keycodes.h"
#include "keymap.h"
#include "accent.h"
#include "layer.h"
#include "report.h"
#include <string.h>

// Test keymap, row 0:  A  MO(1)  LSFT(B)  LCTL  OSM(LSFT)  MUTE  SLEEP  OSL(1)  macro 0  CapsWord  -  Backspace  Space  _ (LSFT(-))  LShift  Ctrl+A
//   layer 1:  X  TRNS ...
uint16_t keymap_default(uint8_t layer, uint8_t row, uint8_t col)
{
    static const uint16_t l0[16] = {KC_A,     MO(1),    LSFT(KC_B), KC_LCTL,    OSM(MOD_LSFT),  KC_MUTE,
                                    KC_SYSTEM_SLEEP, OSL(1), QK_MACRO, QK_CAPS_WORD_TOGGLE, KC_MINS, KC_BSPC, KC_SPC, LSFT(KC_MINS), KC_LSFT, LCTL(KC_A)};

    if (layer == 0) {
        return (row == 0 && col < 16) ? l0[col] : KC_NO;
    }

    return (layer == 1 && row == 0 && col == 0) ? KC_X : KC_TRANSPARENT;
}

uint16_t keymap_default_encoder(uint8_t layer, uint8_t encoder, bool clockwise)
{
    (void)encoder;
    return layer == 0 ? (clockwise ? KC_E : KC_D) : KC_TRANSPARENT; // clockwise types e, counter-clockwise d
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

uint16_t keymap_default_override(uint8_t index, uint8_t field)
{
    (void)index;
    (void)field;
    return 0;
}

// every report sent: kind, then the boot report, consumer and system usages at that moment
typedef struct {
    report_kind_t kind;
    uint8_t       boot[REPORT_BOOT_SIZE];
    uint16_t      consumer;
    uint16_t      system;
} sent_t;

static sent_t sent[64];
static unsigned sent_count;

void report_hook_send(report_kind_t kind)
{
    sent_t *s = &sent[sent_count++];

    s->kind = kind;
    report_build_boot(s->boot);
    s->consumer = report_consumer();
    s->system   = report_system();
}

static uint16_t clock_ms;

static void reset(void)
{
    sent_count = 0;
    clock_ms   = 0;
    keyboard_init();
}

static void key(uint8_t col, bool pressed)
{
    const matrix_event_t ev = {.col = col, .row = 0, .pressed = pressed};

    keyboard_event(&ev, clock_ms++);
}

static void test_plain_key(void)
{
    reset();
    key(0, true);
    CHECK(sent_count == 1 && sent[0].kind == REPORT_KEYBOARD);
    CHECK(sent[0].boot[2] == KC_A && sent[0].boot[0] == 0);
    key(0, false);
    CHECK(sent_count == 2 && sent[1].boot[2] == 0);
}

static void test_modifier_key(void)
{
    reset();
    key(3, true);
    CHECK(sent_count == 1 && sent[0].boot[0] == 0x01 && sent[0].boot[2] == 0);
    key(3, false);
    CHECK(sent[sent_count - 1].boot[0] == 0);
}

static void test_mods_plus_key_order(void)
{
    reset();
    key(2, true); // LSFT(KC_B)
    CHECK(sent_count == 2);
    CHECK(sent[0].boot[0] == 0x02 && sent[0].boot[2] == 0); // shift first
    CHECK(sent[1].boot[0] == 0x02 && sent[1].boot[2] == KC_B);
    key(2, false);
    CHECK(sent_count == 4);
    CHECK(sent[2].boot[0] == 0x02 && sent[2].boot[2] == 0); // key up first
    CHECK(sent[3].boot[0] == 0 && sent[3].boot[2] == 0);
}

static void test_layer_key_then_layer_key(void)
{
    reset();
    key(1, true); // MO(1): a layer key sends nothing
    CHECK(sent_count == 0);
    key(0, true);
    CHECK(sent[0].boot[2] == KC_X);
    key(1, false);
    key(0, false); // still releases X, though the layer is gone
    CHECK(sent[sent_count - 1].boot[2] == 0);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_A);
}

static void test_consumer_and_system(void)
{
    reset();
    key(5, true);
    CHECK(sent_count == 1 && sent[0].kind == REPORT_CONSUMER && sent[0].consumer == 0x0E2);
    key(5, false);
    CHECK(sent[1].kind == REPORT_CONSUMER && sent[1].consumer == 0);

    key(6, true);
    CHECK(sent[2].kind == REPORT_SYSTEM && sent[2].system == 2);
    key(6, false);
    CHECK(sent[3].system == 0);
}

static void test_oneshot_shift_applies_to_one_key(void)
{
    reset();
    key(4, true); // OSM(LSFT) tapped
    key(4, false);
    const unsigned before = sent_count;
    CHECK(sent[before - 1].boot[0] == 0x02); // shift is on

    key(0, true); // the next key
    CHECK(sent[sent_count - 1].boot[0] == 0x02 && sent[sent_count - 1].boot[2] == KC_A);
    key(0, false);
    CHECK(sent[sent_count - 1].boot[0] == 0 && sent[sent_count - 1].boot[2] == 0);

    key(0, true); // and the one after is plain
    CHECK(sent[sent_count - 1].boot[0] == 0);
}

// A key set on the Fn layer from Vial (dynamic_keymap_set_keycode) is what Fn + that key sends, in place of the transparent one
static void test_a_key_edited_on_the_fn_layer_is_what_fn_sends(void)
{
    reset();
    CHECK(keymap_set(1, 0, 2, 0x4Au));

    key(2, true); // without Fn: the base key
    CHECK(sent[sent_count - 1].boot[2] == KC_B && sent[sent_count - 1].boot[0] == 0x02); // LSFT(B)
    key(2, false);

    sent_count = 0;
    key(1, true); // Fn
    key(2, true);
    CHECK(sent_count == 1 && sent[0].boot[2] == 0x4Au && sent[0].boot[0] == 0);
    key(2, false);
    key(1, false);
}

static void test_oneshot_layer_applies_to_one_key(void)
{
    reset();
    key(7, true); // OSL(1) tapped
    key(7, false);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_X);
    key(0, false);
    key(0, true);
    CHECK(sent[sent_count - 1].boot[2] == KC_A);
}

static void test_macro_key(void)
{
    static const uint8_t macro[] = {'h', 'i', 0};

    reset();
    keymap_macro_write(0, sizeof macro, macro);
    key(8, true);
    CHECK(sent_count == 0); // it plays from keyboard_task, a step per ms
    key(8, false);

    for (uint16_t t = 1; t < 30; t++) {
        keyboard_task(t);
    }

    // h down, h up, i down, i up
    CHECK(sent_count == 4);
    CHECK(sent[0].boot[2] == KC_H && sent[1].boot[2] == 0);
    CHECK(sent[2].boot[2] == KC_I && sent[3].boot[2] == 0);
}

static void turn(const uint8_t *states, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        stub_encoder_state = states[i];
        encoder_poll();
    }
}

static void test_encoder_detents_type_their_keys(void)
{
    static const uint8_t clockwise[]    = {0, 1, 3, 2, 0};
    static const uint8_t anticlockwise[] = {0, 2, 3, 1, 0};

    reset();
    stub_encoder_state = 0;
    encoder_poll();
    turn(clockwise, 5);
    keyboard_task(1);
    CHECK(sent_count == 2 && sent[0].boot[2] == KC_E && sent[1].boot[2] == 0); // a tap: down and up

    turn(anticlockwise, 5);
    keyboard_task(2);
    CHECK(sent_count == 4 && sent[2].boot[2] == KC_D && sent[3].boot[2] == 0);

    keyboard_task(3);
    CHECK(sent_count == 4); // nothing left
}

// Caps Word: the toggle key, the letters shifted, the word ending on a key that is not part of it, backspace keeping it alive
static void test_caps_word(void)
{
    reset();

    CHECK(!keyboard_caps_word());

    key(9, true); // the mode key: it never reaches the host
    CHECK(sent_count == 0);
    key(9, false);
    CHECK(keyboard_caps_word());

    key(0, true); // A, shifted: the Shift goes out first, on its own (as LSFT(KC_B) does in the test above)
    CHECK(sent_count == 2);
    CHECK(sent[0].boot[0] == 0x02 && sent[0].boot[2] == 0);
    CHECK(sent[1].boot[0] == 0x02 && sent[1].boot[2] == KC_A);
    key(0, false);
    CHECK(sent[sent_count - 1].boot[0] == 0x00); // the Shift comes off with the letter
    CHECK(keyboard_caps_word());

    key(10, true); // minus: part of a word, not shifted, still on
    CHECK(sent[sent_count - 1].boot[0] == 0x00 && sent[sent_count - 1].boot[2] == KC_MINS);
    key(10, false);
    CHECK(keyboard_caps_word());

    key(13, true); // an underscore (Shift + minus as one keycode) is part of a word too, like the minus: still on
    CHECK(sent[sent_count - 1].boot[0] == 0x02 && sent[sent_count - 1].boot[2] == KC_MINS);
    key(13, false);
    CHECK(keyboard_caps_word());

    key(14, true); // the Shift key held for an underscore: it does not end the word either...
    CHECK(keyboard_caps_word());
    key(10, true); // ...and the minus under it is the underscore
    CHECK(sent[sent_count - 1].boot[0] & 0x02 && sent[sent_count - 1].boot[2] == KC_MINS);
    key(10, false);
    key(14, false);
    CHECK(keyboard_caps_word());

    key(11, true); // fixing a typo does not end it either
    key(11, false);
    CHECK(keyboard_caps_word());

    key(15, true); // a shortcut (Ctrl+A) is not part of a word
    key(15, false);
    CHECK(!keyboard_caps_word());
    key(9, true); // switch it on again for the space
    key(9, false);
    CHECK(keyboard_caps_word());

    key(12, true); // a space ends the word
    key(12, false);
    CHECK(!keyboard_caps_word());

    key(0, true); // and an ordinary letter is ordinary again
    CHECK(sent[sent_count - 1].boot[0] == 0x00);
    key(0, false);
}

// The accented letters' sequence goes out through the same road as a key of the keymap, and puts the modifiers aside and back (core/accent.h)
static void test_the_accent_hooks_send_keys_and_move_the_modifiers(void)
{
    reset();

    key(3, true); // LCTL held
    CHECK(accent_hook_mods_get() == 0x01);

    sent_count = 0;
    accent_hook_mods_set(0); // put aside
    CHECK(accent_hook_mods_get() == 0 && sent_count >= 1 && sent[sent_count - 1].boot[0] == 0);

    accent_hook_key(KC_KP_1, true); // a keypad key, then a modifier key, as the Windows sequence does
    CHECK(sent[sent_count - 1].boot[2] == KC_KP_1);
    accent_hook_key(KC_KP_1, false);
    CHECK(sent[sent_count - 1].boot[2] == 0);

    accent_hook_key(KC_LALT, true);
    CHECK(sent[sent_count - 1].boot[0] == 0x04);
    accent_hook_key(KC_LALT, false);
    CHECK(sent[sent_count - 1].boot[0] == 0);

    accent_hook_mods_set(0x01); // and back
    CHECK(accent_hook_mods_get() == 0x01 && sent[sent_count - 1].boot[0] == 0x01);

    key(3, false);
    CHECK(sent[sent_count - 1].boot[0] == 0);
}

int main(void)
{
    test_plain_key();
    test_modifier_key();
    test_mods_plus_key_order();
    test_layer_key_then_layer_key();
    test_a_key_edited_on_the_fn_layer_is_what_fn_sends();
    test_consumer_and_system();
    test_oneshot_shift_applies_to_one_key();
    test_oneshot_layer_applies_to_one_key();
    test_macro_key();
    test_encoder_detents_type_their_keys();
    test_caps_word();
    test_the_accent_hooks_send_keys_and_move_the_modifiers();
    return tests_result();
}
