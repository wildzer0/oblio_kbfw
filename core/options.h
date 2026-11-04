#pragma once

// Build options. They come from the compiler's command line (`make BATTERY_LOG=0 fw`, which passes -DBATTERY_LOG=0); this file gives them their
// default for the host tests and for anyone who compiles without the Makefile.

// The battery log (core/blog.c): the keyboard writes down its battery reading in a flash sector of its own, so that the history can be read later
// with `tools/diag.py --blog`. It costs about 1 KB of code and the flash sector at BLOG_ADDR, which the application then cannot use: the code
// limit is 0xDA00 with the log and 0xDC00 without it.
#ifndef BATTERY_LOG
#    define BATTERY_LOG 1
#endif

#if BATTERY_LOG
#    define APP_FLASH_END 0xDA00u // the application stops here; the log sector follows, then the keymap store at 0xDC00
#    define BLOG_ADDR     0xDA00u
#else
#    define APP_FLASH_END 0xDC00u
#endif

// A build that times each step of the main loop and keeps the slowest (DIAG_LOOP_WORST: which step, how many milliseconds), for finding what makes a pass of the
// loop long - a long pass starves the backlight (hal/tick.c) and it blinks. It costs a little time in every pass, so it is off by default: `make LOOP_PROFILE=1 fw`.
#ifndef LOOP_PROFILE
#    define LOOP_PROFILE 0
#endif

