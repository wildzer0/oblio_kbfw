#pragma once

// Which way the keyboard talks to the host: the cable (USB), the 2.4 GHz dongle or Bluetooth. The mode selector on the board decides;
// in the two wireless positions the reports go to the radio (radio.h), in USB to the cable.
//
// Bluetooth has three slots (three paired hosts). The keys B68_LK_1..3 choose one with a tap and start pairing with a 3-second hold.
// The slot in use is a setting that survives power-off (settings.h).

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LINK_USB  = 0,
    LINK_2G4  = 1,
    LINK_BT   = 2,
    LINK_NONE = 0xFF, // not decided yet (start-up, after a sleep)
} link_mode_t;

#define LINK_SELECTOR_STABLE_MS 50u   // the selector must read the same this long before it counts
#define LINK_PAIRING_HOLD_MS    3000u

// Provided by the board (or a test).
link_mode_t link_hook_selector(void);                // the position of the selector right now
void        link_hook_flash(uint8_t kind, uint8_t index); // feedback on the number-row keys: led_flash_key()

void link_init(void);

// Regularly from the main loop, with the time in ms: the selector, the pairing hold, and the radio itself.
void link_task(uint16_t now);

// A Bluetooth key (slot 1-3) went down or up.
void link_key(uint8_t slot, bool pressed, uint16_t now);

link_mode_t link_mode(void);
bool        link_wireless(void); // the radio carries the reports

// After a sleep: the radio starts again and the selector is applied anew.
void link_wake(void);
