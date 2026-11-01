#pragma once

// When the keyboard goes to sleep: after a time without use, or when asked. Pure logic; the sequence that puts the hardware to sleep is
// in board/sleep_hw.c.
//
// The time without use is counted in seconds out of the millisecond clock of the main loop (a 16-bit clock wraps in 65 s, so this has
// to be called at least that often). The levels are the ones the keys Fn+O / Fn+P step through: 1, 5, 10, 30 minutes, never.

#include <stdbool.h>
#include <stdint.h>

#define SLEEP_LEVELS        5
#define SLEEP_LEVEL_DEFAULT 2 // 10 minutes

// Seconds without use before sleeping, by level; 0 = never on its own
uint16_t sleep_level_seconds(uint8_t level);

void sleep_init(uint16_t now);

// The time without use is counted from this level on. A level out of range is the default.
void sleep_set_level(uint8_t level);

// Something was used (a key, the knob): the count starts over.
void sleep_note_activity(void);

// Sleep as soon as it is allowed (a key for it).
void sleep_now(void);

// Regularly, with the time in ms. True when the keyboard should go to sleep now: the time without use has passed, or sleeping was
// asked for. `allowed`: sleeping is possible at all (on battery, not with the cable). When it is not, the count is kept at zero, so
// that it starts from the moment it becomes possible; and a request made meanwhile is forgotten.
bool sleep_due(uint16_t now, bool allowed);

// After waking up: the count starts over from `now`.
void sleep_woke(uint16_t now);
