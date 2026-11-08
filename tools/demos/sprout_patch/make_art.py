#!/usr/bin/env python3
"""Generate Sprout Patch's original 32x32 farm tile sheet (deterministic).

Usage: python3 make_art.py OUTPUT.png

Frames are sliced by the editor serializer as cell-<row>-<col>:
  row 0: grass, flowered grass, path, fence (solid)
  row 1: pond water (solid), dry tilled soil, wet tilled soil, seeds
  row 2: sprout, young plant, ripe turnip, target cursor
  row 3: turnip, seed bag, watering can, hoe (spare icons)
"""
import random
import sys

from PIL import Image, ImageDraw

T = 32
rng = random.Random(7)
sheet = Image.new("RGBA", (T * 4, T * 4), (0, 0, 0, 0))


def tile(row, col):
    image = Image.new("RGBA", (T, T), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image), (col * T, row * T)


def speckle(draw, colours, count, area=(0, 0, T, T)):
    for _ in range(count):
        x = rng.randrange(area[0], area[2])
        y = rng.randrange(area[1], area[3])
        draw.point((x, y), fill=rng.choice(colours))


def grass(draw, flowers=False):
    draw.rectangle((0, 0, T - 1, T - 1), fill=(86, 150, 72, 255))
    speckle(draw, [(72, 132, 60, 255), (104, 168, 84, 255)], 90)
    for _ in range(10):
        x, y = rng.randrange(2, T - 2), rng.randrange(3, T - 1)
        draw.line((x, y, x + rng.choice((-1, 0, 1)), y - 2), fill=(58, 116, 52, 255))
    if flowers:
        for _ in range(5):
            x, y = rng.randrange(3, T - 3), rng.randrange(3, T - 3)
            colour = rng.choice([(242, 228, 120, 255), (240, 160, 190, 255), (250, 250, 250, 255)])
            draw.point([(x, y), (x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)], fill=colour)
            draw.point((x, y), fill=(230, 140, 40, 255))


def soil(draw, wet):
    base = (92, 62, 40, 255) if wet else (150, 108, 70, 255)
    ridge = (70, 46, 30, 255) if wet else (122, 86, 54, 255)
    light = (112, 78, 52, 255) if wet else (176, 134, 92, 255)
    draw.rectangle((1, 1, T - 2, T - 2), fill=base)
    for y in range(4, T - 2, 7):
        draw.line((2, y, T - 3, y), fill=ridge)
        draw.line((2, y + 1, T - 3, y + 1), fill=light)
    speckle(draw, [ridge, light], 40, (2, 2, T - 2, T - 2))
    if wet:
        for _ in range(4):
            x, y = rng.randrange(4, T - 6), rng.randrange(4, T - 4)
            draw.line((x, y, x + 3, y), fill=(120, 150, 180, 255))
    draw.rectangle((0, 0, T - 1, T - 1), outline=(64, 42, 28, 255))


def leaf(draw, cx, cy, dx, dy, colour):
    draw.polygon([(cx, cy), (cx + dx, cy + dy - 2), (cx + dx * 1.4, cy + dy), (cx + dx * .5, cy + dy + 1)], fill=colour)


# Row 0.
image, draw, at = tile(0, 0); grass(draw); sheet.paste(image, at)
image, draw, at = tile(0, 1); grass(draw, True); sheet.paste(image, at)
image, draw, at = tile(0, 2)
draw.rectangle((0, 0, T - 1, T - 1), fill=(196, 168, 118, 255))
speckle(draw, [(176, 148, 100, 255), (214, 190, 140, 255), (160, 132, 90, 255)], 120)
sheet.paste(image, at)
image, draw, at = tile(0, 3)
grass(draw)
for x in (5, 26):
    draw.rectangle((x - 2, 4, x + 2, 29), fill=(124, 82, 46, 255), outline=(78, 50, 28, 255))
for y in (9, 19):
    draw.rectangle((0, y, T - 1, y + 3), fill=(158, 110, 64, 255), outline=(92, 60, 34, 255))
sheet.paste(image, at)

# Row 1.
image, draw, at = tile(1, 0)
draw.rectangle((0, 0, T - 1, T - 1), fill=(52, 120, 176, 255))
speckle(draw, [(64, 136, 192, 255), (44, 104, 160, 255)], 70)
for _ in range(5):
    x, y = rng.randrange(2, T - 9), rng.randrange(3, T - 3)
    draw.arc((x, y, x + 7, y + 3), 200, 340, fill=(170, 214, 240, 255))
