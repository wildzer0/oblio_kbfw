#pragma once

#include <stdint.h>

// Decides whether the backlight may light, from the state of the USB bus. From the main loop; `now` in ms.
void led_app_task(uint16_t now);
