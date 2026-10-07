#!/usr/bin/env python3
"""Mozc OSS 辞書から toraIME のバイナリ辞書 (toraime.dic) を生成する。

使い方: python tools/build_dict.py <mozcディレクトリ> <出力ファイル>

バイナリ形式 (リトルエンディアン、メモリマップしてそのまま使える):
  Header (64 bytes)
    char[8]  magic "TORADIC1"
    u32      version
    u32      pos_count            品詞ID数 (連接行列は pos_count x pos_count)
    u32      reading_count
    u32      entry_count
    u32      off_readings         ReadingRec[reading_count] (読みの UTF-16 順にソート)
    u32      off_entries          EntryRec[entry_count]
    u32      off_strings          char16 プール
    u32      string_units         プールの char16 数
    u32      off_matrix           i16[pos_count * pos_count]  cost(prev.rid, next.lid) = m[rid * n + lid]
    u32      off_pos_flags        u8[pos_count]
    u16      id_noun, id_number, id_symbol, id_proper
    u32      max_reading_len
  ReadingRec (16 bytes): u32 str_off, u32 str_len, u32 first_entry, u32 entry_count
  EntryRec   (12 bytes): u32 surface_off, u16 surface_len, u16 lid, u16 rid, i16 cost
"""
import array
import os
import struct
import sys

VERSION = 1

POS_FUNCTIONAL = 1  # 直前の文節にくっつく (助詞・助動詞・接尾など)
POS_PREFIX = 2      # 直後の語と同じ文節になる (接頭詞)
POS_SYMBOL = 4


def pos_flags(pos: str) -> int:
    f = pos.split(",")
    major, minor = f[0], f[1] if len(f) > 1 else "*"
    if major in ("助詞", "助動詞"):
        return POS_FUNCTIONAL
    if minor == "接尾":
        return POS_FUNCTIONAL
    if major in ("動詞", "形容詞") and minor == "非自立":
        return POS_FUNCTIONAL
    if major == "記号":
        return POS_FUNCTIONAL | POS_SYMBOL
    if major == "接頭詞":
        return POS_PREFIX
    return 0


def find_id(ids: dict, pos: str) -> int:
    for i, p in ids.items():
        if p == pos:
            return i
    raise SystemExit(f"POS not found in id.def: {pos}")


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    src, dst = sys.argv[1], sys.argv[2]

    ids = {}
    with open(os.path.join(src, "id.def"), encoding="utf-8") as f:
        for line in f:
            num, pos = line.rstrip("\n").split(" ", 1)
            ids[int(num)] = pos
    pos_count = max(ids) + 1

    with open(os.path.join(src, "connection_single_column.txt"), encoding="utf-8") as f:
        n = int(f.readline())
        if n != pos_count:
            raise SystemExit(f"matrix size {n} != pos count {pos_count}")
        matrix = array.array("h", (int(x) for x in f))
    if len(matrix) != n * n:
        raise SystemExit("broken connection matrix")

    # reading -> {(surface, lid, rid): cost}
    words = {}
    for i in range(10):
        with open(os.path.join(src, f"dictionary{i:02d}.txt"), encoding="utf-8") as f:
            for line in f:
                cols = line.rstrip("\n").split("\t")
                if len(cols) < 5:
                    continue
                if len(cols) > 5 and cols[5]:
                    continue  # SPELLING_CORRECTION などは使わない
                reading, lid, rid, cost, surface = cols[:5]
                cost = min(int(cost), 32767)
                key = (surface, int(lid), int(rid))
                bucket = words.setdefault(reading, {})
                if key not in bucket or bucket[key] > cost:
                    bucket[key] = cost

    def u16key(s: str) -> bytes:
        return s.encode("utf-16-be")  # UTF-16 コード単位順でソートするため

    readings = sorted(words, key=u16key)

    pool = []  # str
    pool_index = {}
    pool_len = 0

    def intern(s: str) -> int:
        nonlocal pool_len
        off = pool_index.get(s)
        if off is None:
            off = pool_len
            pool_index[s] = off
            pool.append(s)
            pool_len += len(s.encode("utf-16-le")) // 2
        return off

    reading_recs = bytearray()
    entry_recs = bytearray()
    entry_count = 0
    max_len = 0
    for r in readings:
        roff = intern(r)
        rlen = len(r.encode("utf-16-le")) // 2
        max_len = max(max_len, rlen)
        items = sorted(words[r].items(), key=lambda kv: kv[1])
        reading_recs += struct.pack("<IIII", roff, rlen, entry_count, len(items))
        for (surface, lid, rid), cost in items:
            soff = intern(surface)
            slen = len(surface.encode("utf-16-le")) // 2
            entry_recs += struct.pack("<IHHHh", soff, slen, lid, rid, cost)
            entry_count += 1

    strings = "".join(pool).encode("utf-16-le")
    flags = bytes(pos_flags(ids.get(i, "*")) for i in range(pos_count))

    header_size = 64
    off_readings = header_size
    off_entries = off_readings + len(reading_recs)
    off_strings = off_entries + len(entry_recs)
    off_matrix = off_strings + len(strings)
    off_matrix += off_matrix % 2
    off_flags = off_matrix + len(matrix) * 2

    header = struct.pack(
        "<8sIIIIIIIIII4HI",
        b"TORADIC1",
        VERSION,
        pos_count,
        len(readings),
        entry_count,
        off_readings,
        off_entries,
        off_strings,
        len(strings) // 2,
        off_matrix,
        off_flags,
        find_id(ids, "名詞,一般,*,*,*,*,*"),
        find_id(ids, "名詞,数,*,*,*,*,*"),
        find_id(ids, "記号,一般,*,*,*,*,*"),
        find_id(ids, "名詞,固有名詞,一般,*,*,*,*"),
        max_len,
    )
    header = header.ljust(header_size, b"\0")

    os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
    with open(dst, "wb") as f:
        f.write(header)
        f.write(reading_recs)
        f.write(entry_recs)
        f.write(strings)
        if f.tell() < off_matrix:
            f.write(b"\0" * (off_matrix - f.tell()))
        if sys.byteorder != "little":
            matrix.byteswap()
        f.write(matrix.tobytes())
        f.write(flags)
    print(f"{dst}: readings={len(readings)} entries={entry_count} size={os.path.getsize(dst)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
