#include "candidate_view.h"

#include <algorithm>
#include <cmath>

namespace toraui {

Fonts::Fonts(const std::wstring& family, float scale)
    : candidate(family, 16 * scale), caption(family, 11.5f * scale), hint(family, 13.5f * scale) {}

namespace {
constexpr float kPad = 6;
constexpr float kRowH = 34;
constexpr float kRadius = 14;
constexpr float kMinWidth = 200;
}  // namespace

SizeI CandidateView::Layout(const CandidateModel& model, const Fonts& fonts) {
  float content_w = kMinWidth * s_;
  for (size_t i = 0; i < model.items.size(); ++i) {
    float w = fonts.candidate.Measure(model.items[i]).w + 52 * s_;
    if (i < model.notes.size() && !model.notes[i].empty()) w += fonts.caption.Measure(model.notes[i]).w + 22 * s_;
    content_w = std::max(content_w, w + 12 * s_);
  }
  footer_h_ = model.page_count > 1 ? 24 * s_ : 0;
  const float m = margin();
  panel_ = {m, m, content_w + 2 * kPad * s_,
            kRowH * s_ * static_cast<float>(model.items.size()) + 2 * kPad * s_ + footer_h_};
  rows_.clear();
  for (size_t i = 0; i < model.items.size(); ++i) {
    rows_.push_back({panel_.x + kPad * s_, panel_.y + kPad * s_ + kRowH * s_ * static_cast<float>(i),
                     panel_.w - 2 * kPad * s_, kRowH * s_});
  }
  return {static_cast<int>(std::ceil(panel_.right() + m)), static_cast<int>(std::ceil(panel_.bottom() + m))};
}

void CandidateView::Draw(Canvas& c, const CandidateModel& model, const Fonts& fonts, const Theme& t) const {
  DrawGlass(c, t, panel_, kRadius * s_, s_, 12);
  for (size_t i = 0; i < rows_.size() && i < model.items.size(); ++i) {
    const RectF row = rows_[i];
    const bool sel = static_cast<int>(i) == model.selected;
    if (sel) {
      // 選択中: 青いガラスの帯 + やわらかい光
      c.Shadow(row.Inset(2 * s_), 9 * s_, 8 * s_, t.accent_glow);
      c.FillRoundRect(row, 10 * s_, t.accent_top, t.accent_bottom);
      c.StrokeRoundRect(row, 10 * s_, 1, Color::Rgb(0xFFFFFF, 0.45f), Color::Rgb(0xFFFFFF, 0.05f));
    }
    // 番号のバッジ
    const float bs = 20 * s_;
    const RectF badge = {row.x + 8 * s_, row.y + (row.h - bs) / 2, bs, bs};
    c.FillRoundRect(badge, bs / 2, sel ? Color::Rgb(0xFFFFFF, 0.25f) : t.well);
    c.Text(std::wstring(1, static_cast<wchar_t>(L'1' + i)), badge, fonts.caption,
           sel ? t.on_accent : t.text_secondary, kAlignCenter);
    // 候補と説明
    const float tx = badge.right() + 10 * s_;
    float right = row.right() - 10 * s_;
    if (i < model.notes.size() && !model.notes[i].empty()) {
      const float nw = fonts.caption.Measure(model.notes[i]).w + 2 * s_;
      c.Text(model.notes[i], {right - nw, row.y, nw, row.h}, fonts.caption,
             sel ? Color::Rgb(0xFFFFFF, 0.8f) : t.text_faint, kAlignRight);
      right -= nw + 12 * s_;
    }
    c.Text(model.items[i], {tx, row.y, std::max(0.0f, right - tx), row.h}, fonts.candidate,
           sel ? t.on_accent : t.text);
  }
  if (footer_h_ > 0) {
    // ページ: 点と「2 / 5」
    const float y = panel_.bottom() - kPad * s_ - footer_h_;
    const size_t dots = std::min<size_t>(model.page_count, 8);
    float x = panel_.x + 16 * s_;
    for (size_t i = 0; i < dots; ++i) {
      const bool on = i == std::min(model.page, dots - 1);
      const float w = (on ? 14 : 5) * s_;
      c.FillRoundRect({x, y + footer_h_ / 2 - 2.5f * s_, w, 5 * s_}, 2.5f * s_,
                      on ? t.accent_top : t.track_off, on ? t.accent_bottom : t.track_off);
      x += w + 4 * s_;
    }
    const std::wstring label = std::to_wstring(model.page + 1) + L" / " + std::to_wstring(model.page_count);
    c.Text(label, {panel_.x, y, panel_.w - 16 * s_, footer_h_}, fonts.caption, t.text_faint, kAlignRight);
  }
}

int CandidateView::HitTest(float x, float y) const {
  for (size_t i = 0; i < rows_.size(); ++i) {
    if (rows_[i].Contains(x, y)) return static_cast<int>(i);
  }
  return -1;
}

SizeI CandidateView::LayoutHint(const std::wstring& text, const Fonts& fonts) {
  const float m = margin();
  const float w = fonts.hint.Measure(text).w + 34 * s_;
  panel_ = {m, m, w, 30 * s_};
  rows_.clear();
  footer_h_ = 0;
  return {static_cast<int>(std::ceil(panel_.right() + m)), static_cast<int>(std::ceil(panel_.bottom() + m))};
}

void CandidateView::DrawHint(Canvas& c, const std::wstring& text, const Fonts& fonts, const Theme& t) const {
  DrawGlass(c, t, panel_, panel_.h / 2, s_, 10);
  // 左の小さなアクセント (入力中であることを示す)
  const float d = 6 * s_;
  c.FillRoundRect({panel_.x + 12 * s_, panel_.y + (panel_.h - d) / 2, d, d}, d / 2, t.accent_top, t.accent_bottom);
  c.Text(text, {panel_.x + 24 * s_, panel_.y, panel_.w - 30 * s_, panel_.h}, fonts.hint, t.text_secondary);
}

}  // namespace toraui
