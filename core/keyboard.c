#include "keyboard.h"
#include "combo.h"
#include "keymap.h"
#include "leader.h"
#include "keycodes.h"
#include "encoder.h"
#include "layer.h"
#include "macro.h"
#include "mouse.h"
#include "override.h"
#include "report.h"
#include "tapping.h"

// Caps Word (see caps_word_before below for what it does)
static bool caps_word;
static bool caps_shift; // the Shift we added for the letter that is down

void keyboard_init(void)
{
    caps_word  = false;
    caps_shift = false;
    keymap_init();
    layer_init();
    report_init();
    tapping_init();
    macro_init();
    encoder_init();
    mouse_init();
    combo_init();
    override_init();
    leader_init();
}

// The layer engine holds modifiers (one-shot, layer + mods) through the same report as the keys.
void layer_hook_mods_add(uint8_t mods)
{
    report_add_mods(mods);
    report_hook_send(REPORT_KEYBOARD);
}

void layer_hook_mods_del(uint8_t mods)
{
    report_del_mods(mods);
    report_hook_send(REPORT_KEYBOARD);
}

// A keycode the layer engine did not take: change the report and tell the host.
static void send_keycode(uint16_t keycode, bool pressed)
{
    if (IS_QK_MODS(keycode)) { // e.g. LSFT(KC_A): the modifiers go down before the key and up after it
        const uint8_t mods = MODS_FROM_MASK(QK_MODS_GET_MODS(keycode));
        const uint8_t key  = (uint8_t)QK_MODS_GET_KEY(keycode);

        if (pressed) {
            layer_hook_mods_add(mods);
            send_keycode(key, true);
        } else {
            send_keycode(key, false);
            layer_hook_mods_del(mods);
        }
        return;
    }

    if (IS_MODIFIER_KEYCODE(keycode)) {
        if (pressed) {
            layer_hook_mods_add((uint8_t)MOD_BIT(keycode));
        } else {
            layer_hook_mods_del((uint8_t)MOD_BIT(keycode));
        }
        return;
    }

    if (IS_KEY_KEYCODE(keycode)) {
        if (pressed) {
            report_add_key((uint8_t)keycode);
        } else {
            report_del_key((uint8_t)keycode);
        }
        report_hook_send(REPORT_KEYBOARD);
        return;
    }

    if (IS_SYSTEM_KEYCODE(keycode)) {
        report_set_system(pressed ? report_keycode_to_system((uint8_t)keycode) : 0);
        report_hook_send(REPORT_SYSTEM);
        return;
    }

    if (IS_CONSUMER_KEYCODE(keycode)) {
        report_set_consumer(pressed ? report_keycode_to_consumer((uint8_t)keycode) : 0);
        report_hook_send(REPORT_CONSUMER);
    }
}

// ---- Caps Word (QMK's "capitalises until the end of the word") ----
// While it is on, every letter goes out with Shift, and it ends by itself at the first key that is not part of a word (a space, a symbol, the
// arrow keys...). Keys that keep it going: the letters themselves, the digits, minus, backspace and delete, and the Shift keys - so "hello-world" can
// be typed without touching Shift, "HELLO_WORLD" with the Shift that makes the underscore, and a typo fixed, without it switching off. A key that is
// Shift + another (LSFT(KC_MINS), the underscore as one keycode) is judged by the other one.
static bool caps_word_letter(uint16_t keycode)
{
    return keycode >= KC_A && keycode <= KC_Z;
}

static bool caps_word_keeps(uint16_t keycode)
{
    if (IS_QK_MODS(keycode)) { // a key with modifiers (LSFT(KC_MINS) is the underscore): with Shift alone the key itself decides, with Ctrl, Alt or Gui it is a shortcut
        const uint8_t mods = QK_MODS_GET_MODS(keycode);

        if (mods != MOD_LSFT && mods != MOD_RSFT) {
            return false;
        }

        keycode = QK_MODS_GET_KEY(keycode);
    }

    // The Shift keys themselves keep the word going: "HELLO_WORLD" is typed by holding Shift for the underscore, and the press of that Shift reaches here
    // BEFORE the minus does. Without this the Shift ended the word and the underscore came out of a word that was already over.
    return caps_word_letter(keycode) || (keycode >= KC_1 && keycode <= KC_0) || keycode == KC_MINS || keycode == KC_BSPC || keycode == KC_DEL
        || keycode == KC_LSFT || keycode == KC_RSFT;
}

// Before the key goes out: the Shift for a letter, and the end of the word for anything else
static void caps_word_before(uint16_t keycode, bool pressed)
{
    if (!caps_word) {
        return;
    }

    if (!pressed) {
        return;
    }

    if (caps_word_letter(keycode)) {
        if (!caps_shift) {
            caps_shift = true;
            layer_hook_mods_add(MOD_LSFT);
        }
    } else if (!caps_word_keeps(keycode)) {
        caps_word = false; // the word is over
    }
}

static void caps_word_after(bool pressed)
{
    if (!pressed && caps_shift) {
        caps_shift = false;
        layer_hook_mods_del(MOD_LSFT);
    }
}

bool keyboard_caps_word(void)
{
    return caps_word;
}

// The time of the event being handled, for the hooks that run inside it (queued events replayed by the tapping logic)
static uint16_t current_now;

