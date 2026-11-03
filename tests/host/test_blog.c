#include "blog.h"
#include "check.h"
#include <string.h>

#if BATTERY_LOG

// The flash, as the log sees it: one sector that only ever loses bits when programmed and comes back all ones when erased
static uint8_t  area[512];
static unsigned erases;
static unsigned programs;

void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len)
{
    memcpy(out, &area[addr - BLOG_ADDR], len);
}

void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    programs++;

    for (uint8_t i = 0; i < len; i++) {
        area[addr - BLOG_ADDR + i] = (uint8_t)(area[addr - BLOG_ADDR + i] & src[i]); // a program can only clear bits
    }
}

void blog_hook_erase(uint16_t addr)
{
    (void)addr;
    erases++;
    memset(area, 0xFF, sizeof area);
}

static bool     on_battery;
static uint16_t now;

static void reset(void)
{
    memset(area, 0xFF, sizeof area);
    erases     = 0;
    programs   = 0;
    on_battery = true;
    now        = 0;
    blog_init();
}

// Time goes on in steps of a second, the way the main loop sees it, with the same reading
static void advance(uint32_t ms, uint16_t reading)
{
    while (ms > 0) {
        const uint16_t step = (ms > 1000u) ? 1000u : (uint16_t)ms;

        now = (uint16_t)(now + step);
        blog_task(now, on_battery, reading);
        ms -= step;
    }
}

static uint16_t reading_of(const uint8_t *record)
{
    return (uint16_t)(record[2] | ((uint16_t)(record[3] & 0x03u) << 8));
}

static void test_nothing_is_written_without_a_reading(void)
{
    reset();
    advance(10u * 60u * 1000u, 0);
    CHECK(blog_count() == 0 && programs == 0);
}

// The first reading after a start is a record, marked as the first after a gap
static void test_the_first_reading_is_written_and_marked(void)
{
    reset();
    advance(1000, 981);
    CHECK(blog_count() == 1);

    uint8_t record[4];
    CHECK(blog_get(0, record));
    CHECK(reading_of(record) == 981);
    CHECK((record[3] & BLOG_FLAG_WAKE) != 0 && (record[3] & BLOG_FLAG_CABLE) == 0);
    CHECK(record[0] == 0 && record[1] == 0);                  // minute 0

    advance(1000, 981);
    CHECK(blog_count() == 1);                                 // nothing changed
}

// The ADC wobbles by a count or two: that is not a change
static void test_a_wobble_is_not_a_record_but_a_step_is(void)
{
    reset();
    advance(1000, 981);
    advance(1000, 982);
    advance(1000, 979);
    CHECK(blog_count() == 1);

    advance(1000, 971); // 10 counts below the last record
    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(1, record) && reading_of(record) == 971 && (record[3] & BLOG_FLAG_WAKE) == 0);
}

static void test_an_unchanged_reading_is_written_every_five_minutes(void)
{
    reset();
    advance(1000, 900);
    advance(4u * 60u * 1000u, 900);
    CHECK(blog_count() == 1); // four minutes: not yet
    advance(60u * 1000u, 900);
    CHECK(blog_count() == 2); // five

    uint8_t record[4];
    CHECK(blog_get(1, record) && (uint16_t)(record[0] | ((uint16_t)record[1] << 8)) == 5); // stamped at minute five
}

// The cable going in or out is a record whatever the reading does: with it the reading saturates
static void test_the_cable_is_marked_and_its_change_is_written(void)
{
    reset();
    advance(1000, 950);
    on_battery = false;
    advance(1000, 1023);
    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(1, record) && (record[3] & BLOG_FLAG_CABLE) != 0 && reading_of(record) == 1023);

    on_battery = true;
    advance(1000, 1023); // the cable is out, the reading has not moved yet
    CHECK(blog_count() == 3);
}

// A wake from sleep: the chip had no clock in between, so the record says there is a gap before it
static void test_a_wake_makes_a_marked_record(void)
{
    reset();
    advance(1000, 940);
    advance(1000, 940);
    CHECK(blog_count() == 1);

    blog_note_wake();
    advance(1000, 939); // the reading is the same within the wobble, and still it is a record
    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(1, record) && (record[3] & BLOG_FLAG_WAKE) != 0 && reading_of(record) == 939);

    advance(1000, 939);
    CHECK(blog_count() == 2); // and only the first
}

// The reason the log lives in flash: plugging the cable resets the board, and the records have to be there afterwards
static void test_it_survives_a_reset_and_goes_on(void)
{
    reset();
    advance(1000, 900);
    advance(1000, 880);
    CHECK(blog_count() == 2);

    blog_init(); // the chip started again: the RAM is gone, the flash is not
    CHECK(blog_count() == 2);

    advance(1000, 880);
    CHECK(blog_count() == 3); // the first reading after a start is always a record
    CHECK(erases == 0);

    uint8_t record[4];
    CHECK(blog_get(0, record) && reading_of(record) == 900);
    CHECK(blog_get(2, record) && (record[3] & BLOG_FLAG_WAKE) != 0);
}

// What the sector held before the log existed (the stock firmware, an older build) is not a log: it is not listed, and the first record starts clean
static void test_a_sector_with_something_else_in_it_is_erased_before_use(void)
{
    reset();
    memset(area, 0x5A, sizeof area);
    blog_init();
    CHECK(blog_count() == 0);

    advance(1000, 950);
    CHECK(erases == 1 && blog_count() == 1);

    uint8_t record[4];
    CHECK(blog_get(0, record) && reading_of(record) == 950);
}

static void test_the_sector_starts_over_when_full(void)
{
    reset();

    for (unsigned i = 0; i < BLOG_RECORDS + 4; i++) {
        advance(1000, (uint16_t)(1000u - 20u * (i % 2u))); // a step of 20 counts each time
    }

    CHECK(erases == 1);                // the sector was given back once
    CHECK(blog_count() == 4);

    uint8_t record[4];
    CHECK(blog_get(0, record) && reading_of(record) == 1000);
    CHECK(!blog_get(4, record));       // and nothing beyond the newest
}

int main(void)
{
    test_nothing_is_written_without_a_reading();
    test_the_first_reading_is_written_and_marked();
    test_a_wobble_is_not_a_record_but_a_step_is();
    test_an_unchanged_reading_is_written_every_five_minutes();
    test_the_cable_is_marked_and_its_change_is_written();
    test_a_wake_makes_a_marked_record();
    test_it_survives_a_reset_and_goes_on();
    test_a_sector_with_something_else_in_it_is_erased_before_use();
    test_the_sector_starts_over_when_full();
    return tests_result();
}

#else

int main(void)
{
    return 0; // the log is left out of this build
}

#endif
