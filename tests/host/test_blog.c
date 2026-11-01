#include "blog.h"
#include "check.h"
#include "radio.h"
#include <string.h>

// The radio, as the log sees it: a status answer that is there or is not, and whether the keyboard is on its battery
static uint8_t status[6];
static bool    have_status;
static bool    on_battery;

static void reset(void)
{
    memset(status, 0, sizeof status);
    have_status = false;
    on_battery  = false;
    blog_init();
}

// The main loop passes the milliseconds it has: feed them in pieces a real clock could produce
static uint16_t now;

static void advance(uint32_t ms)
{
    while (ms > 0) {
        const uint16_t step = (ms > 10000u) ? 10000u : (uint16_t)ms;

        now = (uint16_t)(now + step);
        blog_task(now, on_battery, have_status ? status : 0);
        ms -= step;
    }
}

static void set_battery(uint8_t battery, uint8_t flags)
{
    status[RADIO_STATUS_BATTERY_AT] = battery;
    status[RADIO_STATUS_FLAGS_AT]   = flags;
    have_status = true;
}

static void test_nothing_is_written_without_an_answer(void)
{
    reset();

    on_battery = true; // on battery, but the radio has not answered yet
    advance(10u * 60u * 1000u);

    CHECK(blog_count() == 0);
}

static void test_nothing_is_written_with_the_cable_in(void)
{
    reset();
    set_battery(0x03, 0x01);

    on_battery = false; // the cable is in: the battery is charging, and the byte would be the sentinel
    advance(10u * 60u * 1000u);

    CHECK(blog_count() == 0);
}

static void test_a_changed_answer_is_written_at_once(void)
{
    reset();
    set_battery(0x07, 0x01);
    on_battery = true;

    advance(1000); // a second
    CHECK(blog_count() == 1); // the first reading

    uint8_t record[4];
    CHECK(blog_get(0, record));
    CHECK(record[2] == 0x07 && record[3] == 0x01);
    CHECK(record[0] == 0 && record[1] == 0); // at minute zero

    advance(1000);
    CHECK(blog_count() == 1); // nothing changed, and five minutes have not passed

    set_battery(0x06, 0x01); // the level moved
    advance(1000);
    CHECK(blog_count() == 2);
    CHECK(blog_get(1, record) && record[2] == 0x06);

    set_battery(0x06, 0x00); // the flags moved
    advance(1000);
    CHECK(blog_count() == 3);
    CHECK(blog_get(2, record) && record[3] == 0x00);
}

static void test_an_unchanged_answer_is_written_every_five_minutes(void)
{
    reset();
    set_battery(0x05, 0x01);
    on_battery = true;

    advance(1000);
    CHECK(blog_count() == 1);

    advance(4u * 60u * 1000u); // four minutes: not yet
    CHECK(blog_count() == 1);

    advance(60u * 1000u); // five: one now
    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(1, record));
    CHECK(record[0] == 5 && record[1] == 0); // stamped at minute five
    CHECK(record[2] == 0x05);
}

static void test_the_log_is_a_ring_of_its_size(void)
{
    reset();
    on_battery = true;

    for (uint8_t i = 0; i < BLOG_RECORDS + 4; i++) {
        set_battery((uint8_t)(i & 0x07), 0x01);
        advance(1000);
    }

    CHECK(blog_count() == BLOG_RECORDS);

    uint8_t record[4];
    CHECK(blog_get(0, record) && record[2] == 4);                    // the oldest kept: four were overwritten
    CHECK(blog_get(BLOG_RECORDS - 1, record) && record[2] == (BLOG_RECORDS + 3) % 8); // the newest is the last one written
    CHECK(!blog_get(BLOG_RECORDS, record));                          // and beyond the end there is nothing
}

static void test_the_sleeping_time_is_not_counted(void)
{
    reset();
    set_battery(0x04, 0x01);
    on_battery = true;

    advance(1000);
    CHECK(blog_count() == 1);

    // an hour goes by without the main loop running (the chip is asleep): the clock does not move, and nothing is written
    now = (uint16_t)(now + 500u); // half a second of passes, as a wake-up would produce
    blog_task(now, on_battery, status);
    CHECK(blog_count() == 1);
}

int main(void)
{
    test_nothing_is_written_without_an_answer();
    test_nothing_is_written_with_the_cable_in();
    test_a_changed_answer_is_written_at_once();
    test_an_unchanged_answer_is_written_every_five_minutes();
    test_the_log_is_a_ring_of_its_size();
    test_the_sleeping_time_is_not_counted();
    return tests_result();
}
