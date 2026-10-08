// toraIME 変換エンジンのテスト
//   engine_tests             小さなテスト用辞書で動作を確認する
//   engine_tests --dic PATH  本物の辞書 (toraime.dic) での変換結果も確認する
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

#include "composer.h"
#include "converter.h"
#include "engine.h"
#include "input.h"
#include "romaji.h"
#include "session.h"
#include "text_util.h"

using namespace tora;

namespace {

struct TestCase {
  const char* name;
  std::function<void()> fn;
};
std::vector<TestCase>& Tests() {
  static std::vector<TestCase> tests;
  return tests;
}
struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { Tests().push_back({name, std::move(fn)}); }
};
int g_failures = 0;
const char* g_dic_path = nullptr;

#define TEST(name)                                     \
  static void name();                                  \
  static Registrar registrar_##name(#name, name);      \
  static void name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      ++g_failures;                                                          \
    }                                                                        \
  } while (0)

#define CHECK_EQ(a, b)                                                       \
  do {                                                                       \
    const std::string va_ = ToStr(a), vb_ = ToStr(b);                        \
    if (va_ != vb_) {                                                        \
      std::printf("  FAILED %s:%d: %s == %s\n    actual:   %s\n    expected: %s\n", __FILE__, \
                  __LINE__, #a, #b, va_.c_str(), vb_.c_str());               \
      ++g_failures;                                                          \
    }                                                                        \
  } while (0)

std::string ToStr(const std::string& s) { return s; }
std::string ToStr(const char* s) { return s; }
std::string ToStr(const std::u16string& s) { return Utf16ToUtf8(s); }
std::string ToStr(const char16_t* s) { return Utf16ToUtf8(s); }
template <class T, class = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
std::string ToStr(T v) {
  return std::to_string(v);
}

std::u16string U(const char* s) { return Utf8ToUtf16(s); }

std::string RomajiToKana(const std::string& in, bool final = true) {
  std::string pending;
  std::vector<RomajiChunk> out;
  for (char c : in) {
    pending.push_back(c);
    Romaji::Resolve(&pending, false, &out);
  }
  if (final) Romaji::Resolve(&pending, true, &out);
  std::string s;
  for (const auto& c : out) s += c.kana.empty() ? c.raw : Utf16ToUtf8(c.kana);
  return s + pending;
}

// ---- テスト用の小さな辞書 ----
// 品詞 ID: 0 BOS/EOS, 1 名詞, 2 助詞, 3 記号, 4 数, 5 動詞, 6 固有名詞
enum : uint16_t { kBos = 0, kNoun = 1, kParticle = 2, kSymbol = 3, kNumber = 4, kVerb = 5, kProper = 6 };

std::vector<uint8_t> MakeTestDictionary() {
  std::vector<DictSourceEntry> e = {
      {U("わたし"), U("私"), kNoun, kNoun, 3000},
      {U("わたし"), U("渡し"), kNoun, kNoun, 5000},
      {U("は"), U("は"), kParticle, kParticle, 500},
      {U("は"), U("葉"), kNoun, kNoun, 6000},
      {U("を"), U("を"), kParticle, kParticle, 500},
      {U("が"), U("が"), kParticle, kParticle, 500},
      {U("に"), U("に"), kParticle, kParticle, 500},
      {U("の"), U("の"), kParticle, kParticle, 500},
      {U("がっこう"), U("学校"), kNoun, kNoun, 3000},
      {U("こうえん"), U("公園"), kNoun, kNoun, 3500},
      {U("こうえん"), U("公演"), kNoun, kNoun, 3400},
      {U("こうえん"), U("講演"), kNoun, kNoun, 3600},
      {U("いく"), U("行く"), kVerb, kVerb, 3000},
      {U("する"), U("する"), kVerb, kVerb, 2000},
      {U("せってい"), U("設定"), kNoun, kNoun, 3000},
      {U("にほん"), U("日本"), kNoun, kNoun, 3000},
      {U("にほんご"), U("日本語"), kNoun, kNoun, 3200},
      {U("、"), U("、"), kSymbol, kSymbol, 0},
      {U("。"), U("。"), kSymbol, kSymbol, 0},
      {U("ー"), U("ー"), kSymbol, kSymbol, 0},
      {U("らーめん"), U("ラーメン"), kNoun, kNoun, 3000},
  };
  const uint16_t n = 7;
  std::vector<int16_t> matrix(n * n, 1000);
  auto set = [&](uint16_t rid, uint16_t lid, int16_t v) { matrix[rid * n + lid] = v; };
  set(kNoun, kParticle, 100);
  set(kProper, kParticle, 100);
  set(kNumber, kParticle, 100);
  set(kParticle, kNoun, 300);
  set(kParticle, kVerb, 300);
  set(kBos, kParticle, 3000);
  set(kParticle, kParticle, 3000);
  set(kNoun, kBos, 0);
  set(kVerb, kBos, 0);
  set(kParticle, kBos, 200);
  std::vector<uint8_t> flags = {0, 0, kPosFunctional, kPosFunctional | kPosSymbol, 0, 0, 0};
  return SystemDictionary::BuildImage(e, n, matrix, flags, kNoun, kNumber, kSymbol, kProper);
}

struct Fixture {
  Engine engine;
  Fixture() {
    engine.LoadSystemDictionaryImage(MakeTestDictionary());
    for (const char* w : {"test", "game", "windows", "hello", "iphone", "github"}) engine.english().Add(w);
    for (const char* w : {"Claude", "Tokyo", "Mack", "Mac", "Win", "Window"}) engine.english_large().Add(w);
  }
  std::string Convert(const std::string& keys) {
    Composer c(&engine.config());
    for (char ch : keys) c.InsertChar(ch);
    Input input(c.GetUnits());
    std::string out;
    for (const auto& seg : engine.converter().Segment(engine.converter().Convert(input))) {
      if (!out.empty()) out += "|";
      for (const auto& n : seg) out += Utf16ToUtf8(n.surface);
    }
    return out;
  }
};

void Type(Session& s, const std::string& keys, Output* last = nullptr) {
  for (char c : keys) {
    Output o = s.Process(KeyEvent::Char(static_cast<char16_t>(c)));
    if (last) *last = o;
  }
}

// ---------------- ローマ字 ----------------

TEST(RomajiBasic) {
  CHECK_EQ(RomajiToKana("konnichiha"), "こんにちは");
  CHECK_EQ(RomajiToKana("konnnichiha"), "こんにちは");
  CHECK_EQ(RomajiToKana("konna"), "こんな");
  CHECK_EQ(RomajiToKana("kanji"), "かんじ");
  CHECK_EQ(RomajiToKana("shinbun"), "しんぶん");
  CHECK_EQ(RomajiToKana("gakkou"), "がっこう");
  CHECK_EQ(RomajiToKana("kyakka"), "きゃっか");
  CHECK_EQ(RomajiToKana("tsukue"), "つくえ");
  CHECK_EQ(RomajiToKana("chotto"), "ちょっと");
  CHECK_EQ(RomajiToKana("fairu"), "ふぁいる");
  CHECK_EQ(RomajiToKana("konnyaku"), "こんにゃく");
  CHECK_EQ(RomajiToKana("kan'i"), "かんい");
  CHECK_EQ(RomajiToKana("ltu"), "っ");
  CHECK_EQ(RomajiToKana("tsudzuku"), "つづく");
  CHECK_EQ(RomajiToKana("nihon"), "にほん");             // 末尾の n は確定時に「ん」
  CHECK_EQ(RomajiToKana("nihon", false), "にほn");       // 入力途中はまだ n
  CHECK_EQ(RomajiToKana("ky", false), "ky");
  CHECK_EQ(RomajiToKana("test"), "てst");                // ローマ字にならない英字はそのまま
}

TEST(RomajiReverse) {
  CHECK_EQ(Romaji::KanaToRomaji(u"き"), "ki");
  CHECK_EQ(Romaji::KanaToRomaji(u"し"), "si");
  CHECK_EQ(Romaji::KanaToRomaji(u"ん"), "nn");
}

// ---------------- 文字種 ----------------

TEST(TextUtil) {
  CHECK_EQ(HiraganaToKatakana(u"らーめん"), u"ラーメン");
  CHECK_EQ(ToHalfWidthKatakana(u"がっこう"), u"ｶﾞｯｺｳ");
  CHECK_EQ(ToHalfWidthKatakana(u"ぱーてぃー"), u"ﾊﾟｰﾃｨｰ");
  CHECK_EQ(AsciiToFullWidth(u"Abc 1!"), u"Ａｂｃ　１！");
  CHECK_EQ(FullWidthToAscii(u"Ａｂｃ　１！"), u"Abc 1!");
  CHECK_EQ(Utf16ToUtf8(Utf8ToUtf16("日本語😀")), "日本語😀");
}

// ---------------- 入力単位 ----------------

TEST(ComposerSymbols) {
  Config cfg;
  Composer c(&cfg);
  for (char ch : std::string("ra-men,3.14[a]")) c.InsertChar(ch);
  CHECK_EQ(Converter(nullptr, nullptr, nullptr, nullptr, &cfg).ApplyWidth(u"a"), u"a");
  Input in(c.GetUnits());
  CHECK_EQ(in.Reading(0, in.size()), u"らーめん、3.14「あ」");
}

TEST(ComposerPunctuationStyle) {
  Config cfg;
  cfg.punctuation = PunctuationStyle::kComma;
  Composer c(&cfg);
  for (char ch : std::string("a,i.")) c.InsertChar(ch);
  Input in(c.GetUnits());
  CHECK_EQ(in.Reading(0, in.size()), u"あ，い．");
}

TEST(ComposerFullWidthSymbols) {
  Config cfg;
  {
    Composer c(&cfg);
    for (char ch : std::string("a!?")) c.InsertChar(ch);
    Input in(c.GetUnits());
    CHECK_EQ(in.Reading(0, in.size()), u"あ!?");  // 全角無効なら記号も半角
  }
  cfg.full_width = FullWidthMode::kDefault;
  {
    Composer c(&cfg);
    for (char ch : std::string("a!?")) c.InsertChar(ch);
    Input in(c.GetUnits());
    CHECK_EQ(in.Reading(0, in.size()), u"あ！？");
  }
}

TEST(ComposerZKeys) {
  Config cfg;
  Composer c(&cfg);
  for (char ch : std::string("zhzlz.z[z]z-zkza")) c.InsertChar(ch);
  Input in(c.GetUnits());
  CHECK_EQ(in.Reading(0, in.size()), u"←→…『』〜↑ざ");
}

TEST(SymbolCandidates) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "()");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"()");
  bool has_kagi = false, has_fullwidth = false;
  for (const auto& c : o.candidates) {
    if (c == u"「」") has_kagi = true;
    if (c == u"（）") has_fullwidth = true;
  }
  CHECK(has_kagi);        // 「」が候補に出る
  CHECK(!has_fullwidth);  // 全角が無効なら全角括弧は出さない
  // 「(」だけでも「「」を選べる
  s.Reset();
  Type(s, "(");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"「");
}

