#include "charger_hw.h"
#include "charger.h"
#include "sh68f90.h"

// P4.4: input, no pull-up (P4PCR = 0x6F in board_gpio_init, as the stock leaves it): high with the cable.
// P7.7: input with pull-up: low while the cell is charging.
#define CABLE_BIT  0x10u
#define CHARGE_BIT 0x80u

void charger_hw_sample(uint16_t now_ms)
{
    charger_feed((P4 & CABLE_BIT) != 0, (P7 & CHARGE_BIT) == 0, now_ms);
}

uint8_t charger_hw_pins(void)
{
    return (uint8_t)(((P4 & CABLE_BIT) ? 1u : 0u) | ((P7 & CHARGE_BIT) ? 2u : 0u));
}
