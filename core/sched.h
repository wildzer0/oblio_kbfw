#pragma once

// The timer interrupt plays two kinds of slots, one after the other:
//   LED  - one column subframe of the backlight: the column stays lit for LED_US
//   SCAN - one scan of the key matrix, which takes a little under SCAN_US (the slot is that long, with room to spare)
// After SCHED_SUBFRAMES_PER_SCAN LED slots comes a SCAN slot. The key lines and the LED sinks are the same lines, so the two can never
// overlap; that is what this sequence is for. A frame of the backlight (16 columns) takes 16 LED slots and 4 scans: 8.4 ms, 119 Hz.
//
// The module knows the lengths of the slots, so that the interrupt can also keep the time in milliseconds without a second timer.
// It is pure: the timer and the hardware are in hal/tick.c and board/.

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SLOT_LED,
    SLOT_SCAN,
} slot_t;

#define SCHED_LED_US             400u
#define SCHED_SCAN_US            500u // the scan takes ~345 us (16 columns x 2 readings x 10 us settle, plus the strobes) and the PWM is parked and restarted around it: ~100 us of room
#define SCHED_SUBFRAMES_PER_SCAN 4u

uint16_t sched_slot_us(slot_t slot);

void sched_init(void);

// At each interrupt: returns the slot that starts now (what to do), and says whether a millisecond has passed since the previous
// interrupt (the slot before this one has ended).
slot_t sched_step(bool *millisecond);

// The slot after the one that started in the last sched_step(): the interrupt programs the timer for its length.
slot_t sched_upcoming(void);