TEST(ComposerBackspace) {
  Config cfg;
  Composer c(&cfg);
  for (char ch : std::string("kya")) c.InsertChar(ch);
  CHECK(c.Backspace());
  Input in(c.GetUnits());
  CHECK_EQ(in.Reading(0, in.size()), u"き");
  CHECK(c.Backspace());
  CHECK(c.empty());
  CHECK(!c.Backspace());
}

TEST(ComposerKanaInput) {
  Config cfg;
  Composer c(&cfg);
  c.InsertKana(u'か');
  c.InsertKana(u'゛');
  c.InsertKana(u'は');
  c.InsertKana(u'゜');
  Input in(c.GetUnits());
  CHECK_EQ(in.Reading(0, in.size()), u"がぱ");
}

TEST(InputSliceReparse) {
  Config cfg;
  Composer c(&cfg);
  for (char ch : std::string("testwo")) c.InsertChar(ch);
  Input in(c.GetUnits());
  CHECK_EQ(in.size(), 6u);
  CHECK(in.IsBoundary(3));
  CHECK(!in.IsBoundary(4));  // "two" の途中
  CHECK_EQ(in.Reading(4, 6), u"を");  // "wo" を読み直す
  CHECK_EQ(in.Raw(0, 4), u"test");
}

// ---------------- 辞書 ----------------

