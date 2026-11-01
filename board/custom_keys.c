#include "keyboard.h"
#include "b68_keys.h"
#include "store.h"

// The keycodes of the B68 (b68_keys.h). The profile keys are handled here; lighting, radio and sleep keys join them with
// their modules.
void keyboard_hook_custom(uint16_t keycode, bool pressed)
{
    if (!pressed) {
        return;
    }

    if (keycode >= B68_PROF_1 && keycode <= B68_PROF_4) {
        store_request((uint8_t)(keycode - B68_PROF_1));
    }
}
