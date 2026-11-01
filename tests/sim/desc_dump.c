// The descriptors of core/usb_desc.c, as hex lines "type index wIndex bytes...", for the simulator tests to compare with what the
// firmware sends over the simulated endpoint.
#include "usb_desc.h"
#include <stdio.h>

int main(void)
{
    static const struct { uint8_t type, index; uint16_t w_index; } wanted[] = {
        {1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {3, 1, 0x0409}, {3, 2, 0x0409}, {3, 3, 0x0409},
        {0x21, 0, 0}, {0x21, 0, 1}, {0x22, 0, 0}, {0x22, 0, 1},
    };

    for (unsigned i = 0; i < sizeof wanted / sizeof wanted[0]; i++) {
        const uint8_t *data;
        uint16_t       length;

        if (!usb_desc_get(wanted[i].type, wanted[i].index, wanted[i].w_index, &data, &length)) {
            return 1;
        }

        printf("%u %u %u", wanted[i].type, wanted[i].index, wanted[i].w_index);
        for (uint16_t b = 0; b < length; b++) {
            printf(" %02x", data[b]);
        }
        printf("\n");
    }

    return 0;
}
