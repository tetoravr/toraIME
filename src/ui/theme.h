// グラスモーフィズムの配色 (アイコンの青とオレンジに合わせる)
#pragma once

#include <algorithm>

#include "canvas.h"

namespace toraui {

struct Theme {
  bool dark = false;
  // ガラスのパネル (候補ウィンドウ・カード)
  Color glass_top, glass_bottom;          // 塗り (半透明)
  Color card_top, card_bottom;            // 設定画面のカード (背景の光が透けるように薄め)
  Color edge_top, edge_bottom;            // 内側の縁 (上が明るいハイライト)
  Color hairline;                         // 外側の細い線
  Color shadow;
  // 文字
  Color text, text_secondary, text_faint;
  // アクセント (選択中の候補・オンのスイッチなど)
  Color accent_top, accent_bottom, accent_glow, on_accent;
  Color warm;  // 肉球のオレンジ
  Color danger;
  // 設定画面の背景
  Color bg_top, bg_bottom, glow_blue, glow_orange, glow_violet;
  // 部品
  Color track_off, knob, well, hover, divider;

  static Theme Light();
  static Theme Dark();
  static Theme ForMode(bool dark) { return dark ? Dark() : Light(); }
};

#ifdef _WIN32
// アプリのテーマがダークか (AppsUseLightTheme)
bool IsAppDarkMode();
#endif

inline Theme Theme::Light() {
  Theme t;
  t.dark = false;
  t.glass_top = Color::Rgb(0xFFFFFF, 0.84f);
  t.glass_bottom = Color::Rgb(0xF5F8FF, 0.76f);
  t.card_top = Color::Rgb(0xFFFFFF, 0.62f);
  t.card_bottom = Color::Rgb(0xFFFFFF, 0.42f);
  t.edge_top = Color::Rgb(0xFFFFFF, 0.95f);
  t.edge_bottom = Color::Rgb(0xFFFFFF, 0.35f);
  t.hairline = Color::Rgb(0x0F172A, 0.10f);
  t.shadow = Color::Rgb(0x1E3A8A, 0.20f);
  t.text = Color::Rgb(0x0F172A);
  t.text_secondary = Color::Rgb(0x475569);
  t.text_faint = Color::Rgb(0x94A3B8);
  t.accent_top = Color::Rgb(0x60A5FA);
  t.accent_bottom = Color::Rgb(0x2563EB);
  t.accent_glow = Color::Rgb(0x3B82F6, 0.35f);
  t.on_accent = Color::Rgb(0xFFFFFF);
  t.warm = Color::Rgb(0xFB923C);
  t.danger = Color::Rgb(0xDC2626);
  t.bg_top = Color::Rgb(0xEAF2FF);
  t.bg_bottom = Color::Rgb(0xF8FAFF);
  t.glow_blue = Color::Rgb(0x60A5FA, 0.45f);
  t.glow_orange = Color::Rgb(0xFDBA74, 0.45f);
  t.glow_violet = Color::Rgb(0xC4B5FD, 0.40f);
  t.track_off = Color::Rgb(0x94A3B8, 0.40f);
  t.knob = Color::Rgb(0xFFFFFF);
  t.well = Color::Rgb(0x0F172A, 0.06f);
  t.hover = Color::Rgb(0x3B82F6, 0.08f);
  t.divider = Color::Rgb(0x0F172A, 0.07f);
  return t;
}

inline Theme Theme::Dark() {
  Theme t;
  t.dark = true;
  t.glass_top = Color::Rgb(0x1E293B, 0.86f);
  t.glass_bottom = Color::Rgb(0x0F172A, 0.82f);
  t.card_top = Color::Rgb(0x94A3B8, 0.16f);
  t.card_bottom = Color::Rgb(0x64748B, 0.08f);
  t.edge_top = Color::Rgb(0xFFFFFF, 0.24f);
  t.edge_bottom = Color::Rgb(0xFFFFFF, 0.05f);
  t.hairline = Color::Rgb(0x000000, 0.45f);
  t.shadow = Color::Rgb(0x000000, 0.45f);
  t.text = Color::Rgb(0xF1F5F9);
  t.text_secondary = Color::Rgb(0xCBD5E1);
  t.text_faint = Color::Rgb(0x64748B);
  t.accent_top = Color::Rgb(0x60A5FA);
  t.accent_bottom = Color::Rgb(0x2563EB);
  t.accent_glow = Color::Rgb(0x3B82F6, 0.45f);
  t.on_accent = Color::Rgb(0xFFFFFF);
  t.warm = Color::Rgb(0xFB923C);
  t.danger = Color::Rgb(0xF87171);
  t.bg_top = Color::Rgb(0x0B1222);
  t.bg_bottom = Color::Rgb(0x0F1B35);
  t.glow_blue = Color::Rgb(0x2563EB, 0.45f);
  t.glow_orange = Color::Rgb(0xEA580C, 0.30f);
  t.glow_violet = Color::Rgb(0x7C3AED, 0.30f);
  t.track_off = Color::Rgb(0xFFFFFF, 0.16f);
  t.knob = Color::Rgb(0xFFFFFF);
  t.well = Color::Rgb(0xFFFFFF, 0.06f);
  t.hover = Color::Rgb(0x60A5FA, 0.12f);
  t.divider = Color::Rgb(0xFFFFFF, 0.07f);
  return t;
}

// ガラスのパネルを描く (影・塗り・外側の線・内側のハイライト)
inline void DrawGlass(Canvas& c, const Theme& t, RectF r, float radius, float scale, float shadow_blur,
                      bool card = false) {
  c.Shadow({r.x, r.y + 3 * scale, r.w, r.h}, radius, shadow_blur * scale,
           card ? t.shadow.WithAlpha(t.shadow.a * 0.6f) : t.shadow);
  c.FillRoundRect(r, radius, card ? t.card_top : t.glass_top, card ? t.card_bottom : t.glass_bottom);
  c.StrokeRoundRect({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, radius + 1, 1, t.hairline, t.hairline);
  c.StrokeRoundRect(r, radius, std::max(1.0f, scale), t.edge_top, t.edge_bottom);
}

}  // namespace toraui
