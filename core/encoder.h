#pragma once

// Rotary encoders read as two lines in quadrature. The pins step through the Gray code 0 -> 1 -> 3 -> 2 -> 0 one way (clockwise)
// and the reverse the other way; a detent is a whole cycle of four steps. A jump of two states is a missed sample and counts
// for nothing, and a contact that bounces forward and back cancels out.

#include "keymap.h" // ENCODER_COUNT
#include <stdbool.h>
#include <stdint.h>

#define ENCODER_RESOLUTION 4 // state changes per detent

// Provided by the board (or a test): the two lines of encoder `index`, bit 0 = A, bit 1 = B.
uint8_t encoder_hook_state(uint8_t index);

void encoder_init(void);

// Reads the lines of every encoder: call at every scan of the matrix.
void encoder_poll(void);

// Takes one detent that has not been used yet. False when there is none.
bool encoder_next(uint8_t index, bool *clockwise);
