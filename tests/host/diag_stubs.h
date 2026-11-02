#pragma once

// For tests that link core/diag.c without being about it (the radio, the link, VIA): nothing to refresh, and no radio data.
#include "diag.h"

static const uint8_t diag_stub_zeros[22];

void diag_hook_refresh(void) {}
const uint8_t *diag_hook_radio_status(void) { return diag_stub_zeros; }
