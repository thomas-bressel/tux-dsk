#!/usr/bin/env python3
"""The files the golden discs are made of: inputs.py <folder>.

They are written to <folder>; those of tests/golden/inputs were made by this
script. The twelve big ones (FILL00.DAT...) are not kept there: the tests make
them again by the same rule."""

import struct
import sys
from pathlib import Path


def header(name, extension, kind, load, entry, length):
    """An AMSDOS header, with rubbish in the bytes AMSDOS does not use."""
    h = bytearray(128)
    h[1:9] = name.ljust(8).encode()
    h[9:12] = extension.ljust(3).encode()
    h[0x12] = kind
    struct.pack_into('<H', h, 0x15, load)
    struct.pack_into('<H', h, 0x18, length)
    struct.pack_into('<H', h, 0x1A, entry)
    struct.pack_into('<H', h, 0x40, length)
    for at in range(0x1C, 0x40):
        h[at] = 0xAA
    for at in range(0x45, 0x80):
        h[at] = 0xBB
    struct.pack_into('<H', h, 0x43, sum(h[:67]) & 0xFFFF)
    return bytes(h)


def small(folder):
    files = {
        'hello.bas': b'10 PRINT "HELLO"\r\n',
        'code.bin': bytes(i * 7 & 255 for i in range(300)),
        'withhdr.bin': header('WITHHDR', 'BIN', 2, 0x8000, 0x8010, 300) + bytes(i * 3 & 255 for i in range(300)),
        'k17.dat': bytes(i * 13 & 255 for i in range(17000)),
    }
    # Names, for what becomes of them in a directory.
    for name in ('my.file.name.txt', 'lowercase.bas', 'verylongfilename.binary', 'a#b$c&.x1', 'noext', 'UPPER.BIN',
                 'd1g1t5.b4s'):
        files[name] = b'x' * 10
    for name, content in files.items():
        (folder / name).write_bytes(content)


def big(folder):
    for index in range(12):
        (folder / f'fill{index:02}.dat').write_bytes(bytes([65 + index]) * 16384)


if __name__ == '__main__':
    target = Path(sys.argv[1])
    target.mkdir(parents=True, exist_ok=True)
    small(target)
    if len(sys.argv) > 2 and sys.argv[2] == 'big':
        big(target)
