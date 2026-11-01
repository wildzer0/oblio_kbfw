#pragma once

#include "sh68f90.h"

// `port` is the port number (0-7), pasted into the register name: GPIO_HIGH(6, 0x03) sets P6 bits 0 and 1.
#define GPIO_DIR_WRITE(port, value)    (P##port##CR = (uint8_t)(value)) // 1 = output
#define GPIO_PULLUP_WRITE(port, value) (P##port##PCR = (uint8_t)(value)) // 1 = pull-up on
#define GPIO_WRITE(port, value)        (P##port = (uint8_t)(value))
#define GPIO_HIGH(port, mask)          (P##port |= (uint8_t)(mask))
#define GPIO_LOW(port, mask)           (P##port &= (uint8_t) ~(mask))
