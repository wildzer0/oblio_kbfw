#pragma once

// The battery log: what the radio's status answer says about the battery, written down while the keyboard is running on that battery.
//
// It exists because of a physical fact: the diagnostics travel over the keyboard's own USB, and with the cable in the battery is charging - the
// radio answers 0xFF in the battery byte instead of the level. So the readings that matter can only be taken with the cable out, which is
// exactly when nothing can read them. The keyboard writes them down itself, and the cable only fetches them afterwards.
//
// A record is one status answer: the minutes the keyboard has been awake since it was turned on, the battery byte and the flags byte. One is
// written when the answer changes, and anyway every BLOG_PERIOD_MIN minutes, so the log is a curve and not a flood. It lives in RAM: turning the
// keyboard off (or pulling its battery) starts a new one.

#include <stdbool.h>
#include <stdint.h>

#define BLOG_RECORDS    48u // 48 records of 4 bytes: 192 bytes of XDATA
#define BLOG_PERIOD_MIN 5u  // a record at least this often, even when nothing changed

void blog_init(void);

// From the main loop, with the millisecond clock, whether the keyboard is on its battery and the radio's status answer (0 if there is none).
// Counts the minutes the keyboard is awake and writes the records.
void blog_task(uint16_t now_ms, bool on_battery, const uint8_t *status);

uint8_t blog_count(void); // how many records there are (0 to BLOG_RECORDS)

// Record `index` (0 = the oldest), as `out` = minutes (little endian), battery byte, flags byte. False if there is no such record.
bool blog_get(uint8_t index, uint8_t out[4]);
