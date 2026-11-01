#pragma once

// The chip's program flash, read and written by the application itself (IAP, through the SSP registers).
// Only the area from FLASH_WRITE_MIN to FLASH_WRITE_MAX can be changed: the application below it and the bootloader above it are never
// touched, whatever address a caller passes.

#include <stdbool.h>
#include <stdint.h>

#define FLASH_SECTOR_SIZE 512u
#define FLASH_WRITE_MIN   0xAC00u // the application ends here: profiles, settings and marker follow
#define FLASH_WRITE_MAX   0xF000u // the stock bootloader starts here

void flash_read(uint16_t addr, uint8_t *dst, uint8_t len);

// Erases the sector that holds `addr`. False (and nothing happens) if it is outside the writable area.
bool flash_erase_sector(uint16_t addr);

// Programs bytes in erased cells. False (and nothing happens) if the range is outside the writable area.
bool flash_program(uint16_t addr, const uint8_t *src, uint8_t len);
