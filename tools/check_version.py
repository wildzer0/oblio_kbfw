#!/usr/bin/env python3
"""Checks that a firmware .hex carries its version exactly once, and that it is the one in the file VERSION.

usage: check_version.py [FILE.hex] [--tag vX.Y.Z]

The structure is the one of core/version.h: the eight bytes "OBLIOFW1", major, minor, patch, flags, the commit (eight bytes, seven characters and a zero),
dirty. The web updater (web/js/ihex.js) searches for it the same way. With --tag, the tag of a release has to say the same number as the image
(the release is refused otherwise: continuous integration runs this). Prints the version found.
"""

import re
import sys
from pathlib import Path

MAGIC = b"OBLIOFW1"


def read_hex(path):
    """The image of an Intel hex file as {address: byte}. Only the records the firmware's hex uses (data, end of file)."""
    image = {}
    for number, line in enumerate(Path(path).read_text().splitlines(), 1):
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        if sum(raw) & 0xFF:
            sys.exit("check_version: bad checksum on line {} of {}".format(number, path))
        length, address, kind = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if kind == 0:
            for i in range(length):
                image[address + i] = raw[4 + i]
        elif kind != 1:
            sys.exit("check_version: record type {} on line {} is not one this firmware uses".format(kind, number))
    return image


def version_of(path):
    """The version structure of an image, as a dict {version, rev, dirty, flags, at}; exits if the magic is not there exactly once."""
    image = read_hex(path)
    top = max(image) + 1
    flat = bytes(image.get(a, 0xFF) for a in range(top))  # a gap reads as erased flash

    found = [m.start() for m in re.finditer(re.escape(MAGIC), flat)]
    if len(found) != 1:
        sys.exit("check_version: found the magic {} times in {} (it must be exactly once)".format(len(found), path))

    at = found[0]
    return {
        "version": "{}.{}.{}".format(flat[at + 8], flat[at + 9], flat[at + 10]),
        "flags": flat[at + 11],
        "rev": flat[at + 12:at + 19].decode("ascii"),
        "dirty": flat[at + 20] != 0,
        "at": at,
    }


def main():
    args = sys.argv[1:]
    tag = None
    if "--tag" in args:
        at = args.index("--tag")
        tag = args[at + 1]
        del args[at:at + 2]
    path = args[0] if args else "build/oblio.hex"

    info = version_of(path)
    version = info["version"]

    wanted = Path("VERSION").read_text().strip()
    if version != wanted:
        sys.exit("check_version: the image says {} but VERSION says {}".format(version, wanted))
    if tag is not None and tag != "v" + version:
        sys.exit("check_version: the tag {} does not match the version {} of the image".format(tag, version))

    print("version {} (commit {}{}) flags {:#04x} at {:#06x}".format(version, info["rev"], ", uncommitted changes" if info["dirty"] else "", info["flags"], info["at"]))


if __name__ == "__main__":
    main()
