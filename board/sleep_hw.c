#include "sleep_hw.h"
#include "gpio.h"
#include "led.h"
#include "led_hw.h"
#include "link.h"
#include "matrix_hw.h"
#include "power.h"
#include "pwm.h"
#include "radio.h"
#include "radio_hw.h"
#include "settings.h"
#include "sh68f90.h"
#include "sleep.h"
#include "tick.h"
#include "usb.h"
#include "usb_device.h"
#include "usb_reports.h"

// Sleep of the Yunzii B68, in the wireless modes only. The sequence is the stock firmware's (docs/keyboards/yunzii-b68-radio.md §10),
// written in the same order: the radio is told first, then the serial port is stopped, the PWM channels and the pins are parked, INT4
// is armed, and hal/power.c stops USB, the clock and the regulator.
// HARDWARE NOTE: this cannot be tried without the keyboard. If a pin here were wrong the keyboard would not wake: unplug and replug
// the cable (Esc+1+2 enters the bootloader).

void sleep_hw_apply_level(uint8_t level, bool show)
{
    sleep_set_level(level);

    if (show) {
        led_flash_key(3, (uint8_t)(level + 1u));
    }
}

// 0xAE46 in the dump: every PWM channel control register written with 1
static void pwm_park(void)
{
    for (uint8_t k = 0; k < PWM_CHANNELS; k++) {
        PWM_CON(k) = 1;
    }
}

// 0x9C82 in the dump: the key rows are inputs with pull-ups; everything that drives the columns and the LEDs is an output at 0
static void pins_park(void)
{
    GPIO_INPUT(7, 0x0F);
    GPIO_PULLUP_ON(7, 0x0F);
    GPIO_INPUT(5, 0x18);
    GPIO_PULLUP_ON(5, 0x18);

    GPIO_LOW(6, 0xFF);
    GPIO_LOW(5, 0x87);
    GPIO_LOW(4, 0x6D);
    GPIO_LOW(7, 0x10);
    GPIO_LOW(0, 0xE0);

    GPIO_OUTPUT(6, 0xFF);
    GPIO_OUTPUT(5, 0x87);
    GPIO_OUTPUT(4, 0x6D);
    GPIO_OUTPUT(7, 0x10);
    GPIO_OUTPUT(0, 0xE0);

    // 0x7E93-0x7EB0 in the dump: the USB lines, the selector and the encoder pins (P0.0-1, P0.3-7) outputs at 0, P7.4 and P7.7 at 0,
    // P4.1 high (the wake-up line), P7.6 low
    GPIO_PULLUP_OFF(0, 0xFB);
    GPIO_OUTPUT(0, 0xFB);
    GPIO_LOW(0, 0xFB);
    GPIO_PULLUP_OFF(7, 0x90);
    GPIO_OUTPUT(7, 0x90);
    GPIO_LOW(7, 0x90);
    GPIO_HIGH(4, 0x02);
    GPIO_LOW(7, 0x40);
}

static void sleep_and_wake(void)
{
    radio_sleep(); // the radio first, while the serial port still works (with delays: the timer is not needed)

    tick_pause();
    led_hw_pause();

    IEN1 &= (uint8_t)~IEN1_ES0;
    SCON &= (uint8_t)~0x40u;
    pwm_park();
    pins_park();
    power_wake_arm();

    power_enter_powerdown(); // returns when INT4 or a USB event wakes the core

    power_wake_disable();
    board_gpio_init();   // what the stock does after the wake-up
    led_hw_init();       // the PWM channels were parked
    radio_hw_init();     // the serial port again
    link_wake();         // the radio starts again and the selector is applied anew
    usb_device_reset();  // and USB, which was released
    usb_reports_flush();
    usb_hw_init();
    tick_resume();
}

void sleep_hw_task(uint16_t now)
{
    // Only on battery: with the cable seen by the PC the keyboard is powered and stays awake
    const bool allowed = link_wireless() && !usb_device_configured();

    if (!sleep_due(now, allowed)) {
        return;
    }

    sleep_and_wake();
    sleep_woke(now);
}
