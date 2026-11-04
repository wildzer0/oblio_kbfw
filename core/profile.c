#include "profile.h"

#if LOOP_PROFILE

typedef struct {
    uint16_t count;
    uint16_t min;
    uint16_t max;
    uint32_t sum;
} step_stat_t;

static step_stat_t stat[PROFILE_STEPS];
static uint16_t    bands[PROFILE_BANDS];
static uint32_t    isr_sum[PROFILE_ISR_COUNT];

void profile_reset(void)
{
    for (uint8_t i = 0; i < PROFILE_STEPS; i++) {
        stat[i].count = 0;
        stat[i].min   = 0xFFFFu;
        stat[i].max   = 0;
        stat[i].sum   = 0;
    }

    for (uint8_t i = 0; i < PROFILE_BANDS; i++) {
        bands[i] = 0;
    }

    for (uint8_t i = 0; i < PROFILE_ISR_COUNT; i++) {
        isr_sum[i] = 0;
    }
}

void profile_step(uint8_t id, uint16_t counts)
{
    if (id == 0 || id > PROFILE_STEPS) { // the numbers start at 1: the table starts at 0, so there is no spare entry (10 bytes of the 4 KB)
        return;
    }

    step_stat_t *s = &stat[id - 1u];

    if (s->count != 0xFFFFu) { // a window this long is read before it saturates; if not, the mean is still the mean of what fell in
        s->count++;
        s->sum += counts;
    }

    if (counts < s->min) {
        s->min = counts;
    }

    if (counts > s->max) {
        s->max = counts;
    }
}

void profile_end(uint8_t id, uint16_t started)
{
    profile_step(id, (uint16_t)(profile_hook_now() - started));
}

void profile_pass(uint16_t counts)
{
    uint8_t band = 0;

    for (uint16_t limit = 400u; band < PROFILE_BANDS - 1u && counts >= limit; limit = (uint16_t)(limit << 1)) {
        band++;
    }

    if (bands[band] != 0xFFFFu) {
        bands[band]++;
    }
}

void profile_isr(profile_isr_t which, uint16_t counts)
{
    isr_sum[which] += counts;
}

uint16_t profile_step_count(uint8_t id) { return (id >= 1 && id <= PROFILE_STEPS) ? stat[id - 1u].count : 0; }
uint16_t profile_step_min(uint8_t id) { return (id >= 1 && id <= PROFILE_STEPS && stat[id - 1u].count) ? stat[id - 1u].min : 0; }
uint16_t profile_step_max(uint8_t id) { return (id >= 1 && id <= PROFILE_STEPS) ? stat[id - 1u].max : 0; }
uint32_t profile_step_sum(uint8_t id) { return (id >= 1 && id <= PROFILE_STEPS) ? stat[id - 1u].sum : 0; }
uint16_t profile_band(uint8_t band) { return band < PROFILE_BANDS ? bands[band] : 0; }
uint32_t profile_isr_sum(profile_isr_t which) { return which < PROFILE_ISR_COUNT ? isr_sum[which] : 0; }

#else

typedef int profile_left_out; // the profile build is not this one; a file with nothing in it is not valid C

#endif
