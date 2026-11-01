#include "link.h"
#include "radio.h"
#include "report.h"
#include "usb_reports.h"

// keyboard.c says a report changed. In the wireless modes it goes to the radio, otherwise to the USB queue.
void report_hook_send(report_kind_t kind)
{
    if (!link_wireless()) {
        usb_reports_send(kind);
        return;
    }

    if (kind == REPORT_KEYBOARD) {
        uint8_t boot[REPORT_BOOT_SIZE];

        report_build_boot(boot);
        radio_send_keys(boot[0], &boot[2]);
    } else if (kind == REPORT_CONSUMER) {
        radio_send_consumer(report_consumer());
    }
    // the system keys (power, sleep) and the mouse are not mapped on the radio: REPORT_MOUSE goes nowhere in the wireless modes
}
