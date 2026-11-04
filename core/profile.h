#pragma once

#include "options.h"

#if LOOP_PROFILE

// The profile build (make LOOP_PROFILE=1 fw): how long each step of the main loop takes, how long a whole pass takes, and how much of the CPU each
// interrupt uses. Times are in counts of the timer, 2 to the microsecond (0.5 us). For each step: how many times it ran, the shortest, the
// longest and the sum (the mean is the sum over the count; the host does it). For the passes: how many fell in each band of length. Read with
// `tools/diag.py --profile`, started again with `--profile-reset`.
//
// The times are wall-clock times: a step includes the interrupts that ran during it. The share of each interrupt is measured apart, so the two
// can be told from each other. The measuring itself costs a little in every pass, which is why this is a build of its own.

#    include <stdint.h>

#    define PROFILE_STEPS      20u // step numbers 1 .. 15 are the steps of the main loop, 16 .. 19 are parts of led_task (see tools/diag.py); 0 is spare
#    define PROFILE_BANDS      8u  // pass length bands, in counts: < 400, 800, 1600, 3200, 6400, 12800, 25600, and longer (200 us .. 12.8 ms)

typedef enum { PROFILE_ISR_TIMER, PROFILE_ISR_USB, PROFILE_ISR_UART, PROFILE_ISR_COUNT } profile_isr_t;

void profile_reset(void);

// A step of `id` took `counts`.
void profile_step(uint8_t id, uint16_t counts);

// A whole pass of the main loop took `counts`.
void profile_pass(uint16_t counts);

// An interrupt of this kind ran for `counts`.
void profile_isr(profile_isr_t which, uint16_t counts);

// What has been measured.
uint16_t profile_step_count(uint8_t id);
uint16_t profile_step_min(uint8_t id);
uint16_t profile_step_max(uint8_t id);
uint32_t profile_step_sum(uint8_t id);
uint16_t profile_band(uint8_t band);
uint32_t profile_isr_sum(profile_isr_t which);

// Provided by the board: the clock of the measuring, a count that goes on at 2 a microsecond and wraps at 65536.
uint16_t profile_hook_now(void);

#endif
