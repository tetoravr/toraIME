// アルファ付きの簡単なソフトウェア描画 (グラスモーフィズムの UI 用)
// 色はストレートアルファで指定し、内部は乗算済みアルファ (BGRA) で持つ。
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace toraui {

struct Color {
  float r = 0, g = 0, b = 0, a = 1;  // 0..1
  static Color Rgb(uint32_t rgb, float a = 1.0f) {
    return {((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a};
  }
  Color WithAlpha(float alpha) const { return {r, g, b, alpha}; }
};

struct RectF {
  float x = 0, y = 0, w = 0, h = 0;
  float right() const { return x + w; }
  float bottom() const { return y + h; }
  bool Contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  RectF Inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
};

enum TextAlign : unsigned { kAlignLeft = 0, kAlignCenter = 1, kAlignRight = 2 };

struct SizeI {
  int w = 0;
  int h = 0;
};

// 文字の描画 (Windows では GDI、それ以外では FreeType で実装する)
class Font {
 public:
  // family: Windows ではフォント名、FreeType ではフォントファイルのパス
  Font(const std::wstring& family, float pixel_size, bool bold = false);
  ~Font();
  Font(const Font&) = delete;
  Font& operator=(const Font&) = delete;

  SizeI Measure(const std::wstring& text) const;
  // w x h の枠に文字を描いた被覆率 (0..255) を返す。縦は中央にそろえる
  void Rasterize(const std::wstring& text, int w, int h, unsigned align, std::vector<uint8_t>* coverage) const;
  float pixel_size() const { return pixel_size_; }

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
  float pixel_size_;
};

class Canvas {
 public:
  Canvas(int width, int height);
  int width() const { return width_; }
  int height() const { return height_; }

  void Clear(Color c);
  void FillRect(RectF r, Color c);
  // 角丸四角形 (アンチエイリアス付き)。縦方向のグラデーション
  void FillRoundRect(RectF r, float radius, Color top, Color bottom);
  void FillRoundRect(RectF r, float radius, Color c) { FillRoundRect(r, radius, c, c); }
  // 縁取り (縦方向のグラデーション。上を明るくするとガラスのハイライトになる)
  void StrokeRoundRect(RectF r, float radius, float width, Color top, Color bottom);
  // ぼかした影
  void Shadow(RectF r, float radius, float blur, Color c);
  // 円形のやわらかい光 (背景の色のにじみ)
  void Glow(float cx, float cy, float radius, Color c);
  // 縦方向のグラデーションで全面を塗る
  void VerticalGradient(Color top, Color bottom);
  // 文字 (グレースケールのアンチエイリアス)。r の中で縦は中央にそろえる
  void Text(const std::wstring& text, RectF r, const Font& font, Color c, unsigned align = kAlignLeft);
  // 画像 (ストレートアルファの RGBA。0xAARRGGBB) を size x size に縮小して描く
  void Image(const std::vector<uint32_t>& argb, int src_w, int src_h, int x, int y, int size);

  // 乗算済みアルファの BGRA (0xAARRGGBB)
  const uint32_t* pixels() const { return px_.data(); }

 private:
  void Blend(int x, int y, Color c, float coverage);

  int width_;
  int height_;
  std::vector<uint32_t> px_;
};


}  // namespace toraui
