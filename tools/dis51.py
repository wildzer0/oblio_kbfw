#!/usr/bin/env python3
"""A small 8051 disassembler, for reading the stock firmware's dump (a raw binary; the dump itself is not in the repo).

usage: dis51.py FILE.bin START END          disassembles [START, END) (hex)
       dis51.py FILE.bin --xref ADDR        every LCALL / LJMP / ACALL / AJMP to ADDR, and the SJMP / conditional jumps that land on it

It knows the SH68F90A's special-function registers by name where they matter for reading the code. Not a general tool: no labels, no data
detection, the output is one instruction per line as `ADDR  BYTES  TEXT`.
"""

import sys

SFR = {
    0x80: "P0", 0x81: "SP", 0x82: "DPL", 0x83: "DPH", 0x87: "PCON", 0x88: "TCON", 0x89: "LPDSEL", 0x8C: "DRVCON", 0x8E: "?8E",
    0x90: "P1", 0x91: "USBCON", 0x92: "USBIF1", 0x93: "USBIF2", 0x94: "USBIE1", 0x95: "USBIE2", 0x96: "USBADDR", 0x98: "SCON", 0x99: "EP1CON",
    0x9A: "EP2CON", 0xA0: "P2", 0xA8: "IE", 0xA9: "IEN1", 0xB0: "P3", 0xB1: "?B1", 0xB2: "?B2", 0xB3: "LPDCON", 0xB8: "IP", 0xBA: "?BA",
    0xBC: "?BC", 0xC8: "T2CON", 0xCA: "RCAP2L", 0xCB: "RCAP2H", 0xCC: "TL2", 0xCD: "TH2", 0xD0: "PSW", 0xE0: "ACC", 0xE8: "?E8",
    0xF0: "B", 0xF2: "IB_CON1", 0xF3: "IB_CON2", 0xF4: "IB_CON3", 0xF5: "IB_CON4", 0xF6: "IB_CON5", 0xF7: "XPAGE", 0xFB: "IB_OFFSET", 0xFC: "IB_DATA",
    0xB6: "?B6", 0xB4: "?B4",
}


def dn(x):
    return SFR.get(x, "%02Xh" % x) if x >= 0x80 else "%02Xh" % x


