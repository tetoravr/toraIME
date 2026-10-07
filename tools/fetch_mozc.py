#!/usr/bin/env python3
"""Mozc OSS 辞書データ (IPAdic ベース) をダウンロードする。

使い方: python tools/fetch_mozc.py <出力ディレクトリ>
"""
import os
import sys
import urllib.request

# 再現性のためにコミットを固定する
MOZC_COMMIT = "921b8cc99904c8d31b771e395513da0a5d55182a"
BASE = f"https://raw.githubusercontent.com/google/mozc/{MOZC_COMMIT}/src/data/dictionary_oss/"
FILES = [f"dictionary{i:02d}.txt" for i in range(10)] + [
    "connection_single_column.txt",
    "id.def",
    "README.txt",
]


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "build/mozc"
    os.makedirs(out, exist_ok=True)
    for name in FILES:
        path = os.path.join(out, name)
        if os.path.exists(path) and os.path.getsize(path) > 0:
            print(f"skip {name}")
            continue
        print(f"download {name}")
        tmp = path + ".part"
        urllib.request.urlretrieve(BASE + name, tmp)
        os.replace(tmp, path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
