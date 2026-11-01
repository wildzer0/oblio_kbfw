// Oblio entry point: clock, pins, recovery check, tick, the debounced key matrix, the key logic behind it and the USB side.
// The logic is being added step by step (phase 3): layers, report, tapping, macros and profiles.

#include "clock.h"
#include "usb.h" // declares the USB handler, for the same reason
#include "tick.h" // declares the Timer 2 handler: SDCC wants it visible in the module that defines main()
#include "watchdog.h"
#include "keyboard.h"
#include "matrix.h"
#include "matrix_hw.h"
#include "recovery.h"
#include "settings.h"
#include "encoder.h"
#include "store.h"
#include "usb_app.h"
#include "usb_device.h"
#include "usb_reports.h"
#include "via.h"
#include "sh68f90.h"

// Milliseconds since power-on, for the uptime the Vial app shows and the unlock steps
static uint32_t uptime_ms;

uint32_t via_hook_now(void)
{
    return uptime_ms;
}

static void init(void)
{
    clock_init();
    board_gpio_init();

    recovery_check(); // before USB and radio, so that a broken later stage can always be recovered

    matrix_init();
    keyboard_init();
    settings_load();
    store_init(settings.profile); // the profile in use at power-off, from flash, or the defaults
    encoder_init();
    tick_init();

    usb_device_reset();
    usb_reports_init();
    via_init();
    usb_hw_init();

    EA = 1;
}

void main(void)
{
    uint8_t        raw[MATRIX_COLS];
    matrix_event_t event;
    uint16_t       now_ms = 0;

    init();

    for (;;) {
        watchdog_kick();

        if (tick_take()) {
            now_ms++;
            uptime_ms++;
            matrix_hw_scan(raw);
            matrix_feed(raw);
            encoder_poll();
        }

        while (matrix_event_pop(&event)) {
            keyboard_event(&event, now_ms);
        }

        keyboard_task(now_ms);
        store_task(now_ms, matrix_any_pressed());
        settings_task(now_ms, matrix_any_pressed());
        usb_task(now_ms);
    }
}
