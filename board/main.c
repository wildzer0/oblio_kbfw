// Oblio entry point. Phase 2: clock, pins, recovery check, tick and the debounced key matrix.
// The key events are drained and discarded for now; the layers and report logic of phase 3 take them over.

#include "clock.h"
#include "tick.h" // declares the Timer 2 handler: SDCC wants it visible in the module that defines main()
#include "watchdog.h"
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
    tick_init();

    EA = 1;
}

void main(void)
{
    uint8_t        raw[MATRIX_COLS];
    matrix_event_t event;

    init();

    for (;;) {
        watchdog_kick();

        if (tick_take()) {
            matrix_hw_scan(raw);
            matrix_feed(raw);
        }

        while (matrix_event_pop(&event)) {
        }
    }
}
