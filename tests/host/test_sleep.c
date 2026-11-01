#include "check.h"
#include "sleep.h"

static uint16_t now;

// runs the clock for `ms`, looking every 10 ms as the main loop does
static bool run(unsigned ms, bool allowed)
{
    bool due = false;

    for (unsigned i = 0; i < ms; i += 10) {
        now = (uint16_t)(now + 10);
        due = sleep_due(now, allowed) || due;
    }

    return due;
}

static void reset(uint8_t level)
{
    now = 0;
    sleep_init(now);
    sleep_set_level(level);
    sleep_due(now, true); // the restart of a new level is taken
}

static void test_levels(void)
{
    CHECK(sleep_level_seconds(0) == 60 && sleep_level_seconds(1) == 300 && sleep_level_seconds(2) == 600);
    CHECK(sleep_level_seconds(3) == 1800 && sleep_level_seconds(4) == 0);
    CHECK(sleep_level_seconds(9) == 600); // out of range: the default
}

static void test_it_sleeps_after_the_time_without_use(void)
{
    reset(0); // one minute
    CHECK(!run(59000, true));
    CHECK(run(1500, true));
}

static void test_never_means_never(void)
{
    reset(4);
    CHECK(!run(600000, true)); // ten minutes of idling
    CHECK(!run(600000, true));
}

static void test_use_starts_the_count_over(void)
{
    reset(0);
    run(50000, true);
    sleep_note_activity();
    CHECK(!run(50000, true)); // 50 s since the key, not 100
    CHECK(run(15000, true));
}

static void test_a_shorter_level_restarts_the_count(void)
{
    reset(2); // ten minutes
    run(120000, true);
    sleep_set_level(0); // one minute: counted from now, not found already past
    CHECK(!run(30000, true));
    CHECK(run(40000, true));
}

static void test_it_can_be_asked_for(void)
{
    reset(2);
    sleep_now();
    CHECK(run(10, true));
    CHECK(!run(10, true)); // once
}

static void test_not_allowed_means_awake_and_the_count_starts_when_it_becomes_possible(void)
{
    reset(0);
    CHECK(!run(120000, false)); // on the cable: no
    CHECK(!run(30000, true));   // the cable is out: a minute from now, not from before
    CHECK(run(35000, true));

    reset(2);
    sleep_now();
    CHECK(!run(100, false)); // a request made where sleeping is not possible is forgotten
    CHECK(!run(100, true));
}

static void test_the_clock_wrap(void)
{
    reset(0);
    now = 65000;
    sleep_woke(now);
    CHECK(!run(55000, true)); // across the wrap of the 16-bit clock
    CHECK(run(10000, true));
}

static void test_after_waking_the_count_starts_over(void)
{
    reset(0);
    run(58000, true);
    sleep_woke(now);
    CHECK(!run(50000, true));
    CHECK(run(15000, true));
}

int main(void)
{
    test_levels();
    test_it_sleeps_after_the_time_without_use();
    test_never_means_never();
    test_use_starts_the_count_over();
    test_a_shorter_level_restarts_the_count();
    test_it_can_be_asked_for();
    test_not_allowed_means_awake_and_the_count_starts_when_it_becomes_possible();
    test_the_clock_wrap();
    test_after_waking_the_count_starts_over();
    return tests_result();
}
