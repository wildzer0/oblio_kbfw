#include "check.h"
#include "sched.h"

static void test_four_subframes_then_a_scan(void)
{
    uint8_t ms;

    sched_init();

    for (unsigned round = 0; round < 3; round++) {
        for (unsigned i = 0; i < SCHED_SUBFRAMES_PER_SCAN; i++) {
            CHECK(sched_step(0, &ms) == SLOT_LED);
        }
        CHECK(sched_step(0, &ms) == SLOT_SCAN);
    }
}

// The interrupt programs the timer for the slot that starts now, and has to know the one after it to do the same next time
static void test_upcoming_is_the_slot_that_really_follows(void)
{
    uint8_t ms;

    sched_init();

    for (unsigned i = 0; i < 40; i++) {
        sched_step(0, &ms);
        const slot_t after = sched_upcoming();

        CHECK(sched_step(0, &ms) == after);
    }
}

static void test_the_lengths_the_timer_is_armed_for(void)
{
    CHECK(sched_slot_us(SLOT_LED) == 400);
    CHECK(sched_slot_us(SLOT_SCAN) == 1000);
    CHECK(SCHED_COUNTS_PER_MS == 2000);
}

// The milliseconds follow the time that the interrupt says has passed, whatever the slots were: scans of any length
static void test_milliseconds_follow_the_elapsed_time(void)
{
    uint8_t  ms;
    unsigned elapsed_counts = 0;
    unsigned counted        = 0;

    sched_init();

    for (unsigned i = 0; i < 20000; i++) {
        const unsigned took = (i % 5 == 4) ? 1600u + (i % 7) * 40u : SCHED_LED_US * SCHED_COUNTS_PER_US + (i % 3);

        sched_step((uint16_t)took, &ms);
        counted += ms;
        elapsed_counts += took;
    }

    CHECK(counted * SCHED_COUNTS_PER_MS <= elapsed_counts);
    CHECK(counted * SCHED_COUNTS_PER_MS + SCHED_COUNTS_PER_MS > elapsed_counts);
}

static void test_a_long_slot_counts_every_millisecond_it_covers(void)
{
    uint8_t ms;

    sched_init();
    sched_step(5 * SCHED_COUNTS_PER_MS + 10, &ms);
    CHECK(ms == 5);
    sched_step(SCHED_COUNTS_PER_MS - 10, &ms); // the 10 counts left over complete the next one
    CHECK(ms == 1);
}

static void test_a_short_slot_may_count_none(void)
{
    uint8_t ms;

    sched_init();
    sched_step(800, &ms);
    CHECK(ms == 0);
    sched_step(800, &ms);
    CHECK(ms == 0);
    sched_step(800, &ms);
    CHECK(ms == 1); // 2400 counts
}

int main(void)
{
    test_four_subframes_then_a_scan();
    test_upcoming_is_the_slot_that_really_follows();
    test_the_lengths_the_timer_is_armed_for();
    test_milliseconds_follow_the_elapsed_time();
    test_a_long_slot_counts_every_millisecond_it_covers();
    test_a_short_slot_may_count_none();
    return tests_result();
}
