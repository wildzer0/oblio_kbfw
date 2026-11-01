#pragma once

#include <stdbool.h>

// A 1 ms tick from Timer 2. The interrupt only counts; the main loop takes the ticks one by one with tick_take(),
// so a slow pass is caught up on instead of lost.
//
// The handler is declared here because SDCC builds the vector table from the module that defines main():
// main.c must include this header.
void tick_isr(void) __interrupt(0); // INT_TIMER2

void tick_init(void);

// True once for every tick not yet taken.
bool tick_take(void);