TEST(DictionaryLookup) {
  SystemDictionary dict;
  CHECK(dict.OpenImage(MakeTestDictionary()));
  std::vector<std::string> found;
  dict.LookupPrefixes(u"にほんごを", [&](size_t len, const DictEntry& e) {
    found.push_back(std::to_string(len) + Utf16ToUtf8(e.surface));
  });
  CHECK_EQ(found.size(), 3u);
  if (found.size() == 3) {
    CHECK_EQ(found[0], "1に");
    CHECK_EQ(found[1], "3日本");
    CHECK_EQ(found[2], "4日本語");
  }
  std::vector<std::string> exact;
  dict.LookupExact(u"こうえん", [&](size_t, const DictEntry& e) { exact.push_back(Utf16ToUtf8(e.surface)); });
  CHECK_EQ(exact.size(), 3u);
  if (!exact.empty()) CHECK_EQ(exact[0], "公演");  // コスト順
  CHECK_EQ(dict.Connection(kNoun, kParticle), 100);
  CHECK_EQ(dict.PosFlags(kParticle), static_cast<int>(kPosFunctional));
}

TEST(DictionaryRejectsBrokenData) {
  SystemDictionary dict;
  CHECK(!dict.OpenImage(std::vector<uint8_t>(10, 0)));
  std::vector<uint8_t> image = MakeTestDictionary();
  image.resize(image.size() / 2);
  CHECK(!dict.OpenImage(image));
}

