#include "check.h"
#include "sched.h"

static void test_four_subframes_then_a_scan(void)
{
    bool ms;

    sched_init();

    for (unsigned round = 0; round < 3; round++) {
        for (unsigned i = 0; i < SCHED_SUBFRAMES_PER_SCAN; i++) {
            CHECK(sched_step(&ms) == SLOT_LED);
        }
        CHECK(sched_step(&ms) == SLOT_SCAN);
    }
}

// The interrupt programs the timer for the slot after the one that just started, so it has to know it before it happens
static void test_upcoming_is_the_slot_that_really_follows(void)
{
    bool ms;

    sched_init();

    for (unsigned i = 0; i < 40; i++) {
        sched_step(&ms);
        const slot_t after = sched_upcoming();

        CHECK(sched_step(&ms) == after);
    }
}

static void test_a_frame_and_a_scan_period(void)
{
    const unsigned frame_us = 16 * SCHED_LED_US + (16 / SCHED_SUBFRAMES_PER_SCAN) * SCHED_SCAN_US;
    const unsigned scan_us  = SCHED_SUBFRAMES_PER_SCAN * SCHED_LED_US + SCHED_SCAN_US;

    CHECK(frame_us == 8200); // 122 Hz
    CHECK(scan_us == 2050);  // a key is polled every ~2 ms
    CHECK(sched_slot_us(SLOT_LED) == 400 && sched_slot_us(SLOT_SCAN) == 450);
}

// The milliseconds counted follow the time that really passed (the sum of the slot lengths)
static void test_milliseconds_follow_the_slots(void)
{
    bool     ms;
    unsigned elapsed_us = 0;
    unsigned counted    = 0;

    sched_init();

    for (unsigned i = 0; i < 20000; i++) {
        const slot_t slot = sched_step(&ms);

        counted += ms;
        elapsed_us += sched_slot_us(slot);
    }

    // a slot is counted when it has ended, so the count is up to a slot behind
    CHECK(counted * 1000 <= elapsed_us + SCHED_SCAN_US);
    CHECK(counted * 1000 + 1000 + SCHED_SCAN_US >= elapsed_us);
    CHECK(counted > 8000);
}

static void test_never_two_milliseconds_in_a_row(void)
{
    bool     ms;
    unsigned in_a_row = 0;

    sched_init();

    for (unsigned i = 0; i < 1000; i++) {
        sched_step(&ms);
        in_a_row = ms ? in_a_row + 1 : 0;
        CHECK(in_a_row <= 1); // slots are shorter than a millisecond
    }
}

int main(void)
{
    test_four_subframes_then_a_scan();
    test_upcoming_is_the_slot_that_really_follows();
    test_a_frame_and_a_scan_period();
    test_milliseconds_follow_the_slots();
    test_never_two_milliseconds_in_a_row();
    return tests_result();
}
