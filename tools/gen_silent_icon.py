"""Generate the HUD icon of silent mode: a speaker crossed out (an original texture, drawn here, no third-party art).

Same look as the zone arrow (tools/gen_zone_arrow.py): white fill with a dark outline, so it reads on bright and dark
ground. A speaker (body and cone) with a slash across the whole icon, the slash drawn over the speaker with its own dark
edge. Rendered at 128x128 with 4x4 supersampling into a 32-bit TGA, then converted with DayZ Tools' ImageToPAA; the _ca
suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_silent_icon.py <output_ca.paa>
"""
import os
import struct
import subprocess
import sys

SIZE = 128
SUBSAMPLES = 4
OUTLINE_IN = 7.0      # outline width inside the speaker, in texels
OUTLINE_OUT = 1.5     # outline width outside the speaker, in texels
SLASH_HALF = 6.0      # half width of the slash fill
SLASH_EDGE = 6.0      # dark edge around the slash
FILL = (255, 255, 255, 255)
BORDER = (20, 20, 20, 235)
IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

SPEAKER = (            # body then cone, clockwise from the top left of the body
    (22.0, 48.0), (44.0, 48.0), (76.0, 22.0), (76.0, 106.0), (44.0, 80.0), (22.0, 80.0),
)
SLASH = ((18.0, 16.0), (110.0, 112.0))


def inside(x, y, polygon):
    result = False
    count = len(polygon)
    for i in range(count):
        x1, y1 = polygon[i]
        x2, y2 = polygon[(i + 1) % count]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            result = not result
    return result


def segment_distance(x, y, a, b):
    (x1, y1), (x2, y2) = a, b
    dx, dy = x2 - x1, y2 - y1
    t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
    px, py = x1 + t * dx - x, y1 + t * dy - y
    return (px * px + py * py) ** 0.5


def edge_distance(x, y, polygon):
    count = len(polygon)
    return min(segment_distance(x, y, polygon[i], polygon[(i + 1) % count]) for i in range(count))


def colour_at(x, y):
    colour = None
    distance = edge_distance(x, y, SPEAKER)
    if inside(x, y, SPEAKER):
        colour = BORDER if distance < OUTLINE_IN else FILL
    elif distance < OUTLINE_OUT:
        colour = BORDER
    slash = segment_distance(x, y, SLASH[0], SLASH[1])
    if slash < SLASH_HALF:
        colour = FILL
    elif slash < SLASH_HALF + SLASH_EDGE:
        colour = BORDER
    return colour


def texel(col, row):
    # Premultiplied average of the subsamples
    acc = [0.0, 0.0, 0.0, 0.0]
    for sy in range(SUBSAMPLES):
        for sx in range(SUBSAMPLES):
            colour = colour_at(col + (sx + 0.5) / SUBSAMPLES, row + (sy + 0.5) / SUBSAMPLES)
            if colour is None:
                continue
            alpha = colour[3] / 255.0
            acc[0] += colour[0] * alpha
            acc[1] += colour[1] * alpha
            acc[2] += colour[2] * alpha
            acc[3] += alpha
    alpha = acc[3] / (SUBSAMPLES * SUBSAMPLES)
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
