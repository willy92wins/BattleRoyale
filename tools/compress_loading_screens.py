"""Re-encode the loading screens (gui/textures/loading_screens/*.edds) from 32-bit BGRA to DXT1.

The 0.1.0 files are uncompressed 1920x1080 EDDS textures of 7-11 MB each, with an 11-level mip
chain stored as COPY or LZ4 chunks. DXT1 keeps the size, the mip chain and the file names at
about 1.4 MB per file. Vanilla 1.30 ships EDDS files in DXT1 with COPY chunks, the layout written
here. Every new file is decoded again and compared with the original (PSNR of mip 0).

Usage: python tools/compress_loading_screens.py <loading_screens folder> [--out <folder>]
Without --out the files are replaced in place. Needs Pillow, numpy and lz4.
"""
import argparse
import glob
import io
import math
import os
import struct

import lz4.block
import numpy as np
from PIL import Image

HEADER_SIZE = 128


def read_chunks(data):
    """Returns [(tag, body)] for every mip, smallest mip first, as stored in the file."""
    mips = struct.unpack_from("<I", data, 28)[0]
    off = HEADER_SIZE
    table = []
    for _ in range(mips):
        table.append((data[off:off + 4], struct.unpack_from("<I", data, off + 4)[0]))
        off += 8
    chunks = []
    for tag, size in table:
        chunks.append((tag, data[off:off + size]))
        off += size
    if off != len(data):
        raise ValueError(f"chunk table ends at {off}, file has {len(data)} bytes")
    return chunks


def lz4_chunk(body):
    """LZ4 chunk: uint32 decoded size, then blocks of <uint24 size, uint8 flags, data> that use
    the previous 64 KB of output as dictionary."""
    target = struct.unpack_from("<I", body, 0)[0]
    out = bytearray()
    pos = 4
    while len(out) < target and pos < len(body):
        size = body[pos] | (body[pos + 1] << 8) | (body[pos + 2] << 16)
        pos += 4
        block = bytes(body[pos:pos + size])
        pos += size
        out += lz4.block.decompress(block, uncompressed_size=65536, dict=bytes(out[-65536:]))
    if len(out) != target:
        raise ValueError(f"LZ4 chunk decoded to {len(out)} bytes, expected {target}")
    return bytes(out)


def decode_mip0_bgra32(data):
    """Returns mip 0 of a 32-bit BGRA EDDS file as an RGB image."""
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
    return Image.fromarray(bgra[:, :, [2, 1, 0]].copy(), "RGB")


def mip_sizes(width, height, count):
    return [(max(1, width >> i), max(1, height >> i)) for i in range(count)]


def dxt1_size(width, height):
    return max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * 8


def encode_dxt1(image):
    buf = io.BytesIO()
    image.save(buf, "DDS", pixel_format="DXT1")
    data = buf.getvalue()
    if data[84:88] != b"DXT1":
        raise ValueError("Pillow did not write a DXT1 surface")
    body = data[HEADER_SIZE:]
    expected = dxt1_size(*image.size)
    if len(body) != expected:
        raise ValueError(f"DXT1 {image.size} gave {len(body)} bytes, expected {expected}")
    return body


def dds_header(width, height, mips, linear_size, enfusion=True):
    header = bytearray(HEADER_SIZE)
    header[0:4] = b"DDS "
    # size, flags (CAPS|HEIGHT|WIDTH|PIXELFORMAT|MIPMAPCOUNT|LINEARSIZE), height, width, linear size, depth, mips
    struct.pack_into("<7I", header, 4, 124, 0x000A1007, height, width, linear_size, 0, mips)
    if enfusion:
        # Marker in the second reserved word: without it the engine reads the file as a plain DDS
        # and ignores the chunk table (vanilla and 0.1.0 EDDS files carry it too)
        header[36:40] = b"ENF1"
    struct.pack_into("<2I4s", header, 76, 32, 0x4, b"DXT1")  # pixel format: FOURCC
    struct.pack_into("<I", header, 108, 0x401008)  # caps: COMPLEX | TEXTURE | MIPMAP
    return bytes(header)


def build_dxt1_edds(image, mips):
    width, height = image.size
    levels = []
    for w, h in mip_sizes(width, height, mips):
        level = image if (w, h) == (width, height) else image.resize((w, h), Image.LANCZOS)
        levels.append(encode_dxt1(level))
    levels.reverse()  # EDDS stores the smallest mip first
    table = b"".join(b"COPY" + struct.pack("<I", len(body)) for body in levels)
    return dds_header(width, height, mips, dxt1_size(width, height)) + table + b"".join(levels)


def decode_dxt1_edds_mip0(data):
    height, width = struct.unpack_from("<2I", data, 12)
    if data[36:40] != b"ENF1":
        raise ValueError("missing the ENF1 marker of an EDDS file")
    tag, body = read_chunks(data)[-1]
    if tag != b"COPY":
        raise ValueError("expected a COPY chunk for mip 0")
    plain = dds_header(width, height, 1, len(body), enfusion=False) + bytes(body)
    return Image.open(io.BytesIO(plain)).convert("RGB")


def psnr(a, b):
    diff = np.asarray(a, dtype=np.float64) - np.asarray(b, dtype=np.float64)
    mse = float(np.mean(diff * diff))
    return float("inf") if mse == 0 else 10 * math.log10(255.0 * 255.0 / mse)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--out")
    a = ap.parse_args()
    out_dir = a.out or a.folder
    os.makedirs(out_dir, exist_ok=True)
    total_in = total_out = 0
    for path in sorted(glob.glob(os.path.join(a.folder, "*.edds"))):
        data = open(path, "rb").read()
        mips = struct.unpack_from("<I", data, 28)[0]
        image = decode_mip0_bgra32(data)
        new = build_dxt1_edds(image, mips)
        quality = psnr(image, decode_dxt1_edds_mip0(new))
        target = os.path.join(out_dir, os.path.basename(path))
        with open(target, "wb") as f:
            f.write(new)
        if open(target, "rb").read() != new:
            raise SystemExit(f"read-back mismatch: {target}")
        total_in += len(data)
        total_out += len(new)
        print(f"{os.path.basename(path)}: {len(data)} -> {len(new)} bytes, {image.size[0]}x{image.size[1]}, {mips} mips, PSNR {quality:.1f} dB")
    print(f"total {total_in} -> {total_out} bytes")


if __name__ == "__main__":
    main()
