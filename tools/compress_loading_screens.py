"""Re-encode the loading screens (gui/textures/loading_screens/*.edds) from 32-bit BGRA to DXT.

The 0.1.0 files are uncompressed 1920x1080 EDDS textures of 7-11 MB each, with an 11-level mip
chain stored as COPY or LZ4 chunks. They are written again with the same names, size and mip
chain as DXT1 (8 bits per pixel), or as DXT5 (16 bits per pixel, alpha kept) when some pixel
has an alpha below --alpha-threshold. The 0.1.0 screens carry a soft watermark in the bottom
right corner; an alpha of 230 or more on black differs from opaque by less than DXT1 noise. The layout is the one vanilla 1.30 uses: the ENF1 marker, a COPY
chunk table from the smallest mip to mip 0, then the chunks in the same order.

Each new file is decoded again and compared with the original (PSNR of mip 0 over RGBA) before
anything is written. A file below --min-psnr stops the run. Files are written to a temporary
name, read back and then renamed. Replacing the originals in place needs --backup, a folder
outside the addon where the originals are copied first.

Usage:
  python tools/compress_loading_screens.py <loading_screens folder> --out <folder>
  python tools/compress_loading_screens.py <loading_screens folder> --backup <folder>
Needs Pillow, numpy and lz4.
"""
import argparse
import glob
import hashlib
import io
import math
import os
import shutil
import struct

import lz4.block
import numpy as np
from PIL import Image

HEADER_SIZE = 128
LZ4_WINDOW = 65536
BLOCK_BYTES = {b"DXT1": 8, b"DXT5": 16}


def read_chunks(data):
    """Returns [(tag, body)] for every mip, smallest mip first, as stored in the file."""
    mips = struct.unpack_from("<I", data, 28)[0]
    off = HEADER_SIZE
    table = []
    for _ in range(mips):
        if off + 8 > len(data):
            raise ValueError("chunk table runs past the end of the file")
        table.append((data[off:off + 4], struct.unpack_from("<I", data, off + 4)[0]))
        off += 8
    chunks = []
    for tag, size in table:
        if tag not in (b"COPY", b"LZ4 "):
            raise ValueError(f"unknown chunk tag {tag!r}")
        chunks.append((tag, data[off:off + size]))
        off += size
    if off != len(data):
        raise ValueError(f"chunk table ends at {off}, file has {len(data)} bytes")
    return chunks


def lz4_chunk(body):
    """LZ4 chunk: uint32 decoded size, then blocks of <uint24 size, uint8 flags, data>. Each block
    decodes to at most 64 KB and uses the previous 64 KB of output as dictionary."""
    if len(body) < 4:
        raise ValueError("LZ4 chunk shorter than its size field")
    target = struct.unpack_from("<I", body, 0)[0]
    out = bytearray()
    pos = 4
    while len(out) < target:
        if pos + 4 > len(body):
            raise ValueError("LZ4 chunk ends inside a block header")
        size = body[pos] | (body[pos + 1] << 8) | (body[pos + 2] << 16)
        pos += 4
        if size == 0 or pos + size > len(body):
            raise ValueError(f"LZ4 block of {size} bytes does not fit the chunk")
        part = lz4.block.decompress(bytes(body[pos:pos + size]), uncompressed_size=LZ4_WINDOW,
                                    dict=bytes(out[-LZ4_WINDOW:]))
        if not part:
            raise ValueError("empty LZ4 block")
        out += part
        pos += size
    if len(out) != target or pos != len(body):
        raise ValueError(f"LZ4 chunk decoded {len(out)} of {target} bytes, consumed {pos} of {len(body)}")
    return bytes(out)


def decode_mip0_bgra32(data):
    """Returns mip 0 of a 32-bit BGRA EDDS file as an RGBA image."""
    if data[:4] != b"DDS " or data[36:40] != b"ENF1":
        raise ValueError("not an EDDS file")
    height, width = struct.unpack_from("<2I", data, 12)
    bits = struct.unpack_from("<I", data, 88)[0]
    masks = struct.unpack_from("<4I", data, 92)
    if bits != 32 or masks[:3] != (0x00FF0000, 0x0000FF00, 0x000000FF):
        raise ValueError(f"not a 32-bit BGRA texture (bits {bits}, masks {masks})")
    tag, body = read_chunks(data)[-1]
    raw = lz4_chunk(body) if tag == b"LZ4 " else bytes(body)
    if len(raw) != width * height * 4:
        raise ValueError(f"mip 0 has {len(raw)} bytes for {width}x{height}")
    bgra = np.frombuffer(raw, dtype=np.uint8).reshape(height, width, 4)
    rgba = bgra[:, :, [2, 1, 0, 3]].copy()
    if masks[3] != 0xFF000000:
        rgba[:, :, 3] = 255  # no alpha mask: the fourth byte is padding
    return Image.fromarray(rgba, "RGBA")


def mip_sizes(width, height, count):
    return [(max(1, width >> i), max(1, height >> i)) for i in range(count)]


def block_size(width, height, fourcc):
    return max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * BLOCK_BYTES[fourcc]


def encode_level(image, fourcc):
    buf = io.BytesIO()
    source = image if fourcc == b"DXT5" else image.convert("RGB")
    source.save(buf, "DDS", pixel_format=fourcc.decode())
    data = buf.getvalue()
    if data[84:88] != fourcc:
        raise ValueError(f"Pillow did not write a {fourcc.decode()} surface")
    body = data[HEADER_SIZE:]
    expected = block_size(*image.size, fourcc)
    if len(body) != expected:
        raise ValueError(f"{fourcc.decode()} {image.size} gave {len(body)} bytes, expected {expected}")
    return body


