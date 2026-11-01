#include "tick.h"
#include "sh68f90.h"
#include <stdint.h>

// The timer counts at FREQ_SYS / 12 = 2 MHz (801 counts were 400 us on the SMK port), so 1 ms is 2000 counts.
#define TICK_RELOAD (65536u - 2000u)

static volatile uint8_t ticks;
static uint8_t          taken;

void tick_init(void)
{
    ticks = 0;
    taken = 0;

    TR2    = 0;
    T2CON  = 0;
    T2MOD  = 0;
    RCAP2H = (uint8_t)(TICK_RELOAD >> 8);
    RCAP2L = (uint8_t)(TICK_RELOAD & 0xFF);
    TH2    = (uint8_t)(TICK_RELOAD >> 8);
    TL2    = (uint8_t)(TICK_RELOAD & 0xFF);
    TF2    = 0;
    ET2    = 1;
    TR2    = 1;
}

// Nothing but a counter: no calls from here, or SDCC overlays the callee's locals with the main loop's.
void tick_isr(void) __interrupt(INT_TIMER2)
{
    TF2 = 0;
    ticks++;
}

bool tick_take(void)
{
    if (taken == ticks) {
        return false;
    }

    taken++;
    return true;
}