// ---------------- 変換 ----------------

TEST(ConvertJapanese) {
  Fixture f;
  CHECK_EQ(f.Convert("watashihagakkouniiku"), "私は|学校に|行く");
  CHECK_EQ(f.Convert("nihongo"), "日本語");
  CHECK_EQ(f.Convert("ra-men"), "ラーメン");
}

TEST(ConvertEnglishWords) {
  Fixture f;
  CHECK_EQ(f.Convert("testwosuru"), "testを|する");
  CHECK_EQ(f.Convert("gamewosuru"), "gameを|する");
  CHECK_EQ(f.Convert("windowsnosettei"), "windowsの|設定");
  CHECK_EQ(f.Convert("hello"), "hello");
  CHECK_EQ(f.Convert("iPhone"), "iPhone");
}

TEST(ConvertUnparsableLetters) {
  Fixture f;
  // ローマ字にならない英字を含む並びは英字のまま
  CHECK_EQ(f.Convert("xyzzy"), "xyzzy");
  CHECK_EQ(f.Convert("abcdwosuru"), "abcdを|する");
}

TEST(ConvertUppercase) {
  Fixture f;
  CHECK_EQ(f.Convert("PCwosuru"), "PCを|する");
  CHECK_EQ(f.Convert("Tokyoni"), "Tokyoに");
  CHECK_EQ(f.Convert("I"), "I");
  // 固有名詞の途中で「で」などに区切らない
  CHECK_EQ(f.Convert("Claude"), "Claude");
  CHECK_EQ(f.Convert("Claudeni"), "Claudeに");
  CHECK_EQ(f.Convert("Windowsnosettei"), "Windowsの|設定");
  // 辞書に無い語は英字の続き全部
  CHECK_EQ(f.Convert("Toraime"), "Toraime");
}

