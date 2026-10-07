// 打鍵を入力単位 (Unit) の列にまとめる
#pragma once

#include <string>

#include "config.h"
#include "unit.h"

namespace tora {

class Composer {
 public:
  explicit Composer(const Config* config) : config_(config) {}

  // ローマ字入力モードでの ASCII 1 文字
  void InsertChar(char c);
  // かな入力モードでのかな 1 文字 (゛ ゜ は直前のかなと合成する)
  void InsertKana(char16_t c);
  // Shift+Space の空白
  void InsertSpace();
  // 末尾を 1 つ消す。消すものがなければ false
  bool Backspace();
  void Clear();
  bool empty() const { return units_.empty() && pending_.empty(); }

  // 未確定のローマ字も (末尾の n を「ん」にするなどして) 単位にした列
  Units GetUnits() const;
  // 未確定のローマ字を打ったまま (n も英字のまま) 残した列。読みの表示に使う
  Units GetUnitsAsTyped() const;
  const std::string& pending() const { return pending_; }

 private:
  void FlushPending(bool final);
  void AddUnit(UnitKind kind, std::string raw, std::u16string text);
  std::u16string MapSymbol(char c) const;

  const Config* config_;
  Units units_;
  std::string pending_;
};

}  // namespace tora
