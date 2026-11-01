#pragma once

// The keymap in flash: one keyboard (keys, encoders, macros, tap dances, settings) in a pair of slots A/B of STORE_SLOT_SECTORS
// sectors; the RAM copy is in keymap.c. There used to be four profiles, which cost 16 KB of the chip's flash for a number of layouts
// nobody used: with one keyboard the store sits at the end of the flash and the code gets those bytes.
//
// A save erases the slot that is NOT in use, writes the payload, and writes the header last. The header carries a sequence
// number and a CRC of the payload, so a slot only counts once it is complete; at load the valid slot with the newest sequence
// wins. A power cut during a save leaves the previous copy intact.
//
// Flash map: code up to 0xDC00, the store (2 slots x 4 sectors of 512 bytes) at 0xDC00-0xEBFF, the settings at 0xEC00, the
// bootloader's marker at 0xEE00 (never touched), the stock bootloader at 0xF000.

#include <stdbool.h>
#include <stdint.h>

#define STORE_BASE         0xDC00u // the code ends here
#define STORE_SECTOR_BYTES 512u
#define STORE_SLOT_SECTORS 4u
#define STORE_SLOTS        2u      // A and B, written alternately
#define STORE_END          (STORE_BASE + STORE_SLOTS * STORE_SLOT_SECTORS * STORE_SECTOR_BYTES) // 0xEC00: the settings follow

// Edits are written once nothing has changed for this long and no key is down, so a flash write never lands in the middle of typing.
#define STORE_QUIET_MS 2000u

// Provided by the board (the flash driver) or by a test.
void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len);
void store_hook_erase(uint16_t sector_addr);
void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len);

// Around a whole save (the keymap or the settings): the board stops the timer and parks the backlight once, and starts them again at
// the end. Do not leave the work of the interrupt running in between: a sector erase blocks the CPU for milliseconds.
void store_hook_flush_begin(void);
void store_hook_flush_end(void);

// The keymap has just been loaded (or the defaults taken): the rest of the firmware drops what it kept of the old one.
void store_hook_loaded(void);

// Loads the stored keymap into the RAM copy, or the defaults if nothing valid is there.
void store_init(void);

// Regularly: the save after the quiet time. `now` in ms; `any_key_down` from the matrix.
void store_task(uint16_t now, bool any_key_down);

// Writes the RAM copy to flash now, if it changed. Takes tens of milliseconds.
void store_flush(void);

bool store_dirty(void);
