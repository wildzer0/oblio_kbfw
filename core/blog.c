#include "blog.h"
#include "radio.h"

static uint8_t count; // records in the log
static uint8_t next;  // where the next one goes (the same as count while appending)

static uint16_t last_raw; // what the last record says, so a reading that only wobbles is not written again
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
    last_raw     = 0;
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

static void add(uint16_t raw, uint8_t flags)
{
    if (next >= BLOG_RECORDS) { // full: the sector starts over
        blog_hook_erase(BLOG_ADDR);
        count = 0;
        next  = 0;
    }

    uint8_t record[BLOG_RECORD_BYTES];

    record[0] = (uint8_t)(minutes & 0xFFu);
    record[1] = (uint8_t)(minutes >> 8);
    record[2] = (uint8_t)(raw & 0xFFu);
    record[3] = flags;

    blog_hook_program(record_addr(next), record, BLOG_RECORD_BYTES);

    next++;
    count = next;

    last_raw     = raw;
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
    // the high two bits of the 10-bit reading go in bits 4-5 of the flags byte, so the record keeps the whole reading in four bytes
    uint8_t flags = (uint8_t)((status[RADIO_STATUS_FLAGS_AT] & 0x8Fu) | ((status[RADIO_STATUS_BATTERY_HI_AT] & 0x03u) << 4));

    if (!on_battery) {
        flags |= BLOG_FLAG_CABLE;
    }

    // The ADC wobbles by a count or two from one answer to the next: written on every change the sector would fill in half a minute and be
    // erased again and again (a flash sector lasts some thousands of erases). So a reading counts as changed only when it has moved BLOG_STEP.
    const uint16_t raw   = (uint16_t)(battery | ((uint16_t)(status[RADIO_STATUS_BATTERY_HI_AT] & 0x03u) << 8));
    const uint16_t moved = (raw > last_raw) ? (uint16_t)(raw - last_raw) : (uint16_t)(last_raw - raw);

    if (count == 0 || moved >= BLOG_STEP || ((flags ^ last_flags) & 0xCFu) != 0
        || (uint16_t)(minutes - last_minutes) >= BLOG_PERIOD_MIN) {
        add(raw, flags);
    }
}
