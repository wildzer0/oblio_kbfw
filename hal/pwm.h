#pragma once

// The 18 PWM channels of the SH68F90A: three banks of six (PWM0x, PWM1x, PWM2x) with a shared period each, and per channel a control
// register and two duty registers, 16 bits each (low and high byte in two places). They sit in XDATA at 0xFF80 and up; channel
// `k` is bank * 6 + number. The output is high between DUTY1 and DUTY2, whichever is the larger (checked with the LED probe,
// docs/keyboards/yunzii-b68.md), so DUTY2 - DUTY1 is the width.

#include <stdint.h>

#define PWM_CHANNELS 18

#define PWM_REG(address, k) (*(__xdata volatile uint8_t *)((address) + (k)))

#define PWM_CON(k)    PWM_REG(0xFF80u, k)
#define PWM_DUTY1L(k) PWM_REG(0xFFA0u, k)
#define PWM_DUTY1H(k) PWM_REG(0xFFB8u, k)
#define PWM_DUTY2L(k) PWM_REG(0xFFD0u, k)
#define PWM_DUTY2H(k) PWM_REG(0xFFE8u, k)

// The period of bank b (0-2)
#define PWM_PERDL(b) PWM_REG(0xFF98u, b)
#define PWM_PERDH(b) PWM_REG(0xFF9Cu, b)

#define PWM_CON_BANK_ON    0x89 // the first channel of a bank starts the bank (the values the stock firmware writes)
#define PWM_CON_CHANNEL_ON 0x08
#define PWM_CON_OFF        0x00
