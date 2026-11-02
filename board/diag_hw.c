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
