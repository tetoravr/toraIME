#include "settings_view.h"

#include <algorithm>

namespace toraui {
namespace {

// 部品の ID
enum Id : int {
  kLive = 1,
  kReadingHint,
  kEnglish,
  kLearning,
  kConvertKeys,
  kCapsLock,
  kKanaInput,
  kFullWidth,
  kHalfKana,
  kPunctuation,
  kUserDict,
  kUserEnglish,
  kClearHistory,
  kClose,
};

constexpr float kMargin = 24;
constexpr float kGap = 16;
constexpr float kCardPad = 18;
constexpr float kCardTitle = 34;
constexpr float kToggleRow = 46;
constexpr float kSegmentRow = 72;
constexpr float kButtonRow = 44;
constexpr float kCardRadius = 18;

const tora::LiveConversion kLiveValues[] = {tora::LiveConversion::kFull, tora::LiveConversion::kKeepLastSegment,
                                            tora::LiveConversion::kOff};

}  // namespace

SettingsView::SettingsView(float scale, const std::wstring& family, const std::wstring& bold_family)
    : s_(scale),
      title_font_(bold_family, 24 * scale, true),
      card_font_(bold_family, 14.5f * scale, true),
      body_font_(family, 13.5f * scale),
      small_font_(family, 12 * scale) {
  Build();
}

void SettingsView::SetIcon(std::vector<uint32_t> argb, int w, int h) {
  icon_ = std::move(argb);
  icon_w_ = w;
  icon_h_ = h;
}

float SettingsView::AddCard(float x, float y, const std::wstring& title, const std::vector<int>& rows) {
  const float w = (kWidth - 2 * kMargin - kGap) / 2;
  float cy = y + kCardPad + kCardTitle;
  for (int id : rows) {
    Widget wd{};
    wd.id = id;
    const RectF row = {(x + kCardPad) * s_, cy * s_, (w - 2 * kCardPad) * s_, 0};
    switch (id) {
      case kLive:
      case kFullWidth:
      case kPunctuation: {
        wd.kind = Kind::kSegment;
        wd.rect = {row.x, row.y, row.w, kSegmentRow * s_};
        wd.control = {row.x, row.y + 28 * s_, row.w, 34 * s_};
        if (id == kLive) {
          wd.label = L"入力中の表示";
          wd.options = {L"すべて変換", L"文節はひらがな", L"変換しない"};
        } else if (id == kFullWidth) {
          wd.label = L"全角英数字";
          wd.options = {L"使わない", L"候補と F9 だけ", L"全角で入力"};
        } else {
          wd.label = L"句読点";
          wd.options = {L"、。", L"，．", L"，。", L"、．"};
        }
        cy += kSegmentRow;
        break;
      }
      case kUserDict: {
        // ボタンの並び (説明 + 3 つのボタン)
        notes_.push_back({{row.x, row.y, row.w, 22 * s_}, L"ユーザー辞書と英単語リストはメモ帳で編集できます。"});
        cy += 30;
        const float bw = (row.w - 2 * 10 * s_) / 3;
        const wchar_t* labels[] = {L"ユーザー辞書", L"英単語リスト", L"学習履歴を消去"};
        for (int i = 0; i < 3; ++i) {
          Widget b{};
          b.kind = Kind::kButton;
          b.id = kUserDict + i;
          b.rect = {row.x + i * (bw + 10 * s_), cy * s_, bw, 38 * s_};
          b.control = b.rect;
          b.label = labels[i];
          b.style = i == 2 ? Style::kDanger : Style::kNormal;
          widgets_.push_back(b);
        }
        cy += kButtonRow;
        continue;
      }
      default: {
        wd.kind = Kind::kToggle;
        wd.rect = {row.x, row.y, row.w, kToggleRow * s_};
        wd.control = {row.right() - 46 * s_, row.y + (kToggleRow * s_ - 26 * s_) / 2, 46 * s_, 26 * s_};
        switch (id) {
          case kReadingHint: wd.label = L"入力した読みを下に表示する"; break;
          case kEnglish: wd.label = L"英単語は英字のまま入力する"; break;
          case kLearning: wd.label = L"変換を学習する"; break;
          case kConvertKeys: wd.label = L"変換キーでオン / 無変換キーでオフ"; break;
          case kCapsLock: wd.label = L"CapsLock を無効にする"; break;
          case kKanaInput: wd.label = L"かな入力を使えるようにする"; break;
          case kHalfKana: wd.label = L"半角カタカナを使う"; break;
          default: break;
        }
        cy += kToggleRow;
        break;
      }
    }
    widgets_.push_back(wd);
  }
  const float bottom = cy + kCardPad;
  cards_.push_back({{x * s_, y * s_, w * s_, (bottom - y) * s_}, title});
  return bottom;
}

void SettingsView::Build() {
  const float col1 = kMargin;
  const float col2 = kMargin + (kWidth - 2 * kMargin - kGap) / 2 + kGap;
  const float top = 100;
  float y1 = AddCard(col1, top, L"入力", {kLive, kReadingHint, kEnglish, kLearning});
  AddCard(col1, y1 + kGap, L"キー", {kConvertKeys, kCapsLock, kKanaInput});
  float y2 = AddCard(col2, top, L"文字", {kFullWidth, kPunctuation, kHalfKana});
  AddCard(col2, y2 + kGap, L"辞書", {kUserDict});
  // 閉じるボタン (右下)
  Widget close{};
  close.kind = Kind::kButton;
  close.id = kClose;
  close.rect = {(kWidth - kMargin - 120) * s_, (kHeight - kMargin - 40) * s_, 120 * s_, 40 * s_};
  close.control = close.rect;
  close.label = L"閉じる";
  close.style = Style::kPrimary;
  widgets_.push_back(close);
}

bool SettingsView::GetToggle(int id, const tora::Config& c) {
  switch (id) {
    case kReadingHint: return c.reading_hint;
    case kEnglish: return c.english_detection;
    case kLearning: return c.learning;
    case kConvertKeys: return c.convert_keys_on_off;
    case kCapsLock: return c.caps_lock_disabled;
    case kKanaInput: return c.kana_input_enabled;
    case kHalfKana: return c.half_width_kana_enabled;
    default: return false;
  }
}

int SettingsView::GetSegment(int id, const tora::Config& c) {
  switch (id) {
    case kLive:
      for (int i = 0; i < 3; ++i) {
        if (kLiveValues[i] == c.live_conversion) return i;
      }
      return 0;
    case kFullWidth: return static_cast<int>(c.full_width);
    case kPunctuation: return static_cast<int>(c.punctuation);
    default: return 0;
  }
}

void SettingsView::DrawBackground(Canvas& c, const Theme& t) const {
  // グラデーションとやわらかい色の光
  c.VerticalGradient(t.bg_top, t.bg_bottom);
  const float W = kWidth * s_, H = kHeight * s_;
  c.Glow(W * 0.12f, H * 0.12f, 420 * s_, t.glow_blue);
  c.Glow(W * 0.92f, H * 0.22f, 360 * s_, t.glow_orange);
  c.Glow(W * 0.55f, H * 1.02f, 460 * s_, t.glow_violet);
  c.Glow(W * 0.30f, H * 0.75f, 260 * s_, t.glow_blue.WithAlpha(t.glow_blue.a * 0.6f));
}

void SettingsView::Draw(Canvas& c, const Theme& t, const tora::Config& config) const {
  // 見出し
  const float isz = 52 * s_;
  if (!icon_.empty()) c.Image(icon_, icon_w_, icon_h_, static_cast<int>(kMargin * s_), static_cast<int>(26 * s_), static_cast<int>(isz));
  const float tx = (kMargin + 64) * s_;
  c.Text(L"toraIME", {tx, 26 * s_, 300 * s_, 32 * s_}, title_font_, t.text);
  c.Text(L"日本語入力の設定", {tx, 58 * s_, 300 * s_, 20 * s_}, small_font_, t.text_secondary);
  c.Text(L"変更はすぐに反映されます", {(kWidth / 2) * s_, 40 * s_, (kWidth / 2 - kMargin) * s_, 24 * s_},
         small_font_, t.text_faint, kAlignRight);

  // カード
  for (const Card& card : cards_) {
    DrawGlass(c, t, card.rect, kCardRadius * s_, s_, 18, true);
    // 見出しの前の小さな印 (肉球のオレンジ)
    const float d = 8 * s_;
    c.FillRoundRect({card.rect.x + kCardPad * s_, card.rect.y + kCardPad * s_ + 7 * s_, d, d}, d / 2, t.warm);
    c.Text(card.title, {card.rect.x + (kCardPad + 16) * s_, card.rect.y + kCardPad * s_, card.rect.w, 22 * s_},
           card_font_, t.text);
  }
  for (const auto& [rect, text] : notes_) c.Text(text, rect, small_font_, t.text_secondary);

  for (size_t i = 0; i < widgets_.size(); ++i) {
    const Widget& w = widgets_[i];
    const bool hovered = static_cast<int>(i) == hover_;
    switch (w.kind) {
      case Kind::kToggle: {
        if (hovered) c.FillRoundRect(w.rect.Inset(-4 * s_), 10 * s_, t.hover);
        const bool on = GetToggle(w.id, config);
        c.Text(w.label, {w.rect.x, w.rect.y, w.control.x - w.rect.x - 8 * s_, w.rect.h}, body_font_, t.text);
        const RectF tr = w.control;
        if (on) {
          c.Shadow(tr.Inset(2 * s_), tr.h / 2, 6 * s_, t.accent_glow);
          c.FillRoundRect(tr, tr.h / 2, t.accent_top, t.accent_bottom);
        } else {
          c.FillRoundRect(tr, tr.h / 2, t.track_off);
        }
        c.StrokeRoundRect(tr, tr.h / 2, 1, Color::Rgb(0xFFFFFF, on ? 0.45f : 0.25f), Color::Rgb(0xFFFFFF, 0.0f));
        const float k = tr.h - 6 * s_;
        const RectF knob = {on ? tr.right() - 3 * s_ - k : tr.x + 3 * s_, tr.y + 3 * s_, k, k};
        c.Shadow(knob, k / 2, 3 * s_, Color::Rgb(0x000000, 0.25f));
        c.FillRoundRect(knob, k / 2, t.knob, Color::Rgb(0xF1F5F9));
        break;
      }
      case Kind::kSegment: {
        c.Text(w.label, {w.rect.x, w.rect.y, w.rect.w, 22 * s_}, body_font_, t.text_secondary);
        const RectF box = w.control;
        c.FillRoundRect(box, 11 * s_, t.well);
        c.StrokeRoundRect(box, 11 * s_, 1, t.divider, t.divider);
        const int sel = GetSegment(w.id, config);
        const float sw = box.w / static_cast<float>(w.options.size());
        for (size_t o = 0; o < w.options.size(); ++o) {
          const RectF seg = {box.x + sw * o + 3 * s_, box.y + 3 * s_, sw - 6 * s_, box.h - 6 * s_};
          const bool is_sel = static_cast<int>(o) == sel;
          if (is_sel) {
            c.Shadow(seg, 8 * s_, 6 * s_, t.accent_glow);
            c.FillRoundRect(seg, 8 * s_, t.accent_top, t.accent_bottom);
            c.StrokeRoundRect(seg, 8 * s_, 1, Color::Rgb(0xFFFFFF, 0.45f), Color::Rgb(0xFFFFFF, 0.0f));
          } else if (hovered && static_cast<int>(o) == hover_option_) {
            c.FillRoundRect(seg, 8 * s_, t.hover);
          }
          // 句読点のような短い選択肢は大きめの文字にする
          const Font& f = w.options[o].size() <= 2 ? body_font_ : small_font_;
          c.Text(w.options[o], seg, f, is_sel ? t.on_accent : t.text_secondary, kAlignCenter);
        }
        break;
      }
      case Kind::kButton: {
        const RectF b = w.rect;
        if (w.style == Style::kPrimary) {
          c.Shadow(b.Inset(2 * s_), b.h / 2, 8 * s_, t.accent_glow);
          c.FillRoundRect(b, b.h / 2, t.accent_top, t.accent_bottom);
          c.StrokeRoundRect(b, b.h / 2, 1, Color::Rgb(0xFFFFFF, 0.5f), Color::Rgb(0xFFFFFF, 0.0f));
          if (hovered) c.FillRoundRect(b, b.h / 2, Color::Rgb(0xFFFFFF, 0.12f));
          c.Text(w.label, b, body_font_, t.on_accent, kAlignCenter);
        } else {
          c.FillRoundRect(b, 12 * s_, t.card_top, t.card_bottom);
          c.StrokeRoundRect(b, 12 * s_, 1, t.edge_top, t.edge_bottom);
          c.StrokeRoundRect({b.x - 1, b.y - 1, b.w + 2, b.h + 2}, 13 * s_, 1, t.hairline, t.hairline);
          if (hovered) c.FillRoundRect(b, 12 * s_, t.hover);
          c.Text(w.label, b, small_font_, w.style == Style::kDanger ? t.danger : t.text, kAlignCenter);
        }
        break;
      }
    }
    if (static_cast<int>(i) == focus_) {
      const RectF f = (w.kind == Kind::kToggle ? w.rect : w.control).Inset(-4 * s_);
      c.StrokeRoundRect(f, 12 * s_, 2 * s_, t.accent_top, t.accent_bottom);
    }
  }
}

int SettingsView::SegmentIndexAt(const Widget& w, float x) const {
  const float sw = w.control.w / static_cast<float>(w.options.size());
  const int i = static_cast<int>((x - w.control.x) / sw);
  return std::clamp(i, 0, static_cast<int>(w.options.size()) - 1);
}

SettingsView::Action SettingsView::Activate(const Widget& w, int option, tora::Config* c) {
  switch (w.kind) {
    case Kind::kToggle:
      switch (w.id) {
        case kReadingHint: c->reading_hint = !c->reading_hint; break;
        case kEnglish: c->english_detection = !c->english_detection; break;
        case kLearning: c->learning = !c->learning; break;
        case kConvertKeys: c->convert_keys_on_off = !c->convert_keys_on_off; break;
        case kCapsLock: c->caps_lock_disabled = !c->caps_lock_disabled; break;
        case kKanaInput: c->kana_input_enabled = !c->kana_input_enabled; break;
        case kHalfKana: c->half_width_kana_enabled = !c->half_width_kana_enabled; break;
        default: return Action::kNone;
      }
      return Action::kChanged;
    case Kind::kSegment:
      if (option < 0 || option >= static_cast<int>(w.options.size())) return Action::kNone;
      if (w.id == kLive) c->live_conversion = kLiveValues[option];
      if (w.id == kFullWidth) c->full_width = static_cast<tora::FullWidthMode>(option);
      if (w.id == kPunctuation) c->punctuation = static_cast<tora::PunctuationStyle>(option);
      return Action::kChanged;
    case Kind::kButton:
      switch (w.id) {
        case kUserDict: return Action::kOpenUserDict;
        case kUserEnglish: return Action::kOpenUserEnglish;
        case kClearHistory: return Action::kClearHistory;
        case kClose: return Action::kClose;
        default: return Action::kNone;
      }
  }
  return Action::kNone;
}

SettingsView::Action SettingsView::Click(float x, float y, tora::Config* config) {
  for (size_t i = 0; i < widgets_.size(); ++i) {
    const Widget& w = widgets_[i];
    const RectF hit = w.kind == Kind::kSegment ? w.control : w.rect;
    if (!hit.Contains(x, y)) continue;
    focus_ = -1;  // マウスで操作したらフォーカスの枠は消す
    return Activate(w, w.kind == Kind::kSegment ? SegmentIndexAt(w, x) : 0, config);
  }
  return Action::kNone;
}

bool SettingsView::Hover(float x, float y) {
  int hover = -1, option = -1;
  for (size_t i = 0; i < widgets_.size(); ++i) {
    const Widget& w = widgets_[i];
    const RectF hit = w.kind == Kind::kSegment ? w.control : w.rect;
    if (hit.Contains(x, y)) {
      hover = static_cast<int>(i);
      if (w.kind == Kind::kSegment) option = SegmentIndexAt(w, x);
      break;
    }
  }
  const bool changed = hover != hover_ || option != hover_option_;
  hover_ = hover;
  hover_option_ = option;
  return changed;
}

SettingsView::Action SettingsView::KeyPress(Key key, tora::Config* config) {
  const int n = static_cast<int>(widgets_.size());
  switch (key) {
    case Key::kTab:
      focus_ = (focus_ + 1) % n;
      return Action::kNone;
    case Key::kShiftTab:
      focus_ = focus_ <= 0 ? n - 1 : focus_ - 1;
      return Action::kNone;
    default:
      break;
  }
  if (focus_ < 0 || focus_ >= n) return Action::kNone;
  const Widget& w = widgets_[focus_];
  if (key == Key::kActivate && w.kind != Kind::kSegment) return Activate(w, 0, config);
  if (w.kind == Kind::kSegment && (key == Key::kLeft || key == Key::kRight)) {
    const int cur = GetSegment(w.id, *config);
    const int next = std::clamp(cur + (key == Key::kRight ? 1 : -1), 0, static_cast<int>(w.options.size()) - 1);
    if (next != cur) return Activate(w, next, config);
  }
  return Action::kNone;
}

}  // namespace toraui
