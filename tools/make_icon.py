#!/usr/bin/env python3
"""assets/icon.png (1024x1024, 透過 PNG) から src/tsf/toraime.ico を作る。Pillow が必要。

使い方: python tools/make_icon.py
"""
import os
import sys

from PIL import Image

SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src = os.path.join(root, "assets", "icon.png")
    dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "src", "tsf", "toraime.ico")
    img = Image.open(src).convert("RGBA")
    img.save(dst, format="ICO", sizes=[(s, s) for s in SIZES])
    print(f"{dst}: {os.path.getsize(dst)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
