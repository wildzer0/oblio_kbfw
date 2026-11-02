#pragma once

// The raw HID pipe VIA and Vial talk through: one 32-byte request on EP2 OUT, one 32-byte reply on EP2 IN.
//
// One request is handled at a time. While one is being answered EP2 OUT is not armed, so the host's next packet is NAKed and
// retried: nothing is queued and nothing is lost. The interrupt only notes that a packet is there; the main loop reads it from the endpoint and answers it.

#include "platform.h"
#include "usb_desc.h" // RAW_HID_SIZE
#include <stdbool.h>
#include <stdint.h>

// Provided by the chip driver (or a test).
bool raw_hid_hook_ep2_free(void);
void raw_hid_hook_ep2_send(const uint8_t *data, uint8_t length);
void raw_hid_hook_out_arm(void); // accept the next request
void raw_hid_hook_out_read(XDATA uint8_t *dst); // copies the request from the endpoint's buffer (RAW_HID_SIZE bytes); from the main loop

// Provided by the application: handles a request and leaves the reply in the same buffer (the VIA / Vial protocol).
void raw_hid_hook_process(XDATA uint8_t *packet);

// The host has configured the device (or it was reset): drop any request in flight and accept a new one.
void raw_hid_reset(void);

// From the USB interrupt: a packet of `length` bytes arrived on EP2 OUT. One of another size is not a request of ours and is dropped. The
// interrupt copies nothing (32 bytes through a slow pointer held the timer interrupt up, and the backlight flickered while the HUD polled): the
// endpoint is not armed again until the request is answered, so its buffer stays as it is, and the main loop reads it from there.
void raw_hid_out(uint8_t length);

// Regularly from the main loop: answers the request, then accepts the next.
void raw_hid_task(void);
