#!/usr/bin/env python3
"""Reads the diagnostic counters of an Oblio keyboard.

Linux: /dev/hidraw, no dependencies. Windows and macOS: `pip install hidapi` (the module is called `hid`).

usage: diag.py [--radio] [--blog] [--profile] [--profile-reset] [--watch SECONDS] [--daemon SECONDS [FILE]]

  (no option)  print the counters once
  --radio      also print the radio's last status bytes, with the battery reading they carry
  --blog       print the battery log the keyboard keeps in flash (a build with BATTERY_LOG=1, the default): the readings it took, with the cable out too
  --profile    (a LOOP_PROFILE build) how long each step of the main loop takes: runs, shortest, mean, longest and share of the time, the passes by length,
               and the share of the CPU each interrupt takes. --profile-reset starts the measure again (do it, do what you want to measure, then read)
  --watch N    repeat every N seconds, printing only what changed
  --daemon N   append the counters to a CSV every N seconds

Everything here is read-only and needs no unlock. With the cable the battery reading is saturated: it only means something on battery.
Run it as a user that may open the hidraw device (a udev rule, or sudo).
"""

import glob
import os
import struct
import sys
import time

try:
    import select
except ImportError:  # pragma: no cover
    select = None

try:
    import hid  # pip install hidapi: used where there is no /dev/hidraw
except ImportError:
    hid = None

VID_PID = "0000258A:0000DEAD"
RAW_SIZE = 32

# The steps of the main loop that a LOOP_PROFILE build times (board/main.c)
STEPS = {1: "matrix_feed", 2: "encoder_poll", 3: "keyboard_event", 4: "keyboard_task", 5: "os_switch_task", 6: "store_task", 7: "settings_task",
         8: "usb_wake_task", 9: "usb_task", 10: "led_task", 11: "led_app_task", 12: "link_task (radio)", 13: "battery_task", 14: "blog_task",
         15: "sleep_hw_task", 16: "  led: paint chunk (8 LEDs)"}

# The order of the counters in core/diag.h: keep this list in step with it (an old copy prints the wrong labels)
NAMES = [
    ("reset cause (RSTSTAT)", "{:#04x}"),
    ("stack used (bytes of 223)", "{}"),
    ("LED slots that ran late", "{}"),
    ("slowest main-loop pass (ms)", "{}"),
    ("USB report queue overflows", "{}"),
    ("backlight back-offs (starved)", "{}"),
    ("scan slots that ran late", "{}"),
    ("longest sector erase (us)", "{}"),
    ("radio: frames received (checksum ok)", "{}"),
    ("timer interrupt: longest start delay (us)", "{}"),
    ("USB: times the host was declared gone", "{}"),
    ("keymap: whole saves ever (header sequence)", "{}"),
    ("keymap: records in the log", "{}"),
    ("charger lines (bit mask, decoded below)", "{:#04x}"),
]


def find_device():
    """The hidraw node of the raw HID interface (usage page 0xFF60) of the keyboard, or its hidapi path where there is no hidraw."""
    if not os.path.isdir("/sys/class/hidraw"):
        if hid is None:
            sys.exit("diag: no /dev/hidraw here; install the hidapi module:  py -m pip install hidapi")
        for info in hid.enumerate(0x258A, 0xDEAD):
            if info["usage_page"] == 0xFF60:
                return info["path"]
        return None

    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            uevent = open(os.path.join(node, "device/uevent")).read()
            descriptor = open(os.path.join(node, "device/report_descriptor"), "rb").read()
        except OSError:
            continue
        if VID_PID in uevent.upper() and b"\x06\x60\xff" in descriptor:
            return "/dev/" + os.path.basename(node)
    return None


class Keyboard:
    def __init__(self, path):
        self.device = None
        if isinstance(path, bytes):  # a hidapi path
            self.device = hid.device()
            self.device.open_path(path)
        else:
            self.fd = os.open(path, os.O_RDWR)

    def ask(self, *request):
        packet = bytes(request) + bytes(RAW_SIZE - len(request))

        if self.device:
            self.device.write(b"\x00" + packet)  # report id 0: the interface has none
            reply = self.device.read(RAW_SIZE, 1000)
            if not reply:
                sys.exit("diag: the keyboard did not answer")
            return bytes(reply)

        os.write(self.fd, packet)
        ready, _, _ = select.select([self.fd], [], [], 1.0)
        if not ready:
            sys.exit("diag: the keyboard did not answer")
        return os.read(self.fd, RAW_SIZE)



