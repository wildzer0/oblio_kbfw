#pragma once

// The one place where the 8051 and the host differ for the pure logic: constant tables live in code space on the
// chip (ROM), and are ordinary constants on the host.
#ifdef __SDCC
#    define ROM __code
#else
#    define ROM
#endif
