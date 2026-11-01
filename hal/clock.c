#include "clock.h"
#include "sh68f90.h"
#include "watchdog.h"
#include <stdbool.h>

void clock_init(void)
{
    CLKCON = CLKCON_HFON;
    PLLCON = PLLCON_PLLON;

    while (!(PLLCON & PLLCON_PLLST)) {
        watchdog_kick();
    }

    PLLCON |= PLLCON_PLLFS;
    CLKCON |= CLKCON_FS;
}

void clock_stop(void)
{
    CLKCON &= (uint8_t)~CLKCON_FS;
    PLLCON &= (uint8_t)~PLLCON_PLLFS;
    PLLCON &= (uint8_t)~PLLCON_PLLON;
    CLKCON &= (uint8_t)~CLKCON_HFON;
}

void clock_wake_restart(void)
{
    const bool interrupts = EA;

    EA = 0;
    CLKCON |= CLKCON_HFON;
    PLLCON |= PLLCON_PLLON;
    watchdog_kick();

    for (uint8_t i = 0; i < 200; i++) { // the oscillator settles
        // clang-format off
        __asm
            nop
        __endasm;
        // clang-format on
    }

    PLLCON = PLLCON_PLLFS | PLLCON_PLLON;
    CLKCON = CLKCON_FS | CLKCON_HFON;
    watchdog_kick();
    EA = interrupts;
}
