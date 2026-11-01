#include "sched.h"

static slot_t   current;      // the slot that starts at the next interrupt
static uint16_t previous_us;  // the length of the slot that has just ended
static uint16_t accumulated;  // microseconds not yet counted into a millisecond
static uint8_t  subframes;    // LED slots since the last scan

uint16_t sched_slot_us(slot_t slot)
{
    return slot == SLOT_SCAN ? SCHED_SCAN_US : SCHED_LED_US;
}

void sched_init(void)
{
    current     = SLOT_LED;
    previous_us = SCHED_LED_US; // the start-up period before the first interrupt counts as a slot of that length
    accumulated = 0;
    subframes   = 0;
}

slot_t sched_step(bool *millisecond)
{
    const slot_t now = current;

    accumulated = (uint16_t)(accumulated + previous_us);
    *millisecond = accumulated >= 1000u;

    if (*millisecond) {
        accumulated = (uint16_t)(accumulated - 1000u);
    }

    previous_us = sched_slot_us(now);

    if (now == SLOT_SCAN) {
        subframes = 0;
        current   = SLOT_LED;
    } else {
        subframes++;
        current = (subframes >= SCHED_SUBFRAMES_PER_SCAN) ? SLOT_SCAN : SLOT_LED;
    }

    return now;
}

slot_t sched_upcoming(void)
{
    return current;
}
