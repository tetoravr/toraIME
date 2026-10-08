// Windows の仮想キーを変換エンジンのキーイベントにする
#pragma once

#include "common.h"
#include "session.h"

namespace toraime {

// 日本語キーボードの特殊キー (SDK によって定義が無いことがあるので自前で持つ)
constexpr WPARAM kVkKanji = 0x19;          // 半角/全角 (VK_KANJI)
constexpr WPARAM kVkConvert = 0x1C;        // 変換
constexpr WPARAM kVkNonConvert = 0x1D;     // 無変換
constexpr WPARAM kVkImeOn = 0x16;          // VK_IME_ON
constexpr WPARAM kVkImeOff = 0x1A;         // VK_IME_OFF
constexpr WPARAM kVkDbeAlphanumeric = 0xF0;  // 英数
constexpr WPARAM kVkDbeKatakana = 0xF1;
constexpr WPARAM kVkDbeHiragana = 0xF2;    // カタカナ/ひらがな
constexpr WPARAM kVkOemAuto = 0xF3;        // 半角/全角 (JIS キーボード)
constexpr WPARAM kVkOemEnlw = 0xF4;

// キーを KeyEvent にする。IME が扱わないキー (修飾キー単体など) なら false
// kana_input が true なら JIS かな配列で文字を決める
// ignore_caps_lock が true なら CapsLock の状態を無視する (Shift だけで大文字・小文字が決まる)
bool TranslateKey(WPARAM vk, LPARAM lparam, bool kana_input, bool ignore_caps_lock, tora::KeyEvent* out);

}  // namespace toraime
