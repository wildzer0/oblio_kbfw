#!/usr/bin/env python3
"""Flash/XDATA budget check from the SDCC linker .mem report.

usage: budget.py <file.mem> <code_limit> <xram_size>
Prints "flash used/limit, XDATA used/size"; exits 1 if either is over budget.
"""
import sys


def used(mem, label):
    # rows look like: "   EXTERNAL RAM                           0     4096"
    # or:             "   ROM/EPROM/FLASH  0x0000   0x00ad     174    44032"
    for line in mem.splitlines():
        if line.strip().startswith(label):
            fields = line.split()
            return int(fields[-2])
    sys.exit(f"budget: '{label}' row not found in the .mem file")


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    mem = open(sys.argv[1]).read()
    code_limit = int(sys.argv[2], 0)
    xram_size = int(sys.argv[3], 0)
    flash = used(mem, "ROM/EPROM/FLASH")
    xdata = used(mem, "EXTERNAL RAM")
    print(f"flash {flash}/{code_limit}, XDATA {xdata}/{xram_size}")
    if flash > code_limit or xdata > xram_size:
        sys.exit("budget exceeded")


main()
