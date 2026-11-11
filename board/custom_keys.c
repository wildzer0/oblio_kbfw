#include "keyboard.h"
#include "accent.h"
#include "b68_keys.h"
#include "battery.h"
#include "led.h"
#include "link.h"
#include "os_switch.h"
#include "radio.h"
#include "report.h"
#include "settings.h"
#include "sleep.h"
#include "sleep_hw.h"
#include "timebase.h"
#include "usb_app.h"

#if !LOOP_PROFILE
// An accented letter (core/accent.h). Capital when Shift is down (or armed as a one-shot: it is already in the report), when Caps Word is on, or when the host's
// Caps Lock is. The host's keyboard LEDs (bit 0 Num Lock, bit 1 Caps Lock) come from the cable's report or, in the wireless modes, the radio's status.
static void accent_key(uint8_t index)
{
    const uint8_t leds  = link_wireless() ? radio_host_leds() : usb_host_leds();
    const bool    upper = (report_mods() & 0x22u) != 0 || keyboard_caps_word() || (leds & 0x02u) != 0; // 0x22: left and right Shift
    accent_os_t   os    = settings.os_linux ? ACCENT_OS_LINUX : ACCENT_OS_WINDOWS;

    if (os_switch_mac()) {
        os = ACCENT_OS_MAC;
    }

    accent_start(board_now_ms(), index, upper, os, (leds & 0x01u) != 0);
}
#endif

// The keycodes of the B68 (b68_keys.h): the lighting, Bluetooth, battery, sleep and accent keys.
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

        case B68_BATTERY: battery_show(); break;
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
        case B68_OS_LNX:
            settings_set_os_linux(!settings.os_linux);
            led_flash_key(3, os_switch_mac() ? 2 : (settings.os_linux ? 3 : 1)); // the number row shows the system: key 1 Windows, 2 Mac, 3 Linux
            break;
        default:
#if !LOOP_PROFILE
            if (keycode >= B68_ACC_A && keycode <= B68_ACC_U) {
                accent_key((uint8_t)(keycode - B68_ACC_A));
            }
#endif
            break;
    }
}

// Any key or turn of the knob counts as use for the sleep timer
void keyboard_hook_activity(void)
{
    sleep_note_activity();
}
