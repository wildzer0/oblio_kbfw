#include "delay.h"
#include "sh68f90.h"

// One loop turn is meant to take 24 cycles, i.e. 1 us at 24 MHz: 19 NOPs plus DJNZ taken (5 cycles on this core,
// the figure the SMK port measured). Hypothesis until measured on the board with a scope.
_Static_assert(FREQ_SYS == 24000000u, "delay_us is calibrated for 24 MHz");

void delay_us(uint8_t us) __naked
{
    (void)us;
    // clang-format off
    __asm
        mov     a, dpl              ; the argument arrives in DPL
        jz      00099$
        mov     r7, a
00001$:
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
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        djnz    r7, 00001$
00099$:
        ret
    __endasm;
    // clang-format on
}

void delay_ms(uint16_t ms)
{
    while (ms--) {
        delay_us(250);
        delay_us(250);
        delay_us(250);
        delay_us(250);
    }
}
