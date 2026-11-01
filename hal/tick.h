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
void tick_hook_led(bool give_up);     // play one subframe of the backlight; `give_up`: the timer is running late, switch the backlight off instead

void tick_init(void);

// Stops / restarts the slots (around a flash write, which blocks interrupts for milliseconds).
void tick_pause(void);
void tick_resume(void);

// True once for every millisecond that has passed and has not been taken.
bool tick_take(void);

// The millisecond count of the interrupt, modulo 256 (for the USB start-of-frame to compare its own clock against the bus).
uint8_t tick_ms_now(void);

// True once for every scan of the matrix not yet taken; its result is copied to `raw`.
bool tick_scan_take(uint8_t raw[16]);

// The main loop calls this once per pass. The timer interrupt watches it: if the main loop has not come round for a few scans, the interrupt work
// does not fit its slots and is starving it, and the backlight steps aside (see tick.c) so that the keys, USB and the watchdog get the CPU.
void tick_alive(void);
