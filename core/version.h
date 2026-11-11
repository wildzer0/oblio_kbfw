#pragma once

// The firmware's version, readable without running it (in the .hex) and while it runs (VIA command 0xF1, kind 2: core/diag.c).
//
// It is one structure in the code flash, `version_info`, that starts with the eight characters "OBLIOFW1". The web updater (web/js/ihex.js)
// looks for those eight bytes in the image: no fixed address, so the structure can move when the code changes. The magic ends in a format
// number: a future layout would call itself "OBLIOFW2".
//
// Where the numbers come from (Makefile): the file VERSION in the root of the repository ("major.minor.patch") and the git commit the build was made
// from (the first seven characters of its hash, and whether the working tree had changes that were not committed). A release is a build of a clean tree,
// tagged with the same number: the continuous-integration check refuses a tag that disagrees with VERSION.
//
// Left out of the profile build (LOOP_PROFILE=1): that build has a handful of bytes of flash to spare, and it is a measuring tool, not something to install.

#include "options.h"
#include "platform.h"
#include <stdint.h>

// The defaults are for the host tests and for anyone compiling without the Makefile; the Makefile passes the real ones to core/version.c only
#ifndef FW_MAJOR
#    define FW_MAJOR 0
#endif
#ifndef FW_MINOR
#    define FW_MINOR 0
#endif
#ifndef FW_PATCH
#    define FW_PATCH 0
#endif
#ifndef FW_REV
#    define FW_REV "0000000" // seven characters of the commit hash
#endif
#ifndef FW_DIRTY
#    define FW_DIRTY 0 // 1 when the tree had uncommitted changes to tracked files
#endif

// Bits of `flags`: what this build contains that another build of the same version may not
#define VERSION_FLAG_BATTERY_LOG 0x01u

#if !LOOP_PROFILE

typedef struct {
    char    magic[8]; // "OBLIOFW1", NOT terminated: the web updater searches for these eight bytes
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint8_t flags;    // VERSION_FLAG_*
    char    rev[8];   // the commit, seven characters and a terminator
    uint8_t dirty;    // 1: built from a tree with uncommitted changes
} version_info_t;     // 21 bytes

extern const ROM version_info_t version_info;

#endif