def counters(keyboard):
    reply = keyboard.ask(0xF1, 0)
    if reply[0] != 0xF1:
        sys.exit("diag: the keyboard does not know the diagnostics command (is this an Oblio build?)")
    count = reply[2]
    return list(struct.unpack_from("<" + "H" * count, reply, 3))


def show(values, previous=None):
    for i, value in enumerate(values):
        if previous is not None and previous[i] == value:
            continue
        name, fmt = NAMES[i] if i < len(NAMES) else ("counter {}".format(i), "{}")
        print("  {:32} {}".format(name, fmt(value) if callable(fmt) else fmt.format(value)))


def charger(bits):
    """The lines of the charger (core/charger.h). The polarities come from the stock firmware, not from a measurement: read them in the three states
    (on battery, cable and charging, cable and charged) and compare."""
    print("charger            : P4.4 {} (cable line, high = cable)   P7.7 {} (charge line, low = charging)".format("high" if bits & 1 else "low", "high" if bits & 2 else "low"))
    print("                     debounced: cable {}, charging {}   |   the keyboard concludes: {}   |   USB host configured: {}".format(
        "yes" if bits & 4 else "no", "yes" if bits & 8 else "no", "cable in" if bits & 16 else "on battery", "yes" if bits & 32 else "no"))


def radio(keyboard):
    reply = keyboard.ask(0xF1, 1)
    print("radio status bytes : " + " ".join("{:02x}".format(b) for b in reply[2:8]))

    # bytes 4 and 5 of the status: the battery as a 10-bit ADC reading (see core/radio.h)
    raw = reply[2 + 4] | (reply[2 + 5] << 8)
    note = "  (saturated: the cable)" if raw >= 1021 else "  (ADC not settled yet)" if raw < 600 else ""
    if raw >= 1021:
        print("battery            : reading {}{}: the level cannot be read with the cable".format(raw, note))
    else:
        print("battery            : reading {} -> ~{}%{}".format(raw, battery_percent(raw), note))


# The same curve as board/battery.c: a Li-Po discharge curve over the readings of 3.30 ... 4.20 V (243 counts to the volt, from one measured point)
CURVE = [(800, 0), (850, 10), (875, 20), (899, 35), (923, 50), (948, 65), (972, 80), (996, 90), (1020, 100)]


def battery_percent(raw):
    if raw <= CURVE[0][0]:
        return 0
    for (r0, p0), (r1, p1) in zip(CURVE, CURVE[1:]):
        if raw <= r1:
            return p0 + (raw - r0) * (p1 - p0) // (r1 - r0)
    return 100

def blog(keyboard):
    """The battery log: records of (minutes awake, reading, flags). The minutes count the time the keyboard was awake since it was turned on: while it
    sleeps there is no clock and nothing is sampled, so a record marked WAKE has an unknown gap before it - the battery after a night is the reading of
    the first WAKE record after it, compared with the last record before it."""
    first = keyboard.ask(0xF1, 4, 0)
    if first[0] != 0xF1:
        print("battery log        : this build has no battery log (built with BATTERY_LOG=0)")
        return

    total = first[2]
    if total == 0:
        print("battery log        : empty (it fills up while the keyboard is awake and the radio answers)")
        return

    print("battery log        : {} records   (minutes awake since power-up | reading | ~percent | marks)".format(total))
    index = 0
    while index < total:
        reply = keyboard.ask(0xF1, 4, index)
        for at in range(3, len(reply) - 3, 4):
            if index >= total:
                break
            minutes = reply[at] | (reply[at + 1] << 8)
            flags = reply[at + 3]
            raw = reply[at + 2] | ((flags & 3) << 8)
            marks = []
            if flags & 0x08:
                marks.append("WAKE (or power-up): a gap before it")
            if flags & 0x04:
                marks.append("CABLE: the reading is saturated")
            percent = "   -" if flags & 0x04 else "{:3d}%".format(battery_percent(raw))
            print("  {:5d} min   {:4d}   {}   {}".format(minutes, raw, percent, "; ".join(marks)))
            index += 1


