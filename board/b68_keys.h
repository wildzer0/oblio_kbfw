#pragma once

#include "keycodes.h"

// Keycodes specific to the B68: the first QK_KB values, in the order the Vial definition will list them
// under customKeycodes (same numbers as the SMK port, so a saved Vial layout keeps its meaning).
#define B68_HUD_TOG 0x7E00u
#define B68_FX_PREV 0x7E01u // lighting effect - / +
#define B68_FX_NEXT 0x7E02u
#define B68_BRI_DN  0x7E03u // brightness - / +
#define B68_BRI_UP  0x7E04u
#define B68_SPD_DN  0x7E05u // effect speed - / +
#define B68_SPD_UP  0x7E06u
#define B68_PROF_1  0x7E07u // profiles 1-4
#define B68_PROF_2  0x7E08u
#define B68_PROF_3  0x7E09u
#define B68_PROF_4  0x7E0Au
#define B68_COL_DN  0x7E0Bu // colour of the plain effect - / +
#define B68_COL_UP  0x7E0Cu
#define B68_LK_1    0x7E0Du // Bluetooth slot 1-3: a tap selects it, held for 3 s it starts pairing
#define B68_LK_2    0x7E0Eu
#define B68_LK_3    0x7E0Fu
#define B68_TRACE   0x7E10u // types out the trace of the radio link
#define B68_SLEEP   0x7E11u // sleeps now, only in the wireless modes
#define B68_SLP_DN  0x7E12u // inactivity time before sleep: shorter / longer
#define B68_SLP_UP  0x7E13u
