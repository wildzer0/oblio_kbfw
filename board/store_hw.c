#include "store.h"
#include "flash.h"
#include "layer.h"

void store_hook_read(uint16_t addr, uint8_t *dst, uint8_t len)
{
    flash_read(addr, dst, len);
}

void store_hook_erase(uint16_t sector_addr)
{
    flash_erase_sector(sector_addr);
}

void store_hook_program(uint16_t addr, const uint8_t *src, uint8_t len)
{
    flash_program(addr, src, len);
}

// A new profile brings its own layers: what the layer engine kept of the old one (toggled layers, one-shots) goes.
void store_hook_loaded(void)
{
    layer_init();
}
