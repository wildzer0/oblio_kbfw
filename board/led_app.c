#include "keymap.h"
#include "settings.h"
#include "diag.h"
#include "led.h"
#include "usb_app.h"
#include "led_hw.h"
#include "link.h"
#include "radio.h"
#include "usb.h"
#include "usb_device.h"

// The caps lock of the host. On the cable it comes from the LED report (bit 1); in the wireless modes the radio brings it in its status
// answer, which is what the stock firmware does too (it copies that byte into the host-LED state when the selector is on Bluetooth).
bool led_hook_caps_lock(void)
{
    const uint8_t leds = link_wireless() ? radio_host_leds() : usb_host_leds();

    return (leds & 0x02u) != 0;
}

// The backlight may light once the host has configured the keyboard (a bus-powered device stays within 100 mA until then), not while the
// bus is suspended, when no host is there at all (nothing was heard from the bus half a second after power-on), and in the wireless
// modes (the radio carries the keys and the battery powers the board).
#define NO_HOST_MS 500u

void led_app_task(uint16_t now)
{
    static bool waited; // the 500 ms from power-on have gone by at some point (the clock wraps every 65 s)

    if (!waited && now >= NO_HOST_MS) {
        waited = true;
    }

    const bool on = link_wireless() || (usb_device_configured() && !usb_hw_suspended()) || (!usb_hw_setup_seen() && waited);

    // Compared against the driver's own state, not against a copy kept here: a wake from sleep calls led_hw_init(), which takes the
    // permission away, and the backlight has to come back on its own after it (it did not: the LEDs stayed dark until a power cycle).
    if (on != led_hw_enabled()) {
        led_hw_enable(on);
    }
}

// The lighting is saved with the settings (core/settings.c), from the keymap's cfg fields
uint16_t settings_hook_led_get(uint8_t index)
{
    static const uint8_t field[4] = {CFG_LED_EFFECT, CFG_LED_BRIGHT, CFG_LED_SPEED, CFG_LED_COLOR};

    return keymap_cfg_get(field[index & 3u]);
}

void settings_hook_led_set(uint8_t index, uint16_t value)
{
    static const uint8_t field[4] = {CFG_LED_EFFECT, CFG_LED_BRIGHT, CFG_LED_SPEED, CFG_LED_COLOR};

    keymap_cfg_set(field[index & 3u], value);
}
