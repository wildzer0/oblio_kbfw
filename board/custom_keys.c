#include "keyboard.h"
#include "b68_keys.h"
#include "battery.h"
#include "led.h"
#include "link.h"
#include "settings.h"
#include "sleep.h"
#include "sleep_hw.h"
#include "timebase.h"

// The keycodes of the B68 (b68_keys.h). The lighting and Bluetooth keys are handled here, the sleep keys with their module, and B68_LEADER is
// taken before this, by core/leader.c (it has to swallow the keys that follow it).
void keyboard_hook_custom(uint16_t keycode, bool pressed)
{
    // The Bluetooth keys need the release too: a tap chooses the slot, a hold pairs
    if (keycode >= B68_LK_1 && keycode <= B68_LK_3) {
        link_key((uint8_t)(keycode - B68_LK_1 + 1), pressed, board_now_ms());
        return;
    }

    if (!pressed) {
        return;
    }

    switch (keycode) {

        case B68_BATTERY: led_show_battery(battery_percent()); break;
        case B68_FX_PREV: led_effect_prev(); break;
        case B68_FX_NEXT: led_effect_next(); break;
        case B68_BRI_DN:  led_brightness_down(); break;
        case B68_BRI_UP:  led_brightness_up(); break;
        case B68_SPD_DN:  led_speed_down(); break;
        case B68_SPD_UP:  led_speed_up(); break;
        case B68_COL_DN:  led_color_down(); break;
        case B68_COL_UP:  led_color_up(); break;
        case B68_SLEEP:
            sleep_now(); // does nothing in USB mode or with the cable: sleep_hw_task() only sleeps on battery
            break;
        case B68_SLP_DN:
            settings_set_sleep_level((uint8_t)(settings.sleep_level > 0 ? settings.sleep_level - 1 : 0));
            sleep_hw_apply_level(settings.sleep_level, true);
            break;
        case B68_SLP_UP:
            settings_set_sleep_level((uint8_t)(settings.sleep_level + 1));
            sleep_hw_apply_level(settings.sleep_level, true);
            break;
        default:
            break;
    }
}

// Any key or turn of the knob counts as use for the sleep timer
void keyboard_hook_activity(void)
{
    sleep_note_activity();
}
