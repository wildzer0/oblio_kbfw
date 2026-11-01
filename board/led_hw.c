#include "led_hw.h"
#include "led.h"
#include "matrix_hw.h"
#include "platform.h"
#include "pwm.h"
#include "sh68f90.h"

// RGB backlight of the Yunzii B68, as found with the LED probe (docs/keyboards/yunzii-b68.md).
//
// The colour channels, in the order of the framebuffer (row 0 R G B, row 1 R G B, ...), and the PWM channel each one is on:
//   row 0: P1.3 P1.4 P1.5 = PWM23 PWM24 PWM25    row 1: P2.0 P2.1 P2.2 = PWM10 PWM11 PWM12    row 2: P2.3 P2.4 P2.5 = PWM13 PWM14 PWM15
//   row 3: P3.3 P3.4 P3.5 = PWM03 PWM04 PWM05    row 4: P3.0 P3.1 P3.2 = PWM00 PWM01 PWM02    (the shift row and the bottom row are swapped)
// as PWM channel numbers k = bank * 6 + number (P1.n = PWM2n, P2.n = PWM1n, P3.n = PWM0n):
static const ROM uint8_t channel_pwm[LED_CHANNELS] = {15, 16, 17, 6, 7, 8, 9, 10, 11, 3, 4, 5, 0, 1, 2};

// Every channel that is switched: the 15 above and PWM20, the first of bank 2, which starts the bank and is kept at zero width
static const ROM uint8_t switched_pwm[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 15, 16, 17};

#define PWM_PERIOD_H 0x04 // 1200 counts, as the stock firmware
#define PWM_PERIOD_L 0xB0

// DUTY1 of channel n is 195 + n: staggered like the stock firmware, so that the channels do not all switch at once (its first is 0xC3)
#define PWM_DUTY1_BASE 195u

static volatile bool allowed;

static void pwm_on(void)
{
    for (uint8_t i = 0; i < sizeof switched_pwm; i++) {
        const uint8_t k = switched_pwm[i];

        PWM_CON(k) = (k % 6 == 0) ? PWM_CON_BANK_ON : PWM_CON_CHANNEL_ON;
    }
}

// Parked: the pins fall back to their latches (low), so nothing is driven
static void pwm_off(void)
{
    for (uint8_t i = 0; i < sizeof switched_pwm; i++) {
        PWM_CON(switched_pwm[i]) = PWM_CON_OFF;
    }
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

    for (uint8_t n = 0; n < LED_CHANNELS; n++) {
        const uint16_t duty = (uint16_t)(PWM_DUTY1_BASE + n);

        set_duty(channel_pwm[n], duty, duty);
    }

    set_duty(12, PWM_DUTY1_BASE, PWM_DUTY1_BASE); // PWM20: no width
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
    pwm_off();
    matrix_hw_cols_off();
}

void led_hw_resume(void)
{
    if (allowed) {
        pwm_on();
    }
}

void led_hw_subframe(void)
{
    matrix_hw_cols_off(); // the previous column goes off
    pwm_off();            // parked while the widths change

    const uint8_t column = led_next_column();

    if (!allowed || !led_column_lit(column)) {
        return;
    }

    for (uint8_t n = 0; n < LED_CHANNELS; n++) {
        const uint16_t duty2 = (uint16_t)(PWM_DUTY1_BASE + n + LED_WIDTH(led_level(column, n)));
        const uint8_t  k     = channel_pwm[n];

        PWM_DUTY2H(k) = (uint8_t)(duty2 >> 8);
        PWM_DUTY2L(k) = (uint8_t)duty2;
    }

    matrix_hw_col_on(column); // this column's keys are the ones lit
    pwm_on();                 // enabled last: the column lights now

    PWM_CON(0) &= (uint8_t) ~(1u << 5); // as the stock firmware does after a subframe (the bit looks like a PWM flag; no PWM interrupt is used)
}
