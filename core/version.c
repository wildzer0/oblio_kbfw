#include "version.h"

#if !LOOP_PROFILE

// Written out character by character: a string literal as long as the array would leave out its terminator, which compilers warn about, and the
// magic must not end in a zero in the image (the web updater compares exactly eight bytes).
const ROM version_info_t version_info = {
    {'O', 'B', 'L', 'I', 'O', 'F', 'W', '1'},
    FW_MAJOR,
    FW_MINOR,
    FW_PATCH,
    (BATTERY_LOG ? VERSION_FLAG_BATTERY_LOG : 0u),
    FW_REV,
    FW_DIRTY,
};

#else

typedef int version_left_out; // a file with nothing in it is not valid C

#endif
