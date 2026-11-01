#include "tick.h"
#include "sched.h"
#include "sh68f90.h"

// The timer counts at FREQ_SYS / 12 = 2 MHz, so a slot of N microseconds is 2 * N counts.
#define RELOAD(us) ((uint16_t)(65536u - 2u * (us)))

static volatile uint8_t ticks;
static uint8_t          taken;

static uint8_t          scan_raw[16];
static volatile bool    scan_ready;

static void set_reload(slot_t slot)
{
    const uint16_t reload = RELOAD(sched_slot_us(slot));

    RCAP2H = (uint8_t)(reload >> 8);
    RCAP2L = (uint8_t)(reload & 0xFF);
}

void tick_init(void)
{
    const uint16_t first = RELOAD(sched_slot_us(SLOT_LED));

    ticks      = 0;
    taken      = 0;
    scan_ready = false;
    sched_init();

    TR2   = 0;
    T2CON = 0; // 16-bit auto-reload on overflow
    T2MOD = 0;
    set_reload(SLOT_LED);
    TH2 = (uint8_t)(first >> 8);
    TL2 = (uint8_t)(first & 0xFF);
    TF2 = 0;
    ET2 = 1;
    TR2 = 1;
}

// The slot that begins now is played, and the timer is told how long the one after it is: that is the value it reloads at the next
// overflow. The matrix scan and the LED subframe are in the board's hooks.
void tick_isr(void) __interrupt(INT_TIMER2)
{
    bool millisecond;

    TF2 = 0;

    const slot_t slot = sched_step(&millisecond);

    if (millisecond) {
        ticks++;
    }

    set_reload(sched_upcoming());

    if (slot == SLOT_SCAN) {
        tick_hook_scan(scan_raw);
        scan_ready = true;
    } else {
        tick_hook_led();
    }
}

void tick_pause(void)
{
    ET2 = 0;
}

void tick_resume(void)
{
    ET2 = 1;
}

bool tick_take(void)
{
    if (taken == ticks) {
        return false;
    }

    taken++;
    return true;
}

bool tick_scan_take(uint8_t raw[16])
{
    if (!scan_ready) {
        return false;
    }

    ET2 = 0; // the interrupt writes the buffer: copy it whole
    for (uint8_t i = 0; i < 16; i++) {
        raw[i] = scan_raw[i];
    }
    scan_ready = false;
    ET2 = 1;
    return true;
}
