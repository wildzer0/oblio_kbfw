// Oblio entry point: clock, pins, recovery check, tick, the debounced key matrix and the key logic behind it.
// The logic is being added step by step (phase 3): layers so far; the HID report, tapping, macros and profiles follow.

#include "clock.h"
#include "tick.h" // declares the Timer 2 handler: SDCC wants it visible in the module that defines main()
#include "watchdog.h"
#include "keyboard.h"
#include "matrix.h"
#include "matrix_hw.h"
#include "recovery.h"
#include "sh68f90.h"

static void init(void)
{
    clock_init();
    board_gpio_init();

    recovery_check(); // before USB and radio, so that a broken later stage can always be recovered

    matrix_init();
    keyboard_init();
    tick_init();

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
            matrix_hw_scan(raw);
            matrix_feed(raw);
        }

        while (matrix_event_pop(&event)) {
            keyboard_event(&event, now_ms);
        }
    }
}
