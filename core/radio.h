#pragma once

// The link to the BK3632 radio of the B68 (docs/keyboards/yunzii-b68-radio.md): a serial line at 260870 baud carrying frames
// [01][command][a][b][c][checksum] (the report is longer), a select line SEL that the MCU holds low while a frame goes out and
// releases with the last byte, and the radio's attention line ATT, high when the radio can take a frame and low while it is busy.
// This is how the stock firmware paces itself: a frame goes out only when ATT is high, and the answers (02 <command> ...) are not
// waited for. The radio's own frames are answered: 03 ... with F0, 08 ... with F1; without that it stays busy or refuses reports.
//
// This file is the protocol and the scheduler, pure: the serial port and the pins are behind the radio_hook_* functions
// (board/radio_hw.c, or a test with a simulated radio).

#include <stdbool.h>
#include <stdint.h>

#define RADIO_MODE_2G4 0 // the "slot" of the mode command: 0 = 2.4G, 1-3 = Bluetooth
#define RADIO_MODE_BT3 3

#define RADIO_REPORT_LEN     30 // keyboard report frame
#define RADIO_COMMAND_LEN    6  // mode command, status request, acknowledgements
#define RADIO_CONSUMER_LEN   13
#define RADIO_NAME_FRAME_LEN 32
#define RADIO_NAME_MAX       24
#define RADIO_FRAME_MAX      32

// The Bluetooth name the keyboard shows (sent once per power-up, for both the LE and the classic name)
#define RADIO_NAME "OBLIO"

// ---- frames (pure) ----
uint8_t radio_checksum(const uint8_t *data, uint8_t length); // 0x55 - sum

uint8_t radio_build_command(uint8_t *out, uint8_t command, uint8_t a, uint8_t b, uint8_t c); // 6 bytes, checksum included
uint8_t radio_build_report(uint8_t *out, uint8_t mods, const uint8_t keys[6]);                // 30 bytes
uint8_t radio_build_consumer(uint8_t *out, uint16_t usage);                                    // 13 bytes
uint8_t radio_build_name(uint8_t *out, uint8_t type, const char *name);                        // 32 bytes; type 0 = Bluetooth LE, 1 = classic

// ---- the link ----
void radio_init(void); // state; the port and the pins are set up by the board before

// Starts the radio's work in a mode: slot 0 = 2.4G, 1-3 = Bluetooth; `pairing` starts the pairing. What the old link held down is
// released on the new one.
void radio_set_link(uint8_t slot, bool pairing);

// The selector went to USB: nothing more is sent.
void radio_stop(void);

// Before the MCU sleeps: the frames the stock firmware sends first (01 0C 08 07 00, then 01 0B 00), blocking, with delays by loops
// (the timer is stopped). Leaves the radio idle.
void radio_sleep(void);

// The keyboard's state now: modifiers and six usages. Queued, in order (4 deep, then the newest replaces the last).
void radio_send_keys(uint8_t mods, const uint8_t keys[6]);

// A consumer usage (0 = released), queued like the keys.
void radio_send_consumer(uint16_t usage);

// Regularly from the main loop.
void radio_task(void);

bool radio_active(void);
bool radio_idle(void); // nothing in flight and nothing waiting to go out

// The last answer to the status request (bytes after the command byte), zeros until one has come.
const uint8_t *radio_status(void);

// What the status answer says. Six bytes after the command. Byte 1 carries the host's LEDs in bits 0-2 (num, caps, scroll); the stock firmware
// copies it to its LED state at 0x06B6. Bytes 4 and 5 are the **battery voltage as a 10-bit ADC reading**, little endian (byte 4 low, byte 5 the
// high two bits): the stock handler of the status frame (0x05EA-0x0690) builds XDATA[0x02E6..7] = byte5:byte4 and the gauge routine (0x7CF0)
// turns it into percent as (reading - 715) * 10 / 22, 0 below 715 and 100 above 935. It is *not* a 3-bit level (that was the NuPhy layout, and
// a wrong guess here). With the cable the reading saturates (0x03FF, 0x03FD ...), so it says nothing about the charge.
#define RADIO_STATUS_FLAGS_AT      1u // host LEDs in bits 0-2
#define RADIO_STATUS_BATTERY_AT    4u // battery reading, low byte
#define RADIO_STATUS_BATTERY_HI_AT 5u // battery reading, high byte (0..3)

// The battery reading as the radio sent it (0 to 1023), or 0 when no answer has come yet.
uint16_t radio_battery_raw(void);

// The whole status answer as last received, whatever the state of the cable (0 until one has come). The battery log writes this down as it is.
const uint8_t *radio_status(void);

// Provided by the caller: is the keyboard running on its battery (a wireless mode, no cable)?
bool radio_hook_on_battery(void);
uint8_t radio_host_leds(void);     // bit 0 num lock, bit 1 caps lock, bit 2 scroll lock

// The last 08 frame from the radio, whole (22 bytes, zeros until one has come): its content is not decoded yet.
const uint8_t *radio_last_info(void);

// ---- hooks: the serial port and the pins (board/radio_hw.c, or a test) ----
bool     radio_hook_ready(void);              // ATT: the radio is free to take a frame
void     radio_hook_select(bool low);         // SEL: low while a frame goes out, released (high) afterwards
void     radio_hook_tx_start(const uint8_t *frame, uint8_t length);
bool     radio_hook_tx_done(void);
bool     radio_hook_rx_pop(uint8_t *byte);    // one received byte, false when there is none
uint16_t radio_hook_now_ms(void);
void     radio_hook_delay_ms(uint8_t ms);     // a real delay, which works with the timer stopped (unlike radio_hook_now_ms)
