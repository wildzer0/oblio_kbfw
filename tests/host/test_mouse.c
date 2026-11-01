#include "check.h"
#include "keycodes.h"
#include "mouse.h"
#include "report.h"

// The mouse fills the report state (core/report.c, which is the real one here) and asks the transport to send it: the transport is the
// only thing this test stands in for.
static unsigned sends; // REPORT_MOUSE sends

void report_hook_send(report_kind_t kind)
{
    if (kind == REPORT_MOUSE) {
        sends++;
    }
}

// What a report would carry now; the build uses the movement up, as the real one does
typedef struct {
    uint8_t buttons;
    int16_t x;
    int16_t y;
    int8_t  wheel;
} state_t;

static state_t take(void)
{
    uint8_t out[REPORT_MOUSE_SIZE];
    state_t s;

    report_build_mouse(out);
    s.buttons = out[0];
    s.x       = (int16_t)(out[1] | ((uint16_t)out[2] << 8));
    s.y       = (int16_t)(out[3] | ((uint16_t)out[4] << 8));
    s.wheel   = (int8_t)out[5];
    return s;
}

static void reset(void)
{
    report_init();
    mouse_init();
    sends = 0;
}

static void test_a_direction_moves_at_once_and_then_repeats(void)
{
    reset();

    CHECK(mouse_process(KC_MS_RIGHT, true, 0));
    CHECK(sends == 1);

    state_t s = take();
    CHECK(s.x == 1 && s.y == 0); // one step on the press

    mouse_task(0); // the step of the press is not repeated
    CHECK(take().x == 0);

    mouse_task(MOUSE_REPEAT_MS); // and then every interval
    CHECK(take().x == 1);
    mouse_task((uint16_t)(2 * MOUSE_REPEAT_MS));
    CHECK(take().x == 1);

    mouse_process(KC_MS_RIGHT, false, 100);
    mouse_task(200); // let go: nothing more
    CHECK(take().x == 0);
}

static void test_holding_speeds_up(void)
{
    reset();

    mouse_process(KC_MS_DOWN, true, 0);
    take(); // the first step

    int16_t first = 0;
    int16_t last  = 0;

    for (uint16_t now = MOUSE_REPEAT_MS, n = 1; now <= 3 * MOUSE_ACCEL_MS; now = (uint16_t)(now + MOUSE_REPEAT_MS), n++) {
        mouse_task(now);
        const int16_t step = take().y;

        if (n == 1) {
            first = step;
        }
        last = step;
    }

    CHECK(first == 1);     // it starts at one unit
    CHECK(last > first);   // and the step gets longer while it is held
    CHECK(last <= (int16_t)MOUSE_STEP_MAX);
}

static void test_the_acceleration_keys_make_the_step_longer(void)
{
    reset();

    mouse_process(KC_MS_ACCEL2, true, 0);
    mouse_process(KC_MS_UP, true, 0);
    CHECK(take().y == -5); // one unit, plus two per acceleration step (MS_ACCEL2 = two)

    mouse_process(KC_MS_UP, false, 10);
    mouse_process(KC_MS_ACCEL2, false, 10);
    mouse_process(KC_MS_UP, true, 20);
    CHECK(take().y == -1);
}

static void test_a_wheel_tap_scrolls_once_and_holding_keeps_scrolling(void)
{
    reset();

    CHECK(mouse_process(KC_MS_WH_UP, true, 0));
    CHECK(take().wheel == 1);

    CHECK(mouse_process(KC_MS_WH_UP, false, 10)); // the encoder's detents are a tap: one notch each
    mouse_task(500);
    CHECK(take().wheel == 0);

    mouse_process(KC_MS_WH_DOWN, true, 600);
    CHECK(take().wheel == -1);
    mouse_task((uint16_t)(600 + MOUSE_WHEEL_MS));
    CHECK(take().wheel == -1); // held: it keeps scrolling

    mouse_process(KC_MS_WH_DOWN, false, 700);
    mouse_task(800);
    CHECK(take().wheel == 0);
}

static void test_the_buttons_are_a_mask(void)
{
    reset();

    mouse_process(KC_MS_BTN1, true, 0);
    CHECK(take().buttons == 0x01);

    mouse_process(KC_MS_BTN3, true, 0);
    CHECK(take().buttons == 0x05);

    mouse_process(KC_MS_BTN1, false, 0);
    CHECK(take().buttons == 0x04);

    CHECK(sends == 3); // one report for each change
}

static void test_something_that_is_not_a_mouse_key(void)
{
    reset();

    CHECK(!mouse_process(KC_A, true, 0));
    CHECK(!mouse_process(KC_MPLY, true, 0));
    CHECK(!mouse_process(MO(1), true, 0));
    CHECK(sends == 0);

    state_t s = take();
    CHECK(s.x == 0 && s.y == 0 && s.wheel == 0 && s.buttons == 0);
}

// The movement of a report is used up by it: a relative axis must not be sent twice
static void test_the_movement_is_used_up_by_the_report(void)
{
    reset();

    mouse_process(KC_MS_RIGHT, true, 0);
    CHECK(take().x == 1);
    CHECK(take().x == 0);
}

int main(void)
{
    test_a_direction_moves_at_once_and_then_repeats();
    test_holding_speeds_up();
    test_the_acceleration_keys_make_the_step_longer();
    test_a_wheel_tap_scrolls_once_and_holding_keeps_scrolling();
    test_the_buttons_are_a_mask();
    test_something_that_is_not_a_mouse_key();
    test_the_movement_is_used_up_by_the_report();
    return tests_result();
}
