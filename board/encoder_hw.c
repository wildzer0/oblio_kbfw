#include "encoder.h"
#include "sh68f90.h"

// The encoder of the B68: A on P0.6, B on P0.7, inputs with pull-up (found with the probe layout, docs/keyboards/yunzii-b68.md).
uint8_t encoder_hook_state(uint8_t index)
{
    (void)index;
    return (uint8_t)((P0 >> 6) & 0x03u);
}
