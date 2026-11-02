#include "store.h"
#include "flash.h"
#include "layer.h"
#include "os_switch.h"
#include "diag.h"
#include "settings.h"
#include "sched.h"
#include "tick.h"
#include "led_hw.h"
#include "watchdog.h"

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    flash_read(addr, dst, len);
}

// A sector erase blocks the interrupts for milliseconds: a column of the backlight left lit for that long would flash, and the slots would be
// late. So the timer and the PWM are stopped around the erases only - the programming is a stall per byte, which nothing notices - and the
// backlight comes back as soon as the last sector is erased.
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
    // How long an erase really takes, in the diagnostic: the slot timer is the only clock still running here (the interrupt is off, the timer
    // is not), so the difference is the stall. If it ever shows ~32000 us the timer turned over twice and the figure is the remainder.
    const uint16_t before = tick_count_now();

    watchdog_kick();
    flash_erase_sector(sector_addr);

    diag_max(DIAG_ERASE_US_MAX, (uint16_t)((uint16_t)(tick_count_now() - before) / SCHED_COUNTS_PER_US));
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    watchdog_kick();
    tick_alive(); // a save programs ~1.7 KB in a burst from the main loop: without this the timer interrupt finds the loop "starved" and the backlight steps aside
    flash_program(addr, src, len);
}

// A new keymap brings its own layers: what the layer engine kept of the old one (toggled layers, one-shots) goes.
void store_hook_loaded(void)
{
    layer_init();
    os_switch_apply(); // layer_init() puts the base layer back to 0: the Win/Mac switch decides again which one it is
}
