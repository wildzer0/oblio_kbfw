// Oblio entry point: clock, pins, recovery check, then the timer interrupt (backlight subframes and matrix scans), and a main loop
// that debounces the keys, runs the key logic and looks after USB, profiles, settings and the backlight.

#include "battery.h"
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
#include "stage.h"

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
    recovery_check(); // before USB and radio, so that a broken later stage can always be recovered

    matrix_init();
    keyboard_init();
    watchdog_kick();
    settings_load();
    store_init();     // the keymap from flash, or the defaults
    settings_apply_led(); // the lighting is saved with the settings, not with the keymap
    os_switch_init(); // which of the two base layers the keyboard starts on (the Win/Mac switch)
    encoder_init();
    led_init(); // the framebuffer is cheap and the lighting keys write settings: always there
#if STAGE_LED
    led_hw_init();
#endif
#if STAGE_RADIO
    radio_hw_init();
    link_init();
#endif
#if STAGE_SLEEP
    sleep_init(0);
    sleep_hw_apply_level(settings.sleep_level, false);
#endif
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
            matrix_feed(raw, now_ms);
        }

        diag_max(DIAG_LOOP_MAX_MS, (uint16_t)(now_ms - last_pass)); // the slowest pass of this loop
        last_pass = now_ms;

        encoder_poll(); // every pass: the knob's lines are read much faster than a scan

        while (matrix_event_pop(&event)) {
            keyboard_event(&event, now_ms);
        }

        const bool any_key_down = matrix_any_pressed(); // once: two passes of the matrix cost a third of a loop

        keyboard_task(now_ms);
        os_switch_task(now_ms);
        store_task(now_ms, any_key_down);
        settings_task(now_ms, any_key_down);
        usb_wake_task(any_key_down, now_ms);
        usb_task(now_ms);
#if STAGE_LED
        led_task();
        led_app_task(now_ms);
#endif
#if STAGE_RADIO
        link_task(now_ms);
        battery_task(now_ms);
#endif
#if STAGE_SLEEP
        sleep_hw_task(now_ms);
#endif
    }
}
