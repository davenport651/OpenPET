#!/usr/bin/env python3
"""Build and verify Sera-vice theme packs (SRPK format v1).

The format is specified in ../firmware-s3minipro/src/themepack.h -
this script must match it byte for byte:

    header (all little-endian):
      0   char[4]  magic "SRPK"
      4   u16      format version (= 1)
      6   u16      header size (= 64 + 4 * frameCount)
      8   u32      total pack size (header + payload)
      12  u32      CRC32 of payload (binascii.crc32 / zlib semantics)
      16  u8       frame count
      17  u8       pixel format (1 = RGB565, one uint16 per pixel,
                   stored HIGH byte first - see load_frame())
      18  u16      frame width  (128)
      20  u16      frame height (128)
      22  char[26] pack name, NUL-padded
      48  u8[16]   reserved, zero
      64  u32[N]   frame offsets from pack start
    payload: N frames, 128*128*2 bytes each, row-major, in mood
    enum order (see MOODS below - it mirrors faces.h).

Usage:
    python3 build_pack.py                 # build chibi-sera.pack.bin
    python3 build_pack.py --name my-pack --src my-pack-dir --out my.pack.bin
    python3 build_pack.py --verify chibi-sera.pack.bin
"""
import argparse
import binascii
import struct
import sys
from pathlib import Path

from PIL import Image

# Mood enum order from firmware src/faces.h - frame order is part of
# the format, never reorder this list.
MOODS = ["content", "happy", "thinking", "excited",
         "smug", "worried", "grumpy", "sleepy"]

FRAME_W = 128
FRAME_H = 128
FRAME_BYTES = FRAME_W * FRAME_H * 2
BASE_HEADER = 64
FORMAT_VERSION = 1
PIXEL_FORMAT_RGB565 = 1


def load_frame(path: Path) -> bytes:
    """Center-crop to square, resize to 128x128, encode RGB565.

    Byte order: each pixel's HIGH byte is stored first. The firmware
    hands the frame buffer straight to TFT_eSPI pushImage(), which
    consumes the words big-endian on this stack - storing low byte
    first (plain little-endian) made the panel show every pixel
    byte-swapped (blue skin, lime hair; confirmed on hardware and by
    simulation 2026-11-06). Swapping the bytes here fixes the colors
    with no firmware change; the SRPK version stays 1 so already-
    flashed v2 firmware accepts the pack unchanged.
    """
    img = Image.open(path).convert("RGB")
    w, h = img.size
    side = min(w, h)
    img = img.crop(((w - side) // 2, (h - side) // 2,
                    (w + side) // 2, (h + side) // 2))
    img = img.resize((FRAME_W, FRAME_H), Image.LANCZOS)
    out = bytearray(FRAME_BYTES)
    px = img.tobytes()
    for i in range(FRAME_W * FRAME_H):
        r, g, b = px[3 * i], px[3 * i + 1], px[3 * i + 2]
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out[2 * i] = v >> 8
        out[2 * i + 1] = v & 0xFF
    return bytes(out)


def build(name: str, src_dir: Path, out_path: Path) -> None:
    frames = []
    for mood in MOODS:
        img_path = src_dir / f"{mood}.png"
        if not img_path.exists():
            sys.exit(f"missing frame image: {img_path}")
        frames.append(load_frame(img_path))
        print(f"  frame {mood:<9} <- {img_path.name}")
    payload = b"".join(frames)
    crc = binascii.crc32(payload) & 0xFFFFFFFF
    header_size = BASE_HEADER + 4 * len(frames)
    total = header_size + len(payload)
    name_bytes = name.encode("ascii")[:25]
    header = struct.pack(
        "<4sHHIIBBHH26s16s",
        b"SRPK", FORMAT_VERSION, header_size, total, crc,
        len(frames), PIXEL_FORMAT_RGB565, FRAME_W, FRAME_H,
        name_bytes, b"",
    )
    assert len(header) == BASE_HEADER, len(header)
    offsets = b"".join(
        struct.pack("<I", header_size + i * FRAME_BYTES)
        for i in range(len(frames))
    )
    out_path.write_bytes(header + offsets + payload)
    print(f"wrote {out_path} ({total} bytes, crc32 0x{crc:08x}, "
          f"header {header_size}, {len(frames)} frames)")


def verify(path: Path) -> int:
    data = path.read_bytes()
    problems = []

    def check(label, ok, detail=""):
        print(f"  [{'ok' if ok else 'FAIL'}] {label} {detail}")
        if not ok:
            problems.append(label)

    print(f"verifying {path} ({len(data)} bytes)")
    magic, version, hsize, total, crc_stored, count, fmt, w, h = \
        struct.unpack_from("<4sHHIIBBHH", data, 0)
    name = data[22:48].split(b"\0")[0].decode("ascii", "replace")
    check("magic", magic == b"SRPK", repr(magic))
    check("version", version == FORMAT_VERSION, str(version))
    check("total size", total == len(data), f"header {total} vs file {len(data)}")
    check("header size", hsize == BASE_HEADER + 4 * count,
          f"{hsize} for {count} frames")
    check("pixel format", fmt == PIXEL_FORMAT_RGB565, str(fmt))
    check("frame geometry", (w, h) == (FRAME_W, FRAME_H), f"{w}x{h}")
    check("frame count", count == len(MOODS), str(count))
    offsets = [struct.unpack_from("<I", data, BASE_HEADER + 4 * i)[0]
               for i in range(count)]
    want = [hsize + i * FRAME_BYTES for i in range(count)]
    check("frame offsets", offsets == want, str(offsets))
    payload = data[hsize:total]
    check("payload size", len(payload) == count * FRAME_BYTES,
          str(len(payload)))
    crc = binascii.crc32(payload) & 0xFFFFFFFF
    check("payload crc32", crc == crc_stored,
          f"stored 0x{crc_stored:08x} computed 0x{crc:08x}")
    print(f"  pack name: {name!r}")
    if problems:
        print(f"VERIFY FAILED: {', '.join(problems)}")
        return 1
    print("VERIFY OK")
    return 0


def main() -> int:
    here = Path(__file__).resolve().parent
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--name", default="chibi-sera", help="pack name")
    ap.add_argument("--src", type=Path, default=here / "chibi-sera",
                    help="directory holding <mood>.png frames")
    ap.add_argument("--out", type=Path, default=here / "chibi-sera.pack.bin",
                    help="output pack file")
    ap.add_argument("--verify", type=Path, metavar="PACK",
                    help="verify an existing pack file and exit")
    args = ap.parse_args()
    if args.verify:
        return verify(args.verify)
    build(args.name, args.src, args.out)
    return verify(args.out)


if __name__ == "__main__":
    sys.exit(main())
