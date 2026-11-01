#pragma once

// The USB descriptors of the keyboard, as data.
//
// Interface 0: HID boot keyboard on EP1 IN. Every report carries an ID (keyboard 3, system 1, consumer 2); in boot protocol
//              (a BIOS) the keyboard report goes out without it. A feature report (ID 5) takes the ISP command.
// Interface 1: raw HID (usage page 0xFF60, as VIA and Vial expect) on EP2 IN + EP2 OUT, 32 bytes, no report IDs.
//
// Windows keeps the descriptors it read for a VID:PID and bcdDevice: change USB_BCD_DEVICE whenever the interfaces,
// endpoints or report descriptors change.

#include <stdbool.h>
#include <stdint.h>

#define USB_VID        0x258Au // the keyboard's own, so that sinowisp and Vial find it by the same ids as the SMK port
#define USB_PID        0xDEADu
#define USB_BCD_DEVICE 0x0200u

#define USB_EP0_SIZE 8
#define USB_EP1_SIZE 16
#define RAW_HID_SIZE 32

// HID report IDs on interface 0
#define REPORT_ID_SYSTEM   1
#define REPORT_ID_CONSUMER 2
#define REPORT_ID_KEYBOARD 3
#define REPORT_ID_ISP      5

// Descriptor types
#define USB_DESC_DEVICE        1
#define USB_DESC_CONFIGURATION 2
#define USB_DESC_STRING        3
#define USB_DESC_HID           0x21
#define USB_DESC_HID_REPORT    0x22

#define USB_INTERFACES 2

// The descriptor for a GET_DESCRIPTOR request: `type` and `index` are the high and low byte of wValue, `wIndex` is the
// interface for the HID descriptors and the language for strings. False when there is no such descriptor.
bool usb_desc_get(uint8_t type, uint8_t index, uint16_t wIndex, const uint8_t **data, uint16_t *length);
