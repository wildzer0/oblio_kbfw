#pragma once

// Power-down of the SH68F90A. The caller must arm a wake-up source first (power_wake_arm), or the core never comes back.
//
// The handler is declared here because SDCC builds the vector table from the module that defines main(): main.c must include this
// header.

void power_int4_isr(void) __interrupt(1); // INT_INT4

// INT4 as the wake-up source, with the values the stock firmware writes; and off again.
void power_wake_arm(void);
void power_wake_disable(void);

// Stops the USB controller, the clock and the regulator and halts the core until INT4 (or a USB event) wakes it; then restarts the
// clock and the regulator. The caller starts USB again.
void power_enter_powerdown(void);
