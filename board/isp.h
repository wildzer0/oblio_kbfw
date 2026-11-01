#pragma once

// Jumps to the stock bootloader's ISP mode (at 0xFF00), the way `sinowisp` flashes the keyboard. Never returns. Interrupts are
// switched off first; call it from the main loop, not from an interrupt: the bootloader never executes RETI, so an interrupt
// left in service would stay that way.
void isp_jump(void);