TEST(ConvertEnglishDetectionOff) {
  Fixture f;
  f.engine.config().english_detection = false;
  CHECK_EQ(f.Convert("test"), "てst");
  CHECK_EQ(f.Convert("PC"), "PC");  // 大文字は常に英字
}

TEST(ConvertNumbers) {
  Fixture f;
  CHECK_EQ(f.Convert("123wo"), "123を");
  f.engine.config().full_width = FullWidthMode::kDefault;
  CHECK_EQ(f.Convert("123wo"), "１２３を");
}

TEST(CandidatesIncludeTransliterations) {
  Fixture f;
  Composer c(&f.engine.config());
  for (char ch : std::string("kouenni")) c.InsertChar(ch);
  Input input(c.GetUnits());
  auto segs = f.engine.converter().Segment(f.engine.converter().Convert(input));
  CHECK_EQ(segs.size(), 1u);
  auto cands = f.engine.converter().GetCandidates(input, segs[0]);
  std::vector<std::string> s;
  for (auto& cand : cands) s.push_back(Utf16ToUtf8(cand.surface));
  auto has = [&](const char* x) { for (auto& v : s) if (v == x) return true; return false; };
  CHECK(s.size() >= 6);
  if (!s.empty()) CHECK_EQ(s[0], "公演に");
  CHECK(has("公園に"));
  CHECK(has("講演に"));
  CHECK(has("こうえんに"));
  CHECK(has("コウエンニ"));
  CHECK(has("kouenni"));
  CHECK(!has("ｺｳｴﾝﾆ"));      // 半角カナは無効
  CHECK(!has("ｋｏｕｅｎｎｉ"));  // 全角は無効
  f.engine.config().half_width_kana_enabled = true;
  f.engine.config().full_width = FullWidthMode::kCandidatesOnly;
  s.clear();
  for (auto& cand : f.engine.converter().GetCandidates(input, segs[0])) s.push_back(Utf16ToUtf8(cand.surface));
  CHECK(has("ｺｳｴﾝﾆ"));
  CHECK(has("ｋｏｕｅｎｎｉ"));
}

// ---------------- セッション (キー操作) ----------------

TEST(SessionLiveConversionAndCommit) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "watashiha", &o);
  CHECK(o.consumed);
  CHECK_EQ(o.preedit, u"私は");
  CHECK(s.IsComposing());
  o = s.Process(KeyEvent::Of(KeyCode::kEnter));
  CHECK_EQ(o.commit, u"私は");
  CHECK_EQ(o.preedit, u"");
  CHECK(!s.IsComposing());
}

TEST(SessionPendingRomaji) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "nihon", &o);
  CHECK_EQ(o.preedit, u"日本");  // 末尾の n も「ん」とみなして変換する
  Type(s, "go", &o);
  CHECK_EQ(o.preedit, u"日本語");
  Type(s, "k", &o);
  CHECK_EQ(o.preedit, u"日本語k");
}

TEST(SessionEscape) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "watashi");
  o = s.Process(KeyEvent::Of(KeyCode::kEscape));
  CHECK_EQ(o.preedit, u"わたし");  // 1 回目はひらがなに戻す
  o = s.Process(KeyEvent::Of(KeyCode::kEscape));
  CHECK_EQ(o.preedit, u"");
  CHECK(!s.IsComposing());
}

TEST(SessionBackspace) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "ka");
  o = s.Process(KeyEvent::Of(KeyCode::kBackspace));
  CHECK(!s.IsComposing());
  CHECK(o.consumed);
  // 何も入力していなければ BackSpace はアプリに渡す
  CHECK(!s.WouldConsume(KeyEvent::Of(KeyCode::kBackspace)));
}

