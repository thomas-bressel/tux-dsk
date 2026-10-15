#!/usr/bin/env python3
"""The discs the golden .HFE files are made of: discs.py <folder>.

hfe_data.dsk   three tracks as AMSDOS formats them in its DATA format
hfe_odd.dsk    what else a .DSK file may hold: two sides, ten sectors a
               track, sectors of other sizes, data marked as deleted, a CRC
               that is wrong in an ID and one in the data, a track
               formatted with no sector"""

import sys
from pathlib import Path


def track(number, side, sectors, gap3, extended):
    """A track's bytes. A sector is (C, H, R, N, ST1, ST2, data)."""
    header = bytearray(256)
    header[0:12] = b'Track-Info\r\n'
    header[0x10] = number
    header[0x11] = side
    header[0x14] = sectors[0][3] if sectors else 0
    header[0x15] = len(sectors)
    header[0x16] = gap3
    header[0x17] = 0xE5
    body = bytearray()
    for place, (c, h, r, n, st1, st2, data) in enumerate(sectors):
        at = 0x18 + place * 8
        header[at:at + 6] = bytes([c, h, r, n, st1, st2])
        if extended:
            header[at + 6] = len(data) & 0xFF
            header[at + 7] = len(data) >> 8
        body += data
    block = bytes(header) + bytes(body)
    return block + bytes(-len(block) % 256)


def pattern(size, step, start):
    return bytes((start + i * step) & 0xFF for i in range(size))


def data_disc():
    header = bytearray(256)
    header[0:34] = b'MV - CPCEMU Disk-File\r\nDisk-Info\r\n'
    header[0x30] = 3
    header[0x31] = 1
    header[0x32] = 0x00
    header[0x33] = 0x13
    tracks = b''
    for number in range(3):
        sectors = [(number, 0, r, 2, 0, 0, pattern(512, 3 + number, r)) for r in (0xC1, 0xC6, 0xC2, 0xC7, 0xC3, 0xC8, 0xC4, 0xC9, 0xC5)]
        tracks += track(number, 0, sectors, 0x4E, False)
    return bytes(header) + tracks


def odd_disc():
    blocks = []
    # Track 0, side 0: nine sectors numbered from &41, with the gap a CPC formats.
    blocks.append(track(0, 0, [(0, 0, 0x41 + i, 2, 0, 0, pattern(512, 5, i)) for i in range(9)], 0x52, True))
    # Track 0, side 1: ten sectors, with the small gap they need.
    blocks.append(track(0, 1, [(0, 1, 1 + i, 2, 0, 0, pattern(512, 7, i)) for i in range(10)], 0x18, True))
    # Track 1, side 0: other sizes, data marked as deleted, and CRCs that are wrong.
    blocks.append(track(1, 0, [
        (1, 0, 0x01, 1, 0x00, 0x00, pattern(256, 1, 0x10)),
        (1, 0, 0x02, 3, 0x00, 0x00, pattern(1024, 9, 0x20)),
        (1, 0, 0x03, 2, 0x00, 0x40, pattern(512, 11, 0x30)),
        (1, 0, 0x04, 2, 0x20, 0x20, pattern(512, 13, 0x40)),
        (1, 0, 0x05, 2, 0x20, 0x00, pattern(512, 15, 0x50)),
        (1, 0, 0x06, 0, 0x00, 0x00, pattern(128, 17, 0x60)),
    ], 0x30, True))
    # Track 1, side 1: formatted, with no sector.
    blocks.append(track(1, 1, [], 0x4E, True))
    header = bytearray(256)
    header[0:34] = b'EXTENDED CPC DSK File\r\nDisk-Info\r\n'
    header[0x22:0x28] = b'TuxDSK'
    header[0x30] = 2
    header[0x31] = 2
    for index, block in enumerate(blocks):
        header[0x34 + index] = len(block) // 256
    return bytes(header) + b''.join(blocks)


if __name__ == '__main__':
    folder = Path(sys.argv[1])
    folder.mkdir(parents=True, exist_ok=True)
    (folder / 'hfe_data.dsk').write_bytes(data_disc())
    (folder / 'hfe_odd.dsk').write_bytes(odd_disc())
