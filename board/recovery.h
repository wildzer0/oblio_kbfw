#pragma once

// Recovery chord: hold Esc + 1 + 2 (cells c0r0, c1r0, c2r0) while plugging in the USB cable to jump to the stock
// ISP bootloader, so that a firmware that does not come up can always be replaced. Called right at the start of
// main(), before USB and radio. Returns if the chord is not held.
void recovery_check(void);
