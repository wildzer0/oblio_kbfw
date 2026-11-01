#include "diag.h"
#include "diag_hw.h"
#include "led_hw.h"
#include "link.h"
#include "matrix_hw.h"
#include "matrix.h"
#include "radio.h"
#include "sh68f90.h"
#include "tick.h"
#include "usb_reports.h"

// The stack of the 8051 grows upward in the internal RAM, from 0x21 (see build/oblio.mem) to 0xFF. Painted with a known value at the
// start, the deepest point it reached shows afterwards as the highest byte that is no longer that value. A byte of the stack that
// happens to hold the value itself is not seen, so the figure can only be too low by a few bytes, never too high.
#define STACK_BASE  0x21u
#define STACK_TOP   0xFFu
#define STACK_PAINT 0xA5u

#define IRAM(a) (*(__idata volatile uint8_t *)(uint8_t)(a))

void diag_stack_paint(void)
{
    for (uint16_t a = (uint16_t)(SP + 8u); a <= STACK_TOP; a++) { // a little above the stack as it is now
        IRAM(a) = STACK_PAINT;
    }
}

void diag_hook_refresh(void)
{
    uint16_t a = STACK_TOP;

    while (a > STACK_BASE && IRAM(a) == STACK_PAINT) {
        a--;
    }

    diag_set(DIAG_STACK_USED, (uint16_t)(a - STACK_BASE + 1u));
    diag_set(DIAG_USB_OVERFLOW, usb_reports_overflows());
}

const uint8_t *diag_hook_radio_status(void)
{
    return radio_status();
}

const uint8_t *diag_hook_radio_info(void)
{
    return radio_last_info();
}

void diag_hook_radio_monitor(bool on)
{
    link_monitor(on);
}

// The raw bytes of the ports, for finding out what an input that is not in the matrix is doing. This is how the Win/Mac switch was
// found: P0.5 is high with the switch on Windows and low with it on Mac, and its level does not depend on the column strobes (the pin
// that was presumed, P7.0, never changed). They were read before the timer was stopped at first, which could catch a column still lit by
// an LED subframe: now the timer and the backlight are parked first, so P4, P5 and P6 show the idle state of the strobes.
void diag_hook_ports(XDATA uint8_t *out)
{
    tick_pause();
    led_hw_pause();
    matrix_hw_cols_off();

    out[0] = P0;
    out[1] = P1;
    out[2] = P2;
    out[3] = P3;
    out[4] = P4;
    out[5] = P5;
    out[6] = P6;
    out[7] = P7;

    led_hw_resume();
    tick_resume();
}
