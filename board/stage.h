#pragma once

// Bring-up stage (see the Makefile): how much of the firmware is started. The code is the same in every stage; a lower stage leaves the
// later parts alone, so that a fault on the first runs on the hardware is found in the module that brought it.
//   1  matrix, USB, Vial, HUD, profiles, settings, encoder
//   2  + the backlight (PWM outputs, the timer's LED slots)
//   3  + the radio, the mode selector and the Bluetooth keys
//   4  + sleep: the release build
#ifndef OBLIO_STAGE
#    define OBLIO_STAGE 4
#endif

#define STAGE_LED   (OBLIO_STAGE >= 2)
#define STAGE_RADIO (OBLIO_STAGE >= 3)
#define STAGE_SLEEP (OBLIO_STAGE >= 4)
