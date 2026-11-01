#include "keyboard.h"
#include "keymap.h"
#include "layer.h"

void keyboard_init(void)
{
    keymap_init();
    layer_init();
}

// The report that carries modifiers to the host does not exist yet: until it does, nothing listens to the layer engine.
void layer_hook_mods_add(uint8_t mods)
{
    (void)mods;
}

void layer_hook_mods_del(uint8_t mods)
{
    (void)mods;
}

void keyboard_event(const matrix_event_t *event, uint16_t now)
{
    if (event->pressed) {
        // The key is looked up before layer_key_pressed(): a one-shot layer must still be on for the key it waits for.
        const uint16_t keycode = layer_key_down(event->row, event->col);

        if (!layer_process(keycode, true, now)) {
            layer_key_pressed();
        }
        return;
    }

    const uint16_t keycode = layer_key_up(event->row, event->col);

    if (!layer_process(keycode, false, now)) {
        layer_key_released();
    }
}
