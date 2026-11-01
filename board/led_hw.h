#pragma once

#include <stdbool.h>

// The backlight on the chip: the PWM channels P1.3-5 / P2.0-5 / P3.0-5 as colour sources, the column strobes as sinks.
//
// A subframe lights the keys of one column: the previous column goes off, the 15 pulse widths of this column are loaded, the strobe of
// the column goes low and the PWM outputs are enabled last, so the column lights at once. A column with nothing to show is left dark.

void led_hw_init(void);

// Whether the backlight is allowed to light: it is not while the keyboard waits for the USB host to enumerate it (a bus-powered
// device must stay within 100 mA until configured) or while the bus is suspended. A wake from sleep re-initialises the channels and
// with them this permission (led_hw_init), so the caller has to look at led_hw_enabled() and ask again.
void led_hw_enable(bool on);
bool led_hw_enabled(void);

// One subframe, from the timer interrupt (constant time).
void led_hw_subframe(void);

// The PWM paused / resumed around a matrix scan and a flash write: outputs off, strobes off.
void led_hw_pause(void);
void led_hw_resume(void);

// A subframe given up: everything off. Called instead of led_hw_subframe() when the timer interrupt ran late.
void led_hw_idle(void);
