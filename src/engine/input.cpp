#include "input.h"

#include <algorithm>

#include "romaji.h"
#include "text_util.h"

namespace tora {

uint32_t Input::Width(const Unit& u) {
  return u.raw.empty() ? 1 : static_cast<uint32_t>(u.raw.size());
}

bool Input::IsAlphaUnit(const Unit& u) {
  if (u.raw.empty() || (u.kind != UnitKind::kKana && u.kind != UnitKind::kLetter)) return false;
  for (char c : u.raw) {
    if (!IsAsciiAlpha(c)) return false;
  }
  return true;
}

Input::Input(Units units) : units_(std::move(units)) {
  offsets_.reserve(units_.size() + 1);
  uint32_t off = 0;
  for (const Unit& u : units_) {
    offsets_.push_back(off);
    off += Width(u);
  }
  offsets_.push_back(off);
}

size_t Input::UnitIndexAt(uint32_t off) const {
  // offsets_[i] <= off < offsets_[i + 1] となる i
  auto it = std::upper_bound(offsets_.begin(), offsets_.end(), off);
  return static_cast<size_t>(it - offsets_.begin()) - 1;
}

bool Input::IsBoundary(uint32_t off) const {
  return std::binary_search(offsets_.begin(), offsets_.end(), off);
}

uint32_t Input::NextBoundary(uint32_t off) const {
  auto it = std::upper_bound(offsets_.begin(), offsets_.end(), off);
  return it == offsets_.end() ? size() : *it;
}

uint32_t Input::PrevBoundary(uint32_t off) const {
  auto it = std::lower_bound(offsets_.begin(), offsets_.end(), off);
  if (it == offsets_.begin()) return 0;
  return *(it - 1);
}

char Input::RawCharAt(uint32_t off) const {
  if (off >= size()) return 0;
  size_t i = UnitIndexAt(off);
  const Unit& u = units_[i];
  if (u.raw.empty()) return 0;
  return u.raw[off - offsets_[i]];
}

std::vector<Piece> Input::Slice(uint32_t a, uint32_t b, size_t max_pieces) const {
  std::vector<Piece> out;
  b = std::min(b, size());
  if (a >= b) return out;
  for (size_t i = UnitIndexAt(a); i < units_.size() && offsets_[i] < b; ++i) {
    if (out.size() >= max_pieces) break;
    const Unit& u = units_[i];
    const uint32_t ub = offsets_[i];
    const uint32_t ue = offsets_[i + 1];
    const uint32_t from = std::max(a, ub);
    const uint32_t to = std::min(b, ue);
    if (from == ub && to == ue) {
      out.push_back({u, ub, ue});
      continue;
    }
    if (u.raw.empty()) continue;  // かな入力の単位は分割できない
    // 単位の一部だけ: ローマ字を読み直す
    std::string part = u.raw.substr(from - ub, to - from);
    std::vector<RomajiChunk> chunks;
    Romaji::Resolve(&part, true, &chunks);
    uint32_t pos = from;
    for (auto& c : chunks) {
      Piece p;
      p.unit.raw = c.raw;
      if (c.kana.empty()) {
        p.unit.kind = UnitKind::kLetter;
        p.unit.text = AsciiToU16(c.raw);
      } else {
        p.unit.kind = UnitKind::kKana;
        p.unit.text = std::move(c.kana);
      }
      p.begin = pos;
      pos += static_cast<uint32_t>(c.raw.size());
      p.end = pos;
      out.push_back(std::move(p));
    }
  }
  return out;
}

std::u16string Input::Reading(uint32_t a, uint32_t b) const {
  std::u16string out;
  for (const Piece& p : Slice(a, b)) {
    switch (p.unit.kind) {
      case UnitKind::kKana:
      case UnitKind::kSymbol:
        out += p.unit.text;
        break;
      case UnitKind::kLetter:
      case UnitKind::kDigit:
        out += AsciiToU16(p.unit.raw);
        break;
      case UnitKind::kSpace:
        out += u' ';
        break;
    }
  }
  return out;
}

std::u16string Input::Raw(uint32_t a, uint32_t b) const {
  std::u16string out;
  for (const Piece& p : Slice(a, b)) {
    out += p.unit.raw.empty() ? p.unit.text : AsciiToU16(p.unit.raw);
  }
  return out;
}

}  // namespace tora
