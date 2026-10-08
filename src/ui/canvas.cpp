#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace toraui {
namespace {

float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// 角丸四角形の符号付き距離 (内側が負)
float RoundRectDistance(float px, float py, const RectF& r, float radius) {
  const float cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  const float hx = r.w / 2 - radius, hy = r.h / 2 - radius;
  const float qx = std::fabs(px - cx) - hx, qy = std::fabs(py - cy) - hy;
  const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - radius;
}

Color Lerp(Color a, Color b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

}  // namespace

Canvas::Canvas(int width, int height)
    : width_(std::max(width, 1)), height_(std::max(height, 1)),
      px_(static_cast<size_t>(width_) * height_, 0) {}

void Canvas::Blend(int x, int y, Color c, float coverage) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
  const float a = Clamp01(c.a * coverage);
  if (a <= 0) return;
  uint32_t& d = px_[static_cast<size_t>(y) * width_ + x];
  const float db = (d & 0xFF) / 255.0f, dg = ((d >> 8) & 0xFF) / 255.0f, dr = ((d >> 16) & 0xFF) / 255.0f,
              da = ((d >> 24) & 0xFF) / 255.0f;
  // 乗算済みアルファで「上に重ねる」
  const float ob = c.b * a + db * (1 - a), og = c.g * a + dg * (1 - a), orr = c.r * a + dr * (1 - a),
              oa = a + da * (1 - a);
  auto to8 = [](float v) { return static_cast<uint32_t>(Clamp01(v) * 255.0f + 0.5f); };
  d = to8(ob) | (to8(og) << 8) | (to8(orr) << 16) | (to8(oa) << 24);
}

void Canvas::Clear(Color c) {
  auto to8 = [](float v) { return static_cast<uint32_t>(Clamp01(v) * 255.0f + 0.5f); };
  const uint32_t v = to8(c.b * c.a) | (to8(c.g * c.a) << 8) | (to8(c.r * c.a) << 16) | (to8(c.a) << 24);
  std::fill(px_.begin(), px_.end(), v);
}

void Canvas::FillRect(RectF r, Color c) { FillRoundRect(r, 0, c, c); }

void Canvas::VerticalGradient(Color top, Color bottom) {
  for (int y = 0; y < height_; ++y) {
    const Color c = Lerp(top, bottom, height_ > 1 ? static_cast<float>(y) / (height_ - 1) : 0);
    for (int x = 0; x < width_; ++x) Blend(x, y, c, 1);
  }
}

void Canvas::FillRoundRect(RectF r, float radius, Color top, Color bottom) {
  radius = std::min(radius, std::min(r.w, r.h) / 2);
  const int x0 = std::max(0, static_cast<int>(std::floor(r.x))), x1 = std::min(width_, static_cast<int>(std::ceil(r.right())));
  const int y0 = std::max(0, static_cast<int>(std::floor(r.y))), y1 = std::min(height_, static_cast<int>(std::ceil(r.bottom())));
  for (int y = y0; y < y1; ++y) {
    const Color c = Lerp(top, bottom, r.h > 0 ? Clamp01((y + 0.5f - r.y) / r.h) : 0);
    for (int x = x0; x < x1; ++x) {
      const float d = RoundRectDistance(x + 0.5f, y + 0.5f, r, radius);
      const float cov = Clamp01(0.5f - d);
      if (cov > 0) Blend(x, y, c, cov);
    }
  }
}

void Canvas::StrokeRoundRect(RectF r, float radius, float width, Color top, Color bottom) {
  radius = std::min(radius, std::min(r.w, r.h) / 2);
  const int x0 = std::max(0, static_cast<int>(std::floor(r.x - 1))), x1 = std::min(width_, static_cast<int>(std::ceil(r.right() + 1)));
  const int y0 = std::max(0, static_cast<int>(std::floor(r.y - 1))), y1 = std::min(height_, static_cast<int>(std::ceil(r.bottom() + 1)));
  for (int y = y0; y < y1; ++y) {
    const Color c = Lerp(top, bottom, r.h > 0 ? Clamp01((y + 0.5f - r.y) / r.h) : 0);
    for (int x = x0; x < x1; ++x) {
      // 内側に width の幅で描く
      const float d = RoundRectDistance(x + 0.5f, y + 0.5f, r, radius);
      const float cov = Clamp01(0.5f - d) * Clamp01(d + width + 0.5f);
      if (cov > 0) Blend(x, y, c, cov);
    }
  }
}

