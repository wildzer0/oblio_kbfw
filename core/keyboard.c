#include "keyboard.h"
#include "keymap.h"
#include "keycodes.h"
#include "layer.h"
#include "report.h"

void keyboard_init(void)
{
    keymap_init();
    layer_init();
    report_init();
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

void keyboard_event(const matrix_event_t *event, uint16_t now)
{
    if (event->pressed) {
        // The key is looked up before layer_key_pressed(): a one-shot layer must still be on for the key it waits for.
        const uint16_t keycode = layer_key_down(event->row, event->col);

        if (!layer_process(keycode, true, now)) {
            layer_key_pressed();
            send_keycode(keycode, true);
        }
        return;
    }

    const uint16_t keycode = layer_key_up(event->row, event->col);

    if (!layer_process(keycode, false, now)) {
        send_keycode(keycode, false);
        layer_key_released();
    }
}
