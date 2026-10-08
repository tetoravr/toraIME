// 設定
#pragma once

namespace tora {

// 全角英数字の扱い
enum class FullWidthMode : int {
  kDisabled = 0,        // 全角英数・全角記号・全角スペースを一切出さない (既定)
  kCandidatesOnly = 1,  // 普段は半角。候補や F9 では全角も選べる
  kDefault = 2,         // MS-IME と同じく、ひらがなモードの英数字を全角にする
};

// 句読点
enum class PunctuationStyle : int {
  kTouten = 0,       // 、。
  kComma = 1,        // ，．
  kCommaKuten = 2,   // ，。
  kToutenPeriod = 3, // 、．
};

// 入力中 (Space を押す前) の表示
enum class LiveConversion : int {
  kOff = 0,              // 変換しない (Space で変換)
  kFull = 1,             // すべて自動で変換する
  kKeepLastSegment = 2,  // 入力中の文節だけはひらがなのまま、それより前を変換する
};

struct Config {
  FullWidthMode full_width = FullWidthMode::kDisabled;
  bool kana_input_enabled = false;       // JIS かな入力を許可する (無効なら常にローマ字入力)
  bool half_width_kana_enabled = false;  // 半角カタカナを候補に出す・F8 を使う
  bool english_detection = true;         // 英単語を自動判定して英字のままにする
  LiveConversion live_conversion = LiveConversion::kFull;
  bool reading_hint = true;              // 自動変換中、入力した読みを未確定文字列の下に表示する
  bool learning = true;                  // 選んだ候補を学習する
  bool convert_keys_on_off = true;       // 変換キーでオン、無変換キーでオフ
  bool caps_lock_disabled = true;        // CapsLock を無効にする (オンになったらすぐ戻し、大文字入力にもしない)
  PunctuationStyle punctuation = PunctuationStyle::kTouten;

  bool full_width_allowed() const { return full_width != FullWidthMode::kDisabled; }
};

}  // namespace tora
