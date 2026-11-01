#include "delay.h"
#include "sh68f90.h"

// One loop turn is meant to take 24 cycles, i.e. 1 us at 24 MHz: 19 NOPs plus DJNZ taken (5 cycles on this core,
// the figure the SMK port measured). Hypothesis until measured on the board with a scope.
_Static_assert(FREQ_SYS == 24000000u, "delay_us is calibrated for 24 MHz");

// With WATCHDOG_ENABLE the loop also kicks the watchdog: the body is the 3-cycle write to RSTSTAT (0xB1) and 16 NOPs, the same 19
// cycles, so that a long wait (the recovery check, the radio's sleep frames) never starves it.
void delay_us(uint8_t us) __naked
{
    (void)us;
    // clang-format off
    __asm
        mov     a, dpl              ; the argument arrives in DPL
        jz      00099$
        mov     r7, a
00001$:
#ifdef WATCHDOG_ENABLE
        mov     0xb1, #0x00         ; RSTSTAT: kick the watchdog (3 cycles)
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
#else
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
#endif
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

// The same wait without the kick. The matrix scan uses it because it runs in the timer interrupt: a watchdog kicked from there would
// be kept alive by the interrupt even when the main loop is stuck, and then it could never reset a keyboard that has hung.
void delay_us_nokick(uint8_t us) __naked
{
    (void)us;
    // clang-format off
    __asm
        mov     a, dpl
        jz      00899$
        mov     r7, a
00801$:
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
        djnz    r7, 00801$
00899$:
        ret
    __endasm;
    // clang-format on
}