def profile(keyboard):
    """The profile of a LOOP_PROFILE build (core/profile.h). Times are in counts of 0.5 us; a step is wall-clock time, so it includes the interrupts that ran during it."""
    window = keyboard.ask(0xF1, 8)
    if window[0] != 0xF1:
        print("profile            : this build is not a LOOP_PROFILE build (make LOOP_PROFILE=1 fw)")
        return

    window_ms = int.from_bytes(window[2:6], "little")
    window_counts = max(1, window_ms * 2000)
    print("profile            : window of {:.1f} s since the last reset (times in microseconds)".format(window_ms / 1000.0))
    print("  {:22} {:>8} {:>8} {:>9} {:>9} {:>7}".format("step", "runs", "min", "mean", "max", "share"))

    for step in range(1, 20):
        r = keyboard.ask(0xF1, 5, step)
        count = int.from_bytes(r[3:5], "little")
        low = int.from_bytes(r[5:7], "little")
        high = int.from_bytes(r[7:9], "little")
        total = int.from_bytes(r[9:13], "little")
        if count == 0:
            continue
        print("  {:22} {:8d} {:8.1f} {:9.1f} {:9.1f} {:6.1f}%".format(STEPS.get(step, "step {}".format(step)), count, low / 2.0, total / 2.0 / count,
                                                                       high / 2.0, 100.0 * total / window_counts))

    r = keyboard.ask(0xF1, 6)
    bands = [int.from_bytes(r[2 + 2 * i:4 + 2 * i], "little") for i in range(8)]
    limits = ["< 0.2 ms", "< 0.4 ms", "< 0.8 ms", "< 1.6 ms", "< 3.2 ms", "< 6.4 ms", "< 12.8 ms", ">= 12.8 ms"]
    passes = sum(bands)
    print("  passes of the main loop: {} ({:.0f} a second)".format(passes, passes * 1000.0 / max(1, window_ms)))
    for name, n in zip(limits, bands):
        if n:
            print("    {:10} {:8d}  {:5.1f}%".format(name, n, 100.0 * n / max(1, passes)))

    isr = [int.from_bytes(r[18 + 4 * i:22 + 4 * i], "little") for i in range(3)]
    print("  CPU taken by the interrupts: timer {:.1f}%   USB {:.1f}%   serial (radio) {:.1f}%   (the serial one runs inside the timer's too)".format(
        *[100.0 * v / window_counts for v in isr]))


def daemon(keyboard, seconds, path):
    """The counters (and the radio, if asked) appended to a file every `seconds`, with the time: leave it running and read it afterwards."""
    import datetime

    with open(path, "a") as out:
        out.write("# counters, in the order this tool prints them\n")
        while True:
            values = counters(keyboard)
            line = ",".join(str(v) for v in values)
            out.write("{},{}\n".format(datetime.datetime.now().isoformat(timespec="seconds"), line))
            out.flush()
            time.sleep(seconds)


def main():
    args = sys.argv[1:]
    want_radio = "--radio" in args
    watch = None
    if "--watch" in args:
        watch = float(args[args.index("--watch") + 1])

    daemon_seconds = None
    if "--daemon" in args:
        at = args.index("--daemon")
        more = args[at + 1:]
        daemon_seconds = float(more[0]) if more and not more[0].startswith("--") else 60.0

    path = find_device()
    if not path:
        sys.exit("diag: no Oblio keyboard found (258a:dead, raw HID interface)")

    keyboard = Keyboard(path)

    if daemon_seconds is not None:
        out = [a for a in args[args.index("--daemon") + 1:] if not a.startswith("--")]
        out = out[1] if len(out) > 1 else "oblio-log.csv"
        print("logging every {:g} s to {} (Ctrl+C to stop)".format(daemon_seconds, out))
        daemon(keyboard, daemon_seconds, out)
        return

    if "--profile-reset" in args:
        keyboard.ask(0xF1, 7)
        print("profile            : started again")
        return

    values = counters(keyboard)
    print("counters:")
    show(values)
    if "--blog" in args:
        blog(keyboard)
    if "--profile" in args:
        profile(keyboard)
    if len(values) > 13:
        charger(values[13])
    if len(values) > 11:
        # each whole save erases the four sectors of one of the two slots, so a keymap sector has been erased about half as many times
        print("  keymap flash wear: about {} erase cycles per sector (flash of this kind is typically good for 10 000 to 100 000)".format(values[11] // 2))
    if want_radio:
        radio(keyboard)

    while watch:
        time.sleep(watch)
        new = counters(keyboard)
        if new != values:
            print("changed:")
            show(new, values)
            values = new
        if want_radio:
            radio(keyboard)


if __name__ == "__main__":
    main()
