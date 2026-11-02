#pragma once

// The USB device controller of the SH68F90A: the chip side of core/usb_device (EP0), core/usb_reports (EP1 IN) and, later,
// the raw HID pipe (EP2).
//
// The handler is declared here because SDCC builds the vector table from the module that defines main(): main.c must include
// this header.

#include "sh68f90.h"
#include <stdbool.h>
#include <stdint.h>

void usb_isr(void) __interrupt(7); // INT_USB

// Powers the transceiver and connects to the bus. After the clock is on the PLL, with the interrupts still off.
void usb_hw_init(void);

// True once after a bus reset or a new configuration: whatever the main loop had queued for the host is stale.
bool usb_hw_take_event(void);

// The bus is suspended (no start-of-frame for a while).
bool usb_hw_suspended(void);

// Drives the resume signalling on the bus (USBCON.WKUP, the remote wakeup): on = start, off = end. The host must have suspended the bus and
// enabled the device's remote wakeup. The stock firmware sets this bit when a key wakes the chip while the host is suspended (0x9D73, 0x7F11).
void usb_hw_wake_host(bool on);

// The host has sent at least one SETUP packet since power-on: something is driving the bus.
bool usb_hw_setup_seen(void);

// Disconnects from the bus and switches the controller off, for a power-down.
void usb_hw_deinit(void);
