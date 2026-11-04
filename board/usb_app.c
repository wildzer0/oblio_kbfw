#include "usb_device.h"
#include "usb_reports.h"
#include "usb.h"
#include "isp.h"
#include "raw_hid.h"
#include "via.h"

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

// The raw HID reply goes out first, then the jump: the host reads the answer to the command that asked for it.
#define BOOTLOADER_DELAY_MS 100u

static bool     bootloader_wait;
static uint16_t bootloader_since;

// The remote wakeup: a key pressed while the host has suspended the bus, and the host has allowed the device to wake it, drives the resume
// signal for WAKE_HOLD_MS (the USB spec asks for 1 to 15 ms). The key itself is not sent: the host is asleep and treats it as the wake-up.
#define WAKE_HOLD_MS 10u

void usb_wake_task(bool any_key_down, uint16_t now)
{
    static bool     was_down;
    static bool     waking;
    static uint16_t woke_at;

    if (waking) {
        if ((uint16_t)(now - woke_at) >= WAKE_HOLD_MS) {
            usb_hw_wake_host(false);
            waking = false;
        }
    } else if (any_key_down && !was_down && usb_hw_suspended() && usb_device_remote_wakeup()) {
        usb_hw_wake_host(true);
        woke_at = now;
        waking  = true;
    }

    was_down = any_key_down;
}

// From the main loop
void usb_task(uint16_t now)
{
    usb_device_bus_check(now); // is anybody still driving the bus?

    if (usb_hw_take_event()) {
        usb_reports_flush(); // a bus reset or a new configuration: what was queued is stale

        if (usb_device_configured()) {
            raw_hid_reset(); // the raw HID pipe starts listening
        }
    }

    raw_hid_task();

    usb_reports_pump();

    if (isp_requested) {
        isp_jump(); // the host asked for the bootloader and the status stage is done
    }

    if (via_bootloader_requested()) { // the unlocked Vial app asked
        if (!bootloader_wait) {
            bootloader_wait  = true;
            bootloader_since = now;
        } else if ((uint16_t)(now - bootloader_since) >= BOOTLOADER_DELAY_MS) {
            isp_jump();
        }
    }
}

// Vial / VIA
void raw_hid_hook_process(XDATA uint8_t *packet)
{
    via_receive(packet, RAW_HID_SIZE);
}
