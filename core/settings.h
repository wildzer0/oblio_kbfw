#pragma once

// Settings that belong to the keyboard, not to a profile: the profile in use, the sleep time and the Bluetooth slot. One record in
// the sector at 0xEC00, with a magic, a version and a CRC; it is rewritten (sector erased, record programmed) a moment after a
// change. The profiles themselves are in store.h.
//
// The flash is reached through the hooks of store.h (read, erase, program).

#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_ADDR       0xEC00u // the sector after the profiles
#define SETTINGS_SECTOR     512u
#define SETTINGS_QUIET_MS   500u    // changes are written once nothing has changed for this long and no key is down
#define SETTINGS_SLEEP_DEFAULT 2     // 10 minutes (the levels are listed in sleep.h)

typedef struct {
    uint8_t profile;     // 0 .. STORE_PROFILES - 1
    uint8_t sleep_level; // index of the inactivity time
    uint8_t bt_slot;     // Bluetooth slot 1-3
} settings_t;

extern settings_t settings; // read freely; change through the setters

// Reads the record, or takes the defaults if there is none (or it is not valid). Values out of range are put back in range.
void settings_load(void);

void settings_set_profile(uint8_t profile);
void settings_set_sleep_level(uint8_t level);
void settings_set_bt_slot(uint8_t slot);

// Regularly: writes the record after a change, once the quiet time has passed and no key is down. `now` in ms.
void settings_task(uint16_t now, bool any_key_down);

// Writes now if something changed.
void settings_flush(void);

bool settings_dirty(void);
