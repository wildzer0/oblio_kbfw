#pragma once

// VIA and Vial commands, as the Vial desktop app speaks them over the 32-byte raw HID report.
//
// via_receive() takes one request and rewrites the same buffer into the reply, which the transport sends back as it is, like
// QMK's raw_hid_receive(). A command this firmware does not implement answers with 0xFF in the first byte ("unhandled").

#include "platform.h"
#include "usb_desc.h" // RAW_HID_SIZE
#include <stdbool.h>
#include <stdint.h>

#define VIA_RAW_SIZE RAW_HID_SIZE

// Provided by the board (or a test): the time in ms since power-on (the uptime the GUI shows, and the steps of the unlock).
uint32_t via_hook_now(void);

// Locked, nothing requested, layout options cleared.
void via_init(void);

void via_receive(XDATA uint8_t *data, uint8_t length);

// Set by the "jump to the bootloader" command (it needs the keyboard unlocked): the transport sends the reply first and then
// jumps.
bool via_bootloader_requested(void);
