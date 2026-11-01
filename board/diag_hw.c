#include "delay.h"
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

// P7.0 is the input the stock firmware reads together with the matrix rows without it being a row: it is where a switch that is not
// in the matrix most likely sits. A switch closes it either to ground (low in every column) or onto one column line (low in that
// column only): both show up in the mask, while the raw bytes above show the level when nothing is driven. The other free inputs
// with a pull-up (P4.6, P0.5) are in those raw bytes too, so a grounded switch on either of them shows up without a mask.
void diag_hook_ports(XDATA uint8_t *out)
{
    out[0] = P0;
    out[1] = P1;
    out[2] = P2;
    out[3] = P3;
    out[4] = P4;
    out[5] = P5;
    out[6] = P6;
    out[7] = P7;

    uint8_t low  = 0;
    uint8_t high = 0;

    tick_pause(); // the timer drives the same strobes
    led_hw_pause();

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        matrix_hw_col_on(col);
        delay_us(10); // as long as the matrix scan gives the line to settle

        if (!(P7 & 0x01u)) {
            if (col < 8) {
                low |= (uint8_t)(1u << col);
            } else {
                high |= (uint8_t)(1u << (col - 8));
            }
        }

        matrix_hw_cols_off();
    }

    led_hw_resume();
    tick_resume();

    out[8] = low;  // the columns 0-7 in which P7.0 read low
    out[9] = high; // the columns 8-15
}
