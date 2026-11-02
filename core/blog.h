#pragma once

// The battery log: what the radio's status answer says about the battery, written down in **flash** so that it survives the reset that comes when
// the cable is plugged in - which is exactly when the log is read.
//
// It exists because of two facts: the diagnostics travel over the keyboard's own USB, and with the cable in the battery is charging (the reading saturates at 1023), so the readings that matter can only be taken with the cable out - and nothing can read them then.
//
// A record is one status answer: the minutes the keyboard has been awake since it was turned on, the battery reading (low byte; its two high bits are in bits 4-5 of the flags byte) and the flags byte. Every answer
// goes in - cable or not, marked with BLOG_FLAG_CABLE - one when it changes and anyway every BLOG_PERIOD_MIN minutes. The sector holds 128 of them
// and is erased only when it is full, so a night of samples costs a handful of flash writes.

#include <stdbool.h>
#include <stdint.h>

#define BLOG_ADDR         0xD800u // one sector of its own, just below the keymap store (the application stops at 0xD800)
#define BLOG_RECORD_BYTES 4u      // minutes (little endian), battery byte, flags byte
#define BLOG_RECORDS      128u    // the whole sector, 512 bytes
#define BLOG_STEP         8u      // counts of the reading it has to move to be a change worth a record
#define BLOG_PERIOD_MIN   5u      // a record at least this often, even when nothing changed
#define BLOG_FLAG_CABLE   0x40u   // in the flags byte: the cable was in, so the reading is saturated and says nothing
#define BLOG_EMPTY        0xFFFFu // the minutes of an erased record (45 days of uptime is not a case)

void blog_init(void);

// From the main loop, with the millisecond clock, whether the keyboard is on its battery and the radio's latest status answer (0 if there is
// none). Counts the minutes the keyboard is awake and writes the records.
void blog_task(uint16_t now_ms, bool on_battery, const uint8_t *status);

uint8_t blog_count(void); // how many records there are (0 to BLOG_RECORDS)

// Record `index` (0 = the oldest): minutes (little endian), battery byte, flags byte. False if there is no such record.
bool blog_get(uint8_t index, uint8_t out[4]);

// Provided by the board: the flash the records live in.
void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len);
void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len);
void blog_hook_erase(uint16_t addr);
