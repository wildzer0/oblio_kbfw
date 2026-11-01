#include "tick.h"
#include "diag.h"
#include "sched.h"
#include "sh68f90.h"

// The timer counts at FREQ_SYS / 12 = 2 MHz, so a slot of N microseconds is 2 * N counts.
// LED slots given up while the main loop is starved (two frames of the backlight), and how many scans in a row without a pass of the main
// loop count as starving it (a pass takes about a millisecond even with the interrupt at work)
#define BACK_OFF_SLOTS 32u
#define STARVED_SCANS  3u

#define RELOAD(us) ((uint16_t)(65536u - 2u * (us)))

static volatile uint8_t ticks;
static uint8_t          taken;

static uint8_t          scan_raw[16];
static volatile bool    scan_ready;
static uint8_t          back_off; // LED slots still to be given up after the main loop was starved
static volatile uint8_t alive;    // passes of the main loop
static uint8_t          seen_alive;
static uint16_t         slot_start; // the count the running slot began from (what the timer reloaded)
static uint8_t          starved;  // scans in a row in which the main loop did not come round

static uint16_t         next_start;

static void set_reload(slot_t slot)
{
    const uint16_t reload = RELOAD(sched_slot_us(slot));

    next_start = reload;

    RCAP2H = (uint8_t)(reload >> 8);
    RCAP2L = (uint8_t)(reload & 0xFF);
}

void tick_init(void)
{
    const uint16_t first = RELOAD(sched_slot_us(SLOT_LED));

    ticks      = 0;
    taken      = 0;
    alive      = 0;
    seen_alive = 0;
    starved    = 0;
    back_off   = 0;
    scan_ready = false;
    sched_init();
    slot_start = first;

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
        if (scan_ready) {
            diag_inc(DIAG_SCAN_LOST); // the main loop never took the previous one
        }

        tick_hook_scan(scan_raw);
        scan_ready = true;

        if (alive == seen_alive) {
            if (++starved >= STARVED_SCANS) {
                if (!back_off) {
                    diag_inc(DIAG_BACKOFF);
                }

                back_off = BACK_OFF_SLOTS;
            }
        } else {
            starved = 0;
        }

        seen_alive = alive;
    } else {
        // A slot that ran late means the work of the interrupt does not fit its time, and the main loop is starved of the CPU (no keys, and
        // finally the watchdog). The backlight is the part that can wait: it steps aside for a while and the keys keep working.
        tick_hook_led(back_off != 0);

        if (back_off) {
            back_off--;
        }
    }

    // How long the slot took, from the overflow that began it, in us (the timer counts at 2 MHz)
    uint8_t  high;
    uint8_t  low;
    uint16_t count;

    do {
        high  = TH2;
        low   = TL2;
        count = (uint16_t)((uint16_t)high << 8 | low);
    } while (high != TH2);

    uint16_t elapsed = (uint16_t)(count - slot_start);

    if (TF2) {
        diag_inc(slot == SLOT_SCAN ? DIAG_SCAN_LATE : DIAG_SLOT_LATE); // the next overflow came while this slot's work was still running
        elapsed = (uint16_t)((uint16_t)(0u - slot_start) + (uint16_t)(count - next_start));
    }

    diag_max(slot == SLOT_SCAN ? DIAG_SCAN_US_MAX : DIAG_LED_US_MAX, (uint16_t)(elapsed >> 1));
    slot_start = next_start;
}

uint8_t tick_ms_now(void)
{
    return ticks;
}

void tick_alive(void)
{
    alive++;
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
