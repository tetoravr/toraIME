// 変換候補の一覧を表示するポップアップウィンドウ
#pragma once

#include <functional>
#include <string>
#include <vector>

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
  void Layout(const RECT& anchor);
  void Paint(HDC hdc);
  int HitTest(int y) const;
  int Scale(int v) const;

  HWND hwnd_ = nullptr;
  bool visible_ = false;
  ClickCallback on_click_;
  std::vector<std::wstring> items_;
  std::vector<std::wstring> notes_;
  int selected_ = -1;
  size_t page_ = 0;
  size_t page_count_ = 0;
  UINT dpi_ = 96;
  HFONT font_ = nullptr;
  UINT font_dpi_ = 0;
  int row_height_ = 0;
  bool hint_ = false;
};

}  // namespace toraime
