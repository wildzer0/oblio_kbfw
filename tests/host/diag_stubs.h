#pragma once

// For tests that link core/diag.c without being about it (the radio, the link, VIA): nothing to refresh, and no radio data.
#include "diag.h"

static const uint8_t diag_stub_zeros[22];

void diag_hook_refresh(void) {}
#if BATTERY_LOG
uint8_t diag_hook_blog_count(void) { return 0; }
bool diag_hook_blog_get(uint8_t index, uint8_t out[4]) { (void)index; (void)out; return false; }
#endif
const uint8_t *diag_hook_radio_status(void) { return diag_stub_zeros; }
