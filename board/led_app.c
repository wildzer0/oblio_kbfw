#include "led.h"
#include "store.h"
#include "usb_app.h"

// The caps lock of the host, from the LED report (bit 1)
bool led_hook_caps_lock(void)
{
    return (usb_host_leds() & 0x02u) != 0;
}

uint8_t led_hook_profile(void)
{
    return store_profile();
}
