"""Generate the map and minimap drop-plane icon: a top-down C-130 silhouette pointing up (an original texture, drawn
here, no third-party art).

Rounded nose fuselage, straight high wing with slight taper, four engine nacelles protruding ahead of the wing leading
edge, and horizontal tail stabilizer. White fill with a 3 px dark outline at 128x128 so it stays readable over both
light and dark map terrain; the widget applies any colour tint.
Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA.
The _ca suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_plane_icon.py <output_ca.paa>
"""
import math
import os
import struct
import subprocess
import sys

SIZE = 128
SUBSAMPLES = 4
OUTLINE_WIDTH = 3.0
FILL = (255, 255, 255)
OUTLINE = (20, 20, 20)
IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

CENTRE = SIZE / 2.0

# Fuselage: vertical chain of rounded capsules centred on x = 0 (nose up, towards smaller y)
NOSE_Y1, NOSE_R1 = 23.0, 5.8
NOSE_Y2, NOSE_R2 = 34.0, 7.2
BODY_Y1, BODY_Y2, BODY_R = 34.0, 76.0, 7.2
TAIL_Y1, TAIL_R1 = 76.0, 7.2
TAIL_Y2, TAIL_R2 = 108.0, 2.6

# Straight high wing with slight leading-edge sweep and trailing-edge taper (rounded by WING_ROUND)
WING_ROUND = 1.5
WING_POLY = (
    (-53.5, 50.0),
    (0.0, 47.0),
    (53.5, 50.0),
    (53.5, 57.5),
    (0.0, 64.0),
    (-53.5, 57.5),
)

# Four turboprop nacelles (mirrored across x = 0): (abs_x, y_front, y_back, radius)
NACELLES = (
    (20.5, 37.0, 54.0, 2.9),
    (36.5, 39.0, 54.5, 2.9),
)

# Horizontal stabilizer at the tail (rounded by TAIL_ROUND)
TAIL_ROUND = 1.5
TAIL_POLY = (
    (-21.5, 98.0),
    (0.0, 94.0),
    (21.5, 98.0),
    (21.5, 103.5),
    (0.0, 106.0),
    (-21.5, 103.5),
)


def sd_vertical_capsule(ax, y, y1, y2, radius):
    cy = max(y1, min(y2, y))
    return math.hypot(ax, y - cy) - radius


def sd_vertical_tapered_capsule(ax, y, y1, r1, y2, r2):
    h = y2 - y1
    ly = y - y1
    b = (r1 - r2) / h
    a = math.sqrt(1.0 - b * b)
    k = -b * ax + a * ly
    if k < 0.0:
        return math.hypot(ax, ly) - r1
    if k > a * h:
        return math.hypot(ax, ly - h) - r2
    return ax * a + ly * b - r1


def sd_polygon(px, py, verts):
    count = len(verts)
    min_d2 = float("inf")
    inside = False
    for i in range(count):
        ax, ay = verts[i]
        bx, by = verts[(i + 1) % count]
        ex = bx - ax
        ey = by - ay
        wx = px - ax
        wy = py - ay
        t = max(0.0, min(1.0, (wx * ex + wy * ey) / (ex * ex + ey * ey)))
        dx = wx - ex * t
        dy = wy - ey * t
        d2 = dx * dx + dy * dy
        if d2 < min_d2:
            min_d2 = d2
        if (ay > py) != (by > py) and px < ax + (py - ay) * (bx - ax) / (by - ay):
            inside = not inside
    d = math.sqrt(min_d2)
    return -d if inside else d


def plane_sdf(x, y):
    px = x - CENTRE
    ax = abs(px)

    d = sd_vertical_tapered_capsule(ax, y, NOSE_Y1, NOSE_R1, NOSE_Y2, NOSE_R2)
    d = min(d, sd_vertical_capsule(ax, y, BODY_Y1, BODY_Y2, BODY_R))
    d = min(d, sd_vertical_tapered_capsule(ax, y, TAIL_Y1, TAIL_R1, TAIL_Y2, TAIL_R2))
    d = min(d, sd_polygon(px, y, WING_POLY) - WING_ROUND)
    d = min(d, sd_polygon(px, y, TAIL_POLY) - TAIL_ROUND)

    for nx, ny1, ny2, nr in NACELLES:
        d = min(d, sd_vertical_capsule(ax - nx, y, ny1, ny2, nr))

    return d


def texel(col, row):
    fill_hits = 0
    outline_hits = 0
    for sy in range(SUBSAMPLES):
        y = row + (sy + 0.5) / SUBSAMPLES
        for sx in range(SUBSAMPLES):
            x = col + (sx + 0.5) / SUBSAMPLES
            d = plane_sdf(x, y)
            if d <= 0.0:
                fill_hits += 1
            elif d <= OUTLINE_WIDTH:
                outline_hits += 1
    hits = fill_hits + outline_hits
    if hits == 0:
        return (0, 0, 0, 0)
    alpha = int(round(hits * 255.0 / (SUBSAMPLES * SUBSAMPLES)))
    r = int(round((fill_hits * FILL[0] + outline_hits * OUTLINE[0]) / hits))
    g = int(round((fill_hits * FILL[1] + outline_hits * OUTLINE[1]) / hits))
    b = int(round((fill_hits * FILL[2] + outline_hits * OUTLINE[2]) / hits))
    return (r, g, b, alpha)


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
