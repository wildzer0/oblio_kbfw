#include "usb_device.h"
#include "usb_reports.h"
#include "usb.h"
#include "isp.h"

// What the USB side hands to the rest of the keyboard.
static volatile uint8_t host_leds;
static volatile bool    isp_requested;

void usb_hook_led(uint8_t leds)
{
    host_leds = leds;
}

void usb_hook_isp_request(void)
{
    isp_requested = true;
}

// Bits 0-4: num lock, caps lock, scroll lock, compose, kana (the LED report). The lighting phase shows caps lock with it.
uint8_t usb_host_leds(void)
{
    return host_leds;
}

bool usb_reports_hook_configured(void)
{
    return usb_device_configured();
}

uint8_t usb_reports_hook_protocol(void)
{
    return usb_device_protocol(0);
}

// From the main loop
void usb_task(void)
{
    if (usb_hw_take_event()) {
        usb_reports_flush(); // a bus reset or a new configuration: what was queued is stale
    }

    usb_reports_pump();

    if (isp_requested) {
        isp_jump(); // the host asked for the bootloader and the status stage is done
    }
}
