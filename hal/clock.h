#pragma once

// Switches the system clock to the PLL (24 MHz). First thing after reset.
void clock_init(void);

// After a power-down: the oscillator and the PLL start again. The PLL's "locked" flag trips before a cold oscillator is stable enough
// for USB, so this settles by a delay instead.
void clock_wake_restart(void);

// Stops the PLL and the oscillator, for a power-down.
void clock_stop(void);
