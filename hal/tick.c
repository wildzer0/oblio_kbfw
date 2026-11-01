#include "tick.h"
#include "diag.h"
#include "sched.h"
#include "sh68f90.h"

// The timer counts at FREQ_SYS / 12 = 2 MHz, so a slot of N microseconds is 2 * N counts.
// LED slots given up while the main loop is starved (two frames of the backlight), and how many scans in a row without a pass of the main
// loop count as starving it (a pass takes about a millisecond even with the interrupt at work)
#define BACK_OFF_SLOTS 32u
#define STARVED_SCANS  3u

#define RELOAD(us) ((uint16_t)(65536u - SCHED_COUNTS_PER_US * (us)))

static volatile uint8_t ticks;
static uint8_t          taken;

static uint8_t          scan_raw[16];
static volatile bool    scan_ready;
static uint8_t          back_off; // LED slots still to be given up after the main loop was starved
static volatile uint8_t alive;    // passes of the main loop
static uint8_t          seen_alive;
static uint8_t          starved; // scans in a row in which the main loop did not come round

// The timer is restarted at the start of every slot (as in the SMK port), so each slot is timed from its own interrupt. The next
// interrupt comes when the timer overflows, or at once after a scan, which ends its own slot.
static uint16_t pending_reload; // what the timer is armed with at the next interrupt: the length of the slot that starts then
static uint16_t prev_reload;    // the same for the slot that began at the previous interrupt: the count the timer reloaded to
static uint16_t prev_len;       // how long that slot was, in counts, from its start to its end

static uint16_t timer_count(void)
{
    uint8_t high;
    uint8_t low;

    do {
        high = TH2;
        low  = TL2;
    } while (high != TH2);

    return (uint16_t)((uint16_t)high << 8 | low);
}

// Stopped while it is loaded, so that the count and the reload value agree
static void timer_start(uint16_t reload)
{
    TR2    = 0;
    RCAP2H = (uint8_t)(reload >> 8);
    RCAP2L = (uint8_t)(reload & 0xFF);
    TH2    = (uint8_t)(reload >> 8);
    TL2    = (uint8_t)(reload & 0xFF);
    TR2    = 1;
}

void tick_init(void)
{
    ticks      = 0;
    taken      = 0;
    alive      = 0;
    seen_alive = 0;
    starved    = 0;
    back_off   = 0;
    scan_ready = false;
    sched_init();

    pending_reload = RELOAD(sched_slot_us(SLOT_LED));
    prev_reload    = pending_reload;
    prev_len       = SCHED_LED_US * SCHED_COUNTS_PER_US; // the start-up period before the first interrupt counts as an LED slot

    TR2   = 0;
    T2CON = 0; // 16-bit auto-reload on overflow
    T2MOD = 0;
    timer_start(pending_reload);
    TF2 = 0;
    ET2 = 1;
}

// The slot that begins now is played. The timer is restarted at once for its length, so the next interrupt is that long after this one began
// (or, after a scan, as soon as the scan has finished). The matrix scan and the LED subframe are in the board's hooks.
void tick_isr(void) __interrupt(INT_TIMER2)
{
    uint8_t milliseconds;

    TF2 = 0;

    const uint16_t entered = timer_count(); // counted from the overflow that brought us here: the latency of the interrupt
    const uint16_t reload  = pending_reload;

    timer_start(reload);

    const slot_t slot = sched_step((uint16_t)(prev_len + (uint16_t)(entered - prev_reload)), &milliseconds);

    ticks += milliseconds;
    pending_reload = RELOAD(sched_slot_us(sched_upcoming()));

    if (slot == SLOT_SCAN) {
        if (scan_ready) {
            diag_inc(DIAG_SCAN_LOST); // the main loop never took the previous one
        }

        tick_hook_scan(scan_raw);
        scan_ready = true;

        diag_set(DIAG_PASSES_LAST, (uint8_t)(alive - seen_alive));

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
        // The main loop being starved of the CPU (no keys, and finally the watchdog) is what a backlight that takes too much of it
        // does. The backlight is the part that can wait: it steps aside for a while and the keys keep working.
        tick_hook_led(back_off != 0);

        if (back_off) {
            back_off--;
        }
    }

    // How long the work took, from the restart, in counts
    uint16_t used = (uint16_t)(timer_count() - reload);

    if (TF2) { // the timer overflowed again before the work was done: the slot is over its length
        diag_inc(slot == SLOT_SCAN ? DIAG_SCAN_LATE : DIAG_SLOT_LATE);
        used = (uint16_t)(SCHED_COUNTS_PER_US * sched_slot_us(slot));
    }

    diag_max(slot == SLOT_SCAN ? DIAG_SCAN_US_MAX : DIAG_LED_US_MAX, (uint16_t)(used / SCHED_COUNTS_PER_US));

    prev_reload = reload;

    if (slot == SLOT_SCAN) {
        prev_len = used;
        // the scan is over: the next slot starts now, not when the timer would have run out. The counter is set one count short of the
        // overflow; RCAP2 still holds `reload`, which is what it reloads to
        TR2 = 0;
        TH2 = 0xFFu;
        TL2 = 0xFFu;
        TR2 = 1;
    } else {
        prev_len = (uint16_t)(SCHED_COUNTS_PER_US * SCHED_LED_US);
    }
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
