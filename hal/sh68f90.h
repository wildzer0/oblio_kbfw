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

SFR(SP, 0x81); // the stack pointer

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

// Voltage regulator that powers the USB transceiver
SFR(REGCON, 0x8f);
#define REGCON_REGEN (1u << 0)

// USB device controller
SFR(USBCON, 0x91);
SFR(USBIF1, 0x92);
SFR(USBIF2, 0x93);
SFR(USBIE1, 0x94);
SFR(USBIE2, 0x95);
SFR(USBADDR, 0x96);
SFR(EP0CON, 0x97);
SFR(EP1CON, 0x99);
SFR(EP2CON, 0x9a);
SFR(IEP0CNT, 0x9b);
SFR(IEP1CNT, 0x9c);
SFR(IEP2CNT, 0x9d);
SFR(OEP0CNT, 0x9e);
SFR(OEP1CNT, 0x9f);
SFR(OEP2CNT, 0xa4);
SFR(IEN1, 0xa9);

#define USBCON_GOSUSP (1u << 0)
#define USBCON_WKUP   (1u << 1)
#define USBCON_SW2CON (1u << 2)
#define USBCON_SWRST  (1u << 5) // reset the USB module
#define USBCON_SW1CON (1u << 6) // connect: D+ pull-up
#define USBCON_ENUSB  (1u << 7)

#define USBIF1_USBRSTIF (1u << 0) // bus reset
#define USBIF1_SUSPIF   (1u << 1)
#define USBIF1_RESMIF   (1u << 2)
#define USBIF1_SOFIF    (1u << 3)
#define USBIF1_SETUPIF  (1u << 4)
#define USBIF1_OW       (1u << 5)
#define USBIF1_OVERIF   (1u << 6)
#define USBIF1_PUPIF    (1u << 7) // power-up

#define USBIF2_IEP0IF (1u << 0)
#define USBIF2_IEP1IF (1u << 1)
#define USBIF2_IEP2IF (1u << 2)
#define USBIF2_OEP0IF (1u << 4)
#define USBIF2_OEP1IF (1u << 5)
#define USBIF2_OEP2IF (1u << 6)

#define USBIE1_PBRSTIE (1u << 0)
#define USBIE1_SUSPIE  (1u << 1)
#define USBIE1_RESMIE  (1u << 2)
#define USBIE1_SOFIA   (1u << 3)
#define USBIE1_SETUPIE (1u << 4)

#define USBIE2_IEP0IE (1u << 0)
#define USBIE2_OEP0IE (1u << 4)
#define USBIE2_OEP2IE (1u << 6)

#define IEN1_EUSB (1u << 0)

// EP0CON / EP1CON / EP2CON: O = out (host to device), I = in
#define EPCON_ORDY (1u << 0)
#define EPCON_OSTL (1u << 1)
#define EPCON_IRDY (1u << 2)
#define EPCON_ISTL (1u << 3)
#define EPCON_ODTG (1u << 6)
#define EPCON_IDTG (1u << 7)

// Endpoint buffers (their own memory, beyond the 4 KB of XDATA)
#define EP0_BUF_SIZE 8
#define EP1_BUF_SIZE 16
#define EP2_BUF_SIZE 64

static __xdata __at(0x1100) volatile uint8_t EP0_OUT_BUF[EP0_BUF_SIZE];
static __xdata __at(0x1108) volatile uint8_t EP0_IN_BUF[EP0_BUF_SIZE];
static __xdata __at(0x1120) volatile uint8_t EP1_IN_BUF[EP1_BUF_SIZE];
static __xdata __at(0x1140) volatile uint8_t EP2_OUT_BUF[EP2_BUF_SIZE];
static __xdata __at(0x1180) volatile uint8_t EP2_IN_BUF[EP2_BUF_SIZE];

// Power-down and the wake-up line (INT4)
SFR(SUSLO, 0x8e);
SFR(IENC, 0xba);
SFR(EXF0, 0xb6);
SFR(EXF1, 0xe8);
SBIT(EX4, 0xa8, 1);

#define PCON_PD            (1u << 1)
#define SUSLO_POWERDOWN_KEY 0x55 // written right before PCON.PD
#define USBIE1_BOOTS       (1u << 7)
// INT4's edge and trigger mode: the datasheet does not document these two registers; the values are the stock firmware's
#define IENC_INT4_CONFIG  0xF3
#define EXF0_INT4_TRIGGER 0x40

// The serial port (EUART0), which carries the link to the radio
SFR(PCON, 0x87);
SFR(SCON, 0xd8);
SFR(SBUF, 0xaa);
SFR(SBRTH, 0xad);
SFR(SBRTL, 0xae);
SFR(SFINE, 0xaf);
SFR(IPL1, 0xb9);
SFR(IPH1, 0xb5);

SBIT(RI, 0xd8, 0);
SBIT(TI, 0xd8, 1);
SBIT(SM2_TXCOL, 0xd8, 5); // with PCON.SSTAT set, the top three bits of SCON are error flags: framing, receive overrun, transmit collision
SBIT(SM1_RXOV, 0xd8, 6);
SBIT(SM0_FE, 0xd8, 7);

#define PCON_SSTAT (1u << 6)
#define IEN1_ES0   (1u << 6) // also the bit of the serial port in IPL1 / IPH1

// Interrupt numbers, as SDCC's __interrupt(n) wants them: vector 0x0003 + 8 * n. On this chip Timer 2 sits on vector 0.
#define INT_TIMER2 0
#define INT_USB    7
#define INT_EUART0 13
#define INT_INT4   1
