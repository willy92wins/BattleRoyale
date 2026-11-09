"""Builds the loading screen EDDS files from the source images.

Each image in BattleRoyale_dev/assets/loading_screens (br_loading_N.jpg/.png) is cropped to 16:9,
resized to 1920x1080 and written as a DXT1 EDDS with a full mip chain to
BattleRoyale/addons/gui/textures/loading_screens/br_loading_N.edds.

Usage: python -I tools/make_loading_screens.py [--check]
"""

import argparse
import glob
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from compress_loading_screens import build_edds, decode_dxt_edds_mip0, psnr, write_verified  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(ROOT, "BattleRoyale_dev", "assets", "loading_screens")
TARGET = os.path.join(ROOT, "BattleRoyale", "addons", "gui", "textures", "loading_screens")
WIDTH, HEIGHT, MIPS = 1920, 1080, 11
MIN_PSNR = 28.0


def fit_16_9(image):
    w, h = image.size
    if w * HEIGHT > h * WIDTH:
        nw = round(h * WIDTH / HEIGHT)
        left = (w - nw) // 2
        image = image.crop((left, 0, left + nw, h))
    else:
        nh = round(w * HEIGHT / WIDTH)
        top = (h - nh) // 2
        image = image.crop((0, top, w, top + nh))
    return image.resize((WIDTH, HEIGHT), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="encode and report without writing")
    args = ap.parse_args()

    sources = sorted(glob.glob(os.path.join(SOURCE, "br_loading_*.jpg")) + glob.glob(os.path.join(SOURCE, "br_loading_*.png")))
    if not sources:
        raise SystemExit(f"no source images in {SOURCE}")
    for src in sources:
        name = os.path.splitext(os.path.basename(src))[0]
        image = fit_16_9(Image.open(src).convert("RGB")).convert("RGBA")
        data = build_edds(image, MIPS, b"DXT1")
        quality = psnr(decode_dxt_edds_mip0(data), image)
        if quality < MIN_PSNR:
            raise SystemExit(f"{name}: PSNR {quality:.1f} dB below {MIN_PSNR}")
        print(f"{name}.edds  {len(data)} bytes  PSNR {quality:.1f} dB")
        if not args.check:
            write_verified(os.path.join(TARGET, name + ".edds"), data)


if __name__ == "__main__":
    main()