// A keycode going down or up, from a key of the keymap or from a tap dance.
static void run_keycode(uint16_t keycode, bool pressed)
{
    if (leader_process(keycode, pressed, current_now)) {
        return; // the Leader is collecting: this key is part of a sequence, not typing
    }

    if (IS_QK_CAPS_WORD(keycode)) { // the key of the mode itself: it never reaches the host
        if (pressed) {
            caps_word = !caps_word;
            caps_shift = false;
        }
        return;
    }

    if (layer_process(keycode, pressed, current_now)) {
        return;
    }

    if (IS_QK_MACRO(keycode)) {
        if (pressed) {
            layer_key_pressed();
            macro_start((uint8_t)(keycode - QK_MACRO));
        } else {
            layer_key_released();
        }
        return;
    }

    if (IS_MOUSE_KEYCODE(keycode)) { // a direction, a button, a wheel or the acceleration: the mouse owns it
        if (pressed) {
            layer_key_pressed();
        }

        mouse_process(keycode, pressed, current_now);

        if (!pressed) {
            layer_key_released();
        }
        return;
    }

    if (IS_QK_KB(keycode)) {
        if (pressed) {
            layer_key_pressed();
        }

        keyboard_hook_custom(keycode, pressed);

        if (!pressed) {
            layer_key_released();
        }
        return;
    }

    if (pressed) {
        // The key was looked up before this: a one-shot layer must still be on for the key it waits for.
        layer_key_pressed();
    }

    caps_word_before(keycode, pressed);
    send_keycode(keycode, pressed);
    caps_word_after(pressed);

    if (!pressed) {
        layer_key_released();
    }
}

static void process_key_state(uint8_t row, uint8_t col, bool pressed)
{
    if (tapping_intercept(row, col, pressed, current_now)) {
        return; // a tap-or-hold key is undecided: this waits behind it (or decides it)
    }

    const uint16_t keycode = pressed ? layer_key_down(row, col) : layer_key_up(row, col);

    if (tapping_event(row, col, keycode, pressed, current_now)) {
        return; // a tap-or-hold key starts waiting, or lets go after becoming a hold
    }

    if (override_process(row, col, keycode, pressed, report_mods())) {
        return; // a key override: the trigger never reached the host, the replacement did
    }

    if (combo_process(row, col, keycode, pressed, current_now)) {
        return; // held back for a chord, swallowed as part of one, or turned into the combo's output
    }

    run_keycode(keycode, pressed);
}

void keyboard_event(const matrix_event_t *event, uint16_t now)
{
    current_now = now;
    keyboard_hook_activity();
    process_key_state(event->row, event->col, event->pressed);
}

#define ENCODER_TAPS_PER_PASS 8 // a fast spin is worked off over a few passes

// Every detent is a tap of the keycode the active layers give for that direction
static void encoder_task(void)
{
    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        bool clockwise;

        for (uint8_t taps = 0; taps < ENCODER_TAPS_PER_PASS && encoder_next(i, &clockwise); taps++) {
            const uint16_t keycode = layer_lookup_encoder(i, clockwise);

            keyboard_hook_activity(); // turning the knob is use

            run_keycode(keycode, true);
            run_keycode(keycode, false);
        }
    }
}

void keyboard_task(uint16_t now)
{
    current_now = now;
    tapping_task(now);
    macro_task(now);
    encoder_task();
    mouse_task(now);
    combo_task(now);
    leader_task(now);
}

// ---- what tapping.c needs from the rest of the firmware ----
void tapping_hook_replay(uint8_t row, uint8_t col, bool pressed)
{
    process_key_state(row, col, pressed);
}

void tapping_hook_tap(uint8_t basic_keycode)
{
    layer_key_pressed(); // a tapped key is a key press as far as one-shots go
    send_keycode(basic_keycode, true);
    send_keycode(basic_keycode, false);
    layer_key_released();
}

void tapping_hook_mods(uint8_t mods, bool on)
{
    if (on) {
        layer_hook_mods_add(mods);
    } else {
        layer_hook_mods_del(mods);
    }
}

void tapping_hook_layer(uint8_t layer, bool on)
{
    layer_hold(layer, on);
}

void tapping_hook_keycode(uint16_t keycode, bool pressed)
{
    run_keycode(keycode, pressed);
}

// The action of a Leader sequence: a tap of a keycode, the same way a key of the keymap goes out
void keyboard_tap(uint16_t keycode)
{
    layer_key_pressed();
    send_keycode(keycode, true);
    send_keycode(keycode, false);
    layer_key_released();
}

// A chord's keys, once it is decided, are pressed and released from here: the same way a key of the keymap goes out
void combo_hook_send(uint16_t keycode, bool pressed)
{
    send_keycode(keycode, pressed);
}

// A key override's replacement and the modifiers it takes away, by the same road
void override_hook_send(uint16_t keycode, bool pressed)
{
    send_keycode(keycode, pressed);
}

void override_hook_mods(uint8_t mods, bool on)
{
    if (on) {
        layer_hook_mods_add(mods);
    } else {
        layer_hook_mods_del(mods);
    }
}

void macro_hook_press(uint16_t keycode)
{
    send_keycode(keycode, true);
}

void macro_hook_release(uint16_t keycode)
{
    send_keycode(keycode, false);
}
