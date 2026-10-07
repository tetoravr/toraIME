// ローマ字 -> ひらがな変換
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tora {

struct RomajiChunk {
  std::string raw;      // 消費したローマ字 (例: "kya")
  std::u16string kana;  // 変換結果。空ならローマ字にならなかった英字 (raw は 1 文字)
};

class Romaji {
 public:
  // pending (小文字英字) をできるところまでかなにする。
  // 確定したものは out に追加し、pending から取り除く。
  // final == true のときは入力の終わりとみなす (末尾の n は「ん」、その他の残りは英字)。
  static void Resolve(std::string* pending, bool final, std::vector<RomajiChunk>* out);

  // ひらがな 1 文字を代表的なローマ字にする (BackSpace で「きゃ」->「き」にするときなどに使う)
  static std::string KanaToRomaji(std::u16string_view kana);

  // テーブルのキーのいずれかの (真の) 接頭辞か
  static bool IsPrefixOfKey(std::string_view s);
};

}  // namespace tora
