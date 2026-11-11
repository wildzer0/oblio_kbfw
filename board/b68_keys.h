#pragma once

#include "keycodes.h"

// Keycodes specific to the B68: the first QK_KB values, in the order the Vial definition lists them under customKeycodes. They are
// numbered from 0x7E00 with no gap (check_vial.py enforces it), so removing one from the middle renumbers the ones after it: the
// four profile keys went when the profiles did.
#define B68_UNUSED  0x7E00u // reserved: it was the HUD toggle. Does nothing, and keeps the numbers of the keycodes after it, which the keymaps saved in flash hold
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
#define B68_UNUSED2 0x7E10u // reserved: it was the Leader key. Does nothing, and keeps the numbers of the keycodes after it, which saved keymaps hold
#define B68_ACC_A   0x7E11u // accented letters (core/accent.h): a grave, e grave, e acute, i grave, o grave, u grave; capital with Shift, Caps Word or Caps Lock
#define B68_ACC_EG  0x7E12u
#define B68_ACC_EA  0x7E13u
#define B68_ACC_I   0x7E14u
#define B68_ACC_O   0x7E15u
#define B68_ACC_U   0x7E16u
#define B68_OS_LNX  0x7E17u // switches the accented letters between the Windows and the Linux sequence (the Win/Mac switch selects the Mac one); the key flashes the system
