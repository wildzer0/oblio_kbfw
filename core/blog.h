#pragma once

#include "options.h"

#if BATTERY_LOG

// The battery log: the battery reading, written down in **flash** (a sector of its own at BLOG_ADDR) so that it can be read later and survives the
// reset that comes when the cable is plugged in - which is exactly when the log is read. It can be left out of the build: BATTERY_LOG=0 (options.h).
//
// It exists because the diagnostics travel over the keyboard's own USB, and with the cable in the battery is charging and the reading saturates at
// 1023: the readings that matter can only be taken with the cable out, when nothing can read them.
//
// A record is four bytes: the minutes the keyboard has been awake since it was turned on, the battery reading (low byte), and a flags byte: bits 0-1
// the high two bits of the reading (bits 2-3 below), BLOG_FLAG_CABLE (the cable was in: the reading is saturated and says nothing), BLOG_FLAG_WAKE (the first record
// since the keyboard was turned on or woke from sleep: the time before it is not in the minutes, because a sleeping chip has no clock). A record is
// written when the reading has moved by BLOG_STEP counts, when the cable goes in or out, on the first reading after a wake, and anyway every
// BLOG_PERIOD_MIN minutes. The sector holds 128 and is erased only when it is full, so a day of samples costs a handful of flash writes.
//
// What the log can and cannot say: while the keyboard sleeps nothing is sampled. A night of sleep shows as a wake record whose reading is the
// battery after the night; comparing it with the last record before the sleep is how the discharge in between is read.

#    include <stdbool.h>
#    include <stdint.h>

#    define BLOG_RECORD_BYTES 4u      // minutes (little endian), reading low byte, flags
#    define BLOG_RECORDS      128u    // the whole sector, 512 bytes
#    define BLOG_STEP         8u      // counts the reading has to move to be a change worth a record
#    define BLOG_PERIOD_MIN   5u      // a record at least this often, even when nothing changed
#    define BLOG_FLAG_CABLE   0x04u
#    define BLOG_FLAG_WAKE    0x08u
#    define BLOG_MARK         0xA0u // the top nibble of the flags of every record: a sector with anything else in it (what was in the flash before) is not ours
#    define BLOG_EMPTY        0xFFFFu // the minutes of an erased record (45 days of uptime is not a case)

void blog_init(void);

// The keyboard has just woken from sleep (or started): the next record is marked as the first after a gap.
void blog_note_wake(void);

// From the main loop, with the millisecond clock, whether the keyboard is on its battery and the battery reading (0 if there is none yet, or it is
// not settled). Counts the minutes the keyboard is awake and writes the records.
void blog_task(uint16_t now_ms, bool on_battery, uint16_t reading);

uint8_t blog_count(void); // how many records there are (0 to BLOG_RECORDS)

// Record `index` (0 = the oldest): minutes (little endian), reading low byte, flags. False if there is no such record.
bool blog_get(uint8_t index, uint8_t out[BLOG_RECORD_BYTES]);

// Provided by the board: the flash the records live in.
void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len);
void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len);
void blog_hook_erase(uint16_t addr);

#endif
