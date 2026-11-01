#include "usb_reports.h"

// keyboard.c says a report changed: it goes to the USB queue (the radio will take it in other modes, phase 6).
void report_hook_send(report_kind_t kind)
{
    usb_reports_send(kind);
}
