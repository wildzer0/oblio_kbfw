#pragma once

#include <stdint.h>

// Milliseconds since power-on, wrapping at 65536: the clock of the main loop (counted from the timer's slots, see core/sched.h).
uint16_t board_now_ms(void);
