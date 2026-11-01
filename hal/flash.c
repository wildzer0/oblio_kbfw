#include "flash.h"
#include "sh68f90.h"

// Self-programming: the address goes to XPAGE / IB_OFFSET, the data to IB_DATA, the operation to IB_CON1 and the four key
// bytes to IB_CON2..5 start it. The CPU stalls while the flash is busy (a sector erase takes milliseconds).
#define SSP_KEY_2 0x05u
#define SSP_KEY_3 0x0Au
#define SSP_KEY_4 0x09u
#define SSP_KEY_5 0x06u

#define SSP_PROGRAM 0x6Eu
#define SSP_ERASE   0xE6u

static bool writable(uint16_t addr, uint16_t len)
{
    return addr >= FLASH_WRITE_MIN && (uint32_t)addr + len <= FLASH_WRITE_MAX;
}

// Reads through the code space (MOVC). Interrupts stay on: the window select (FLASHCON) is left at 0.
void flash_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        __code const uint8_t *p = (__code const uint8_t *)(addr + i);

        dst[i] = *p;
    }
}

static void ssp_run(uint16_t addr, uint8_t op, uint8_t data)
{
    const bool interrupts = EA; // the caller may be running with interrupts off (start-up)

    EA = 0;

    FLASHCON  = 0;
    XPAGE     = (uint8_t)(addr >> 8);
    IB_OFFSET = (uint8_t)(addr & 0xFFu);
    IB_DATA   = data;
    IB_CON1   = op;
    IB_CON2   = SSP_KEY_2;
    IB_CON3   = SSP_KEY_3;
    IB_CON4   = SSP_KEY_4;
    IB_CON5   = SSP_KEY_5;

    // the SH68F89's own ISP code waits twelve cycles here; the other parts settle in four
    // clang-format off
    __asm
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
    __endasm;
    // clang-format on

    // disarm the keys, as the stock ISP code does: left set, a later IB_* write could start another operation
    IB_CON1  = 0;
    IB_CON2  = 0;
    IB_CON3  = 0;
    IB_CON4  = 0;
    IB_CON5  = 0;
    XPAGE    = 0;
    FLASHCON = 0;

    EA = interrupts;
}

bool flash_erase_sector(uint16_t addr)
{
    const uint16_t base = (uint16_t)(addr & ~(FLASH_SECTOR_SIZE - 1u));

    if (!writable(base, FLASH_SECTOR_SIZE)) {
        return false;
    }

    ssp_run(base, SSP_ERASE, 0);
    return true;
}

bool flash_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    if (!writable(addr, len)) {
        return false;
    }

    for (uint8_t i = 0; i < len; i++) {
        ssp_run((uint16_t)(addr + i), SSP_PROGRAM, src[i]);
    }

    return true;
}
