#!/usr/bin/env python3
"""Original white alpha masks for editor 2D primitives; standard library only."""
from pathlib import Path
import struct
import zlib


def write(path, width, height, capsule):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            dx = x + .5 - width / 2
            dy = abs(y + .5 - height / 2)
            if capsule:
                dy = max(0, dy - 64)
            alpha = 255 if dx * dx + dy * dy <= (width / 2) ** 2 else 0
            rows.extend((255, 255, 255, alpha))
    def chunk(name, data):
        return struct.pack('!I', len(data)) + name + data + struct.pack('!I', zlib.crc32(name + data))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' +
                     chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


if __name__ == '__main__':
    root = Path(__file__).resolve().parents[2] / 'docs/judasjs/examples/2d'
    write(root / 'circle.png', 64, 64, False)
    write(root / 'capsule.png', 96, 224, True)
