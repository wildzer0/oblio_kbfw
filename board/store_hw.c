#include "store.h"
#include "flash.h"
#include "layer.h"
#include "diag.h"
#include "settings.h"
#include "tick.h"
#include "led_hw.h"

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    flash_read(addr, dst, len);
}

// A flash operation blocks the interrupts for milliseconds: a column of the backlight left lit for that long would flash, and the
// slots would be late. So the timer and the PWM are stopped across it. (The clock in milliseconds loses that time.)
void store_hook_erase(uint16_t sector_addr)
{
    diag_inc(DIAG_FLASH_ERASES);
    tick_pause();
    led_hw_pause();
    flash_erase_sector(sector_addr);
    led_hw_resume();
    tick_resume();
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    tick_pause();
    led_hw_pause();
    flash_program(addr, src, len);
    led_hw_resume();
    tick_resume();
}

// A new profile brings its own layers: what the layer engine kept of the old one (toggled layers, one-shots) goes. And the choice is
// remembered for the next power-on.
void store_hook_loaded(void)
{
    layer_init();
    settings_set_profile(store_profile());
}
