#pragma once

// For tests that link core/diag.c without being about it (the radio, the link, VIA): nothing to refresh, and no radio data.
#include "diag.h"

static const uint8_t diag_stub_zeros[22];

void diag_hook_refresh(void) {}
const uint8_t *diag_hook_radio_status(void) { return diag_stub_zeros; }
const uint8_t *diag_hook_radio_info(void) { return diag_stub_zeros; }
void diag_hook_radio_monitor(bool on) { (void)on; }
void diag_hook_ports(uint8_t *out) { (void)out; }
void diag_hook_radio_on_battery(uint8_t *battery, uint8_t *flags) { (void)battery; (void)flags; }

// core/blog.c keeps its records in the board's flash: this is not that test
void blog_hook_read(uint16_t addr, uint8_t *out, uint8_t len) { (void)addr; (void)len; for (uint8_t i = 0; i < len; i++) out[i] = 0xFF; }
void blog_hook_program(uint16_t addr, const uint8_t *src, uint8_t len) { (void)addr; (void)src; (void)len; }
void blog_hook_erase(uint16_t addr) { (void)addr; }
