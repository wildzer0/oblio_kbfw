#include "keyboard.h"
#include "b68_keys.h"
#include "led.h"
#include "store.h"

// The keycodes of the B68 (b68_keys.h). The profile and lighting keys are handled here; the radio and sleep keys join them with
// their modules.
void keyboard_hook_custom(uint16_t keycode, bool pressed)
{
    if (!pressed) {
        return;
    }

    switch (keycode) {
        case B68_FX_PREV: led_effect_prev(); break;
        case B68_FX_NEXT: led_effect_next(); break;
        case B68_BRI_DN:  led_brightness_down(); break;
        case B68_BRI_UP:  led_brightness_up(); break;
        case B68_SPD_DN:  led_speed_down(); break;
        case B68_SPD_UP:  led_speed_up(); break;
        case B68_COL_DN:  led_color_down(); break;
        case B68_COL_UP:  led_color_up(); break;
        default:
            if (keycode >= B68_PROF_1 && keycode <= B68_PROF_4) {
                store_request((uint8_t)(keycode - B68_PROF_1));
            }
            break;
    }
}
