#pragma once

// The serial port and the pins of the link to the BK3632 radio: TXD = P5.5, RXD = P5.6 (EUART0), SEL = P0.2 (an output of the MCU,
// low while a frame goes out), ATT = P4.7 (an input: high = the radio can take a frame).
//
// The handler is declared here because SDCC builds the vector table from the module that defines main(): main.c must include
// this header.

void radio_uart_isr(void) __interrupt(13); // INT_EUART0

// Sets up the port and the interrupt. Before link_init().
void radio_hw_init(void);
