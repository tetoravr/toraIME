// 変換候補と入力した読みを表示するポップアップ (グラスモーフィズム)
// ピクセル単位のアルファを持つレイヤードウィンドウに、src/ui の描画をそのまま出す。
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "candidate_view.h"
#include "common.h"

namespace toraime {

class CandidateWindow {
 public:
  using ClickCallback = std::function<void(size_t index_on_page)>;

  explicit CandidateWindow(ClickCallback on_click);
  ~CandidateWindow();
  CandidateWindow(const CandidateWindow&) = delete;
  CandidateWindow& operator=(const CandidateWindow&) = delete;

  // anchor: 注目文節の位置 (スクリーン座標)。その下に表示する
  void Show(const RECT& anchor, const std::vector<std::wstring>& items,
            const std::vector<std::wstring>& notes, int selected, size_t page, size_t page_count);
  // 候補の代わりに 1 行の文字列 (入力した読み) を表示する
  void ShowHint(const RECT& anchor, const std::wstring& text);
  void Hide();
  bool visible() const { return visible_; }

  static void UnregisterWindowClass();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  bool EnsureWindow();
  void Render(const RECT& anchor);

  HWND hwnd_ = nullptr;
  bool visible_ = false;
  bool hint_ = false;
  ClickCallback on_click_;
  toraui::CandidateModel model_;
  std::wstring hint_text_;
  float scale_ = 0;
  std::unique_ptr<toraui::Fonts> fonts_;
  std::unique_ptr<toraui::CandidateView> view_;
};

}  // namespace toraime