TEST(SessionConvertAndChooseCandidate) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "kouenniiku");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"公演に行く");
  CHECK(o.candidates_visible);
  CHECK_EQ(o.focus_begin, 0u);
  CHECK_EQ(o.focus_length, 3u);
  CHECK_EQ(o.selected, 0);
  CHECK_EQ(o.attrs.size(), 2u);
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.selected, 1);
  CHECK_EQ(o.preedit, u"公園に行く");
  o = s.Process(KeyEvent::Of(KeyCode::kEnter));
  CHECK_EQ(o.commit, u"公園に行く");
  // 学習したので次からは「公園」が先に来る
  Type(s, "kouenni", &o);
  CHECK_EQ(o.preedit, u"公園に");
  s.Process(KeyEvent::Of(KeyCode::kEscape));
  s.Process(KeyEvent::Of(KeyCode::kEscape));
}

TEST(SessionNumberKeySelectsCandidate) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "kouen");
  s.Process(KeyEvent::Of(KeyCode::kSpace));
  o = s.Process(KeyEvent::Char(u'3'));
  CHECK_EQ(o.preedit, u"講演");
  o = s.Process(KeyEvent::Of(KeyCode::kEnter));
  CHECK_EQ(o.commit, u"講演");
}

TEST(SessionTypingInConvertCommits) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "kouen");
  s.Process(KeyEvent::Of(KeyCode::kSpace));
  o = s.Process(KeyEvent::Char(u'h'));
  CHECK_EQ(o.commit, u"公演");
  CHECK_EQ(o.preedit, u"h");
  CHECK(s.IsComposing());
}

TEST(SessionSegmentMoveAndResize) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "watashihagakkouni");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"私は学校に");
  CHECK_EQ(o.focus_length, 2u);
  o = s.Process(KeyEvent::Of(KeyCode::kRight));
  CHECK_EQ(o.focus_begin, 2u);
  o = s.Process(KeyEvent::Of(KeyCode::kLeft));
  CHECK_EQ(o.focus_begin, 0u);
  // 文節を縮める: 「わたしは」->「わたし」->「わた」
  o = s.Process(KeyEvent::Of(KeyCode::kLeft, true));
  CHECK_EQ(o.preedit, u"私は学校に");
  CHECK_EQ(o.focus_length, 1u);  // 「私」
  o = s.Process(KeyEvent::Of(KeyCode::kLeft, true));
  CHECK_EQ(o.preedit.substr(0, 2), u"わた");
  CHECK_EQ(o.focus_begin, 0u);
  CHECK_EQ(o.focus_length, 2u);
  // 伸ばして元に戻す
  s.Process(KeyEvent::Of(KeyCode::kRight, true));
  o = s.Process(KeyEvent::Of(KeyCode::kRight, true));
  CHECK_EQ(o.preedit, u"私は学校に");
  CHECK_EQ(o.focus_length, 2u);
  o = s.Process(KeyEvent::Of(KeyCode::kEscape));
  CHECK_EQ(o.attrs.size(), 1u);
  CHECK(o.attrs[0].type == AttrType::kInput);
}

TEST(SessionFunctionKeys) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "ra-men");
  o = s.Process(KeyEvent::Of(KeyCode::kF7));
  CHECK_EQ(o.preedit, u"ラーメン");
  o = s.Process(KeyEvent::Of(KeyCode::kF6));
  CHECK_EQ(o.preedit, u"らーめん");
  o = s.Process(KeyEvent::Of(KeyCode::kF8));  // 半角カナは無効なので変わらない
  CHECK_EQ(o.preedit, u"らーめん");
  s.Reset();
  Type(s, "tokyo");
  o = s.Process(KeyEvent::Of(KeyCode::kF10));
  CHECK_EQ(o.preedit, u"tokyo");
  o = s.Process(KeyEvent::Of(KeyCode::kF10));
  CHECK_EQ(o.preedit, u"TOKYO");
  o = s.Process(KeyEvent::Of(KeyCode::kF10));
  CHECK_EQ(o.preedit, u"Tokyo");
  o = s.Process(KeyEvent::Of(KeyCode::kF9));  // 全角は無効なので半角のまま
  CHECK_EQ(o.preedit, u"tokyo");
  f.engine.config().full_width = FullWidthMode::kCandidatesOnly;
  o = s.Process(KeyEvent::Of(KeyCode::kF9));
  CHECK_EQ(o.preedit, u"ｔｏｋｙｏ");
}

