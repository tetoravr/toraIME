#include "text_util.h"

namespace tora {

std::u16string Utf8ToUtf16(std::string_view s) {
  std::u16string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    char32_t cp;
    size_t n;
    if (c < 0x80) {
      cp = c;
      n = 1;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      n = 2;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      n = 3;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      n = 4;
    } else {
      out.push_back(u'�');
      ++i;
      continue;
    }
    if (i + n > s.size()) {
      out.push_back(u'�');
      break;
    }
    for (size_t k = 1; k < n; ++k) {
      cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    }
    i += n;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    } else {
      out.push_back(static_cast<char16_t>(cp));
    }
  }
  return out;
}

std::string Utf16ToUtf8(std::u16string_view s) {
  std::string out;
  out.reserve(s.size() * 3);
  for (size_t i = 0; i < s.size(); ++i) {
    char32_t cp = s[i];
    if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size() && s[i + 1] >= 0xDC00 &&
        s[i + 1] < 0xE000) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
      ++i;
    }
    if (cp < 0x80) {
      out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

std::u16string AsciiToU16(std::string_view s) {
  return std::u16string(s.begin(), s.end());
}

bool IsHiragana(char16_t c) { return (c >= 0x3041 && c <= 0x3096) || c == 0x309D || c == 0x309E; }
bool IsKatakana(char16_t c) { return (c >= 0x30A1 && c <= 0x30F6) || c == 0x30FD || c == 0x30FE; }
bool IsAsciiAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsAsciiUpper(char c) { return c >= 'A' && c <= 'Z'; }
char AsciiLower(char c) { return IsAsciiUpper(c) ? static_cast<char>(c - 'A' + 'a') : c; }

std::string AsciiLower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = AsciiLower(c);
  return out;
}

std::u16string HiraganaToKatakana(std::u16string_view s) {
  std::u16string out(s);
  for (char16_t& c : out) {
    if (IsHiragana(c)) c = static_cast<char16_t>(c + 0x60);
  }
  return out;
}

std::u16string KatakanaToHiragana(std::u16string_view s) {
  std::u16string out(s);
  for (char16_t& c : out) {
    if (c >= 0x30A1 && c <= 0x30F6) c = static_cast<char16_t>(c - 0x60);
  }
  return out;
}

namespace {

// U+30A1 (ァ) .. U+30F6 (ヶ) の半角表記
const char16_t* const kHalfKatakana[] = {
    u"ｧ", u"ｱ", u"ｨ", u"ｲ", u"ｩ", u"ｳ", u"ｪ", u"ｴ", u"ｫ", u"ｵ",          // ァ-オ
    u"ｶ", u"ｶﾞ", u"ｷ", u"ｷﾞ", u"ｸ", u"ｸﾞ", u"ｹ", u"ｹﾞ", u"ｺ", u"ｺﾞ",    // カ-ゴ
    u"ｻ", u"ｻﾞ", u"ｼ", u"ｼﾞ", u"ｽ", u"ｽﾞ", u"ｾ", u"ｾﾞ", u"ｿ", u"ｿﾞ",    // サ-ゾ
    u"ﾀ", u"ﾀﾞ", u"ﾁ", u"ﾁﾞ", u"ｯ", u"ﾂ", u"ﾂﾞ", u"ﾃ", u"ﾃﾞ", u"ﾄ", u"ﾄﾞ",  // タ-ド
    u"ﾅ", u"ﾆ", u"ﾇ", u"ﾈ", u"ﾉ",                                          // ナ-ノ
    u"ﾊ", u"ﾊﾞ", u"ﾊﾟ", u"ﾋ", u"ﾋﾞ", u"ﾋﾟ", u"ﾌ", u"ﾌﾞ", u"ﾌﾟ",           // ハ-プ
    u"ﾍ", u"ﾍﾞ", u"ﾍﾟ", u"ﾎ", u"ﾎﾞ", u"ﾎﾟ",                                // ヘ-ポ
    u"ﾏ", u"ﾐ", u"ﾑ", u"ﾒ", u"ﾓ",                                          // マ-モ
    u"ｬ", u"ﾔ", u"ｭ", u"ﾕ", u"ｮ", u"ﾖ",                                    // ャ-ヨ
    u"ﾗ", u"ﾘ", u"ﾙ", u"ﾚ", u"ﾛ",                                          // ラ-ロ
    u"ﾜ", u"ﾜ", u"ｲ", u"ｴ", u"ｦ", u"ﾝ", u"ｳﾞ", u"ｶ", u"ｹ",                // ヮ-ヶ
};

static_assert(sizeof(kHalfKatakana) / sizeof(kHalfKatakana[0]) == 0x30F6 - 0x30A1 + 1,
              "half-width katakana table size");

}  // namespace

std::u16string ToHalfWidthKatakana(std::u16string_view s) {
  std::u16string kata = HiraganaToKatakana(s);
  std::u16string out;
  for (char16_t c : kata) {
    if (c >= 0x30A1 && c <= 0x30F6) {
      out += kHalfKatakana[c - 0x30A1];
      continue;
    }
    switch (c) {
      case u'ー': out += u'ｰ'; break;
      case u'。': out += u'｡'; break;
      case u'、': out += u'､'; break;
      case u'「': out += u'｢'; break;
      case u'」': out += u'｣'; break;
      case u'・': out += u'･'; break;
      case u'゛': out += u'ﾞ'; break;
      case u'゜': out += u'ﾟ'; break;
      default: out += c; break;
    }
  }
  return out;
}

std::u16string AsciiToFullWidth(std::u16string_view s) {
  std::u16string out;
  out.reserve(s.size());
  for (char16_t c : s) {
    if (c == u' ') {
      out += u'　';
    } else if (c > 0x20 && c < 0x7F) {
      out += static_cast<char16_t>(c - 0x20 + 0xFF00);
    } else {
      out += c;
    }
  }
  return out;
}

std::u16string FullWidthToAscii(std::u16string_view s) {
  std::u16string out;
  out.reserve(s.size());
  for (char16_t c : s) {
    if (c == u'　') {
      out += u' ';
    } else if (c > 0xFF00 && c < 0xFF5F) {
      out += static_cast<char16_t>(c - 0xFF00 + 0x20);
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace tora
