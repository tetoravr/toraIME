// 設定画面の見た目と操作 (OS に依存しない)
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "canvas.h"
#include "config.h"
#include "theme.h"

namespace toraui {

class SettingsView {
 public:
  enum class Action { kNone, kChanged, kOpenUserDict, kOpenUserEnglish, kClearHistory, kClose };
  enum class Key { kTab, kShiftTab, kActivate, kLeft, kRight };

  // family: Windows ではフォント名、FreeType ではフォントファイル
  SettingsView(float scale, const std::wstring& family, const std::wstring& bold_family);

  SizeI size() const { return {static_cast<int>(kWidth * s_), static_cast<int>(kHeight * s_)}; }
  void SetIcon(std::vector<uint32_t> argb, int w, int h);
  // 背景 (変わらないのでキャッシュできる) と、カードや部品
  void DrawBackground(Canvas& canvas, const Theme& theme) const;
  void Draw(Canvas& canvas, const Theme& theme, const tora::Config& config) const;

  Action Click(float x, float y, tora::Config* config);
  // ホバーが変わったら true (再描画が必要)
  bool Hover(float x, float y);
  Action KeyPress(Key key, tora::Config* config);

  static constexpr float kWidth = 860;
  static constexpr float kHeight = 630;

 private:
  enum class Kind { kToggle, kSegment, kButton };
  enum class Style { kNormal, kDanger, kPrimary };
  struct Widget {
    Kind kind;
    int id;
    RectF rect;                       // 操作できる範囲
    RectF control;                    // スイッチや選択肢の枠
    std::wstring label;
    std::vector<std::wstring> options;  // 選択肢
    Style style = Style::kNormal;
  };
  struct Card {
    RectF rect;
    std::wstring title;
  };

  void Build();
  float AddCard(float x, float y, const std::wstring& title, const std::vector<int>& rows);
  int SegmentIndexAt(const Widget& w, float x) const;
  Action Activate(const Widget& w, int option, tora::Config* config);
  static bool GetToggle(int id, const tora::Config& c);
  static int GetSegment(int id, const tora::Config& c);

  float s_;
  Font title_font_;
  Font card_font_;
  Font body_font_;
  Font small_font_;
  std::vector<Card> cards_;
  std::vector<Widget> widgets_;
  std::vector<std::pair<RectF, std::wstring>> notes_;
  std::vector<uint32_t> icon_;
  int icon_w_ = 0;
  int icon_h_ = 0;
  int hover_ = -1;
  int hover_option_ = -1;
  int focus_ = -1;
};

}  // namespace toraui
