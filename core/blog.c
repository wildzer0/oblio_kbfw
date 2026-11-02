#include "blog.h"
#include "radio.h"

static uint8_t count; // records in the log
static uint8_t next;  // where the next one goes (the same as count while appending)

static uint8_t  last_battery; // what the last record says, so an unchanged answer is not written twice
static uint8_t  last_flags;
static uint16_t last_minutes;

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
    count        = 0;
    next         = 0;
    ms_frac      = 0;
    last_ms      = 0;
    seconds      = 0;
    minutes      = 0;
    last_battery = 0;
    last_flags   = 0;
    last_minutes = (uint16_t)(0u - BLOG_PERIOD_MIN); // so the first answer after a reset always gets a record: minutes - last >= period

    uint8_t record[BLOG_RECORD_BYTES];

    for (uint8_t i = 0; i < BLOG_RECORDS; i++) {
        blog_hook_read(record_addr(i), record, BLOG_RECORD_BYTES);

        const uint16_t stamped = (uint16_t)(record[0] | ((uint16_t)record[1] << 8));

        if (stamped == BLOG_EMPTY) {
            break; // the sector was erased here: the rest is empty
        }

        count++;
        next++;
    }
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

    blog_hook_read(record_addr(index), out, BLOG_RECORD_BYTES);
    return true;
}

static void add(uint8_t battery, uint8_t flags)
{
    if (next >= BLOG_RECORDS) { // full: the sector starts over
        blog_hook_erase(BLOG_ADDR);
        count = 0;
        next  = 0;
    }

    uint8_t record[BLOG_RECORD_BYTES];

    record[0] = (uint8_t)(minutes & 0xFFu);
    record[1] = (uint8_t)(minutes >> 8);
    record[2] = battery;
    record[3] = flags;

    blog_hook_program(record_addr(next), record, BLOG_RECORD_BYTES);

    next++;
    count = next;

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

    if (status == 0) {
        return; // the radio has not answered yet
    }

    const uint8_t battery = status[RADIO_STATUS_BATTERY_AT];
    const uint8_t flags   = on_battery ? status[RADIO_STATUS_FLAGS_AT]
                                       : (uint8_t)(status[RADIO_STATUS_FLAGS_AT] | BLOG_FLAG_CABLE);

    if (count == 0 || battery != last_battery || flags != last_flags
        || (uint16_t)(minutes - last_minutes) >= BLOG_PERIOD_MIN) {
        add(battery, flags);
    }
}
