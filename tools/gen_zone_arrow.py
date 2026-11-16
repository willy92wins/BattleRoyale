"""Generate the HUD arrow that points at the zone (an original texture, drawn here, no third-party art).

A notched arrowhead pointing up, with the proportions of the map's player arrow (BR_DrawPlayerArrow in MapMenu.c):
half height H, half width 0.75 H, notch 0.5 H behind the centre. White fill with a dark outline, so it reads on bright and
dark ground. Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA.
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
OUTLINE_IN = 7.0     # outline width inside the shape, in texels
OUTLINE_OUT = 1.5    # outline width outside the shape, in texels
FILL = (255, 255, 255, 255)
BORDER = (20, 20, 20, 235)
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


def edge_distance(x, y):
    best = 1e9
    count = len(POLYGON)
    for i in range(count):
        x1, y1 = POLYGON[i]
        x2, y2 = POLYGON[(i + 1) % count]
        dx, dy = x2 - x1, y2 - y1
        t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
        px, py = x1 + t * dx - x, y1 + t * dy - y
        best = min(best, (px * px + py * py) ** 0.5)
    return best


def texel(col, row):
    # Premultiplied average of the subsamples
    acc = [0.0, 0.0, 0.0, 0.0]
    for sy in range(SUBSAMPLES):
        for sx in range(SUBSAMPLES):
            x = col + (sx + 0.5) / SUBSAMPLES
            y = row + (sy + 0.5) / SUBSAMPLES
            distance = edge_distance(x, y)
            if inside(x, y):
                colour = BORDER if distance < OUTLINE_IN else FILL
            elif distance < OUTLINE_OUT:
                colour = BORDER
            else:
                continue
            alpha = colour[3] / 255.0
            acc[0] += colour[0] * alpha
            acc[1] += colour[1] * alpha
            acc[2] += colour[2] * alpha
            acc[3] += alpha
    n = SUBSAMPLES * SUBSAMPLES
    alpha = acc[3] / n
    if alpha <= 0:
        return (0, 0, 0, 0)
    return tuple(int(round(acc[i] / acc[3])) for i in range(3)) + (int(round(alpha * 255)),)


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