sheet.paste(image, at)
image, draw, at = tile(1, 1); soil(draw, False); sheet.paste(image, at)
image, draw, at = tile(1, 2); soil(draw, True); sheet.paste(image, at)
image, draw, at = tile(1, 3)
for x, y in ((10, 12), (20, 10), (15, 20), (23, 21), (9, 23)):
    draw.ellipse((x - 2, y - 1, x + 2, y + 2), fill=(232, 210, 150, 255), outline=(120, 92, 52, 255))
sheet.paste(image, at)

# Row 2: growth stages share a stem base at the cell's lower centre.
image, draw, at = tile(2, 0)
draw.line((16, 26, 16, 18), fill=(70, 150, 60, 255), width=2)
leaf(draw, 16, 19, -6, -4, (110, 200, 90, 255)); leaf(draw, 16, 19, 6, -4, (110, 200, 90, 255))
sheet.paste(image, at)
image, draw, at = tile(2, 1)
draw.line((16, 27, 16, 12), fill=(60, 140, 54, 255), width=2)
for dy, size in ((-2, 9), (-8, 8), (-13, 6)):
    leaf(draw, 16, 26 + dy, -size, -5, (84, 180, 72, 255)); leaf(draw, 16, 26 + dy, size, -5, (100, 196, 84, 255))
sheet.paste(image, at)
image, draw, at = tile(2, 2)
for dx in (-7, -3, 2, 6):
    draw.polygon([(16, 18), (16 + dx - 3, 3), (16 + dx + 3, 5)], fill=(70, 170, 66, 255), outline=(44, 120, 44, 255))
draw.ellipse((8, 15, 24, 29), fill=(236, 232, 226, 255), outline=(150, 120, 140, 255))
draw.pieslice((8, 15, 24, 29), 180, 360, fill=(170, 70, 150, 255))
draw.line((16, 29, 16, 31), fill=(200, 190, 170, 255))
sheet.paste(image, at)
image, draw, at = tile(2, 3)
for inset, alpha in ((0, 255), (1, 160)):
    draw.rectangle((inset, inset, T - 1 - inset, T - 1 - inset), outline=(255, 248, 200, alpha))
for x, y in ((0, 0), (T - 6, 0), (0, T - 6), (T - 6, T - 6)):
    draw.rectangle((x, y, x + 5, y + 5), outline=(255, 255, 255, 255))
sheet.paste(image, at)

# Row 3: spare icons.
image, draw, at = tile(3, 0)
draw.ellipse((6, 10, 26, 30), fill=(236, 232, 226, 255), outline=(150, 120, 140, 255))
draw.pieslice((6, 10, 26, 30), 180, 360, fill=(170, 70, 150, 255))
draw.polygon([(16, 12), (10, 1), (14, 2)], fill=(70, 170, 66, 255)); draw.polygon([(16, 12), (22, 1), (18, 2)], fill=(70, 170, 66, 255))
sheet.paste(image, at)
image, draw, at = tile(3, 1)
draw.polygon([(8, 10), (24, 10), (27, 29), (5, 29)], fill=(214, 186, 130, 255), outline=(120, 92, 52, 255))
draw.rectangle((10, 6, 22, 10), fill=(190, 160, 104, 255), outline=(120, 92, 52, 255))
draw.ellipse((12, 16, 20, 24), fill=(110, 200, 90, 255))
sheet.paste(image, at)
image, draw, at = tile(3, 2)
draw.rectangle((6, 12, 22, 28), fill=(120, 160, 190, 255), outline=(60, 84, 110, 255))
draw.line((22, 16, 29, 9), fill=(100, 140, 170, 255), width=3)
draw.arc((8, 4, 20, 18), 180, 360, fill=(60, 84, 110, 255), width=2)
sheet.paste(image, at)
image, draw, at = tile(3, 3)
draw.line((8, 28, 22, 8), fill=(150, 100, 56, 255), width=3)
draw.polygon([(18, 4), (29, 8), (27, 13), (20, 10)], fill=(170, 176, 184, 255), outline=(90, 96, 104, 255))
sheet.paste(image, at)

if len(sys.argv) != 2:
    raise SystemExit(__doc__)
sheet.save(sys.argv[1])
