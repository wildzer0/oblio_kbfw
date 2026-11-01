#!/usr/bin/env python3
"""Checks that the Vial definition agrees with the firmware's own tables.

usage: check_vial.py VIAL_JSON KEYMAP_DEFAULT_C B68_KEYS_H MATRIX_H UNLOCK_KEYS

 - the keys drawn in the layout are exactly the cells of the default keymap (LAYOUT_B68), and the matrix size matches
 - every key held to unlock exists
 - the custom keycodes listed are as many as the B68_* codes, which are numbered from 0x7E00 with no gap
 - the definition compresses and decompresses to itself
"""

import json
import lzma
import re
import sys


def fail(message):
    print("check_vial: " + message, file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 6:
        print(__doc__, file=sys.stderr)
        return 1

    json_path, keymap_c, keys_h, matrix_h, unlock = sys.argv[1:]

    with open(json_path) as f:
        definition = json.load(f)

    # --- the matrix size
    matrix = open(matrix_h).read()
    rows = int(re.search(r"#define\s+MATRIX_ROWS\s+(\d+)", matrix).group(1))
    cols = int(re.search(r"#define\s+MATRIX_COLS\s+(\d+)", matrix).group(1))
    if definition["matrix"] != {"rows": rows, "cols": cols}:
        fail("matrix {} in the definition, {}x{} in the firmware".format(definition["matrix"], rows, cols))

    # --- the drawn keys: "row,col" labels; an 'e' on the last line of the label marks an encoder, not a matrix cell
    drawn = set()
    for line in definition["layouts"]["keymap"]:
        for item in line:
            if not isinstance(item, str):
                continue
            parts = item.split("\n")
            if parts[-1] == "e" and len(parts) > 1:
                continue
            row, col = (int(n) for n in parts[0].split(","))
            if (row, col) in drawn:
                fail("key {},{} is drawn twice".format(row, col))
            drawn.add((row, col))

    # --- the cells of the default keymap: the LAYOUT_B68 parameter names K<col>_<row>
    source = open(keymap_c).read()
    layout = re.search(r"#define\s+LAYOUT_B68\((.*?)\)\s*\{", source, re.S).group(1)
    cells = {(int(r), int(c)) for c, r in re.findall(r"K(\d\d)_(\d)", layout)}

    if drawn != cells:
        fail("layout and default keymap differ: only drawn {}, only in the keymap {}".format(sorted(drawn - cells), sorted(cells - drawn)))

    # --- the unlock keys
    for pair in unlock.split(";"):
        row, col = (int(n) for n in pair.split(","))
        if (row, col) not in cells:
            fail("unlock key {},{} is not a key of the board".format(row, col))

    # --- the custom keycodes
    codes = sorted(int(v, 16) for v in re.findall(r"#define\s+B68_\w+\s+(0x7E[0-9A-Fa-f]{2})u", open(keys_h).read()))
    if codes != list(range(0x7E00, 0x7E00 + len(codes))):
        fail("the B68_* codes are not contiguous from 0x7E00: {}".format([hex(c) for c in codes]))
    if len(definition["customKeycodes"]) != len(codes):
        fail("{} custom keycodes in the definition, {} B68_* codes".format(len(definition["customKeycodes"]), len(codes)))

    # --- the packed form
    packed = json.dumps(definition, separators=(",", ":")).encode()
    if json.loads(lzma.decompress(lzma.compress(packed))) != definition:
        fail("the definition does not survive compression")

    print("vial definition ok: {} keys, {} custom keycodes".format(len(cells), len(codes)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
