#include "mouse.h"
#include "keycodes.h"
#include "report.h"

// The directions held down, as a mask
#define DIR_UP    0x01u
#define DIR_DOWN  0x02u
#define DIR_LEFT  0x04u
#define DIR_RIGHT 0x08u

// The wheel keys, one at a time (a wheel turned by a key has one direction at a time)
#define WHEEL_UP    1u
#define WHEEL_DOWN  2u

static uint8_t  held;      // the direction keys down
static uint8_t  speed;     // units per step, grows while a direction is held
static uint16_t step_due;  // when the next step is due
static uint16_t speed_due; // when the step grows again
static uint8_t  accel;     // the acceleration key down: 0, 1 or 2 extra units per step
static uint8_t  wheel;     // the wheel key down, 0 = none
static uint16_t wheel_due; // when the next wheel notch is due

// One step, in units: one, plus one for every 32 ms the direction has been held (up to the maximum), plus the acceleration key
static uint8_t step_units(void)
{
    const uint8_t n = (uint8_t)(speed + (uint8_t)(2u * accel));

    return (n > MOUSE_STEP_MAX) ? MOUSE_STEP_MAX : n;
}

static void step(void)
{
    const uint8_t n  = step_units();
    int8_t        dx = 0;
    int8_t        dy = 0;

    if (held & DIR_UP) {
        dy = (int8_t)-n;
    } else if (held & DIR_DOWN) {
        dy = (int8_t)n;
    }

    if (held & DIR_LEFT) {
        dx = (int8_t)-n;
    } else if (held & DIR_RIGHT) {
        dx = (int8_t)n;
    }

    if (dx || dy) {
        report_mouse_move(dx, dy);
        report_hook_send(REPORT_MOUSE);
    }
}

static void wheel_step(void)
{
    // One wheel axis: a horizontal wheel would need the pan field, which the mouse interface does not have (the stock firmware's had it)
    if (wheel == WHEEL_UP) {
        report_mouse_wheel(1);
    } else if (wheel == WHEEL_DOWN) {
        report_mouse_wheel(-1);
    } else {
        return;
    }

    report_hook_send(REPORT_MOUSE);
}

void mouse_init(void)
{
    held      = 0;
    speed     = 1;
    step_due  = 0;
    speed_due = 0;
    accel     = 0;
    wheel     = 0;
    wheel_due = 0;
}

static uint8_t direction_bit(uint16_t keycode)
{
    switch (keycode) {
        case KC_MS_UP:    return DIR_UP;
        case KC_MS_DOWN:  return DIR_DOWN;
        case KC_MS_LEFT:  return DIR_LEFT;
        case KC_MS_RIGHT: return DIR_RIGHT;
        default:          return 0;
    }
}

static uint8_t wheel_kind(uint16_t keycode)
{
    switch (keycode) {
        case KC_MS_WH_UP:   return WHEEL_UP;
        case KC_MS_WH_DOWN: return WHEEL_DOWN;
        default:            return 0; // MS_WH_LEFT / RIGHT: no axis for them (see wheel_step)
    }
}

bool mouse_process(uint16_t keycode, bool pressed, uint16_t now)
{
    if (!IS_MOUSE_KEYCODE(keycode)) {
        return false;
    }

    const uint8_t dir = direction_bit(keycode);

    if (dir) {
        if (pressed) {
            const bool from_rest = (held == 0);

            held |= dir;

            if (from_rest) { // the first direction: one step at once, and the acceleration starts over
                speed     = 1;
                speed_due = (uint16_t)(now + MOUSE_ACCEL_MS);
                step_due  = (uint16_t)(now + MOUSE_REPEAT_MS);
                step();
            }
        } else {
            held &= (uint8_t)~dir;

            if (!held) {
                speed = 1;
            }
        }

        return true;
    }

    const uint8_t w = wheel_kind(keycode);

    if (w) {
        if (pressed) {
            wheel     = w;
            wheel_due = (uint16_t)(now + MOUSE_WHEEL_MS);
            wheel_step(); // a tap scrolls once, holding it keeps scrolling
        } else if (wheel == w) {
            wheel = 0;
        }

        return true;
    }

    if (keycode >= KC_MS_BTN1 && keycode <= KC_MS_BTN5) {
        report_mouse_button((uint8_t)(1u << (keycode - KC_MS_BTN1)), pressed);
        report_hook_send(REPORT_MOUSE);
        return true;
    }

    // MS_ACCEL0-2 and MS_WH_LEFT / RIGHT: the first three make the step longer while held, the last two have no axis and do nothing
    if (keycode >= KC_MS_ACCEL0 && keycode <= KC_MS_ACCEL2) {
        accel = pressed ? (uint8_t)(keycode - KC_MS_ACCEL0) : 0; // MS_ACCEL0 is the base speed, 1 and 2 add 2 and 4 units
    }

    return true;
}

void mouse_task(uint16_t now)
{
    if (held && (int16_t)(now - step_due) >= 0) {
        step_due = (uint16_t)(now + MOUSE_REPEAT_MS);
        step();

        if (speed < MOUSE_STEP_MAX && (int16_t)(now - speed_due) >= 0) {
            speed_due = (uint16_t)(now + MOUSE_ACCEL_MS);

            if (speed < MOUSE_STEP_MAX) {
                speed++;
            }
        }
    }

    if (wheel && (int16_t)(now - wheel_due) >= 0) {
        wheel_due = (uint16_t)(now + MOUSE_WHEEL_MS);
        wheel_step();
    }
}
