"""Generate the HUD icon of the zone radius: a ring with its centre dot and a radius line (an original texture, drawn
here, no third-party art).

White fill, no dark outline; the HUD draws its shadow, as for the zone arrow (tools/gen_zone_arrow.py).
Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA.
The _ca suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_zone_radius_icon.py <output_ca.paa>
"""
import math
import os
import struct
import subprocess
import sys

SIZE = 128
SUBSAMPLES = 4
RING_RADIUS = 54.0     # centre line of the ring
RING_HALF = 6.0        # half width of the ring
DOT_RADIUS = 10.0      # centre dot
LINE_HALF = 4.0        # half width of the radius line, from the centre to the ring, to the right
LINE_ANGLE = 0.0
FILL = (255, 255, 255, 255)
IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

CENTRE = SIZE / 2.0
LINE_END = (CENTRE + RING_RADIUS * math.cos(LINE_ANGLE), CENTRE + RING_RADIUS * math.sin(LINE_ANGLE))


def segment_distance(x, y, ax, ay, bx, by):
    dx = bx - ax
    dy = by - ay
    t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - (ax + t * dx), y - (ay + t * dy))


def inside(x, y):
    d = math.hypot(x - CENTRE, y - CENTRE)
    if abs(d - RING_RADIUS) <= RING_HALF or d <= DOT_RADIUS:
        return True
    return segment_distance(x, y, CENTRE, CENTRE, LINE_END[0], LINE_END[1]) <= LINE_HALF


def texel(col, row):
    hits = 0
    for sy in range(SUBSAMPLES):
        for sx in range(SUBSAMPLES):
            if inside(col + (sx + 0.5) / SUBSAMPLES, row + (sy + 0.5) / SUBSAMPLES):
                hits += 1
    if hits == 0:
        return (0, 0, 0, 0)
    alpha = int(round(hits * 255.0 / (SUBSAMPLES * SUBSAMPLES)))
    return (FILL[0], FILL[1], FILL[2], alpha)


def main():
    out_paa = os.path.abspath(sys.argv[1])
    tga = os.path.splitext(out_paa)[0] + ".tga"
    os.makedirs(os.path.dirname(out_paa), exist_ok=True)
    pixels = bytearray()
    for row in range(SIZE):
        for col in range(SIZE):
            r, g, b, a = texel(col, row)
            pixels += bytes((b, g, r, a))
    # Uncompressed true colour, 32 bpp, 8 alpha bits, top-left origin
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 32, 0x28)
    with open(tga, "wb") as handle:
        handle.write(header + bytes(pixels))
    subprocess.run([IMAGE_TO_PAA, tga, out_paa], check=True)
    os.remove(tga)
    print("wrote", out_paa, os.path.getsize(out_paa), "bytes")


if __name__ == "__main__":
    main()
