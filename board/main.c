// Oblio entry point: clock, pins, recovery check, then the timer interrupt (backlight subframes and matrix scans), and a main loop
// that debounces the keys, runs the key logic and looks after USB, profiles, settings and the backlight.

#include "clock.h"
#include "usb.h" // declares the USB handler, for the same reason
#include "tick.h" // declares the Timer 2 handler: SDCC wants it visible in the module that defines main()
#include "watchdog.h"
#include "keyboard.h"
#include "led.h"
#include "led_app.h"
#include "led_hw.h"
#include "link.h"
#include "radio_hw.h" // declares the serial handler, for the same reason
#include "timebase.h"
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

// Milliseconds since power-on, for the uptime the Vial app shows and the unlock steps; and the 16-bit clock of the main loop
static uint32_t uptime_ms;
static uint16_t now_ms;

uint16_t board_now_ms(void)
{
    return now_ms;
}

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
    led_init();
    led_hw_init();
    radio_hw_init();
    link_init();
    tick_init(); // from here on the timer plays the slots: LED subframes and scans (see core/sched.h)

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

    init();

    for (;;) {
        watchdog_kick();

        while (tick_take()) { // a millisecond has passed
            now_ms++;
            uptime_ms++;
        }

        if (tick_scan_take(raw)) { // the interrupt scanned the matrix
            matrix_feed(raw);
        }

        encoder_poll(); // every pass: the knob's lines are read much faster than a scan

        while (matrix_event_pop(&event)) {
            keyboard_event(&event, now_ms);
        }

        keyboard_task(now_ms);
        store_task(now_ms, matrix_any_pressed());
        settings_task(now_ms, matrix_any_pressed());
        usb_task(now_ms);
        led_task();
        led_app_task(now_ms);
        link_task(now_ms);
    }
}
