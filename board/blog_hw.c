#include "blog.h"
#include "diag.h"
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
    diag_inc(DIAG_BLOG_WRITES);

    // Held? The log never trusts the write: it reads the record back, and counts it if the flash refused or kept something else.
    uint8_t back[BLOG_RECORD_BYTES];

    if (!flash_program(addr, src, len)) {
        diag_inc(DIAG_BLOG_BAD);
        return;
    }

    flash_read(addr, back, len);

    for (uint8_t i = 0; i < len; i++) {
        if (back[i] != src[i]) {
            diag_inc(DIAG_BLOG_BAD);
            break;
        }
    }
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
