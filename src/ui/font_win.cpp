// Font の Windows (GDI) 実装
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>

#include "canvas.h"

namespace toraui {

struct Font::Impl {
  HFONT font = nullptr;
};

Font::Font(const std::wstring& family, float pixel_size, bool bold)
    : impl_(std::make_unique<Impl>()), pixel_size_(pixel_size) {
  impl_->font = CreateFontW(-static_cast<int>(pixel_size + 0.5f), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE,
                            FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, family.c_str());
}

Font::~Font() {
  if (impl_->font != nullptr) DeleteObject(impl_->font);
}

SizeI Font::Measure(const std::wstring& text) const {
  SIZE sz{};
  HDC dc = CreateCompatibleDC(nullptr);
  HGDIOBJ old = SelectObject(dc, impl_->font);
  if (text.empty()) {
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    sz.cy = tm.tmHeight;
  } else {
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
  }
  SelectObject(dc, old);
  DeleteDC(dc);
  return {static_cast<int>(sz.cx), static_cast<int>(sz.cy)};
}

void Font::Rasterize(const std::wstring& text, int w, int h, unsigned align,
                     std::vector<uint8_t>* coverage) const {
  coverage->assign(static_cast<size_t>(w) * h, 0);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dc = CreateCompatibleDC(nullptr);
  HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp == nullptr || bits == nullptr) {
    DeleteDC(dc);
    return;
  }
  std::memset(bits, 0, static_cast<size_t>(w) * h * 4);
  HGDIOBJ old_bmp = SelectObject(dc, bmp);
  HGDIOBJ old_font = SelectObject(dc, impl_->font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));
  RECT rc = {0, 0, w, h};
  UINT fmt = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
  fmt |= align == kAlignCenter ? DT_CENTER : (align == kAlignRight ? DT_RIGHT : DT_LEFT);
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, fmt);
  GdiFlush();
  const auto* src = static_cast<const uint32_t*>(bits);
  for (size_t i = 0; i < coverage->size(); ++i) (*coverage)[i] = static_cast<uint8_t>((src[i] >> 8) & 0xFF);
  SelectObject(dc, old_font);
  SelectObject(dc, old_bmp);
  DeleteObject(bmp);
  DeleteDC(dc);
}

}  // namespace toraui

#include "theme.h"

namespace toraui {

bool IsAppDarkMode() {
  DWORD value = 1;
  DWORD size = sizeof(value);
  RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
  return value == 0;
}

}  // namespace toraui
