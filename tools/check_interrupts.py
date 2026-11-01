#!/usr/bin/env python3
"""Checks two invariants over the linked firmware.

usage: check_interrupts.py FIRMWARE_HEX LINKER_MAP

1. The interrupt vectors are exactly the ones the firmware means to have. SDCC builds the vector table from the handlers it sees
   and fills the free slots below the highest one with `reti`; an unwanted handler, a missing one or a slot that is neither is
   a mistake (a source enabled without a handler would run whatever the linker put after the table).
2. SDCC's static overlay is empty. SDCC packs the locals and parameters of non-reentrant functions into one shared area, an analysis
   that ignores interrupts: an ISR could overwrite the variables of the main loop (it once hung the SMK port). The firmware is built
   with --stack-auto, which keeps locals on the stack; the check makes sure it stays that way.

Interrupt n has its vector at 0x0003 + 8 * n (the numbers are those of hal/sh68f90.h).
"""

import re
import sys

# vector number -> the handler that must be there
EXPECTED = {
    0: "_tick_isr",        # Timer 2: LED subframes and key scans
    1: "_power_int4_isr",  # INT4: wake-up from power-down
    7: "_usb_isr",         # USB
    13: "_radio_uart_isr", # EUART0: the radio link
}

LJMP = 0x02
RETI = 0x32


def fail(message):
    print("check_interrupts: " + message, file=sys.stderr)
    sys.exit(1)


def read_hex(path):
    memory = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            count = int(line[1:3], 16)
            address = int(line[3:7], 16)
            kind = int(line[7:9], 16)
            if kind != 0:
                continue
            for i in range(count):
                memory[address + i] = int(line[9 + 2 * i:11 + 2 * i], 16)
    return memory


def read_symbols(path):
    symbols = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"^\s*C:\s+([0-9A-Fa-f]{8})\s+(_\w+)", line)
            if m:
                symbols[m.group(2)] = int(m.group(1), 16)
    return symbols


def overlay_size(path):
    with open(path) as f:
        for line in f:
            m = re.match(r"^\s*C:\s+([0-9A-Fa-f]{8})\s+l_OSEG\b", line)
            if m:
                return int(m.group(1), 16)
    return 0


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 1

    memory = read_hex(sys.argv[1])
    symbols = read_symbols(sys.argv[2])
    highest = max(EXPECTED)

    for n in range(0, highest + 1):
        address = 3 + 8 * n
        opcode = memory.get(address)

        if n in EXPECTED:
            name = EXPECTED[n]
            if opcode != LJMP:
                fail("vector {} (0x{:04X}) should jump to {} but holds {}".format(n, address, name, "nothing" if opcode is None else hex(opcode)))
            target = (memory.get(address + 1, 0) << 8) | memory.get(address + 2, 0)
            if name not in symbols:
                fail("{} is not in the linker map".format(name))
            if target != symbols[name]:
                fail("vector {} jumps to 0x{:04X}, which is not {} (0x{:04X})".format(n, target, name, symbols[name]))
        elif opcode != RETI:
            fail("vector {} (0x{:04X}) is not claimed and should be a reti, but holds {}".format(n, address, "nothing" if opcode is None else hex(opcode)))

    size = overlay_size(sys.argv[2])
    if size:
        fail("the static overlay is {} bytes: functions the interrupts reach may share variables with the main loop".format(size))

    print("interrupt vectors ok ({} handlers), no static overlay".format(len(EXPECTED)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
