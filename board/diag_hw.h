#pragma once

#include <stdint.h>

// Fills the stack with a known value, above where it is now. Early in main(), before interrupts are on.
void diag_stack_paint(void);