def disassemble(data, pc):
    o = data[pc]
    b1 = data[pc + 1] if pc + 1 < len(data) else 0
    b2 = data[pc + 2] if pc + 2 < len(data) else 0
    lo, hi = o & 15, o >> 4

    def rel(n, x):
        return "%04X" % ((pc + n + (x - 256 if x > 127 else x)) & 0xFFFF)

    if lo == 1:
        a = ((o >> 5) << 8) | b1
        return ("ACALL" if o & 16 else "AJMP") + " %04X" % (((pc + 2) & 0xF800) | a), 2

    spec = {
        0x00: ("NOP", 1), 0x02: ("LJMP %04X" % ((b1 << 8) | b2), 3), 0x03: ("RR A", 1), 0x04: ("INC A", 1), 0x05: ("INC " + dn(b1), 2),
        0x10: ("JBC %02Xh,%s" % (b1, rel(3, b2)), 3), 0x12: ("LCALL %04X" % ((b1 << 8) | b2), 3), 0x13: ("RRC A", 1), 0x14: ("DEC A", 1),
        0x15: ("DEC " + dn(b1), 2), 0x20: ("JB %02Xh,%s" % (b1, rel(3, b2)), 3), 0x22: ("RET", 1), 0x23: ("RL A", 1),
        0x24: ("ADD A,#%02Xh" % b1, 2), 0x25: ("ADD A," + dn(b1), 2), 0x30: ("JNB %02Xh,%s" % (b1, rel(3, b2)), 3), 0x32: ("RETI", 1),
        0x33: ("RLC A", 1), 0x34: ("ADDC A,#%02Xh" % b1, 2), 0x35: ("ADDC A," + dn(b1), 2), 0x40: ("JC " + rel(2, b1), 2),
        0x42: ("ORL %s,A" % dn(b1), 2), 0x43: ("ORL %s,#%02Xh" % (dn(b1), b2), 3), 0x44: ("ORL A,#%02Xh" % b1, 2), 0x45: ("ORL A," + dn(b1), 2),
        0x50: ("JNC " + rel(2, b1), 2), 0x52: ("ANL %s,A" % dn(b1), 2), 0x53: ("ANL %s,#%02Xh" % (dn(b1), b2), 3),
        0x54: ("ANL A,#%02Xh" % b1, 2), 0x55: ("ANL A," + dn(b1), 2), 0x60: ("JZ " + rel(2, b1), 2), 0x62: ("XRL %s,A" % dn(b1), 2),
        0x63: ("XRL %s,#%02Xh" % (dn(b1), b2), 3), 0x64: ("XRL A,#%02Xh" % b1, 2), 0x65: ("XRL A," + dn(b1), 2), 0x70: ("JNZ " + rel(2, b1), 2),
        0x72: ("ORL C,%02Xh" % b1, 2), 0x73: ("JMP @A+DPTR", 1), 0x74: ("MOV A,#%02Xh" % b1, 2), 0x75: ("MOV %s,#%02Xh" % (dn(b1), b2), 3),
        0x80: ("SJMP " + rel(2, b1), 2), 0x82: ("ANL C,%02Xh" % b1, 2), 0x83: ("MOVC A,@A+PC", 1), 0x84: ("DIV AB", 1),
        0x85: ("MOV %s,%s" % (dn(b2), dn(b1)), 3), 0x90: ("MOV DPTR,#%04X" % ((b1 << 8) | b2), 3), 0x92: ("MOV %02Xh,C" % b1, 2),
        0x93: ("MOVC A,@A+DPTR", 1), 0x94: ("SUBB A,#%02Xh" % b1, 2), 0x95: ("SUBB A," + dn(b1), 2), 0xA0: ("ORL C,/%02Xh" % b1, 2),
        0xA2: ("MOV C,%02Xh" % b1, 2), 0xA3: ("INC DPTR", 1), 0xA4: ("MUL AB", 1), 0xA5: ("???", 1), 0xB0: ("ANL C,/%02Xh" % b1, 2),
        0xB2: ("CPL %02Xh" % b1, 2), 0xB3: ("CPL C", 1), 0xB4: ("CJNE A,#%02Xh,%s" % (b1, rel(3, b2)), 3),
        0xB5: ("CJNE A,%s,%s" % (dn(b1), rel(3, b2)), 3), 0xC0: ("PUSH " + dn(b1), 2), 0xC2: ("CLR %02Xh" % b1, 2), 0xC3: ("CLR C", 1),
        0xC4: ("SWAP A", 1), 0xC5: ("XCH A," + dn(b1), 2), 0xD0: ("POP " + dn(b1), 2), 0xD2: ("SETB %02Xh" % b1, 2), 0xD3: ("SETB C", 1),
        0xD4: ("DA A", 1), 0xD5: ("DJNZ %s,%s" % (dn(b1), rel(3, b2)), 3), 0xE0: ("MOVX A,@DPTR", 1), 0xE4: ("CLR A", 1),
        0xE5: ("MOV A," + dn(b1), 2), 0xF0: ("MOVX @DPTR,A", 1), 0xF4: ("CPL A", 1), 0xF5: ("MOV %s,A" % dn(b1), 2),
    }
    if o in spec:
        return spec[o]

    if lo in (6, 7):
        ind = "@R%d" % (o & 1)
        if hi == 0xE:
            return ("MOVX A," + ind, 1)
        if hi == 0xF:
            return ("MOVX " + ind + ",A", 1)
        if hi == 0xB:
            return ("CJNE %s,#%02Xh,%s" % (ind, b1, rel(3, b2)), 3)
        if hi == 0x7:
            return ("MOV %s,#%02Xh" % (ind, b1), 2)
        if hi == 0x8:
            return ("MOV %s,%s" % (dn(b1), ind), 2)
        if hi == 0xA:
            return ("MOV %s,%s" % (ind, dn(b1)), 2)
        names = {0: "INC %s", 1: "DEC %s", 2: "ADD A,%s", 3: "ADDC A,%s", 4: "ORL A,%s", 5: "ANL A,%s", 6: "XRL A,%s",
                 9: "SUBB A,%s", 0xC: "XCH A,%s", 0xD: "XCHD A,%s", 0xE: "MOV A,%s", 0xF: "MOV %s,A"}
        return (names[hi] % ind, 1) if hi in names else ("db %02Xh" % o, 1)

    if lo >= 8:
        r = "R%d" % (o & 7)
        if hi == 0x7:
            return ("MOV %s,#%02Xh" % (r, b1), 2)
        if hi == 0xB:
            return ("CJNE %s,#%02Xh,%s" % (r, b1, rel(3, b2)), 3)
        if hi == 0x8:
            return ("MOV %s,%s" % (dn(b1), r), 2)
        if hi == 0xA:
            return ("MOV %s,%s" % (r, dn(b1)), 2)
        if hi == 0xD:
            return ("DJNZ %s,%s" % (r, rel(2, b1)), 2)
        names = {0: "INC %s", 1: "DEC %s", 2: "ADD A,%s", 3: "ADDC A,%s", 4: "ORL A,%s", 5: "ANL A,%s", 6: "XRL A,%s",
                 9: "SUBB A,%s", 0xC: "XCH A,%s", 0xE: "MOV A,%s", 0xF: "MOV %s,A"}
        return (names[hi] % r, 1) if hi in names else ("db %02Xh" % o, 1)

    return ("db %02Xh" % o, 1)


def listing(data, start, end):
    pc = start
    while pc < end:
        text, n = disassemble(data, pc)
        yield pc, data[pc:pc + n].hex(), text
        pc += n


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)

    data = open(sys.argv[1], "rb").read()

    if sys.argv[2] == "--xref":
        target = int(sys.argv[3], 16)
        for pc, raw, text in listing(data, 0, 0xF000):
            word = text.split()
            if len(word) == 2 and word[0] in ("LCALL", "LJMP", "ACALL", "AJMP", "SJMP", "JZ", "JNZ", "JC", "JNC") and word[1] == "%04X" % target:
                print("%04X  %-8s %s" % (pc, raw, text))
        return

    for pc, raw, text in listing(data, int(sys.argv[2], 16), int(sys.argv[3], 16)):
        print("%04X  %-8s %s" % (pc, raw, text))


if __name__ == "__main__":
    main()
