#include "usb.h"
#include "usb_device.h"
#include "usb_reports.h"
#include "raw_hid.h"

static volatile bool event;     // a bus reset or a configuration change: see usb_hw_take_event()
static volatile bool suspended;
static volatile bool setup_seen;

// ---- endpoint control ----
static void ep0_count(uint8_t n)
{
    IEP0CNT = (uint8_t)((IEP0CNT & ~0x0F) | n);
}

void usb_hook_ep0_in(const uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        EP0_IN_BUF[i] = data[i];
    }

    ep0_count(len);
    EP0CON |= EPCON_IRDY;
}

void usb_hook_ep0_out_arm(void)
{
    EP0CON |= EPCON_ORDY;
}

void usb_hook_ep0_stall(void)
{
    EP0CON |= EPCON_ISTL | EPCON_OSTL;
}

void usb_hook_set_address(uint8_t address)
{
    USBADDR = address;
}

// A configuration starts every endpoint clean: no stall, data toggle back to DATA0
void usb_hook_configured(bool configured)
{
    (void)configured;

    EP1CON &= (uint8_t) ~(EPCON_ISTL | EPCON_IDTG);
    EP2CON &= (uint8_t) ~(EPCON_ISTL | EPCON_IDTG | EPCON_OSTL | EPCON_ODTG);
    event = true;
}

void usb_hook_ep_halt(uint8_t endpoint, bool halted)
{
    if (endpoint == 0x81) {
        if (halted) {
            EP1CON |= EPCON_ISTL;
        } else {
            EP1CON &= (uint8_t) ~(EPCON_ISTL | EPCON_IDTG);
        }
    } else if (endpoint == 0x82) {
        if (halted) {
            EP2CON |= EPCON_ISTL;
        } else {
            EP2CON &= (uint8_t) ~(EPCON_ISTL | EPCON_IDTG);
        }
    } else if (endpoint == 0x02) {
        if (halted) {
            EP2CON |= EPCON_OSTL;
        } else {
            EP2CON &= (uint8_t) ~(EPCON_OSTL | EPCON_ODTG);
        }
    }
}

// ---- EP1 IN: the keyboard, system and consumer reports ----
bool usb_reports_hook_ep1_free(void)
{
    return !(EP1CON & EPCON_IRDY);
}

void usb_reports_hook_ep1_send(const uint8_t *data, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++) {
        EP1_IN_BUF[i] = data[i];
    }

    IEP1CNT = (uint8_t)((IEP1CNT & ~0x1F) | length);
    EP1CON |= EPCON_IRDY;
}

// ---- EP2: the raw HID pipe ----
bool raw_hid_hook_ep2_free(void)
{
    return !(EP2CON & EPCON_IRDY);
}

void raw_hid_hook_ep2_send(const uint8_t *data, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++) {
        EP2_IN_BUF[i] = data[i];
    }

    IEP2CNT = (uint8_t)((IEP2CNT & ~0x7F) | length);
    EP2CON |= EPCON_IRDY;
}

void raw_hid_hook_out_arm(void)
{
    EP2CON |= EPCON_ORDY;
}

// ---- the interrupt ----
void usb_hw_init(void)
{
    REGCON = REGCON_REGEN;

    USBADDR = 0;
    USBIE1  = USBIE1_SETUPIE | USBIE1_SOFIA | USBIE1_RESMIE | USBIE1_SUSPIE | USBIE1_PBRSTIE;
    USBIE2  = USBIE2_OEP0IE | USBIE2_IEP0IE | USBIE2_OEP2IE;
    USBCON  = USBCON_ENUSB | USBCON_SW1CON;
    IEN1 |= IEN1_EUSB;
}

bool usb_hw_take_event(void)
{
    const bool taken = event;

    event = false;
    return taken;
}

bool usb_hw_setup_seen(void)
{
    return setup_seen;
}

bool usb_hw_suspended(void)
{
    return suspended;
}

static void bus_reset(void)
{
    // the USB module is reset by pulsing SWRST; a few cycles in between
    USBCON |= USBCON_SWRST;
    // clang-format off
    __asm
        nop
        nop
        nop
        nop
        nop
        nop
    __endasm;
    // clang-format on
    USBCON &= (uint8_t)~USBCON_SWRST;

    usb_device_reset();
    usb_hw_init();
    EP0CON |= EPCON_ORDY;
    event = true;
}

// The ISR calls only raw_hid_out() of what the main loop shares, which hands over through a flag and does not run while
// raw_hid_task() works on the same packet (the endpoint is not armed then). Otherwise it does not call anything the main loop calls: it works on the EP0 state and the hardware, and tells the main loop
// through `event`, the ISP flag and the LED byte.
void usb_isr(void) __interrupt(7)
{
    const uint8_t if1 = USBIF1;

    if (if1 & (USBIF1_USBRSTIF | USBIF1_PUPIF)) {
        USBIF1 &= (uint8_t) ~(USBIF1_USBRSTIF | USBIF1_PUPIF);
        bus_reset();
        return;
    }

    if (if1 & USBIF1_SOFIF) {
        USBIF1 &= (uint8_t)~USBIF1_SOFIF;
        suspended = false; // a start-of-frame means the host is driving the bus again
    }

    if (if1 & USBIF1_RESMIF) {
        USBIF1 &= (uint8_t)~USBIF1_RESMIF;
    }

    if (if1 & USBIF1_SUSPIF) {
        USBIF1 &= (uint8_t)~USBIF1_SUSPIF;

        if (usb_device_configured()) {
            suspended = true;
        }
    }

    if (if1 & USBIF1_SETUPIF) {
        uint8_t packet[USB_SETUP_SIZE];

        setup_seen = true;

        USBIF1 &= (uint8_t) ~USBIF1_SETUPIF;
        USBIF1 &= (uint8_t) ~(USBIF1_OVERIF | USBIF1_OW);

        for (uint8_t i = 0; i < USB_SETUP_SIZE; i++) {
            packet[i] = EP0_OUT_BUF[i];
        }

        // an IN completion still pending belongs to the previous transfer's status stage
        const bool previous_done = (USBIF2 & USBIF2_IEP0IF) != 0;

        USBIF2 &= (uint8_t) ~(USBIF2_IEP0IF | USBIF2_OEP0IF);
        usb_device_setup(packet, previous_done);
    }

    const uint8_t if2 = USBIF2;

    if (if2 & USBIF2_OEP0IF) {
        uint8_t data[EP0_BUF_SIZE];
        const uint8_t n = OEP0CNT & 0x0F;

        USBIF2 &= (uint8_t)~USBIF2_OEP0IF;

        for (uint8_t i = 0; i < EP0_BUF_SIZE; i++) {
            data[i] = EP0_OUT_BUF[i];
        }

        usb_device_ep0_out(data, n);
    }

    if (if2 & USBIF2_IEP0IF) {
        USBIF2 &= (uint8_t)~USBIF2_IEP0IF;
        usb_device_ep0_in_done();
    }

    if (if2 & USBIF2_OEP2IF) {
        uint8_t       data[RAW_HID_SIZE];
        const uint8_t n = OEP2CNT & 0x7F;

        USBIF2 &= (uint8_t)~USBIF2_OEP2IF;

        for (uint8_t i = 0; i < RAW_HID_SIZE; i++) {
            data[i] = EP2_OUT_BUF[i];
        }

        raw_hid_out(data, n);
    }

    // the completion of EP1 / EP2 IN needs no bookkeeping: IRDY reads back as busy / free
    USBIF2 &= (uint8_t) ~(USBIF2_IEP1IF | USBIF2_IEP2IF);
}
