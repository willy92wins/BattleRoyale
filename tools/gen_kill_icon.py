"""Generate the HUD kill counter icon: a skull (an original texture, drawn here, no third-party art).

A bold white skull silhouette with no dark outline (the HUD draws its shadow): round cranium, narrower jaw with rounded
bottom corners, two large transparent eye sockets, a transparent nose cavity, and two vertical tooth gaps in the jaw.
Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA; the _ca
suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_kill_icon.py <output_ca.paa>
"""
import os
import struct
import subprocess
import sys

SIZE = 128
SUBSAMPLES = 4
FILL = (255, 255, 255, 255)
IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

CENTRE_X = SIZE / 2.0

# Cranium: ellipse centred in the upper half
CRANIUM_CY = 50.0
CRANIUM_RX = 46.0
CRANIUM_RY = 40.0

# Jaw: narrower rectangle with rounded bottom corners
JAW_LEFT = 36.0
JAW_RIGHT = 92.0
JAW_TOP = 76.0
JAW_BOTTOM = 118.0
JAW_RADIUS = 10.0

# Eye sockets: symmetric ellipses cut out of the cranium
EYE_DX = 19.0
EYE_CY = 56.0
EYE_RX = 13.5
EYE_RY = 14.5

# Nose cavity: upward-pointing triangle cut out below the eye bridge
NOSE = (
    (CENTRE_X, 71.0),
    (CENTRE_X - 8.5, 89.0),
    (CENTRE_X + 8.5, 89.0),
)

# Tooth gaps: two vertical slits with rounded tops cutting up from the bottom of the jaw
TOOTH_GAP_X = (CENTRE_X - 10.0, CENTRE_X + 10.0)
TOOTH_GAP_HALF = 3.5
TOOTH_GAP_TOP = 99.0


def in_ellipse(x, y, cx, cy, rx, ry):
    dx = (x - cx) / rx
    dy = (y - cy) / ry
    return dx * dx + dy * dy <= 1.0


def in_polygon(x, y, polygon):
    result = False
    count = len(polygon)
    for i in range(count):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % count]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            result = not result
    return result


def in_jaw(x, y):
    if x < JAW_LEFT or x > JAW_RIGHT or y < JAW_TOP or y > JAW_BOTTOM:
        return False
    corner_y = JAW_BOTTOM - JAW_RADIUS
    if y > corner_y:
        left_x = JAW_LEFT + JAW_RADIUS
        right_x = JAW_RIGHT - JAW_RADIUS
        if x < left_x:
            dx = x - left_x
            dy = y - corner_y
            return dx * dx + dy * dy <= JAW_RADIUS * JAW_RADIUS
        if x > right_x:
            dx = x - right_x
            dy = y - corner_y
            return dx * dx + dy * dy <= JAW_RADIUS * JAW_RADIUS
    return True


def in_tooth_gap(x, y):
    if y < TOOTH_GAP_TOP or y > JAW_BOTTOM:
        return False
    cap_y = TOOTH_GAP_TOP + TOOTH_GAP_HALF
    for cx in TOOTH_GAP_X:
        dx = x - cx
        if abs(dx) <= TOOTH_GAP_HALF:
            if y >= cap_y:
                return True
            dy = y - cap_y
            if dx * dx + dy * dy <= TOOTH_GAP_HALF * TOOTH_GAP_HALF:
                return True
    return False


def in_skull(x, y):
    if not (in_ellipse(x, y, CENTRE_X, CRANIUM_CY, CRANIUM_RX, CRANIUM_RY) or in_jaw(x, y)):
        return False
    if in_ellipse(x, y, CENTRE_X - EYE_DX, EYE_CY, EYE_RX, EYE_RY):
        return False
    if in_ellipse(x, y, CENTRE_X + EYE_DX, EYE_CY, EYE_RX, EYE_RY):
        return False
    if in_polygon(x, y, NOSE):
        return False
    if in_tooth_gap(x, y):
        return False
    return True


def texel(col, row):
    hits = 0
    for sy in range(SUBSAMPLES):
        for sx in range(SUBSAMPLES):
            x = col + (sx + 0.5) / SUBSAMPLES
            y = row + (sy + 0.5) / SUBSAMPLES
            if in_skull(x, y):
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
