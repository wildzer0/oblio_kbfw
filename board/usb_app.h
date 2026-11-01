#pragma once

#include <stdint.h>

// Runs the USB side from the main loop: the report queue and the ISP request.
void usb_task(void);

// The host's keyboard LEDs (bit 1 = caps lock).
uint8_t usb_host_leds(void);
