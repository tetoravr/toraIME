#!/usr/bin/env python3
"""大文字で始めた語の判定に使う英単語リスト (english_large.txt) を作る。

SCOWL 由来の Hunspell 英語辞書 (LibreOffice の dictionaries リポジトリ) から、
英字だけの語 (人名・地名などの固有名詞を含む) を取り出す。

使い方: python tools/build_english.py <出力ディレクトリ>
  <出力ディレクトリ>/english_large.txt と ENGLISH_WORDS_README.txt (ライセンス) を作る。
"""
import os
import sys
import urllib.request

# 再現性のためにコミットを固定する
COMMIT = "32b006a2c22a4ac7e8ed3f03346f7b3d85a970a4"
BASE = f"https://raw.githubusercontent.com/LibreOffice/dictionaries/{COMMIT}/en/"


def fetch(name: str) -> bytes:
    with urllib.request.urlopen(BASE + name) as r:
        return r.read()


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(out, exist_ok=True)
    dic = fetch("en_US.dic").decode("utf-8", errors="replace").splitlines()
    words = set()
    for line in dic[1:]:  # 1 行目は語数
        word = line.split("/", 1)[0].strip()
        if len(word) >= 2 and word.isascii() and word.isalpha():
            words.add(word)
    with open(os.path.join(out, "english_large.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# SCOWL (en_US Hunspell) から作った英単語リスト。ライセンスは ENGLISH_WORDS_README.txt\n")
        for w in sorted(words, key=str.lower):
            f.write(w + "\n")
    with open(os.path.join(out, "ENGLISH_WORDS_README.txt"), "wb") as f:
        f.write(fetch("README_en_US.txt"))
    print(f"{out}/english_large.txt: {len(words)} words")
    return 0


if __name__ == "__main__":
    sys.exit(main())
