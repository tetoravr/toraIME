// 変換対象の入力。入力単位 (Unit) の列を、打鍵した文字の位置 (オフセット) で扱えるようにする。
//
// 例えば "testwo" は ローマ字としては て|s|とぉ と区切られるが、英単語 "test" を見つけたときは
// "two" の途中で区切り、残りの "wo" をローマ字として読み直して「を」にしたい。
// そのため変換ではオフセットを位置として使い、単位の途中から始まる部分は読み直す。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "unit.h"

namespace tora {

struct Piece {
  Unit unit;
  uint32_t begin = 0;  // オフセット
  uint32_t end = 0;
};

class Input {
 public:
  Input() = default;
  explicit Input(Units units);

  const Units& units() const { return units_; }
  uint32_t size() const { return offsets_.empty() ? 0 : offsets_.back(); }
  bool empty() const { return size() == 0; }

  // オフセットが入力単位の境界か
  bool IsBoundary(uint32_t off) const;
  // off より後ろ / 前の境界 (なければ size() / 0)
  uint32_t NextBoundary(uint32_t off) const;
  uint32_t PrevBoundary(uint32_t off) const;
  // 位置 off の打鍵文字 (英字かどうかの判定用。かな入力の部分は 0)
  char RawCharAt(uint32_t off) const;

  // [a, b) を入力単位の列にする。単位の途中で切れる部分はローマ字を読み直す。
  std::vector<Piece> Slice(uint32_t a, uint32_t b, size_t max_pieces = SIZE_MAX) const;

  // ひらがなの読み (変換できない英字・数字・記号はそのまま)
  std::u16string Reading(uint32_t a, uint32_t b) const;
  // 打鍵した文字 (かな入力の部分はかな)
  std::u16string Raw(uint32_t a, uint32_t b) const;

  static uint32_t Width(const Unit& u);
  static bool IsAlphaUnit(const Unit& u);

 private:
  size_t UnitIndexAt(uint32_t off) const;  // off を含む単位

  Units units_;
  std::vector<uint32_t> offsets_;  // offsets_[i] = 単位 i の開始位置。末尾は全体の長さ
};

}  // namespace tora
