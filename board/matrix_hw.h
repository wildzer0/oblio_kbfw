#pragma once

#include "matrix.h"

// Puts the pins in the state the stock firmware leaves them in at idle (see matrix_hw.c). Right after clock_init().
void board_gpio_init(void);

// Scans the 16 columns once. raw[col] has bit `row` set for a pressed key. A column whose two readings
// disagree is reported as empty.
void matrix_hw_scan(uint8_t raw[MATRIX_COLS]);

// The column strobes are also the sinks of the backlight (board/led_hw.c): all off, or one low.
void matrix_hw_cols_off(void);
void matrix_hw_col_on(uint8_t col);
