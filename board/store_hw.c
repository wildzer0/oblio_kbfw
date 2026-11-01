#include "store.h"
#include "flash.h"
#include "layer.h"
#include "os_switch.h"
#include "diag.h"
#include "settings.h"
#include "tick.h"
#include "led_hw.h"
#include "watchdog.h"

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    flash_read(addr, dst, len);
}

// A flash operation blocks the interrupts for milliseconds: a column of the backlight left lit for that long would flash, and the
// slots would be late. So the timer and the PWM are stopped once for the whole save (not once per chunk, which made the backlight
// blink and cost a pause and a start for every 64 bytes). The clock in milliseconds loses that time.
static bool flushing;

void store_hook_flush_begin(void)
{
    if (!flushing) {
        flushing = true;
        tick_pause();
        led_hw_pause();
    }
}

void store_hook_flush_end(void)
{
    if (flushing) {
        flushing = false;
        led_hw_resume();
        tick_resume();
    }
}

// The main loop is the one that kicks the watchdog and it is stuck in here: kick before every operation, or a slow erase runs past
// the period and resets the keyboard in the middle of a save (the other copy of the profile survives, but the save is lost).
void store_hook_erase(uint16_t sector_addr)
{
    watchdog_kick();
    flash_erase_sector(sector_addr);
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    watchdog_kick();
    flash_program(addr, src, len);
}

// A new keymap brings its own layers: what the layer engine kept of the old one (toggled layers, one-shots) goes.
void store_hook_loaded(void)
{
    layer_init();
    os_switch_apply(); // layer_init() puts the base layer back to 0: the Win/Mac switch decides again which one it is
}
