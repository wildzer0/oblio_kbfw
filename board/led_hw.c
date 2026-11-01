#include "led_hw.h"
#include "led.h"
#include "matrix_hw.h"
#include "platform.h"
#include "pwm.h"
#include "sh68f90.h"
#include "stage.h"

// RGB backlight of the Yunzii B68, as found with the LED probe (docs/keyboards/yunzii-b68.md).
//
// The colour channels, in the order of the framebuffer (row 0 R G B, row 1 R G B, ...), and the PWM channel each one is on:
//   row 0: P1.3 P1.4 P1.5 = PWM23 PWM24 PWM25    row 1: P2.0 P2.1 P2.2 = PWM10 PWM11 PWM12    row 2: P2.3 P2.4 P2.5 = PWM13 PWM14 PWM15
//   row 3: P3.3 P3.4 P3.5 = PWM03 PWM04 PWM05    row 4: P3.0 P3.1 P3.2 = PWM00 PWM01 PWM02    (the shift row and the bottom row are swapped)
// as PWM channel numbers k = bank * 6 + number (P1.n = PWM2n, P2.n = PWM1n, P3.n = PWM0n); see LED_PWM below.

#define PWM_PERIOD_H 0x04 // 1200 counts, as the stock firmware
#define PWM_PERIOD_L 0xB0

static volatile bool allowed;

// The interrupt does its work with constant addresses: a loop over a table, or any arithmetic, costs several times the 400 us of a slot on this
// core (the first version computed the widths here and looped over the channels, and took the whole CPU). X(n, k): colour channel n is
// PWM channel k.
#define LED_PWM(X)                                                                                                        \
    X(0, 15) X(1, 16) X(2, 17) X(3, 6) X(4, 7) X(5, 8) X(6, 9) X(7, 10) X(8, 11) X(9, 3) X(10, 4) X(11, 5) X(12, 0) X(13, 1) X(14, 2)

// Every channel that is switched: the 15 above, and PWM20 (k = 12), which starts bank 2 and is kept at zero width. The first channel of a bank
// (k = 0, 6, 12) is written with the bank-start value.
#define SWITCHED(X) X(0, PWM_CON_BANK_ON) X(1, PWM_CON_CHANNEL_ON) X(2, PWM_CON_CHANNEL_ON) X(3, PWM_CON_CHANNEL_ON) X(4, PWM_CON_CHANNEL_ON) X(5, PWM_CON_CHANNEL_ON) X(6, PWM_CON_BANK_ON) X(7, PWM_CON_CHANNEL_ON) X(8, PWM_CON_CHANNEL_ON) X(9, PWM_CON_CHANNEL_ON) X(10, PWM_CON_CHANNEL_ON) X(11, PWM_CON_CHANNEL_ON) \
    X(12, PWM_CON_BANK_ON) X(15, PWM_CON_CHANNEL_ON) X(16, PWM_CON_CHANNEL_ON) X(17, PWM_CON_CHANNEL_ON)

#define SET_ON(k, value)  PWM_CON(k) = (value);
#define SET_OFF(k, value) PWM_CON(k) = PWM_CON_OFF;

static void pwm_on(void)
{
    SWITCHED(SET_ON)
}

// Parked: the pins fall back to their latches (low), so nothing is driven
static void pwm_off(void)
{
    SWITCHED(SET_OFF)
}

static void set_duty(uint8_t k, uint16_t duty1, uint16_t duty2)
{
    PWM_DUTY1H(k) = (uint8_t)(duty1 >> 8);
    PWM_DUTY1L(k) = (uint8_t)duty1;
    PWM_DUTY2H(k) = (uint8_t)(duty2 >> 8);
    PWM_DUTY2L(k) = (uint8_t)duty2;
}

void led_hw_init(void)
{
    allowed = false;

    for (uint8_t bank = 0; bank < 3; bank++) {
        PWM_PERDH(bank) = PWM_PERIOD_H;
        PWM_PERDL(bank) = PWM_PERIOD_L;
    }

#define INIT_CHANNEL(n, k) set_duty((k), (uint16_t)(LED_DUTY1_BASE + (n)), (uint16_t)(LED_DUTY1_BASE + (n)));
    LED_PWM(INIT_CHANNEL)
#undef INIT_CHANNEL

    set_duty(12, LED_DUTY1_BASE, LED_DUTY1_BASE); // PWM20: no width
}

void led_hw_enable(bool on)
{
    allowed = on;

    if (!on) {
        led_hw_pause();
    }
}

void led_hw_pause(void)
{
#if STAGE_LED
    pwm_off();
    matrix_hw_cols_off();
#endif
}

void led_hw_resume(void)
{
#if STAGE_LED
    if (allowed) {
        pwm_on();
    }
#endif
}

void led_hw_subframe(void)
{
    matrix_hw_cols_off(); // the previous column goes off
    pwm_off();            // parked while the widths change

    const uint8_t column = led_next_column();

    if (!allowed || !led_column_lit(column)) {
        return;
    }

    XDATA const uint16_t *d = led_duty2(column);

#define LOAD_CHANNEL(n, k)             \
    PWM_DUTY2H(k) = (uint8_t)(d[n] >> 8); \
    PWM_DUTY2L(k) = (uint8_t)d[n];
    LED_PWM(LOAD_CHANNEL)
#undef LOAD_CHANNEL

    matrix_hw_col_on(column); // this column's keys are the ones lit
    pwm_on();                 // enabled last: the column lights now

    PWM_CON(0) &= (uint8_t) ~(1u << 5); // as the stock firmware does after a subframe (the bit looks like a PWM flag; no PWM interrupt is used)
}

// The timer was late: the backlight stands aside for a few slots so that the keys and the main loop get the CPU. Nothing is lit meanwhile.
void led_hw_idle(void)
{
    matrix_hw_cols_off();
    pwm_off();
}
