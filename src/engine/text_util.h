// 文字列ユーティリティ (UTF-8/16 変換、ひらがな・カタカナ・全角・半角変換)
#pragma once

#include <string>
#include <string_view>

namespace tora {

std::u16string Utf8ToUtf16(std::string_view s);
std::string Utf16ToUtf8(std::u16string_view s);
std::u16string AsciiToU16(std::string_view s);

bool IsHiragana(char16_t c);
bool IsKatakana(char16_t c);
bool IsAsciiAlpha(char c);
bool IsAsciiUpper(char c);
char AsciiLower(char c);
std::string AsciiLower(std::string_view s);

std::u16string HiraganaToKatakana(std::u16string_view s);
std::u16string KatakanaToHiragana(std::u16string_view s);
// ひらがな/カタカナ/記号を半角カタカナにする (ガ -> ｶﾞ)
std::u16string ToHalfWidthKatakana(std::u16string_view s);
// ASCII 英数記号を全角にする (スペースは全角スペース)
std::u16string AsciiToFullWidth(std::u16string_view s);
// 全角英数記号を ASCII に戻す
std::u16string FullWidthToAscii(std::u16string_view s);

}  // namespace tora
