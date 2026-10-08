// 候補ウィンドウと読みの表示の見た目
#pragma once

#include <string>
#include <vector>

#include "canvas.h"
#include "theme.h"

namespace toraui {

struct CandidateModel {
  std::vector<std::wstring> items;
  std::vector<std::wstring> notes;
  int selected = -1;
  size_t page = 0;
  size_t page_count = 0;
};

struct Fonts {
  // family: Windows ではフォント名、FreeType ではフォントファイル
  Fonts(const std::wstring& family, float scale);
  Font candidate;  // 候補
  Font caption;    // 番号・説明 (small は Windows のマクロと衝突する)
  Font hint;       // 読み
};

class CandidateView {
 public:
  explicit CandidateView(float scale) : s_(scale) {}

  // 影の余白を含めたキャンバスの大きさ
  SizeI Layout(const CandidateModel& model, const Fonts& fonts);
  void Draw(Canvas& canvas, const CandidateModel& model, const Fonts& fonts, const Theme& theme) const;
  // キャンバス上の座標から候補の番号 (ページ内)。外なら -1
  int HitTest(float x, float y) const;
  // パネル (影を除いた部分) の左上がキャンバスのどこにあるか
  float margin() const { return 16 * s_; }

  SizeI LayoutHint(const std::wstring& text, const Fonts& fonts);
  void DrawHint(Canvas& canvas, const std::wstring& text, const Fonts& fonts, const Theme& theme) const;

 private:
  float s_;
  RectF panel_;
  std::vector<RectF> rows_;
  float footer_h_ = 0;
};

}  // namespace toraui
