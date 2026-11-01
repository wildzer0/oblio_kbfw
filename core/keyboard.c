#include "keyboard.h"
#include "keymap.h"
#include "keycodes.h"
#include "layer.h"
#include "macro.h"
#include "report.h"
#include "tapping.h"

void keyboard_init(void)
{
    keymap_init();
    layer_init();
    report_init();
    tapping_init();
    macro_init();
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

// The time of the event being handled, for the hooks that run inside it (queued events replayed by the tapping logic)
static uint16_t current_now;

// A keycode going down or up, from a key of the keymap or from a tap dance.
static void run_keycode(uint16_t keycode, bool pressed)
{
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

    send_keycode(keycode, pressed);

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

    run_keycode(keycode, pressed);
}

void keyboard_event(const matrix_event_t *event, uint16_t now)
{
    current_now = now;
    process_key_state(event->row, event->col, event->pressed);
}

void keyboard_task(uint16_t now)
{
    current_now = now;
    tapping_task(now);
    macro_task(now);
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

void macro_hook_press(uint16_t keycode)
{
    send_keycode(keycode, true);
}

void macro_hook_release(uint16_t keycode)
{
    send_keycode(keycode, false);
}
