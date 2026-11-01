#pragma once

// Mouse keys (KC_MS_*): a pointing device that no other key of the keyboard needs. A direction held down moves the cursor in steps, faster
// the longer it is held (and faster again with MS_ACCEL1/2 held), a wheel key scrolls, and the buttons 1-5 hold their bit. The report is
// sent through core/report.c like the keyboard's, so the transport decides where it goes: USB has a mouse interface (REPORT_ID_MOUSE on
// interface 0, as the stock firmware had), the radio protocol has no mouse frame and drops it.
//
// This is why the encoder can be a scroll wheel: assign MS_WH_UP / MS_WH_DOWN to its two directions in Vial and each detent scrolls once.

#include <stdbool.h>
#include <stdint.h>

#define MOUSE_REPEAT_MS 8u  // a step every 8 ms while a direction is held (the interval QMK uses)
#define MOUSE_ACCEL_MS  32u // the step grows every 32 ms it is held...
#define MOUSE_STEP_MAX  8u  // ...up to this many units a step
#define MOUSE_WHEEL_MS  48u // a wheel key held scrolls every 48 ms (a tapped one scrolls once)

void mouse_init(void);

// A mouse keycode went down or up. False (and nothing done) for a keycode that is not a mouse one.
bool mouse_process(uint16_t keycode, bool pressed, uint16_t now);

// Regularly from the main loop: the repeats while a direction or a wheel key is held.
void mouse_task(uint16_t now);
