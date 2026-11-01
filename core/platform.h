#pragma once

// The one place where the 8051 and the host differ for the pure logic: constant tables live in code space on the
// chip (ROM), and are ordinary constants on the host.
#ifdef __SDCC
#    define ROM   __code
#    define XDATA __xdata // a pointer into external RAM: two bytes and direct access, where a generic pointer is three and slow
#else
#    define ROM
#    define XDATA
#endif
