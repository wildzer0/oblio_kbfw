#include "link.h"
#include "led.h"
#include "sh68f90.h"

// The mode selector near the USB-C port, read by the MCU: P0.4 low = 2.4G, P0.3 low = Bluetooth, both high = USB (the cable).
link_mode_t link_hook_selector(void)
{
    if (!(P0 & 0x10u)) {
        return LINK_2G4;
    }

    if (!(P0 & 0x08u)) {
        return LINK_BT;
    }

    return LINK_USB;
}

void link_hook_flash(uint8_t kind, uint8_t index)
{
    led_flash_key(kind, index);
}
