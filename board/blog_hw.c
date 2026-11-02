#include "blog.h"
#include "flash.h"
#include "led_hw.h"
#include "tick.h"
#include "watchdog.h"

void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len)
{
    flash_read(addr, out, len);
}

void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    watchdog_kick(); // the main loop is the one that kicks the watchdog and it is stuck in here
    flash_program(addr, src, len);
}

void blog_hook_erase(uint16_t addr)
{
    // A sector erase stalls the CPU for milliseconds: the timer and the backlight stand still, as for the keymap store. It happens once every
    // 128 records, so it is not worth spreading out.
    watchdog_kick();
    tick_pause();
    led_hw_pause();
    flash_erase_sector(addr);
    led_hw_resume();
    tick_resume();
}