TEST(SessionSpaceWhenEmpty) {
  Fixture f;
  Session s(&f.engine);
  // 全角無効: スペースはアプリにそのまま渡す (半角スペース)
  CHECK(!s.WouldConsume(KeyEvent::Of(KeyCode::kSpace)));
  f.engine.config().full_width = FullWidthMode::kDefault;
  CHECK(s.WouldConsume(KeyEvent::Of(KeyCode::kSpace)));
  Output o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.commit, u"　");
  CHECK(!s.WouldConsume(KeyEvent::Of(KeyCode::kSpace, true)));  // Shift+Space は半角
}

TEST(SessionShiftSpaceInComposition) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "hello");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace, true));
  Type(s, "game", &o);
  CHECK_EQ(o.preedit, u"hello game");
}

TEST(SessionFullWidthMode) {
  Fixture f;
  Session s(&f.engine);
  s.set_mode(InputMode::kFullWidthAlnum);
  CHECK(s.mode() == InputMode::kHalfWidthAlnum);  // 全角が無効なら全角英数モードにならない
  CHECK(!s.WouldConsume(KeyEvent::Char(u'a')));
  f.engine.config().full_width = FullWidthMode::kCandidatesOnly;
  s.set_mode(InputMode::kFullWidthAlnum);
  Output o = s.Process(KeyEvent::Char(u'a'));
  CHECK_EQ(o.commit, u"ａ");
}

TEST(SessionCtrlKeysPassThroughWhenEmpty) {
  Fixture f;
  Session s(&f.engine);
  KeyEvent k = KeyEvent::Char(u'c');
  k.ctrl = true;
  CHECK(!s.WouldConsume(k));
}

TEST(SessionLiveConversionOff) {
  Fixture f;
  f.engine.config().live_conversion = LiveConversion::kOff;
  Session s(&f.engine);
  Output o;
  Type(s, "watashiha", &o);
  CHECK_EQ(o.preedit, u"わたしは");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"私は");
}

TEST(SessionReadingHint) {
  Fixture f;
  Session s(&f.engine);
  Output o;
  Type(s, "watashihak", &o);
  CHECK_EQ(o.preedit, u"私はk");
  CHECK_EQ(o.reading, u"わたしはk");  // どこまで打ったかが分かる
  Type(s, "ouen", &o);
  CHECK_EQ(o.reading, u"わたしはこうえn");  // 末尾の n も打ったまま
  // 表示が読みと同じなら出さない
  s.Reset();
  Type(s, "ka", &o);
  CHECK_EQ(o.preedit, u"か");
  CHECK_EQ(o.reading, u"");
  // Esc でひらがな表示にしているときも出さない
  s.Reset();
  Type(s, "watashi");
  o = s.Process(KeyEvent::Of(KeyCode::kEscape));
  CHECK_EQ(o.reading, u"");
  // 設定で無効にできる
  s.Reset();
  f.engine.config().reading_hint = false;
  Type(s, "watashi", &o);
  CHECK_EQ(o.reading, u"");
}

TEST(SessionKeepLastSegment) {
  Fixture f;
  f.engine.config().live_conversion = LiveConversion::kKeepLastSegment;
  Session s(&f.engine);
  Output o;
  Type(s, "watashiha", &o);
  CHECK_EQ(o.preedit, u"わたしは");  // 打っている文節はひらがなのまま
  Type(s, "gakkouni", &o);
  CHECK_EQ(o.preedit, u"私はがっこうに");  // 前の文節は変換される
  Type(s, "test", &o);
  CHECK_EQ(o.preedit, u"私は学校にtest");  // 英字は英字のまま
  o = s.Process(KeyEvent::Of(KeyCode::kEnter));
  CHECK_EQ(o.commit, u"私は学校にtest");  // 表示されているとおりに確定する
  // Space で最後の文節も変換される
  Type(s, "watashihagakkouni");
  o = s.Process(KeyEvent::Of(KeyCode::kSpace));
  CHECK_EQ(o.preedit, u"私は学校に");
}

