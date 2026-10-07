#!/usr/bin/env python3
"""toraIME のアイコン (src/tsf/toraime.ico) を作る。外部ライブラリは使わない。

オレンジの角丸四角に黒い縞 (トラ) の簡単な絵を、16/32/48/256 px で描く。
"""
import math
import struct
import sys


def render(size):
    px = []
    radius = size * 0.22
    for y in range(size):
        row = []
        for x in range(size):
            cx, cy = x + 0.5, y + 0.5
            # 角丸四角の内側か (アンチエイリアスは 4x4 のサンプリング)
            cover = 0
            stripe = 0
            for sy in range(4):
                for sx in range(4):
                    px_ = x + (sx + 0.5) / 4
                    py_ = y + (sy + 0.5) / 4
                    dx = max(radius - px_, 0, px_ - (size - radius))
                    dy = max(radius - py_, 0, py_ - (size - radius))
                    if dx * dx + dy * dy <= radius * radius:
                        cover += 1
                        # 斜めの縞
                        t = (px_ + py_ * 0.6) / size
                        if math.sin(t * math.pi * 7) > 0.55 and 0.12 * size < py_ < 0.88 * size:
                            stripe += 1
            a = cover / 16
            if a == 0:
                row.append((0, 0, 0, 0))
                continue
            s = stripe / max(cover, 1)
            r = int(242 * (1 - s) + 30 * s)
            g = int(140 * (1 - s) + 24 * s)
            b = int(40 * (1 - s) + 20 * s)
            row.append((r, g, b, int(255 * a)))
        px.append(row)
    return px


def bmp_entry(size):
    pixels = render(size)
    header = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    data = bytearray()
    for row in reversed(pixels):  # ボトムアップ
        for r, g, b, a in row:
            data += bytes((b, g, r, a))
    mask_row = ((size + 31) // 32) * 4
    data += b"\0" * (mask_row * size)
    return header + bytes(data)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "src/tsf/toraime.ico"
    sizes = [16, 24, 32, 48]
    images = [bmp_entry(s) for s in sizes]
    ico = struct.pack("<HHH", 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    for s, img in zip(sizes, images):
        ico += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(img), offset)
        offset += len(img)
    for img in images:
        ico += img
    with open(out, "wb") as f:
        f.write(ico)
    print(f"{out}: {len(ico)} bytes")


if __name__ == "__main__":
    main()
