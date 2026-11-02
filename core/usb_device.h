#pragma once

// The USB device on endpoint 0: enumeration and the HID class requests. Pure logic: the chip driver feeds it what arrives
// (a SETUP packet, an OUT packet, "the IN packet was taken") and it answers through the hooks below. It knows nothing of
// registers, so it runs under the host tests.
//
// A control transfer, as the chip sees it:
//   no data:  SETUP -> we arm an empty IN packet (status) -> its completion (usb_device_ep0_in_done)
//   IN data:  SETUP -> IN packets of up to 8 bytes, each completion sends the next -> the host's empty OUT (status)
//   OUT data: SETUP -> the host's OUT packet (usb_device_ep0_out) -> we arm an empty IN packet (status) -> its completion

#include <stdbool.h>
#include <stdint.h>

#define USB_SETUP_SIZE 8

// Provided by the chip driver (or a test).
void usb_hook_ep0_in(const uint8_t *data, uint8_t len); // arm EP0 IN with this packet (len 0 = an empty one)
void usb_hook_ep0_out_arm(void);                         // accept the next OUT packet on EP0
void usb_hook_ep0_stall(void);                           // answer the transfer with a STALL
void usb_hook_set_address(uint8_t address);              // the device address, applied after the status stage
void usb_hook_configured(bool configured);

// Provided by the hardware: start-of-frame interrupts seen since power-on, wrapping. It stops counting when nothing drives the bus, which is
// what the cable being pulled out looks like. 16 bits: one start-of-frame a millisecond, an 8-bit counter came round exactly once in 256 ms, so a
// look at the bus that came 256 ms after the last one saw "no change" and declared the host gone (the backlight went off and the keys stopped).
uint16_t usb_hook_sof_ticks(void);               // SET_CONFIGURATION: endpoints in the initial state (toggles, stalls)
void usb_hook_ep_halt(uint8_t endpoint, bool halted);    // SET/CLEAR_FEATURE(ENDPOINT_HALT) on 0x81, 0x82 or 0x02

// Provided by the application.
void usb_hook_led(uint8_t leds);  // the LED output report from the host (num, caps, scroll lock... in bits 0-4)
void usb_hook_isp_request(void);  // the host asked for the ISP bootloader; to be acted on from the main loop

void usb_device_reset(void); // bus reset

// A SETUP packet arrived. `previous_status_done`: the IN completion of the status stage of the previous transfer is still
// pending in the hardware (a new SETUP can overtake it), and counts as having happened.
void usb_device_setup(const uint8_t packet[USB_SETUP_SIZE], bool previous_status_done);

void usb_device_ep0_in_done(void);
void usb_device_ep0_out(const uint8_t *data, uint8_t len);

// From the main loop, every so often: notices that the host has gone away (the cable is out), which nothing else reports.
void usb_device_bus_check(uint16_t now);

// True while a host is there and has configured the keyboard: the cable is in *and* somebody is driving the bus.
bool    usb_device_configured(void);
bool    usb_device_remote_wakeup(void);
uint8_t usb_device_protocol(uint8_t interface); // 0 boot, 1 report
bool    usb_device_ep_halted(uint8_t endpoint);