TEST(SessionKanaInput) {
  Fixture f;
  Session s(&f.engine);
  KeyEvent k = KeyEvent::Char(u'に');
  k.kana = true;
  Output o = s.Process(k);
  k.ch = u'ほ';
  o = s.Process(k);
  k.ch = u'ん';
  o = s.Process(k);
  CHECK_EQ(o.preedit, u"日本");
}

// ---------------- 本物の辞書 ----------------

TEST(RealDictionary) {
  if (g_dic_path == nullptr) {
    std::printf("  (skipped: --dic not given)\n");
    return;
  }
  Engine engine;
  CHECK(engine.LoadSystemDictionary(g_dic_path));
  CHECK(engine.LoadEnglishWords(std::string(TORA_SOURCE_DIR) + "/data/english_words.txt"));
  // 辞書と同じフォルダーにあれば大きな英単語リストも使う
  const std::filesystem::path large = std::filesystem::path(g_dic_path).parent_path() / "english_large.txt";
  const bool has_large = engine.LoadLargeEnglishWords(large);
  engine.config().learning = false;
  auto conv = [&](const std::string& keys) {
    Composer c(&engine.config());
    for (char ch : keys) c.InsertChar(ch);
    Input input(c.GetUnits());
    std::string out;
    for (const auto& n : engine.converter().Convert(input)) out += Utf16ToUtf8(n.surface);
    return out;
  };
  CHECK_EQ(conv("watashihagakkouniikimasu"), "私は学校に行きます");
  CHECK_EQ(conv("kyouhaiitenkidesune"), "今日はいい天気ですね");
  CHECK_EQ(conv("nihongowokakunohamuzukashii"), "日本語を書くのは難しい");
  CHECK_EQ(conv("windowsnosettei"), "windowsの設定");
  CHECK_EQ(conv("Windowsnosettei"), "Windowsの設定");
  CHECK_EQ(conv("watashihaenglishgasuki"), "私はenglishが好き");
  CHECK_EQ(conv("testwosuru"), "testをする");
  CHECK_EQ(conv("gamewoshitai"), "gameをしたい");
  CHECK_EQ(conv("iPhonewokatta"), "iPhoneを買った");
  CHECK_EQ(conv("PCwokidou"), "PCを起動");
  CHECK_EQ(conv("Tokyonihaitta"), "Tokyoに入った");
  CHECK_EQ(conv("kaigiha3jikaradesu"), "会議は3時からです");
  CHECK_EQ(conv("ra-menwotabeta."), "ラーメンを食べた。");
  CHECK_EQ(conv("made"), "まで");
  CHECK_EQ(conv("sakewonomu"), "酒を飲む");
  CHECK_EQ(conv("ikitemo"), "生きても");
  CHECK_EQ(conv("githubnirepositorywotsukuru"), "githubにrepositoryを作る");
  CHECK_EQ(conv("tesutodesu"), "テストです");
  if (has_large) {
    CHECK_EQ(conv("Claudenikiku"), "Claudeに聞く");
    CHECK_EQ(conv("Claudedeshirabeta"), "Claudeで調べた");
    CHECK_EQ(conv("Pythonnohon"), "Pythonの本");
  } else {
    std::printf("  (english_large.txt not found next to the dictionary)\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--dic") == 0) g_dic_path = argv[i + 1];
  }
  for (const auto& t : Tests()) {
    int before = g_failures;
    std::printf("[ RUN  ] %s\n", t.name);
    t.fn();
    std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", t.name);
  }
  std::printf("%zu tests, %d failures\n", Tests().size(), g_failures);
  return g_failures == 0 ? 0 : 1;
}
