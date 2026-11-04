#include "check.h"
#include "charger.h"

static uint16_t now;

// `ms` milliseconds with the pins as given, looked at every 3 ms as the main loop does
static void hold(unsigned ms, bool cable, bool charging)
{
    for (unsigned i = 0; i < ms; i += 3) {
        now = (uint16_t)(now + 3);
        charger_feed(cable, charging, now);
    }
}

static void reset(void)
{
    now = 1000;
    charger_init();
}

static void test_the_first_look_takes_the_levels_as_they_are(void)
{
    reset();
    charger_feed(true, true, now);
    CHECK(charger_cable() && charger_charging());

    reset();
    charger_feed(false, false, now);
    CHECK(!charger_cable() && !charger_charging());
}

static void test_the_cable_needs_five_milliseconds(void)
{
    reset();
    charger_feed(false, false, now);
    hold(3, true, false);
    CHECK(!charger_cable()); // a glitch of a few milliseconds is not a cable
    hold(3, false, false);
    CHECK(!charger_cable());
    hold(12, true, false);
    CHECK(charger_cable());
    hold(3, false, false);
    CHECK(charger_cable()); // and not a cable pulled out either
    hold(12, false, false);
    CHECK(!charger_cable());
}

static void test_charging_is_taken_quickly_and_let_go_slowly(void)
{
    reset();
    charger_feed(true, false, now);
    hold(15, true, true);
    CHECK(!charger_charging()); // 20 ms
    hold(15, true, true);
    CHECK(charger_charging());
    hold(120, true, false);
    CHECK(charger_charging()); // a flicker of the status line near the end of the charge does not end it
    hold(120, true, false);
    CHECK(!charger_charging()); // 200 ms
}

static void test_a_glitch_restarts_the_count(void)
{
    reset();
    charger_feed(true, false, now);
    hold(150, true, true);
    CHECK(charger_charging());
    hold(150, true, false);
    hold(3, true, true); // back for a moment
    hold(150, true, false);
    CHECK(charger_charging()); // the 200 ms start again from the last change
    hold(60, true, false);
    CHECK(!charger_charging());
}

static void test_the_clock_wraps(void)
{
    reset();
    now = 0xFFF0u;
    charger_feed(false, false, now);
    hold(30, true, false);
    CHECK(charger_cable());
}

int main(void)
{
    test_the_first_look_takes_the_levels_as_they_are();
    test_the_cable_needs_five_milliseconds();
    test_charging_is_taken_quickly_and_let_go_slowly();
    test_a_glitch_restarts_the_count();
    test_the_clock_wraps();
    return tests_result();
}
