#include "candidate_window.h"

#include <algorithm>
#include <cstring>

#include "canvas.h"
#include "theme.h"

namespace toraime {
namespace {

constexpr wchar_t kClassName[] = L"toraIME.CandidateWindow";
constexpr wchar_t kFontFamily[] = L"Yu Gothic UI";

UINT MonitorDpi(HMONITOR monitor) {
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  static GetDpiForMonitorFn get_dpi = [] {
    HMODULE shcore = LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return shcore ? reinterpret_cast<GetDpiForMonitorFn>(
                        reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor")))
                  : nullptr;
  }();
  UINT dx = 96, dy = 96;
  if (get_dpi != nullptr && SUCCEEDED(get_dpi(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy))) return dx;
  return 96;
}

}  // namespace

CandidateWindow::CandidateWindow(ClickCallback on_click) : on_click_(std::move(on_click)) {}

CandidateWindow::~CandidateWindow() {
  if (hwnd_ != nullptr) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
  }
}

void CandidateWindow::UnregisterWindowClass() { UnregisterClassW(kClassName, g_instance); }

bool CandidateWindow::EnsureWindow() {
  if (hwnd_ != nullptr) return true;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_IME;
    wc.lpfnWndProc = &CandidateWindow::WndProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
  }
  hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"",
                          WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, g_instance, nullptr);
  if (hwnd_ == nullptr) return false;
  SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  return true;
}

void CandidateWindow::Show(const RECT& anchor, const std::vector<std::wstring>& items,
                           const std::vector<std::wstring>& notes, int selected, size_t page,
                           size_t page_count) {
  if (items.empty()) {
    Hide();
    return;
  }
  hint_ = false;
  model_.items = items;
  model_.notes = notes;
  model_.selected = selected;
  model_.page = page;
  model_.page_count = page_count;
  Render(anchor);
}

void CandidateWindow::ShowHint(const RECT& anchor, const std::wstring& text) {
  if (text.empty()) {
    Hide();
    return;
  }
  hint_ = true;
  hint_text_ = text;
  Render(anchor);
}

void CandidateWindow::Render(const RECT& anchor) {
  if (!EnsureWindow()) return;
  POINT pt = {anchor.left, anchor.bottom};
  HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  const float scale = static_cast<float>(MonitorDpi(monitor)) / 96.0f;
  if (!fonts_ || scale != scale_) {
    scale_ = scale;
    fonts_ = std::make_unique<toraui::Fonts>(kFontFamily, scale);
    view_ = std::make_unique<toraui::CandidateView>(scale);
  }

  const toraui::Theme theme = toraui::Theme::ForMode(toraui::IsAppDarkMode());
  const toraui::SizeI size = hint_ ? view_->LayoutHint(hint_text_, *fonts_) : view_->Layout(model_, *fonts_);
  toraui::Canvas canvas(size.w, size.h);
  if (hint_) {
    view_->DrawHint(canvas, hint_text_, *fonts_, theme);
  } else {
    view_->Draw(canvas, model_, *fonts_, theme);
  }

  // パネルの左上を文字の左下に合わせる (影の余白の分だけずらす)。画面からはみ出さないようにする
  const int m = static_cast<int>(view_->margin());
  const int gap = static_cast<int>(4 * scale);
  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  GetMonitorInfoW(monitor, &mi);
  const int panel_w = size.w - 2 * m, panel_h = size.h - 2 * m;
  int x = anchor.left - static_cast<int>(6 * scale);
  int y = anchor.bottom + gap;
  if (x + panel_w > mi.rcWork.right) x = mi.rcWork.right - panel_w;
  if (x < mi.rcWork.left) x = mi.rcWork.left;
  if (y + panel_h > mi.rcWork.bottom) y = anchor.top - gap - panel_h;
  if (y < mi.rcWork.top) y = mi.rcWork.top;

  HDC screen = GetDC(nullptr);
  HDC mem = CreateCompatibleDC(screen);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size.w;
  bmi.bmiHeader.biHeight = -size.h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp != nullptr && bits != nullptr) {
    memcpy(bits, canvas.pixels(), static_cast<size_t>(size.w) * size.h * 4);
    HGDIOBJ old = SelectObject(mem, bmp);
    POINT dst = {x - m, y - m};
    SIZE sz = {size.w, size.h};
    POINT src = {0, 0};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, screen, &dst, &sz, mem, &src, 0, &blend, ULW_ALPHA);
    SelectObject(mem, old);
    DeleteObject(bmp);
  }
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  visible_ = true;
}

void CandidateWindow::Hide() {
  if (hwnd_ != nullptr && visible_) ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;  // クリックしてもフォーカスを奪わない
    case WM_LBUTTONUP:
      if (self != nullptr && self->on_click_ && !self->hint_ && self->view_) {
        const int row = self->view_->HitTest(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
        if (row >= 0) self->on_click_(static_cast<size_t>(row));
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace toraime
