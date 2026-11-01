#include "sched.h"

static slot_t   current;     // the slot that starts at the next interrupt
static uint16_t accumulated; // timer counts not yet counted into a millisecond
static uint8_t  subframes;   // LED slots since the last scan

uint16_t sched_slot_us(slot_t slot)
{
    return slot == SLOT_SCAN ? SCHED_SCAN_MAX_US : SCHED_LED_US;
}

void sched_init(void)
{
    current     = SLOT_LED;
    accumulated = 0;
    subframes   = 0;
}

slot_t sched_step(uint16_t elapsed, uint8_t *milliseconds)
{
    const slot_t now = current;

    accumulated = (uint16_t)(accumulated + elapsed);
    *milliseconds = 0;

    while (accumulated >= SCHED_COUNTS_PER_MS) {
        accumulated = (uint16_t)(accumulated - SCHED_COUNTS_PER_MS);
        (*milliseconds)++;
    }

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
