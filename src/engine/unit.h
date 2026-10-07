// 入力の最小単位。ローマ字 1 音節 (例: "kya" -> 「きゃ」) や英字 1 文字など。
#pragma once

#include <string>
#include <vector>

namespace tora {

enum class UnitKind : unsigned char {
  kKana,    // かな (ローマ字やかな入力から。長音「ー」も含む)
  kLetter,  // ローマ字にならなかった英字 1 文字
  kDigit,   // 数字 1 文字
  kSymbol,  // 記号 1 文字 (「、」「。」「!」など。text は変換後の文字)
  kSpace,   // Shift+Space で入れた空白
};

struct Unit {
  UnitKind kind = UnitKind::kKana;
  std::string raw;      // 打鍵した ASCII (かな入力のときは空)
  std::u16string text;  // 表示用の文字 (かなはひらがな)
};

using Units = std::vector<Unit>;

}  // namespace tora
