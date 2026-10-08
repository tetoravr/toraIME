// Font の FreeType 実装 (Windows 以外。デザインの確認用ツールで使う)
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <string>

#include "canvas.h"

namespace toraui {

struct Font::Impl {
  FT_Library lib = nullptr;
  FT_Face face = nullptr;
  bool bold = false;
};

Font::Font(const std::wstring& family, float pixel_size, bool bold)
    : impl_(std::make_unique<Impl>()), pixel_size_(pixel_size) {
  impl_->bold = bold;
  if (FT_Init_FreeType(&impl_->lib) != 0) return;
  std::string path(family.begin(), family.end());
  if (FT_New_Face(impl_->lib, path.c_str(), 0, &impl_->face) != 0) {
    impl_->face = nullptr;
    return;
  }
  FT_Set_Pixel_Sizes(impl_->face, 0, static_cast<FT_UInt>(pixel_size + 0.5f));
}

Font::~Font() {
  if (impl_->face) FT_Done_Face(impl_->face);
  if (impl_->lib) FT_Done_FreeType(impl_->lib);
}

SizeI Font::Measure(const std::wstring& text) const {
  if (!impl_->face) return {};
  int w = 0;
  for (wchar_t ch : text) {
    if (FT_Load_Char(impl_->face, static_cast<FT_ULong>(ch), FT_LOAD_DEFAULT) == 0) {
      w += static_cast<int>(impl_->face->glyph->advance.x >> 6) + (impl_->bold ? 1 : 0);
    }
  }
  const int h = static_cast<int>(impl_->face->size->metrics.height >> 6);
  return {w, h};
}

void Font::Rasterize(const std::wstring& text, int w, int h, unsigned align,
                     std::vector<uint8_t>* coverage) const {
  coverage->assign(static_cast<size_t>(w) * h, 0);
  if (!impl_->face) return;
  const SizeI size = Measure(text);
  int pen = align == kAlignCenter ? (w - size.w) / 2 : (align == kAlignRight ? w - size.w : 0);
  const int ascender = static_cast<int>(impl_->face->size->metrics.ascender >> 6);
  const int baseline = (h - size.h) / 2 + ascender;
  for (wchar_t ch : text) {
    if (FT_Load_Char(impl_->face, static_cast<FT_ULong>(ch), FT_LOAD_RENDER) != 0) continue;
    const FT_GlyphSlot g = impl_->face->glyph;
    for (int pass = 0; pass < (impl_->bold ? 2 : 1); ++pass) {  // 太字は 1px ずらして重ねる
      for (unsigned row = 0; row < g->bitmap.rows; ++row) {
        for (unsigned col = 0; col < g->bitmap.width; ++col) {
          const int x = pen + g->bitmap_left + static_cast<int>(col) + pass;
          const int y = baseline - g->bitmap_top + static_cast<int>(row);
          if (x < 0 || y < 0 || x >= w || y >= h) continue;
          uint8_t& d = (*coverage)[static_cast<size_t>(y) * w + x];
          d = std::max(d, g->bitmap.buffer[row * g->bitmap.pitch + col]);
        }
      }
    }
    pen += static_cast<int>(g->advance.x >> 6) + (impl_->bold ? 1 : 0);
  }
}

}  // namespace toraui