void Canvas::Shadow(RectF r, float radius, float blur, Color c) {
  const int x0 = std::max(0, static_cast<int>(r.x - blur * 2)), x1 = std::min(width_, static_cast<int>(r.right() + blur * 2));
  const int y0 = std::max(0, static_cast<int>(r.y - blur * 2)), y1 = std::min(height_, static_cast<int>(r.bottom() + blur * 2));
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const float d = RoundRectDistance(x + 0.5f, y + 0.5f, r, radius);
      if (d <= 0) continue;  // 影は外側だけ (中はパネルで隠れる)
      const float t = d / std::max(blur, 1.0f);
      const float cov = std::exp(-t * t * 2.0f);
      Blend(x, y, c, cov);
    }
  }
}

void Canvas::Glow(float cx, float cy, float radius, Color c) {
  const int x0 = std::max(0, static_cast<int>(cx - radius)), x1 = std::min(width_, static_cast<int>(cx + radius));
  const int y0 = std::max(0, static_cast<int>(cy - radius)), y1 = std::min(height_, static_cast<int>(cy + radius));
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      const float t = std::sqrt(dx * dx + dy * dy) / radius;
      if (t >= 1) continue;
      const float cov = (1 - t * t) * (1 - t * t);  // 中心から外へなめらかに薄くする
      Blend(x, y, c, cov);
    }
  }
}

void Canvas::Text(const std::wstring& text, RectF r, const Font& font, Color c, unsigned align) {
  if (text.empty() || r.w <= 0 || r.h <= 0) return;
  const int w = static_cast<int>(std::ceil(r.w)), h = static_cast<int>(std::ceil(r.h));
  std::vector<uint8_t> cov;
  font.Rasterize(text, w, h, align, &cov);
  if (cov.size() < static_cast<size_t>(w) * h) return;
  const int ox = static_cast<int>(std::floor(r.x)), oy = static_cast<int>(std::floor(r.y));
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t v = cov[static_cast<size_t>(y) * w + x];
      if (v > 0) Blend(ox + x, oy + y, c, v / 255.0f);
    }
  }
}

void Canvas::Image(const std::vector<uint32_t>& argb, int src_w, int src_h, int x, int y, int size) {
  if (src_w <= 0 || src_h <= 0 || argb.size() < static_cast<size_t>(src_w) * src_h || size <= 0) return;
  // 面積平均で縮小する
  for (int dy = 0; dy < size; ++dy) {
    for (int dx = 0; dx < size; ++dx) {
      const int sx0 = dx * src_w / size, sx1 = std::max(sx0 + 1, (dx + 1) * src_w / size);
      const int sy0 = dy * src_h / size, sy1 = std::max(sy0 + 1, (dy + 1) * src_h / size);
      float r = 0, g = 0, b = 0, a = 0;
      int n = 0;
      for (int sy = sy0; sy < sy1; ++sy) {
        for (int sx = sx0; sx < sx1; ++sx) {
          const uint32_t p = argb[static_cast<size_t>(sy) * src_w + sx];
          const float pa = ((p >> 24) & 0xFF) / 255.0f;
          r += ((p >> 16) & 0xFF) / 255.0f * pa;
          g += ((p >> 8) & 0xFF) / 255.0f * pa;
          b += (p & 0xFF) / 255.0f * pa;
          a += pa;
          ++n;
        }
      }
      if (a <= 0) continue;
      Blend(x + dx, y + dy, Color{r / a, g / a, b / a, a / n}, 1);
    }
  }
}

}  // namespace toraui
