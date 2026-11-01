#include "report.h"

// No transport yet: USB (phase 4) and the radio (phase 6) will turn the report state into packets here.
void report_hook_send(report_kind_t kind)
{
    (void)kind;
}
