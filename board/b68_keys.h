#pragma once

#include "keycodes.h"

// Keycodes specific to the B68: the first QK_KB values, in the order the Vial definition lists them under customKeycodes. They are
// numbered from 0x7E00 with no gap (check_vial.py enforces it), so removing one from the middle renumbers the ones after it: the
// four profile keys went when the profiles did.
#define B68_HUD_TOG 0x7E00u
#define B68_FX_PREV 0x7E01u // lighting effect - / +
#define B68_FX_NEXT 0x7E02u
#define B68_BRI_DN  0x7E03u // brightness - / +
#define B68_BRI_UP  0x7E04u
#define B68_SPD_DN  0x7E05u // effect speed - / +
#define B68_SPD_UP  0x7E06u
#define B68_COL_DN  0x7E07u // colour of the plain effect - / +
#define B68_COL_UP  0x7E08u
#define B68_LK_1    0x7E09u // Bluetooth slot 1-3: a tap selects it, held for 3 s it starts pairing
#define B68_LK_2    0x7E0Au
#define B68_LK_3    0x7E0Bu
#define B68_BATTERY 0x7E0Cu // shows the battery level on the number keys (it was the radio trace in the SMK port, which Oblio does not have)
#define B68_SLEEP   0x7E0Du // sleeps now, only in the wireless modes
#define B68_SLP_DN  0x7E0Eu // inactivity time before sleep: shorter / longer
#define B68_SLP_UP  0x7E0Fu
#define B68_LEADER  0x7E10u // starts a Leader sequence (the sequences are in board/leader.c)