def dds_header(width, height, mips, linear_size, fourcc, enfusion=True):
    header = bytearray(HEADER_SIZE)
    header[0:4] = b"DDS "
    # size, flags (CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT|LINEARSIZE), height, width, linear size, depth, mips
    struct.pack_into("<7I", header, 4, 124, 0x000A1007, height, width, linear_size, 0, mips)
    if enfusion:
        # Marker in the second reserved word: without it the engine reads the file as a plain DDS
        # and ignores the chunk table (vanilla and 0.1.0 EDDS files carry it too)
        header[36:40] = b"ENF1"
    struct.pack_into("<2I4s", header, 76, 32, 0x4, fourcc)  # pixel format: FOURCC
    struct.pack_into("<I", header, 108, 0x401008)  # caps: COMPLEX | TEXTURE | MIPMAP
    return bytes(header)


def build_edds(image, mips, fourcc):
    width, height = image.size
    levels = []
    for w, h in mip_sizes(width, height, mips):
        level = image if (w, h) == (width, height) else image.resize((w, h), Image.LANCZOS)
        levels.append(encode_level(level, fourcc))
    levels.reverse()  # EDDS stores the smallest mip first
    table = b"".join(b"COPY" + struct.pack("<I", len(body)) for body in levels)
    return dds_header(width, height, mips, block_size(width, height, fourcc), fourcc) + table + b"".join(levels)


def decode_dxt_edds_mip0(data):
    """Returns mip 0 of a DXT1/DXT5 EDDS file written by build_edds as an RGBA image."""
    if data[36:40] != b"ENF1":
        raise ValueError("missing the ENF1 marker of an EDDS file")
    height, width = struct.unpack_from("<2I", data, 12)
    fourcc = data[84:88]
    tag, body = read_chunks(data)[-1]
    if tag != b"COPY":
        raise ValueError("expected a COPY chunk for mip 0")
    plain = dds_header(width, height, 1, len(body), fourcc, enfusion=False) + bytes(body)
    return Image.open(io.BytesIO(plain)).convert("RGBA")


def psnr(a, b):
    diff = np.asarray(a, dtype=np.float64) - np.asarray(b, dtype=np.float64)
    mse = float(np.mean(diff * diff))
    return float("inf") if mse == 0 else 10 * math.log10(255.0 * 255.0 / mse)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for part in iter(lambda: f.read(1 << 20), b""):
            h.update(part)
    return h.hexdigest()


def write_verified(target, data):
    """Writes to a temporary name next to the target, reads it back, then renames it."""
    temp = target + ".tmp"
    with open(temp, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    with open(temp, "rb") as f:
        if f.read() != data:
            os.remove(temp)
            raise SystemExit(f"read-back mismatch, nothing replaced: {target}")
    os.replace(temp, target)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--out", help="folder for the new files; the originals are not touched")
    ap.add_argument("--backup", help="replace in place after copying the originals to this folder")
    ap.add_argument("--min-psnr", type=float, default=28.0, help="lowest accepted PSNR in dB (default 28)")
    ap.add_argument("--alpha-threshold", type=int, default=230,
                    help="an alpha below this in any pixel keeps the alpha with DXT5 (default 230)")
    a = ap.parse_args()
    if bool(a.out) == bool(a.backup):
        raise SystemExit("give either --out <folder> or --backup <folder>")
    out_dir = a.out or a.folder

    # Encode and check everything first: nothing is written if one file fails
    results = []
    for path in sorted(glob.glob(os.path.join(a.folder, "*.edds"))):
        data = open(path, "rb").read()
        if data[84:88] in BLOCK_BYTES:
            print(f"{os.path.basename(path)}: already {data[84:88].decode()}, skipped")
            continue
        mips = struct.unpack_from("<I", data, 28)[0]
        image = decode_mip0_bgra32(data)
        alpha = np.asarray(image)[:, :, 3]
        fourcc = b"DXT5" if int(alpha.min()) < a.alpha_threshold else b"DXT1"
        new = build_edds(image, mips, fourcc)
        quality = psnr(image, decode_dxt_edds_mip0(new))
        if quality < a.min_psnr:
            raise SystemExit(f"{os.path.basename(path)}: PSNR {quality:.1f} dB below {a.min_psnr} dB, nothing written")
        results.append((path, data, new, fourcc, quality, image.size, mips, int((alpha < 255).sum()), int(alpha.min())))

    if a.backup:
        os.makedirs(a.backup, exist_ok=True)
        for path, *_ in results:
            copy = os.path.join(a.backup, os.path.basename(path))
            shutil.copy2(path, copy)
            if sha256(copy) != sha256(path):
                raise SystemExit(f"backup mismatch, nothing replaced: {copy}")
    os.makedirs(out_dir, exist_ok=True)

    total_in = total_out = 0
    for path, data, new, fourcc, quality, size, mips, translucent, min_alpha in results:
        write_verified(os.path.join(out_dir, os.path.basename(path)), new)
        total_in += len(data)
        total_out += len(new)
        note = ""
        if translucent:
            kept = "kept" if fourcc == b"DXT5" else "made opaque"
            note = f", {translucent} translucent pixels (min alpha {min_alpha}) {kept}"
        print(f"{os.path.basename(path)}: {len(data)} -> {len(new)} bytes, {fourcc.decode()}, {size[0]}x{size[1]}, {mips} mips, PSNR {quality:.1f} dB{note}")
    print(f"total {total_in} -> {total_out} bytes")


if __name__ == "__main__":
    main()
