#pragma once

// Counters for the first runs on the hardware, read over the raw HID interface (VIA command 0xF1, only on an unlocked keyboard;
// tools/diag.py prints them). They are there to answer questions the host tests cannot: did the chip reset and why, how deep did
// the stack go, did the timer slots fit in their time, did the main loop stall.
//
// Each counter has one writer (an interrupt or the main loop), so none needs a lock; a 16-bit value is read twice until it agrees.

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DIAG_RESET_CAUSE,      // RSTSTAT as read at power-on, before the first watchdog kick
    DIAG_STACK_USED,       // bytes of the stack used at most (internal RAM from 0x21 up)
    DIAG_SLOT_LATE,        // LED slots whose work ran past their end (the timer had already overflowed again)
    DIAG_SCAN_LOST,        // scans overwritten before the main loop took them: harmless, the matrix is a state (as in the SMK port), only the debounce is slower
    DIAG_LOOP_MAX_MS,      // the slowest pass of the main loop
    DIAG_USB_OVERFLOW,     // USB report packets that replaced or pushed out another
    DIAG_LED_US_MAX,       // longest LED slot, from its start to the end of its work, in us (the slot is 400; more means it ran late)
    DIAG_SCAN_US_MAX,      // the same for the scan slot (500)
    DIAG_PASSES_LAST,      // passes of the main loop between the last two scans
    DIAG_BACKOFF,          // times the main loop was found starved and the backlight stood aside
    DIAG_SCAN_LATE,        // the same as DIAG_SLOT_LATE, for the scan slots
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
//   kind 1: the radio: [0xF1, 1, 6 status bytes, 22 bytes of the last 08 frame]
//   kind 2: [0xF1, 2, on]: listen to the radio with the cable in (on = 1), or stop; the reply echoes `on`
//   kind 3: [0xF1, 3, P0, P1, P2, P3, P4, P5, P6, P7]: the raw port bytes, for finding out what an input that is not in the matrix does
//   anything else: [0xFF, ...] (unhandled)
void diag_receive(XDATA uint8_t *data, uint8_t length);

void diag_hook_radio_monitor(bool on); // provided by the board

// Provided by the board: the raw bytes of P0 .. P7, read with the timer and the backlight stopped (so the strobes show idle).
#define DIAG_PORT_BYTES 8u
void diag_hook_ports(XDATA uint8_t *out);

// Provided by the board: the radio's last status bytes (6) and last 08 frame (22), for kind 1
const uint8_t *diag_hook_radio_status(void);
const uint8_t *diag_hook_radio_info(void);
