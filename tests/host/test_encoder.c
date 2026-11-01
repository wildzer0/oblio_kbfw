#include "check.h"
#include "encoder.h"

static uint8_t state;

uint8_t encoder_hook_state(uint8_t index)
{
    (void)index;
    return state;
}

static void go(uint8_t next)
{
    state = next;
    encoder_poll();
}

static void reset(uint8_t at)
{
    encoder_init();
    state = at;
    encoder_poll(); // the first reading: where the knob is
}

// 0 -> 1 -> 3 -> 2 -> 0 is clockwise
static void cw_cycle(void)
{
    go(1);
    go(3);
    go(2);
    go(0);
}

static void ccw_cycle(void)
{
    go(2);
    go(3);
    go(1);
    go(0);
}

static unsigned count(bool expect_clockwise)
{
    bool     clockwise;
    unsigned n = 0;

    while (encoder_next(0, &clockwise)) {
        CHECK(clockwise == expect_clockwise);
        n++;
    }

    return n;
}

static void test_a_cycle_is_a_detent(void)
{
    reset(0);
    cw_cycle();
    CHECK(count(true) == 1);

    reset(0);
    ccw_cycle();
    CHECK(count(false) == 1);
}

static void test_part_of_a_cycle_is_not_a_detent(void)
{
    reset(0);
    go(1);
    go(3);
    go(2);
    CHECK(count(true) == 0);
    go(0); // the fourth change completes it
    CHECK(count(true) == 1);
}

static void test_detents_add_up_in_order_of_arrival(void)
{
    reset(0);
    cw_cycle();
    cw_cycle();
    cw_cycle();
    CHECK(count(true) == 3);
}

static void test_the_detent_does_not_have_to_start_at_rest(void)
{
    reset(3); // at a detent of an encoder whose rest is state 3
    go(1);
    go(0);
    go(2);
    go(3);
    CHECK(count(false) == 1); // 3 -> 1 -> 0 -> 2 -> 3 is the reverse way
}

static void test_a_bounce_cancels(void)
{
    reset(0);
    go(1);
    go(0);
    go(1);
    go(0);
    go(1);
    go(0);
    CHECK(count(true) == 0);
    cw_cycle();
    CHECK(count(true) == 1); // and the count is back at zero, not off by the bounces
}

static void test_a_missed_sample_counts_for_nothing(void)
{
    reset(0);
    go(3); // 0 -> 3: two steps at once
    go(2);
    go(0);
    CHECK(count(true) == 0);
}

static void test_a_direction_change_inside_a_detent(void)
{
    reset(0);
    go(1);
    go(3);
    go(1);
    go(0); // back where it started
    CHECK(count(true) == 0);
}

static void test_the_first_reading_is_not_a_move(void)
{
    encoder_init();
    state = 2;
    encoder_poll();
    CHECK(count(true) == 0);
    go(3);
    go(1);
    go(0);
    go(2);
    CHECK(count(false) == 1);
}

static void test_nothing_for_an_encoder_that_does_not_exist(void)
{
    bool clockwise;

    reset(0);
    cw_cycle();
    CHECK(!encoder_next(ENCODER_COUNT, &clockwise));
}

int main(void)
{
    test_a_cycle_is_a_detent();
    test_part_of_a_cycle_is_not_a_detent();
    test_detents_add_up_in_order_of_arrival();
    test_the_detent_does_not_have_to_start_at_rest();
    test_a_bounce_cancels();
    test_a_missed_sample_counts_for_nothing();
    test_a_direction_change_inside_a_detent();
    test_the_first_reading_is_not_a_move();
    test_nothing_for_an_encoder_that_does_not_exist();
    return tests_result();
}
