"""Generate the textures of the leaderboard panel (original textures, drawn here, no third-party art).

- br_lb_fade_v_ca.paa: white, opaque at the top and transparent at the bottom (the glow of the podium cards).
- br_lb_fade_h_ca.paa: white, opaque on the left and transparent on the right (the sheen of the title band).
- br_lb_crown_ca.paa: a crown, white with a dark outline like the other HUD icons (first place).

The layout tints them with `color`. Each one is rendered into a 32-bit TGA and converted with DayZ Tools'
ImageToPAA; the _ca suffix makes ImageToPAA keep the full alpha channel.

Usage: python -I tools/gen_leaderboard_textures.py <output_folder>
"""
import os
import struct
import subprocess
import sys

IMAGE_TO_PAA = "C:/Program Files (x86)/Steam/steamapps/common/DayZ Tools/Bin/ImageToPAA/ImageToPAA.exe"

FADE_LONG = 64        # texels along the fade
FADE_SHORT = 16       # texels across it
FADE_POWER = 1.6      # >1: the fade falls quickly at first and has a long faint tail

CROWN_SIZE = 128
SUBSAMPLES = 4
OUTLINE_IN = 6.0      # outline width inside the crown, in texels
OUTLINE_OUT = 1.5     # outline width outside the crown, in texels
FILL = (255, 255, 255, 255)
BORDER = (20, 20, 20, 235)

# Clockwise from the bottom left: band, left point, valley, centre point, valley, right point
CROWN = (
    (22.0, 104.0), (14.0, 44.0), (42.0, 66.0), (64.0, 24.0), (86.0, 66.0), (114.0, 44.0), (106.0, 104.0),
)


def write_paa(out_paa, width, height, texel):
    tga = os.path.splitext(out_paa)[0] + ".tga"
    pixels = bytearray()
    for row in range(height):
        for col in range(width):
            r, g, b, a = texel(col, row)
            pixels += bytes((b, g, r, a))
    # Uncompressed true colour, 32 bpp, 8 alpha bits, top-left origin
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 0x28)
    with open(tga, "wb") as handle:
        handle.write(header + bytes(pixels))
    subprocess.run([IMAGE_TO_PAA, tga, out_paa], check=True)
    os.remove(tga)
    print("wrote", out_paa, os.path.getsize(out_paa), "bytes")


def fade_alpha(index):
    t = (index + 0.5) / FADE_LONG
    return int(round(255 * (1.0 - t) ** FADE_POWER))


def fade_v(col, row):
    return (255, 255, 255, fade_alpha(row))


def fade_h(col, row):
    return (255, 255, 255, fade_alpha(col))


def inside(x, y):
    result = False
    count = len(CROWN)
    for i in range(count):
        x1, y1 = CROWN[i]
        x2, y2 = CROWN[(i + 1) % count]
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            result = not result
    return result


def edge_distance(x, y):
    best = 1e9
    count = len(CROWN)
    for i in range(count):
        x1, y1 = CROWN[i]
        x2, y2 = CROWN[(i + 1) % count]
        dx, dy = x2 - x1, y2 - y1
        t = max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / (dx * dx + dy * dy)))
        px, py = x1 + t * dx - x, y1 + t * dy - y
        best = min(best, (px * px + py * py) ** 0.5)
    return best


def crown(col, row):
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
    folder = os.path.abspath(sys.argv[1])
    os.makedirs(folder, exist_ok=True)
    write_paa(os.path.join(folder, "br_lb_fade_v_ca.paa"), FADE_SHORT, FADE_LONG, fade_v)
    write_paa(os.path.join(folder, "br_lb_fade_h_ca.paa"), FADE_LONG, FADE_SHORT, fade_h)
    write_paa(os.path.join(folder, "br_lb_crown_ca.paa"), CROWN_SIZE, CROWN_SIZE, crown)


if __name__ == "__main__":
    main()
