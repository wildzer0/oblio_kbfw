#pragma once

// The timer interrupt plays two kinds of slots, one after the other:
//   LED  - one column subframe of the backlight: the column stays lit for SCHED_LED_US, counted from the start of the slot
//   SCAN - one scan of the key matrix, which takes as long as it takes (up to SCHED_SCAN_MAX_US) and is followed at once by the next slot
// After SCHED_SUBFRAMES_PER_SCAN LED slots comes a SCAN slot. The key lines and the LED sinks are the same lines, so the two can never
// overlap; that is what this sequence is for.
//
// Every slot is timed from the start of its own interrupt, as the SMK port does: an LED slot is always a full SCHED_LED_US, whatever the scan
// before it took. (A fixed grid of slots cut the LED slot after a long scan short, and one column in five lit for a fraction of its time: a
// flicker.) So the frame of the backlight is not fixed: 16 LED slots and 2 scans, about 8 ms.
//
// Why two scans and not four: a scan measured 753 us on the keyboard against a 500 us slot, so four per frame took a third of the CPU for the
// keys and left the main loop one pass between scans - a key press, which sends a radio frame, was enough to starve it and the backlight then
// stood aside. Halving the scans is half that load. The debounce is a time (MATRIX_DEBOUNCE_MS), so the keys do not pay for it.
//
// The module counts time in timer counts (SCHED_COUNTS_PER_US per microsecond) and keeps the milliseconds from the time the interrupt says has
// really passed, so the clock does not depend on the lengths of the slots. It is pure: the timer and the hardware are in hal/tick.c and board/.

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SLOT_LED,
    SLOT_SCAN,
} slot_t;

#define SCHED_COUNTS_PER_US      2u // the timer counts at FREQ_SYS / 12 = 2 MHz
#define SCHED_COUNTS_PER_MS      (1000u * SCHED_COUNTS_PER_US)
#define SCHED_LED_US             400u
#define SCHED_SCAN_MAX_US        1000u // the timer is armed for this long during a scan, more than any scan takes; the interrupt then ends the slot itself
#define SCHED_SUBFRAMES_PER_SCAN 4u

// The length the timer is armed for at the start of a slot
uint16_t sched_slot_us(slot_t slot);

void sched_init(void);

// At each interrupt: `elapsed` is the time that has passed, in timer counts, since the previous interrupt began. Returns the slot that starts now
// (what to do) and the number of whole milliseconds that have passed (usually 0 or 1; more only if a slot ran very long).
slot_t sched_step(uint16_t elapsed, uint8_t *milliseconds);

// The slot after the one that started in the last sched_step(): the interrupt programs the timer for its length.
slot_t sched_upcoming(void);
