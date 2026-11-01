#include "blog.h"
#include "radio.h" // RADIO_STATUS_BATTERY_AT and RADIO_STATUS_FLAGS_AT: where the answer keeps what it has to say

static uint8_t data[BLOG_RECORDS][4]; // minutes (2), battery byte, flags byte

static uint8_t at;   // where the next record goes
static uint8_t count;

static uint8_t  last_battery; // what the last record says, so an unchanged answer is not written twice
static uint8_t  last_flags;
static uint16_t last_minutes;

// The clock the records are stamped with: seconds and minutes counted from the passes of the main loop. It does not advance while the chip
// sleeps, and that is the honest thing - nothing is sampled there either, so a record never claims to be older than the reading it holds.
static uint16_t ms_frac;
static uint16_t last_ms;
static uint8_t  seconds;
static uint16_t minutes;

static uint8_t wrap(uint8_t index) // the records are a ring: 48 of them, and no division to pay for
{
    while (index >= BLOG_RECORDS) {
        index = (uint8_t)(index - BLOG_RECORDS);
    }

    return index;
}

void blog_init(void)
{
    at     = 0;
    count  = 0;
    ms_frac = 0;
    last_ms = 0;
    seconds = 0;
    minutes = 0;
    last_minutes = 0;
    last_battery = 0;
    last_flags   = 0;
}

uint8_t blog_count(void)
{
    return count;
}

bool blog_get(uint8_t index, uint8_t out[4])
{
    if (index >= count) {
        return false;
    }

    const uint8_t i = wrap((uint8_t)(at + BLOG_RECORDS - count + index));

    for (uint8_t b = 0; b < 4; b++) {
        out[b] = data[i][b];
    }

    return true;
}

static void add(uint8_t battery, uint8_t flags)
{
    data[at][0] = (uint8_t)(minutes & 0xFFu);
    data[at][1] = (uint8_t)(minutes >> 8);
    data[at][2] = battery;
    data[at][3] = flags;

    at = wrap((uint8_t)(at + 1u));

    if (count < BLOG_RECORDS) {
        count++;
    }

    last_battery = battery;
    last_flags   = flags;
    last_minutes = minutes;
}

void blog_task(uint16_t now_ms, bool on_battery, const uint8_t *status)
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

    if (!on_battery || status == 0) {
        return; // the cable is in (the answer would be the sentinel) or there is no answer at all
    }

    const uint8_t battery = status[RADIO_STATUS_BATTERY_AT];
    const uint8_t flags   = status[RADIO_STATUS_FLAGS_AT];

    if (count == 0 || battery != last_battery || flags != last_flags
        || (uint16_t)(minutes - last_minutes) >= BLOG_PERIOD_MIN) {
        add(battery, flags);
    }
}
