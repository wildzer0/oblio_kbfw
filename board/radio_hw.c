#include "radio_hw.h"
#include "gpio.h"
#include "delay.h"
#include "radio.h"
#include "sh68f90.h"
#include "timebase.h"
#include <string.h>

// The EUART in mode 1 (asynchronous 8N1) as the stock firmware sets it up: SCON = 0x50, PCON.SSTAT (the top bits of SCON are then
// error flags, not the mode), SBRT = 0x7FFB and SFINE = 12, i.e. 24 MHz / (16 * 5 + 12) = 260870 baud.

#define SEL_MASK 0x04u
#define RING     32u // a power of two

static uint8_t          ring[RING];
static volatile uint8_t ring_head; // written by the interrupt
static uint8_t          ring_tail;

static uint8_t          tx_buf[RADIO_FRAME_MAX];
static volatile uint8_t tx_pos;
static volatile uint8_t tx_len;
static volatile bool    tx_busy;

// Short: it runs for every byte, in both directions
void radio_uart_isr(void) __interrupt(INT_EUART0)
{
    if (RI) {
        RI = 0;

        const uint8_t b    = SBUF;
        const uint8_t next = (uint8_t)((ring_head + 1u) & (RING - 1u));

        if (next != ring_tail) { // a full ring loses the byte
            ring[ring_head] = b;
            ring_head       = next;
        }
    }

    if (TI) {
        TI = 0;

        if (tx_pos < tx_len) {
            SBUF = tx_buf[tx_pos++];
        } else {
            tx_busy = false;
        }
    }

    SM0_FE    = 0; // the error flags (SSTAT): the stock clears them in its interrupt too
    SM1_RXOV  = 0;
    SM2_TXCOL = 0;
}

void radio_hw_init(void)
{
    IEN1 &= (uint8_t)~IEN1_ES0;
    SCON  = 0x00;
    SBRTH = 0xFF;
    SBRTL = 0xFB;
    SFINE = 0x0C;
    SCON  = 0x50;
    PCON |= PCON_SSTAT;

    tx_busy   = false;
    ring_head = 0;
    ring_tail = 0;

    // The highest priority (level 3), as the stock firmware sets it (IPH1 = 0x42, IPL1 = 0x41): the timer interrupt runs for up to
    // ~0.4 ms, and a byte lasts 38 us on this line. At the same level as the timer, the serial interrupt waited for it, the frames went
    // out with gaps and the radio's answers were lost: the radio refused or half took the reports (keys that did not release).
    IPH1 |= IEN1_ES0;
    IPL1 |= IEN1_ES0;
    IEN1 |= IEN1_ES0;
}

bool radio_hook_ready(void)
{
    return (P4 & 0x80u) != 0; // ATT, P4.7
}

void radio_hook_select(bool low)
{
    if (low) {
        GPIO_LOW(0, SEL_MASK);
        GPIO_OUTPUT(0, SEL_MASK);
    } else {
        GPIO_INPUT(0, SEL_MASK); // released, as the stock firmware does at the end of a frame
        GPIO_HIGH(0, SEL_MASK);
    }
}

void radio_hook_tx_start(const uint8_t *frame, uint8_t length)
{
    memcpy(tx_buf, frame, length);
    tx_pos  = 1;
    tx_len  = length;
    tx_busy = true;
    SBUF    = tx_buf[0];
}

bool radio_hook_tx_done(void)
{
    return !tx_busy;
}

bool radio_hook_rx_pop(uint8_t *byte)
{
    if (ring_tail == ring_head) {
        return false;
    }

    *byte     = ring[ring_tail];
    ring_tail = (uint8_t)((ring_tail + 1u) & (RING - 1u));
    return true;
}

uint16_t radio_hook_now_ms(void)
{
    return board_now_ms();
}

void radio_hook_delay_ms(uint8_t ms)
{
    delay_ms(ms);
}
