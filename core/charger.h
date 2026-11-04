#pragma once

// The two lines the stock firmware reads to know about the charger (found in the dump, routine at 0xA058, called from the main loop at 0x9B4D):
//   P4.4  an input with no pull-up of its own: HIGH when the cable is there (the supply from outside). Taken as present after 5 samples that agree.
//   P7.7  an input with a pull-up: LOW while the cell is charging, HIGH when it is not (done, or no cable). The stock takes "charging" after 20 samples low and
//         "not charging" only after 200 samples high, as a charger's status output flickers when it is nearly done.
// The polarities are read from what the stock does with them (the gauge creeps up while the cable is in and stops at 99 % while P7.7 is low; the report
// to the PC software carries 0x10 when the cable is in): they are NOT measured on the keyboard yet. tools/diag.py shows the raw pins to check them.
//
// This module only debounces. The caller reads the pins (board/charger_hw.c) and feeds the levels as "asserted" values: cable_asserted is true when
// P4.4 is high, charging_asserted is true when P7.7 is low.

#include <stdbool.h>
#include <stdint.h>

// The debounce, in milliseconds (the stock counts samples; its sampling interval is not known, 1 ms is assumed)
#define CHARGER_CABLE_MS       5u
#define CHARGER_CHARGING_ON_MS 20u
#define CHARGER_CHARGING_OFF_MS 200u

void charger_init(void); // host tests only: the chip starts with zeroed memory

// From the main loop, as often as it comes round. The first call takes the levels as they are.
void charger_feed(bool cable_asserted, bool charging_asserted, uint16_t now_ms);

bool charger_cable(void);    // the cable is in (debounced)
bool charger_charging(void); // the cell is being charged (debounced)
