#include "blog.h"
#include "check.h"
#include "radio.h"
#include <string.h>

// The flash, as the log sees it: one sector that only ever loses bits when programmed and comes back all ones when erased
static uint8_t  area[512];
static unsigned erases;
static unsigned programs;

void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len)
{
    const uint16_t at = (uint16_t)(addr - BLOG_ADDR);

    for (uint8_t i = 0; i < len; i++) {
        out[i] = area[at + i];
    }
}

void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    const uint16_t at = (uint16_t)(addr - BLOG_ADDR);

    programs++;

    for (uint8_t i = 0; i < len; i++) {
        area[at + i] = (uint8_t)(area[at + i] & src[i]); // a program can only clear bits
    }
}

void blog_hook_erase(uint16_t addr)
{
    (void)addr;
    erases++;
    memset(area, 0xFF, sizeof area);
}

static uint8_t status[6];
static bool    on_battery;
static uint16_t now;

static void reset(void)
{
    memset(area, 0xFF, sizeof area);
    memset(status, 0, sizeof status);
    erases   = 0;
    programs = 0;
    on_battery = true;
    now      = 0;
    blog_init();
}

static void advance(uint32_t ms, bool have_status)
{
    while (ms > 0) {
        const uint16_t step = (ms > 10000u) ? 10000u : (uint16_t)ms;

        now = (uint16_t)(now + step);
        blog_task(now, on_battery, have_status ? status : 0);
        ms -= step;
    }
}

static void test_nothing_is_written_without_an_answer(void)
{
    reset();
    advance(10u * 60u * 1000u, false);
    CHECK(blog_count() == 0);
    CHECK(programs == 0);
}

// Every answer goes in, cable or not: with the cable the byte is the charging sentinel and the record says so
static void test_every_answer_is_written_and_the_cable_marked(void)
{
    reset();
    status[RADIO_STATUS_BATTERY_AT] = 0x03;
    status[RADIO_STATUS_FLAGS_AT]   = 0x01;

    advance(1000, true);
    CHECK(blog_count() == 1);

    uint8_t record[4];
    CHECK(blog_get(0, record));
    CHECK(record[2] == 0x03);
    CHECK((record[3] & BLOG_FLAG_CABLE) == 0); // on battery

    on_battery = false; // the cable is in
    status[RADIO_STATUS_BATTERY_AT] = 0xFF;
    status[RADIO_STATUS_FLAGS_AT]   = 0x03;
    advance(1000, true);

    CHECK(blog_count() == 2);
    CHECK(blog_get(1, record));
    CHECK(record[2] == 0xFF);                        // the sentinel, as the radio sent it
    CHECK((record[3] & BLOG_FLAG_CABLE) != 0);       // and the mark that says why
    CHECK((record[3] & (uint8_t)~BLOG_FLAG_CABLE) == 0x03);
}

static void test_a_changed_answer_is_written_at_once(void)
{
    reset();
    status[RADIO_STATUS_BATTERY_AT] = 0x07;
    status[RADIO_STATUS_FLAGS_AT]   = 0x01;

    advance(1000, true);
    CHECK(blog_count() == 1);

    advance(1000, true);
    CHECK(blog_count() == 1); // nothing changed, and five minutes have not passed

    status[RADIO_STATUS_BATTERY_AT] = 0x06;
    advance(1000, true);
    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(1, record) && record[2] == 0x06);
}

static void test_an_unchanged_answer_every_five_minutes(void)
{
    reset();
    status[RADIO_STATUS_BATTERY_AT] = 0x05;
    status[RADIO_STATUS_FLAGS_AT]   = 0x01;

    advance(1000, true);
    CHECK(blog_count() == 1);

    advance(4u * 60u * 1000u, true);
    CHECK(blog_count() == 1); // four minutes: not yet

    advance(60u * 1000u, true);
    CHECK(blog_count() == 2); // five: one now

    uint8_t record[4];
    CHECK(blog_get(1, record));
    CHECK((uint16_t)(record[0] | ((uint16_t)record[1] << 8)) == 5); // stamped at minute five
}

// The reason the log lives in flash: plugging the cable resets the board, and the readings have to be there afterwards
static void test_it_survives_a_reset(void)
{
    reset();
    status[RADIO_STATUS_BATTERY_AT] = 0x04;
    status[RADIO_STATUS_FLAGS_AT]   = 0x01;

    advance(1000, true);
    status[RADIO_STATUS_BATTERY_AT] = 0x03;
    advance(60u * 1000u, true);
    CHECK(blog_count() == 2);

    blog_init(); // the chip started again: the RAM is gone, the flash is not

    CHECK(blog_count() == 2);

    uint8_t record[4];
    CHECK(blog_get(0, record) && record[2] == 0x04);
    CHECK(blog_get(1, record) && record[2] == 0x03);

    // and it goes on writing after the reset
    status[RADIO_STATUS_BATTERY_AT] = 0x02;
    advance(1000, true);
    CHECK(blog_count() == 3);
}

static void test_the_sector_starts_over_when_full(void)
{
    reset();
    status[RADIO_STATUS_FLAGS_AT] = 0x01;

    for (uint8_t i = 0; i < BLOG_RECORDS + 4; i++) {
        status[RADIO_STATUS_BATTERY_AT] = (uint8_t)(i & 0x07u);
        advance(1000, true);
    }

    CHECK(erases == 1); // the sector was given back once
    CHECK(blog_count() == 4);

    uint8_t record[4];
    CHECK(blog_get(0, record) && record[2] == 0);                              // the oldest kept
    CHECK(blog_get(3, record) && record[2] == (BLOG_RECORDS + 3) % 8);         // the newest
    CHECK(!blog_get(4, record));                                              // and nothing beyond it
}

int main(void)
{
    test_nothing_is_written_without_an_answer();
    test_every_answer_is_written_and_the_cable_marked();
    test_a_changed_answer_is_written_at_once();
    test_an_unchanged_answer_every_five_minutes();
    test_it_survives_a_reset();
    test_the_sector_starts_over_when_full();
    return tests_result();
}
