#pragma once

// Counters for the first runs on the hardware, read over the raw HID interface (VIA command 0xF1, no unlock needed: they are read-only;
// tools/diag.py prints them). They are there to answer questions the host tests cannot: did the chip reset and why, how deep did
// the stack go, did the timer slots fit in their time, did the main loop stall.
//
// Each counter has one writer (an interrupt or the main loop), so none needs a lock; a 16-bit value is read twice until it agrees.

#include "options.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DIAG_RESET_CAUSE,      // RSTSTAT as read at power-on, before the first watchdog kick
    DIAG_STACK_USED,       // bytes of the stack used at most (internal RAM from 0x21 up)
    DIAG_SLOT_LATE,        // LED slots whose work ran past their end (the timer had already overflowed again)
    DIAG_LOOP_MAX_MS,      // the slowest pass of the main loop
    DIAG_USB_OVERFLOW,     // USB report packets that replaced or pushed out another
    DIAG_BACKOFF,          // times the main loop was found starved and the backlight stood aside
    DIAG_SCAN_LATE,        // the same as DIAG_SLOT_LATE, for the scan slots
    DIAG_ERASE_US_MAX,     // the longest sector erase of a flash write, in us (the chip stalls while it runs)
    DIAG_RADIO_RX,         // frames from the radio that passed the checksum
    DIAG_TIMER_LATE_US,    // the longest the timer interrupt waited to start after its slot ended, in us (another interrupt or the flash holding it up; ignores the
                           // flash pauses, over 1 ms): above ~50 the backlight columns show it as a flicker
    DIAG_USB_GONE,         // times the keyboard declared the host gone (no start-of-frame in a look at the bus): with the cable in and the PC on this stays 0
    DIAG_STORE_SAVES,      // times the whole keymap has been written to flash, ever (the sequence number in the header of the slot in use: it survives power-off)
    DIAG_STORE_LOG,        // records in the log of the slot in use (the keymap edits written since the last whole save)
    DIAG_LOOP_WORST,       // LOOP_PROFILE build only: the slowest step of the main loop so far, as (step number << 8) | milliseconds
    DIAG_COUNT
} diag_id_t;

// Provided by the board: bring the counters that are read from somewhere else (stack, USB queue, matrix) up to date.
void diag_hook_refresh(void);

void     diag_init(void);
void     diag_set(diag_id_t id, uint16_t value);
void     diag_inc(diag_id_t id); // saturates
void     diag_max(diag_id_t id, uint16_t value);
uint16_t diag_get(diag_id_t id);

// VIA command 0xF1: the request is in `data` (32 bytes) and becomes the reply. [0xF1, kind, ...]
//   kind 0: [0xF1, 0, DIAG_COUNT, counter 0 (lo, hi), counter 1, ...]
//   kind 1: the radio: [0xF1, 1, its 6 status bytes] (the battery reading is in bytes 4-5)
//   kind 4 (with BATTERY_LOG): [0xF1, 4, first record] -> [0xF1, 4, count, records of four bytes, as many as fit]: the battery log (blog.h)
//   all are read-only, so they need no unlock
//   anything else: [0xFF, ...] (unhandled)
void diag_receive(XDATA uint8_t *data, uint8_t length);

#if BATTERY_LOG
// Provided by the board: the battery log (blog.h), for kind 4
uint8_t diag_hook_blog_count(void);
bool    diag_hook_blog_get(uint8_t index, uint8_t out[4]);
#endif

// Provided by the board: the radio's last status bytes (6), for kind 1 (null until the first answer)
const uint8_t *diag_hook_radio_status(void);
