#include "battery.h"
#include "led.h"
#include "radio.h"

// The battery the way the radio reports it: seven levels, in the low three bits of one byte of its status answer (core/radio.h has the
// layout and why those are the bytes). Seven levels of a lithium cell are coarse, and the radio does not say what they mean in volts: the
// steps below are even, which is the honest thing to do with what there is - and the "critically low" flag the radio also sends turns the
// gauge red whatever the level says.
//
// With the cable in and no radio (USB mode) there is no reading, and the gauge says so instead of inventing one.
uint8_t battery_percent(void)
{
    const uint8_t level = radio_battery_level();

    if (level == RADIO_BATTERY_NONE) {
        return LED_BATTERY_UNKNOWN;
    }

    const uint8_t percent = (uint8_t)((uint16_t)level * 100u / 7u);

    return radio_low_power() && percent > LED_BATTERY_LOW ? LED_BATTERY_LOW : percent;
}
