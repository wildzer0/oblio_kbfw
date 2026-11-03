#include "blog.h"

#if BATTERY_LOG

static uint8_t count; // records in the log
static uint8_t next;  // where the next one goes (the same as count while appending)

static uint16_t last_reading; // what the last record says, so a reading that only wobbles is not written again
static uint8_t  last_flags;
static uint16_t last_minutes;
static bool     wake_pending; // the next record is the first since a start or a wake

// The clock the records are stamped with: seconds and minutes counted from the passes of the main loop. It does not advance while the chip
// sleeps, and that is the honest thing - nothing is sampled there either, so a record never claims to be older than the reading it holds.
static uint16_t ms_frac;
static uint16_t last_ms;
static uint8_t  seconds;
static uint16_t minutes;

static uint16_t record_addr(uint8_t index)
{
    return (uint16_t)(BLOG_ADDR + (uint16_t)index * BLOG_RECORD_BYTES);
}

void blog_init(void)
{
    count         = 0;
    next          = 0;
    ms_frac       = 0;
    last_ms       = 0;
    seconds       = 0;
    minutes       = 0;
    last_reading  = 0;
    last_flags    = 0;
    last_minutes  = 0;
    wake_pending  = true;

    uint8_t record[BLOG_RECORD_BYTES];

    for (uint8_t i = 0; i < BLOG_RECORDS; i++) {
        blog_hook_read(record_addr(i), record, BLOG_RECORD_BYTES);

        if (record[0] == 0xFFu && record[1] == 0xFFu && record[2] == 0xFFu && record[3] == 0xFFu) {
            break; // the sector was erased here: the rest is empty
        }

        if ((record[3] & 0xF0u) != BLOG_MARK) {
            // Not a record of ours: whatever the flash held before (the stock firmware, an older build), or a write cut short. Nothing is listed
            // from here and the first record erases the sector, so that it starts clean.
            next = BLOG_RECORDS;
            return;
        }

        count++;
        next++;
    }
}

void blog_note_wake(void)
{
    wake_pending = true;
}

uint8_t blog_count(void)
{
    return count;
}

bool blog_get(uint8_t index, uint8_t out[BLOG_RECORD_BYTES])
{
    if (index >= count) {
        return false;
    }

    blog_hook_read(record_addr(index), out, BLOG_RECORD_BYTES);
    return true;
}

static void add(uint16_t reading, uint8_t flags)
{
    if (next >= BLOG_RECORDS) { // full: the sector starts over
        blog_hook_erase(BLOG_ADDR);
        count = 0;
        next  = 0;
    }

    uint8_t record[BLOG_RECORD_BYTES];

    record[0] = (uint8_t)(minutes & 0xFFu);
    record[1] = (uint8_t)(minutes >> 8);
    record[2] = (uint8_t)(reading & 0xFFu);
    record[3] = flags;

    blog_hook_program(record_addr(next), record, BLOG_RECORD_BYTES);

    next++;
    count = next;

    last_reading = reading;
    last_flags   = flags;
    last_minutes = minutes;
    wake_pending = false;
}

void blog_task(uint16_t now_ms, bool on_battery, uint16_t reading)
{
    const uint16_t d = (uint16_t)(now_ms - last_ms);

    if (d != 0) {
        last_ms = now_ms;
        ms_frac = (uint16_t)(ms_frac + d);

        while (ms_frac >= 1000u) {
            ms_frac = (uint16_t)(ms_frac - 1000u);

            if (++seconds >= 60u) {
                seconds = 0;
                minutes++;
            }
        }
    }

    if (reading == 0) {
        return; // the radio has not answered yet, or the reading has not settled
    }

    uint8_t flags = (uint8_t)(BLOG_MARK | ((reading >> 8) & 0x03u));

    if (!on_battery) {
        flags |= BLOG_FLAG_CABLE;
    }

    // The ADC wobbles by a count or two from one answer to the next: written on every change the sector would fill in half a minute and be erased
    // again and again (a flash sector lasts some thousands of erases). So a reading counts as changed only when it has moved BLOG_STEP.
    const uint16_t moved = (reading > last_reading) ? (uint16_t)(reading - last_reading) : (uint16_t)(last_reading - reading);

    if (wake_pending) {
        flags |= BLOG_FLAG_WAKE;
    }

    if (count == 0 || wake_pending || moved >= BLOG_STEP || ((flags ^ last_flags) & BLOG_FLAG_CABLE) != 0
        || (uint16_t)(minutes - last_minutes) >= BLOG_PERIOD_MIN) {
        add(reading, flags);
    }
}

#else

typedef int blog_left_out; // the log is not in this build; a file with nothing in it is not valid C

#endif
