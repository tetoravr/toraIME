#include "romaji.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace tora {
namespace {

struct Rule {
  const char* romaji;
  const char16_t* kana;
};

// MS-IME 互換のローマ字表 (主要なもの)
const Rule kRules[] = {
    {"a", u"あ"}, {"i", u"い"}, {"u", u"う"}, {"e", u"え"}, {"o", u"お"},
    {"ka", u"か"}, {"ki", u"き"}, {"ku", u"く"}, {"ke", u"け"}, {"ko", u"こ"},
    {"kya", u"きゃ"}, {"kyi", u"きぃ"}, {"kyu", u"きゅ"}, {"kye", u"きぇ"}, {"kyo", u"きょ"},
    {"kwa", u"くぁ"}, {"qa", u"くぁ"}, {"qi", u"くぃ"}, {"qu", u"く"}, {"qe", u"くぇ"}, {"qo", u"くぉ"},
    {"ca", u"か"}, {"ci", u"し"}, {"cu", u"く"}, {"ce", u"せ"}, {"co", u"こ"},
    {"ga", u"が"}, {"gi", u"ぎ"}, {"gu", u"ぐ"}, {"ge", u"げ"}, {"go", u"ご"},
    {"gya", u"ぎゃ"}, {"gyi", u"ぎぃ"}, {"gyu", u"ぎゅ"}, {"gye", u"ぎぇ"}, {"gyo", u"ぎょ"},
    {"gwa", u"ぐぁ"},
    {"sa", u"さ"}, {"si", u"し"}, {"shi", u"し"}, {"su", u"す"}, {"se", u"せ"}, {"so", u"そ"},
    {"sya", u"しゃ"}, {"syi", u"しぃ"}, {"syu", u"しゅ"}, {"sye", u"しぇ"}, {"syo", u"しょ"},
    {"sha", u"しゃ"}, {"shu", u"しゅ"}, {"she", u"しぇ"}, {"sho", u"しょ"},
    {"swa", u"すぁ"},
    {"za", u"ざ"}, {"zi", u"じ"}, {"zu", u"ず"}, {"ze", u"ぜ"}, {"zo", u"ぞ"},
    {"zya", u"じゃ"}, {"zyi", u"じぃ"}, {"zyu", u"じゅ"}, {"zye", u"じぇ"}, {"zyo", u"じょ"},
    {"ja", u"じゃ"}, {"ji", u"じ"}, {"ju", u"じゅ"}, {"je", u"じぇ"}, {"jo", u"じょ"},
    {"jya", u"じゃ"}, {"jyi", u"じぃ"}, {"jyu", u"じゅ"}, {"jye", u"じぇ"}, {"jyo", u"じょ"},
    {"ta", u"た"}, {"ti", u"ち"}, {"chi", u"ち"}, {"tu", u"つ"}, {"tsu", u"つ"}, {"te", u"て"}, {"to", u"と"},
    {"tya", u"ちゃ"}, {"tyi", u"ちぃ"}, {"tyu", u"ちゅ"}, {"tye", u"ちぇ"}, {"tyo", u"ちょ"},
    {"cha", u"ちゃ"}, {"chu", u"ちゅ"}, {"che", u"ちぇ"}, {"cho", u"ちょ"},
    {"cya", u"ちゃ"}, {"cyi", u"ちぃ"}, {"cyu", u"ちゅ"}, {"cye", u"ちぇ"}, {"cyo", u"ちょ"},
    {"tsa", u"つぁ"}, {"tsi", u"つぃ"}, {"tse", u"つぇ"}, {"tso", u"つぉ"},
    {"tha", u"てゃ"}, {"thi", u"てぃ"}, {"thu", u"てゅ"}, {"the", u"てぇ"}, {"tho", u"てょ"},
    {"twa", u"とぁ"}, {"twi", u"とぃ"}, {"twu", u"とぅ"}, {"twe", u"とぇ"}, {"two", u"とぉ"},
    {"da", u"だ"}, {"di", u"ぢ"}, {"du", u"づ"}, {"de", u"で"}, {"do", u"ど"},
    {"dya", u"ぢゃ"}, {"dyi", u"ぢぃ"}, {"dyu", u"ぢゅ"}, {"dye", u"ぢぇ"}, {"dyo", u"ぢょ"},
    {"dha", u"でゃ"}, {"dhi", u"でぃ"}, {"dhu", u"でゅ"}, {"dhe", u"でぇ"}, {"dho", u"でょ"},
    {"dwa", u"どぁ"}, {"dwi", u"どぃ"}, {"dwu", u"どぅ"}, {"dwe", u"どぇ"}, {"dwo", u"どぉ"},
    {"na", u"な"}, {"ni", u"に"}, {"nu", u"ぬ"}, {"ne", u"ね"}, {"no", u"の"},
    {"nya", u"にゃ"}, {"nyi", u"にぃ"}, {"nyu", u"にゅ"}, {"nye", u"にぇ"}, {"nyo", u"にょ"},
    {"nn", u"ん"}, {"n'", u"ん"}, {"xn", u"ん"},
    {"ha", u"は"}, {"hi", u"ひ"}, {"hu", u"ふ"}, {"fu", u"ふ"}, {"he", u"へ"}, {"ho", u"ほ"},
    {"hya", u"ひゃ"}, {"hyi", u"ひぃ"}, {"hyu", u"ひゅ"}, {"hye", u"ひぇ"}, {"hyo", u"ひょ"},
    {"fa", u"ふぁ"}, {"fi", u"ふぃ"}, {"fe", u"ふぇ"}, {"fo", u"ふぉ"},
    {"fya", u"ふゃ"}, {"fyi", u"ふぃ"}, {"fyu", u"ふゅ"}, {"fye", u"ふぇ"}, {"fyo", u"ふょ"},
    {"ba", u"ば"}, {"bi", u"び"}, {"bu", u"ぶ"}, {"be", u"べ"}, {"bo", u"ぼ"},
    {"bya", u"びゃ"}, {"byi", u"びぃ"}, {"byu", u"びゅ"}, {"bye", u"びぇ"}, {"byo", u"びょ"},
    {"va", u"ゔぁ"}, {"vi", u"ゔぃ"}, {"vu", u"ゔ"}, {"ve", u"ゔぇ"}, {"vo", u"ゔぉ"},
    {"vya", u"ゔゃ"}, {"vyu", u"ゔゅ"}, {"vyo", u"ゔょ"},
    {"pa", u"ぱ"}, {"pi", u"ぴ"}, {"pu", u"ぷ"}, {"pe", u"ぺ"}, {"po", u"ぽ"},
    {"pya", u"ぴゃ"}, {"pyi", u"ぴぃ"}, {"pyu", u"ぴゅ"}, {"pye", u"ぴぇ"}, {"pyo", u"ぴょ"},
    {"ma", u"ま"}, {"mi", u"み"}, {"mu", u"む"}, {"me", u"め"}, {"mo", u"も"},
    {"mya", u"みゃ"}, {"myi", u"みぃ"}, {"myu", u"みゅ"}, {"mye", u"みぇ"}, {"myo", u"みょ"},
    {"ya", u"や"}, {"yi", u"い"}, {"yu", u"ゆ"}, {"ye", u"いぇ"}, {"yo", u"よ"},
    {"ra", u"ら"}, {"ri", u"り"}, {"ru", u"る"}, {"re", u"れ"}, {"ro", u"ろ"},
    {"rya", u"りゃ"}, {"ryi", u"りぃ"}, {"ryu", u"りゅ"}, {"rye", u"りぇ"}, {"ryo", u"りょ"},
    {"wa", u"わ"}, {"wi", u"うぃ"}, {"wu", u"う"}, {"we", u"うぇ"}, {"wo", u"を"},
    {"wha", u"うぁ"}, {"whi", u"うぃ"}, {"whu", u"う"}, {"whe", u"うぇ"}, {"who", u"うぉ"},
    {"wyi", u"ゐ"}, {"wye", u"ゑ"},
    {"xa", u"ぁ"}, {"xi", u"ぃ"}, {"xu", u"ぅ"}, {"xe", u"ぇ"}, {"xo", u"ぉ"},
    {"la", u"ぁ"}, {"li", u"ぃ"}, {"lu", u"ぅ"}, {"le", u"ぇ"}, {"lo", u"ぉ"},
    {"xya", u"ゃ"}, {"xyu", u"ゅ"}, {"xyo", u"ょ"}, {"lya", u"ゃ"}, {"lyu", u"ゅ"}, {"lyo", u"ょ"},
    {"xtu", u"っ"}, {"xtsu", u"っ"}, {"ltu", u"っ"}, {"ltsu", u"っ"},
    {"xwa", u"ゎ"}, {"lwa", u"ゎ"}, {"xka", u"ヵ"}, {"lka", u"ヵ"}, {"xke", u"ヶ"}, {"lke", u"ヶ"},
};

struct Table {
  std::unordered_map<std::string, std::u16string> map;
  std::unordered_set<std::string> prefixes;  // キーの真の接頭辞
  std::unordered_map<char16_t, std::string> reverse;
  size_t max_len = 0;

