#include "diag.h"
#include "led.h"
#include "usb_app.h"
#include "led_hw.h"
#include "link.h"
#include "usb.h"
#include "usb_device.h"

// The caps lock of the host, from the LED report (bit 1)
bool led_hook_caps_lock(void)
{
    return (usb_host_leds() & 0x02u) != 0;
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
