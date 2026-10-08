// UI のデザインを確認するための画像を作る (Linux, FreeType)
//   ui_preview <フォントファイル> <アイコンの生 ARGB (128x128)> <出力ディレクトリ>
// 候補ウィンドウ・読みの表示・設定画面をライト/ダークで描いて PAM 画像にする。
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "candidate_view.h"
#include "settings_view.h"

using namespace toraui;

static void WritePam(const Canvas& c, const std::string& path) {
  std::ofstream out(path, std::ios::binary);
  out << "P7\nWIDTH " << c.width() << "\nHEIGHT " << c.height() << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";
  const uint32_t* p = c.pixels();
  for (int i = 0; i < c.width() * c.height(); ++i) {
    const uint32_t v = p[i];
    const unsigned a = v >> 24;
    auto un = [a](unsigned ch) -> unsigned char { return a ? static_cast<unsigned char>(std::min(255u, ch * 255 / a)) : 0; };
    const unsigned char px[4] = {un((v >> 16) & 0xFF), un((v >> 8) & 0xFF), un(v & 0xFF), static_cast<unsigned char>(a)};
    out.write(reinterpret_cast<const char*>(px), 4);
  }
}

int main(int argc, char** argv) {
  if (argc < 4) return 1;
  const std::wstring font(argv[1], argv[1] + std::string(argv[1]).size());
  std::vector<uint32_t> icon(128 * 128);
  std::ifstream in(argv[2], std::ios::binary);
  in.read(reinterpret_cast<char*>(icon.data()), icon.size() * 4);
  const std::string out = argv[3];
  const float s = 1.5f;

  for (bool dark : {false, true}) {
    const Theme t = Theme::ForMode(dark);
    const std::string suffix = dark ? "_dark" : "_light";
    Fonts fonts(font, s);
    CandidateView view(s);
    CandidateModel m;
    m.items = {L"公園", L"公演", L"講演", L"後援", L"好演", L"こうえん", L"コウエン", L"kouen", L"Kouen"};
    m.notes = {L"", L"", L"", L"", L"", L"ひらがな", L"カタカナ", L"半角英数", L"半角英数"};
    m.selected = 1;
    m.page = 0;
    m.page_count = 3;
    SizeI sz = view.Layout(m, fonts);
    Canvas c(sz.w, sz.h);
    view.Draw(c, m, fonts, t);
    WritePam(c, out + "/candidate" + suffix + ".pam");

    CandidateView hint(s);
    sz = hint.LayoutHint(L"わたしはこうえんにいk", fonts);
    Canvas h(sz.w, sz.h);
    hint.DrawHint(h, L"わたしはこうえんにいk", fonts, t);
    WritePam(h, out + "/hint" + suffix + ".pam");

    SettingsView sv(1.0f, font, font);
    sv.SetIcon(icon, 128, 128);
    tora::Config cfg;
    sv.Hover(700, 300);
    SizeI ss = sv.size();
    Canvas sc(ss.w, ss.h);
    sv.DrawBackground(sc, t);
    sv.Draw(sc, t, cfg);
    WritePam(sc, out + "/settings" + suffix + ".pam");
  }
  return 0;
}
