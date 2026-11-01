#pragma once

// From the report state to the packets on EP1. report_hook_send() (keyboard.c's way of saying "this changed") lands here:
// the packet is built from the report state right away and queued, and the main loop hands the queue to the endpoint one
// packet at a time as the host takes them. A key pressed and released in the same pass therefore still reaches the host
// as two reports.
//
// Packets, by kind: keyboard = [ID 3] + modifiers and the NKRO bitmap, 16 bytes (in boot protocol the 8-byte six-key report, no ID),
// system = [ID 1, bits], consumer = [ID 2, usage lo, hi],
// mouse = [ID 7, buttons, x lo, x hi, y lo, y hi, wheel] (relative axes: the packet carries the movement accumulated until it was built).
// Consumer and system reports have no boot-protocol form: a BIOS gets the keyboard report only.

#include "report.h"
#include <stdbool.h>
#include <stdint.h>

#define USB_REPORT_QUEUE 8

// Provided by the chip driver (or a test).
bool usb_reports_hook_ep1_free(void);
void usb_reports_hook_ep1_send(const uint8_t *data, uint8_t length);

// Provided by the USB device: the host has configured us, and which protocol interface 0 is in (0 boot, 1 report).
bool    usb_reports_hook_configured(void);
uint8_t usb_reports_hook_protocol(void);

void usb_reports_init(void);

// Queues the report of `kind` as the report state is now. Dropped when the host has not configured the device.
void usb_reports_send(report_kind_t kind);

// Regularly from the main loop: sends the oldest queued packet if the endpoint is free.
void usb_reports_pump(void);

// Forgets what is queued (a bus reset or a new configuration).
void usb_reports_flush(void);

// Packets that replaced or pushed out another because the queue was full.
uint8_t usb_reports_overflows(void);
