#include "tick.h"
#include "led_hw.h"
#include "matrix_hw.h"

// The two kinds of slot of the timer interrupt (core/sched.h)
void tick_hook_led(void)
{
    led_hw_subframe();
}

// A scan: the backlight is parked while the strobes are used to read the keys, then started again. A column lit by the previous
// subframe is switched off by the scan itself (it deselects all columns first).
void tick_hook_scan(uint8_t raw[16])
{
    led_hw_pause();
    matrix_hw_scan(raw);
    led_hw_resume();
}