  Table() {
    for (const Rule& r : kRules) {
      std::string key = r.romaji;
      map[key] = r.kana;
      max_len = std::max(max_len, key.size());
      for (size_t i = 1; i < key.size(); ++i) prefixes.insert(key.substr(0, i));
    }
    // 逆引きは標準的な綴りを優先する (し -> "si"。"ci" ではなく)
    for (bool preferred : {true, false}) {
      for (const Rule& r : kRules) {
        std::u16string kana = r.kana;
        std::string key = r.romaji;
        const bool is_preferred = std::string("cqlxw").find(key[0]) == std::string::npos;
        if (kana.size() != 1 || is_preferred != preferred) continue;
        auto it = reverse.find(kana[0]);
        if (it == reverse.end() || (preferred && it->second.size() > key.size())) reverse[kana[0]] = key;
      }
    }
    reverse[u'っ'] = "xtu";
    reverse[u'ん'] = "nn";
  }
};

const Table& GetTable() {
  static const Table table;
  return table;
}

bool IsVowel(char c) { return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; }
bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

}  // namespace

bool Romaji::IsPrefixOfKey(std::string_view s) {
  return GetTable().prefixes.count(std::string(s)) > 0;
}

void Romaji::Resolve(std::string* pending, bool final, std::vector<RomajiChunk>* out) {
  const Table& t = GetTable();
  std::string& p = *pending;
  while (!p.empty()) {
    // 促音: 同じ子音が 2 つ続いたら「っ」
    if (p.size() >= 2 && p[0] == p[1] && IsLower(p[0]) && !IsVowel(p[0]) && p[0] != 'n') {
      out->push_back({p.substr(0, 1), u"っ"});
      p.erase(0, 1);
      continue;
    }
    // 撥音: n の後に母音・y・n・' 以外が来たら「ん」
    if (p.size() >= 2 && p[0] == 'n' && !IsVowel(p[1]) && p[1] != 'y' && p[1] != 'n' &&
        p[1] != '\'') {
      out->push_back({"n", u"ん"});
      p.erase(0, 1);
      continue;
    }
    // "nn" の後に母音か y が続くなら、最初の n だけを「ん」にする (konnichiha -> こんにちは)
    if (p.size() >= 3 && p[0] == 'n' && p[1] == 'n' && (IsVowel(p[2]) || p[2] == 'y')) {
      out->push_back({"n", u"ん"});
      p.erase(0, 1);
      continue;
    }
    // まだ長いキーになり得るなら待つ
    if (!final && (t.prefixes.count(p) > 0 || p == "nn")) break;

    // 最長一致
    size_t matched = 0;
    for (size_t len = std::min(p.size(), t.max_len); len > 0; --len) {
      if (t.map.count(p.substr(0, len)) > 0) {
        matched = len;
        break;
      }
    }
    if (matched > 0) {
      std::string key = p.substr(0, matched);
      out->push_back({key, t.map.at(key)});
      p.erase(0, matched);
      continue;
    }
    if (final && p == "n") {
      out->push_back({"n", u"ん"});
      p.clear();
      break;
    }
    // ローマ字にならない先頭 1 文字は英字のまま
    out->push_back({p.substr(0, 1), u""});
    p.erase(0, 1);
  }
}

std::string Romaji::KanaToRomaji(std::u16string_view kana) {
  const Table& t = GetTable();
  std::string out;
  for (char16_t c : kana) {
    auto it = t.reverse.find(c);
    if (it != t.reverse.end()) out += it->second;
  }
  return out;
}

}  // namespace tora
