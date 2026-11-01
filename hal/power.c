#include "power.h"
#include "clock.h"
#include "delay.h"
#include "sh68f90.h"
#include "usb.h"
#include "watchdog.h"

static volatile bool woke_by_int4;

static void int4_clear(void)
{
    EXF1 = 0;
}

void power_wake_arm(void)
{
    int4_clear();
    IENC = IENC_INT4_CONFIG;
    EXF0 = EXF0_INT4_TRIGGER;
    EX4  = 1;
}

void power_wake_disable(void)
{
    EX4 = 0;
    int4_clear();
}

void power_int4_isr(void) __interrupt(INT_INT4)
{
    int4_clear();
    woke_by_int4 = true;
}

// The key write and PCON.PD must be consecutive instructions; nothing may come between them.
static void halt_until_wake(void)
{
    // clang-format off
    __asm
        nop
    __endasm;
    // clang-format on
    SUSLO = SUSLO_POWERDOWN_KEY;
    PCON |= PCON_PD;
    // clang-format off
    __asm
        nop
        nop
        nop
        nop
        nop
        nop
    __endasm;
    // clang-format on
}

void power_enter_powerdown(void)
{
    usb_hw_deinit();
    clock_stop();

    // the wake-up sources: a USB event, and INT4 (armed by the caller). All the other interrupts are off while asleep.
    USBIE1 |= (USBIE1_BOOTS | USBIE1_RESMIE | USBIE1_PBRSTIE);
    USBIF1 &= (uint8_t) ~(USBIF1_SUSPIF | USBIF1_SOFIF | USBIF1_SETUPIF | USBIF1_OW | USBIF1_OVERIF);
    IEN1 = 0;
    REGCON &= (uint8_t)~REGCON_REGEN;

    watchdog_kick();
    woke_by_int4 = false;

    halt_until_wake();

    clock_wake_restart();
    USBIF1 &= (uint8_t)~USBIF1_SUSPIF;
    USBCON &= (uint8_t)~USBCON_GOSUSP;
    REGCON |= REGCON_REGEN;
    delay_us(250); // the regulator settles (~500 us)
    delay_us(250);
}
