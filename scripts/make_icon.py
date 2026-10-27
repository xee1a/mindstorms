"""Renders the NXT Studio logo (same shapes as resources/icons/app.svg) into resources/app.ico.

Pure standard library: supersampled rasteriser + PNG-in-ICO writer.
Run: python scripts/make_icon.py
"""
import struct
import zlib
from pathlib import Path

SS = 4  # supersampling factor
BASE = 256


def rounded_rect(x, y, x0, y0, w, h, r):
    if x < x0 or y < y0 or x > x0 + w or y > y0 + h:
        return False
    cx = min(max(x, x0 + r), x0 + w - r)
    cy = min(max(y, y0 + r), y0 + h - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def render(size):
    """Returns RGBA rows for the logo at `size` px (designed on a 32-unit grid)."""
    n = size * SS
    k = 32 / n
    acc = [[[0.0, 0.0, 0.0, 0.0] for _ in range(size)] for _ in range(size)]
    for py in range(n):
        y = (py + 0.5) * k
        row = acc[py // SS]
        for px in range(n):
            x = (px + 0.5) * k
            color = None
            if rounded_rect(x, y, 3, 3, 26, 26, 7):
                t = ((x - 3) + (y - 3)) / 52  # diagonal gradient #3d8bfd -> #12b5c9
                color = (0x3D + (0x12 - 0x3D) * t, 0x8B + (0xB5 - 0x8B) * t, 0xFD + (0xC9 - 0xFD) * t, 255)
                if rounded_rect(x, y, 9, 8, 14, 9, 2):
                    a = 0.8  # screen: #0d1117 at 80 %
                    color = (color[0] * (1 - a) + 0x0D * a, color[1] * (1 - a) + 0x11 * a, color[2] * (1 - a) + 0x17 * a, 255)
                if (x - 16) ** 2 + (y - 23) ** 2 <= 9:
                    color = (0xFF, 0xB0, 0x20, 255)
            if color:
                cell = row[px // SS]
                cell[0] += color[0]
                cell[1] += color[1]
                cell[2] += color[2]
                cell[3] += color[3]
    div = SS * SS
    out = []
    for row in acc:
        line = bytearray([0])  # PNG filter type 0
        for r, g, b, a in row:
            alpha = a / div
            if alpha > 0:
                # Un-premultiply so edges keep their colour.
                scale = 255 / alpha
                line += bytes((min(255, round(r / div * scale)), min(255, round(g / div * scale)), min(255, round(b / div * scale)), round(alpha)))
            else:
                line += b"\0\0\0\0"
        out.append(bytes(line))
    return out


def png(size, rows):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")


def main():
    sizes = [256, 64, 48, 32, 24, 16]
    images = [png(s, render(s)) for s in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries = b""
    for s, data in zip(sizes, images):
        entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    out = Path(__file__).resolve().parent.parent / "resources" / "app.ico"
    out.write_bytes(header + entries + b"".join(images))
    print(f"wrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
