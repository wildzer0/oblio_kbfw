#pragma once

// Timer 2 drives the whole timing of the keyboard: it plays the slots of core/sched.h (an LED subframe, or a scan of the key matrix)
// and counts milliseconds from their lengths. Auto-reload mode: the length of a slot is the value the timer reloads when the slot
// begins, so the slots are exact whatever the latency of the interrupt.
//
// The handler is declared here because SDCC builds the vector table from the module that defines main(): main.c must include
// this header.

#include <stdbool.h>
#include <stdint.h>

void tick_isr(void) __interrupt(0); // INT_TIMER2

// Provided by the board: the work of the two kinds of slots. They run inside the interrupt and must fit in their slot (400 / 450 us).
void tick_hook_scan(uint8_t raw[16]); // scan the key matrix: raw[col] has bit `row` set for a pressed key
void tick_hook_led(void);             // play one subframe of the backlight

void tick_init(void);

// Stops / restarts the slots (around a flash write, which blocks interrupts for milliseconds).
void tick_pause(void);
void tick_resume(void);

// True once for every millisecond that has passed and has not been taken.
bool tick_take(void);

// True once for every scan of the matrix not yet taken; its result is copied to `raw`.
bool tick_scan_take(uint8_t raw[16]);
