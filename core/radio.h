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

// Listens to the radio without giving it a mode: only the status requests and the acknowledgements of its own frames go out (nothing the
// radio would take for a link). For finding out what the radio says about the battery with the cable in; `false` stops it.
void radio_monitor(bool on);

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
