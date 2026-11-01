#include "clock.h"
#include "sh68f90.h"
#include "watchdog.h"

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
