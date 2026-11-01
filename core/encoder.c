#include "encoder.h"
#include "platform.h"

// Change in position for a move from state `old` to state `new`, indexed old * 4 + new
static const ROM int8_t delta[16] = {
    0,  1, -1, 0,  // from 0
    -1, 0, 0,  1,  // from 1
    1,  0, 0,  -1, // from 2
    0,  -1, 1, 0,  // from 3
};

static uint8_t last[ENCODER_COUNT];
static int8_t  position[ENCODER_COUNT]; // state changes inside the current detent
static int8_t  steps[ENCODER_COUNT];    // detents not used yet
static bool    primed;

void encoder_init(void)
{
    primed = false;

    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        last[i]     = 0;
        position[i] = 0;
        steps[i]    = 0;
    }
}

static void sample(uint8_t index, uint8_t state)
{
    const int8_t move = delta[(uint8_t)((last[index] << 2) | state)];

    last[index] = state;

    if (!move) {
        return;
    }

    position[index] += move;

    if (position[index] >= ENCODER_RESOLUTION) {
        position[index] -= ENCODER_RESOLUTION;

        if (steps[index] < 127) {
            steps[index]++;
        }
    } else if (position[index] <= -ENCODER_RESOLUTION) {
        position[index] += ENCODER_RESOLUTION;

        if (steps[index] > -127) {
            steps[index]--;
        }
    }
}

void encoder_poll(void)
{
    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        const uint8_t state = (uint8_t)(encoder_hook_state(i) & 3u);

        if (primed) {
            sample(i, state);
        } else {
            last[i] = state; // the first reading is where the knob is, not a move
        }
    }

    primed = true;
}

bool encoder_next(uint8_t index, bool *clockwise)
{
    if (index >= ENCODER_COUNT || steps[index] == 0) {
        return false;
    }

    if (steps[index] > 0) {
        steps[index]--;
        *clockwise = true;
    } else {
        steps[index]++;
        *clockwise = false;
    }

    return true;
}
