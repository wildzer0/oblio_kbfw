#pragma once

#include <stdbool.h>
#include <stdint.h>

// Runs the USB side from the main loop: the report queue and the ISP request.
void usb_task(uint16_t now);

// Remote wakeup: a key pressed while the host has suspended the bus wakes it (if the host allowed it). From the main loop.
void usb_wake_task(bool any_key_down, uint16_t now);

// The host's keyboard LEDs (bit 1 = caps lock).
uint8_t usb_host_leds(void);
