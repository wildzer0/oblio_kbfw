#pragma once

// SH68F90A registers used by Oblio, and nothing else. Addresses are those of the chip (the same ones the
// stock firmware dump and the SMK port use); a register is added here when a module starts needing it.

#include <stdint.h>

#define SFR(name, addr)       __sfr __at(addr) name
#define SBIT(name, addr, bit) __sbit __at((addr) + (bit)) name

// Ports 0-7: data, direction (1 = output), pull-up (1 = on)
SFR(P0, 0x80);
SFR(P1, 0x90);
SFR(P2, 0x98);
SFR(P3, 0xa0);
SFR(P4, 0xb0);
SFR(P5, 0x88);
SFR(P6, 0xc0);
SFR(P7, 0xf8);

SFR(P0CR, 0xe1);
SFR(P1CR, 0xe2);
SFR(P2CR, 0xe3);
SFR(P3CR, 0xe4);
SFR(P4CR, 0xe5);
SFR(P5CR, 0xe6);
SFR(P6CR, 0xe7);
SFR(P7CR, 0xd1);

SFR(P0PCR, 0xe9);
SFR(P1PCR, 0xea);
SFR(P2PCR, 0xeb);
SFR(P3PCR, 0xec);
SFR(P4PCR, 0xed);
SFR(P5PCR, 0xee);
SFR(P6PCR, 0xef);
SFR(P7PCR, 0xd9);

// Output drive strength: DRVCON selects which port's P?DRV the next write reaches, 0 locks it again
SFR(DRVCON, 0x8c);
SFR(P1DRV, 0xa5);
SFR(P2DRV, 0xa6);
SFR(P3DRV, 0xbb);
SFR(P5DRV, 0x8d);

#define DRVCON_UNLOCK_P1 0x05
#define DRVCON_UNLOCK_P2 0x45
#define DRVCON_UNLOCK_P3 0x85
#define DRVCON_UNLOCK_P5 0xc5
#define DRVCON_LOCK      0x00
#define DRIVE_25MA       0x00

// Watchdog: writing RSTSTAT kicks it
SFR(RSTSTAT, 0xb1);
#define WATCHDOG_PERIOD 0x00

// System clock: internal oscillator, PLL, 24 MHz
SFR(CLKCON, 0xb2);
SFR(PLLCON, 0xbc);

#define CLKCON_FS    (1u << 2)
#define CLKCON_HFON  (1u << 3)
#define PLLCON_PLLFS (1u << 0)
#define PLLCON_PLLON (1u << 1)
#define PLLCON_PLLST (1u << 2)

#define FREQ_SYS 24000000u

// Timer 2 (the tick) and the global interrupt enable
SFR(T2CON, 0xc8);
SFR(T2MOD, 0xc9);
SFR(RCAP2L, 0xca);
SFR(RCAP2H, 0xcb);
SFR(TL2, 0xcc);
SFR(TH2, 0xcd);

SBIT(EA, 0xa8, 7);
SBIT(ET2, 0xa8, 0);
SBIT(TF2, 0xc8, 7);
SBIT(TR2, 0xc8, 2);

// Self-programming of the flash (see flash.c)
SFR(IB_OFFSET, 0xfb);
SFR(IB_DATA, 0xfc);
SFR(IB_CON1, 0xf2);
SFR(IB_CON2, 0xf3);
SFR(IB_CON3, 0xf4);
SFR(IB_CON4, 0xf5);
SFR(IB_CON5, 0xf6);
SFR(XPAGE, 0xf7);
SFR(FLASHCON, 0xa7);

// Interrupt numbers, as SDCC's __interrupt(n) wants them: vector 0x0003 + 8 * n. On this chip Timer 2 sits on vector 0.
#define INT_TIMER2 0
