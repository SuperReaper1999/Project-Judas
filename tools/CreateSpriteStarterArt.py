#!/usr/bin/env python3
"""Reproduce the original Stage 1 explorer pixel art (no external assets)."""
import struct,zlib
from pathlib import Path
w,h=32,40
pixels=[[(0,0,0,0) for x in range(w)] for y in range(h)]
def box(x0,y0,x1,y1,c):
 for y in range(y0,y1):
  for x in range(x0,x1):pixels[y][x]=c
# Original hand-authored pixel explorer. Transparent border is intentional.
box(9,6,23,19,(37,43,63,255));box(11,8,21,18,(237,185,132,255))
box(8,5,24,10,(217,145,69,255));box(5,9,27,12,(217,145,69,255))
box(8,19,24,30,(58,145,145,255));box(11,19,21,24,(89,184,166,255))
box(5,21,8,29,(237,185,132,255));box(24,21,27,29,(237,185,132,255))
box(9,30,15,35,(48,59,86,255));box(17,30,23,35,(48,59,86,255))
box(7,35,15,37,(29,34,51,255));box(17,35,25,37,(29,34,51,255))
box(13,13,15,15,(37,43,63,255));box(19,13,21,15,(37,43,63,255))
def chunk(n,d):return struct.pack('>I',len(d))+n+d+struct.pack('>I',zlib.crc32(n+d)&0xffffffff)
raw=b''.join(b'\0'+bytes(v for c in row for v in c) for row in pixels)
(Path(__file__).resolve().parent.parent/'docs/judasjs/examples/2d/explorer.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
