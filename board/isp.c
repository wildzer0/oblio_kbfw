#include "isp.h"

// The bootloader expects the keys 0x5A in A and 0xA5 in B.
void isp_jump(void) __naked
{
    // clang-format off
    __asm
        clr     IE.7
        mov     B, #0xa5
        mov     A, #0x5a
        ljmp    0xff00
    __endasm;
    // clang-format on
}
