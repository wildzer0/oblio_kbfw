#include "battery.h"
#include "led.h"

// The battery level, 0-100, or LED_BATTERY_UNKNOWN. There is no source yet: the stock firmware does not use the chip's low-voltage comparator
// (it only clears it) and what the radio's status and 08 frames carry has not been decoded. tools/diag.py --radio reads those frames; the
// mapping to a percentage is written once the data from a battery that charges and discharges has been seen, and not before.
uint8_t battery_percent(void)
{
    return LED_BATTERY_UNKNOWN;
}
