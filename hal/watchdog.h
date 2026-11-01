#pragma once

#include "sh68f90.h"

// Writing RSTSTAT kicks the watchdog. Compiled in only with -DWATCHDOG_ENABLE, so a build that does not use it
// never touches the register.
#ifdef WATCHDOG_ENABLE
#    define watchdog_kick() (RSTSTAT = WATCHDOG_PERIOD)
#else
#    define watchdog_kick() ((void)0)
#endif
