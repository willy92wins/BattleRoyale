"""Generate the HUD arrow that points at the zone (an original texture, drawn here, no third-party art).

A notched arrowhead pointing up, with the proportions of the map's player arrow (BR_DrawPlayerArrow in MapMenu.c):
half height H, half width 0.75 H, notch 0.5 H behind the centre. White fill, no dark outline; the HUD draws its shadow.
Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA.
The _ca suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_zone_arrow.py <output_ca.paa>
"""
import os
import struct
import subprocess
import sys

SIZE = 128
SUBSAMPLES = 4
HALF_H = 56.0
HALF_W = HALF_H * 0.75
NOTCH = HALF_H * 0.5
FILL = (255, 255, 255, 255)
IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

CENTRE = SIZE / 2.0
POLYGON = (
    (CENTRE, CENTRE - HALF_H),             # tip
    (CENTRE + HALF_W, CENTRE + HALF_H),    # right tail
    (CENTRE, CENTRE + NOTCH),              # notch
    (CENTRE - HALF_W, CENTRE + HALF_H),    # left tail
)


def inside(x, y):
    result = False
    count = len(POLYGON)
    for i in range(count):
        x1, y1 = POLYGON[i]
        x2, y2 = POLYGON[(i + 1) % count]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            result = not result
    return result


def texel(col, row):
    hits = 0
    for sy in range(SUBSAMPLES):
        for sx in range(SUBSAMPLES):
            x = col + (sx + 0.5) / SUBSAMPLES
            y = row + (sy + 0.5) / SUBSAMPLES
            if inside(x, y):
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
