// Oblio entry point: clock, pins, recovery check, then the timer interrupt (backlight subframes and matrix scans), and a main loop
// that debounces the keys, runs the key logic and looks after USB, profiles, settings and the backlight.

#include "battery.h"
#include "blog.h"
#include "clock.h"
#include "diag.h"
#include "diag_hw.h"
#include "usb.h" // declares the USB handler, for the same reason
#include "tick.h" // declares the Timer 2 handler: SDCC wants it visible in the module that defines main()
#include "watchdog.h"
#include "keyboard.h"
#include "led.h"
#include "led_app.h"
#include "led_hw.h"
#include "link.h"
#include "power.h" // declares the wake-up handler, for the same reason
#include "sleep.h"
#include "sleep_hw.h"
#include "radio_hw.h" // declares the serial handler, for the same reason
#include "timebase.h"
#include "matrix.h"
#include "radio.h"
#include "matrix_hw.h"
#include "os_switch.h"
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
    // RSTSTAT tells what reset the chip, but it is also the watchdog kick: the flags are read before the first kick
    const uint8_t reset_cause = RSTSTAT;

    diag_init();
    diag_set(DIAG_RESET_CAUSE, reset_cause);
    diag_stack_paint();

    REGCON = REGCON_REGEN; // the regulator first, as the stock firmware does (it powers the USB transceiver)
    clock_init();
    board_gpio_init();

    watchdog_kick();
    recovery_check(); // before USB and radio, so that a broken later step can always be recovered

    matrix_init();
    keyboard_init();
#if BATTERY_LOG
    blog_init();
#endif
    watchdog_kick();
    settings_load();
    store_init();     // the keymap from flash, or the defaults
    settings_apply_led(); // the lighting is saved with the settings, not with the keymap
    os_switch_init(); // which of the two base layers the keyboard starts on (the Win/Mac switch)
    encoder_init();
    led_init(); // the framebuffer is cheap and the lighting keys write settings: always there
    led_hw_init();
    radio_hw_init();
    link_init();
    sleep_init(0);
    sleep_hw_apply_level(settings.sleep_level, false);
    tick_init(); // from here on the timer plays the slots: LED subframes and scans (see core/sched.h)

    usb_device_reset();
    usb_reports_init();
    via_init();
    usb_hw_init();

    EA = 1;
}

#if LOOP_PROFILE
// Times a step of the loop in milliseconds of the timer's clock (which includes the interrupts that ran meanwhile) and keeps the slowest one
static uint8_t worst_ms = 1; // under 2 ms is the clock's own resolution
static void    step_done(uint8_t id, uint8_t started)
{
    const uint8_t took = (uint8_t)(tick_ms_now() - started);

    if (took > worst_ms) {
        worst_ms = took;
        diag_set(DIAG_LOOP_WORST, (uint16_t)(((uint16_t)id << 8) | took));
    }
}
#    define STEP(id, call)                    \
        do {                                  \
            const uint8_t started_ = tick_ms_now(); \
            call;                             \
            step_done(id, started_);          \
        } while (0)
#else
#    define STEP(id, call) call
#endif

void main(void)
{
    uint8_t        raw[MATRIX_COLS];
    matrix_event_t event;

    uint16_t last_pass = 0;

    init();

    for (;;) {
        watchdog_kick();
        tick_alive();

        while (tick_take()) { // a millisecond has passed
            now_ms++;
            uptime_ms++;
        }

        if (tick_scan_take(raw)) { // the interrupt scanned the matrix
            STEP(1, matrix_feed(raw, now_ms));
        }

        diag_max(DIAG_LOOP_MAX_MS, (uint16_t)(now_ms - last_pass)); // the slowest pass of this loop
        last_pass = now_ms;

        STEP(2, encoder_poll()); // every pass: the knob's lines are read much faster than a scan

        while (matrix_event_pop(&event)) {
            STEP(3, keyboard_event(&event, now_ms));
        }

        const bool any_key_down = matrix_any_pressed(); // once: two passes of the matrix cost a third of a loop

        STEP(4, keyboard_task(now_ms));
        STEP(5, os_switch_task(now_ms));
        STEP(6, store_task(now_ms, any_key_down));
        STEP(7, settings_task(now_ms, any_key_down));
        STEP(8, usb_wake_task(any_key_down, now_ms));
        STEP(9, usb_task(now_ms));
        STEP(10, led_task());
        STEP(11, led_app_task(now_ms));
        STEP(12, link_task(now_ms));
        STEP(13, battery_task(now_ms));
#if BATTERY_LOG
        STEP(14, blog_task(now_ms, radio_hook_on_battery(), battery_reading()));
#endif
        STEP(15, sleep_hw_task(now_ms));
    }
}
