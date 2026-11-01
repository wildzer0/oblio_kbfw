#pragma once

// Profiles in flash. STORE_PROFILES complete keymaps (keys, encoders, macros, tap dances, settings), each in its own pair of
// slots A/B of STORE_SLOT_SECTORS sectors; the RAM copy (keymap.c) holds one profile at a time.
//
// A save erases the slot that is NOT in use, writes the payload, and writes the header last. The header carries a sequence
// number and a CRC of the payload, so a slot only counts once it is complete; at load the valid slot with the newest sequence
// wins. A power cut during a save leaves the previous copy intact.
//
// Flash map (docs: CLAUDE.md): profiles 0-3 at 0xAC00-0xEBFF, 2 slots x 4 sectors of 512 bytes each.

#include <stdbool.h>
#include <stdint.h>

#define STORE_PROFILES     4
#define STORE_BASE         0xAC00u
#define STORE_SECTOR_BYTES 512u
#define STORE_SLOT_SECTORS 4u
#define STORE_END          (STORE_BASE + STORE_PROFILES * 2u * STORE_SLOT_SECTORS * STORE_SECTOR_BYTES) // 0xEC00: the settings follow

// Edits are written once nothing has changed for this long and no key is down, so a flash write never lands in the middle of typing.
#define STORE_QUIET_MS 2000u

// Provided by the board (the flash driver) or by a test.
void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len);
void store_hook_erase(uint16_t sector_addr);
void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len);

// A profile has just been loaded (or the defaults taken): the rest of the firmware drops what it kept of the old one.
void store_hook_loaded(void);

// Loads profile 0 (or the defaults, if nothing valid is stored) into the keymap's RAM copy.
void store_init(void);

uint8_t store_profile(void);

// Switches to another profile once no key is down; the one in use is written first if it changed. False for one that
// does not exist.
bool store_request(uint8_t profile);

// Regularly: the pending switch, and the save after the quiet time. `now` in ms; `any_key_down` from the matrix.
void store_task(uint16_t now, bool any_key_down);

// Writes the RAM copy to flash now, if it changed. Takes tens of milliseconds.
void store_flush(void);

bool store_dirty(void);
