#include "candidate_window.h"

#include <algorithm>

namespace toraime {
namespace {

constexpr wchar_t kClassName[] = L"toraIME.CandidateWindow";
constexpr int kPadding = 4;
constexpr int kLabelWidth = 22;
constexpr int kMinWidth = 160;
constexpr int kFontSize = 16;

}  // namespace

CandidateWindow::CandidateWindow(ClickCallback on_click) : on_click_(std::move(on_click)) {}

CandidateWindow::~CandidateWindow() {
  if (hwnd_ != nullptr) {
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
  }
  if (font_ != nullptr) DeleteObject(font_);
}

bool CandidateWindow::EnsureWindow() {
  if (hwnd_ != nullptr) return true;
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW | CS_IME;
    wc.lpfnWndProc = &CandidateWindow::WndProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    registered = true;
  }
  hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"",
                          WS_POPUP | WS_BORDER, 0, 0, 1, 1, nullptr, nullptr, g_instance, nullptr);
  if (hwnd_ == nullptr) return false;
  SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  return true;
}

void CandidateWindow::UnregisterWindowClass() { UnregisterClassW(kClassName, g_instance); }

int CandidateWindow::Scale(int v) const { return MulDiv(v, static_cast<int>(dpi_), 96); }

void CandidateWindow::Show(const RECT& anchor, const std::vector<std::wstring>& items,
                           const std::vector<std::wstring>& notes, int selected, size_t page,
                           size_t page_count) {
  if (items.empty()) {
    Hide();
    return;
  }
  if (!EnsureWindow()) return;
  items_ = items;
  notes_ = notes;
  selected_ = selected;
  page_ = page;
  page_count_ = page_count;

  // 表示先のモニターの DPI
  POINT pt = {anchor.left, anchor.bottom};
  HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  dpi_ = 96;
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  static GetDpiForMonitorFn get_dpi = [] {
    HMODULE shcore = LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return shcore ? reinterpret_cast<GetDpiForMonitorFn>(
                        reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor")))
                  : nullptr;
  }();
  UINT dx = 96, dy = 96;
  if (get_dpi != nullptr && SUCCEEDED(get_dpi(monitor, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy))) dpi_ = dx;

  if (font_ == nullptr || font_dpi_ != dpi_) {
    if (font_ != nullptr) DeleteObject(font_);
    font_ = CreateFontW(-Scale(kFontSize), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
    font_dpi_ = dpi_;
  }

  // 大きさを測る
  HDC hdc = GetDC(hwnd_);
  HGDIOBJ old = SelectObject(hdc, font_);
  TEXTMETRICW tm;
  GetTextMetricsW(hdc, &tm);
  row_height_ = tm.tmHeight + Scale(6);
  int width = Scale(kMinWidth);
  for (size_t i = 0; i < items_.size(); ++i) {
    SIZE sz{};
    std::wstring text = items_[i];
    if (i < notes_.size() && !notes_[i].empty()) text += L"   " + notes_[i];
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &sz);
    width = std::max(width, static_cast<int>(sz.cx) + Scale(kLabelWidth + kPadding * 4));
  }
  SelectObject(hdc, old);
  ReleaseDC(hwnd_, hdc);
  const int height = row_height_ * static_cast<int>(items_.size() + 1) + Scale(kPadding * 2);

  // 画面からはみ出さない位置
  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  GetMonitorInfoW(monitor, &mi);
  int x = anchor.left;
  int y = anchor.bottom + Scale(2);
  if (x + width > mi.rcWork.right) x = mi.rcWork.right - width;
  if (x < mi.rcWork.left) x = mi.rcWork.left;
  if (y + height > mi.rcWork.bottom) y = anchor.top - height - Scale(2);
  if (y < mi.rcWork.top) y = mi.rcWork.top;

  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(hwnd_, nullptr, TRUE);
  visible_ = true;
}

void CandidateWindow::Hide() {
  if (hwnd_ != nullptr && visible_) ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

int CandidateWindow::HitTest(int y) const {
  int row = (y - Scale(kPadding)) / std::max(row_height_, 1);
  return (row >= 0 && row < static_cast<int>(items_.size())) ? row : -1;
}

void CandidateWindow::Paint(HDC hdc) {
  RECT rc;
  GetClientRect(hwnd_, &rc);
  const COLORREF bg = GetSysColor(COLOR_WINDOW);
  const COLORREF fg = GetSysColor(COLOR_WINDOWTEXT);
  const COLORREF sel_bg = GetSysColor(COLOR_HIGHLIGHT);
  const COLORREF sel_fg = GetSysColor(COLOR_HIGHLIGHTTEXT);
  const COLORREF gray = GetSysColor(COLOR_GRAYTEXT);

  HBRUSH bg_brush = CreateSolidBrush(bg);
  FillRect(hdc, &rc, bg_brush);
  DeleteObject(bg_brush);
  HGDIOBJ old = SelectObject(hdc, font_);
  SetBkMode(hdc, TRANSPARENT);

  for (size_t i = 0; i < items_.size(); ++i) {
    RECT row = {rc.left + Scale(kPadding), rc.top + Scale(kPadding) + row_height_ * static_cast<int>(i),
                rc.right - Scale(kPadding), 0};
    row.bottom = row.top + row_height_;
    const bool selected = static_cast<int>(i) == selected_;
    if (selected) {
      HBRUSH b = CreateSolidBrush(sel_bg);
      FillRect(hdc, &row, b);
      DeleteObject(b);
    }
    wchar_t label[4] = {static_cast<wchar_t>(L'1' + i), 0};
    RECT lr = row;
    lr.left += Scale(kPadding);
    SetTextColor(hdc, selected ? sel_fg : gray);
    DrawTextW(hdc, label, -1, &lr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    RECT tr = row;
    tr.left += Scale(kLabelWidth + kPadding);
    SetTextColor(hdc, selected ? sel_fg : fg);
    DrawTextW(hdc, items_[i].c_str(), static_cast<int>(items_[i].size()), &tr,
              DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    if (i < notes_.size() && !notes_[i].empty()) {
      RECT nr = row;
      nr.right -= Scale(kPadding);
      SetTextColor(hdc, selected ? sel_fg : gray);
      DrawTextW(hdc, notes_[i].c_str(), static_cast<int>(notes_[i].size()), &nr,
                DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
    }
  }
  // ページ表示
  wchar_t footer[32];
  wsprintfW(footer, L"%u / %u", static_cast<unsigned>(page_ + 1), static_cast<unsigned>(page_count_));
  RECT fr = {rc.left, rc.bottom - row_height_ - Scale(kPadding), rc.right - Scale(kPadding * 2),
             rc.bottom - Scale(kPadding)};
  SetTextColor(hdc, gray);
  DrawTextW(hdc, footer, -1, &fr, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
  SelectObject(hdc, old);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;  // クリックしてもフォーカスを奪わない
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (self != nullptr) self->Paint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_LBUTTONUP:
      if (self != nullptr && self->on_click_) {
        int row = self->HitTest(static_cast<short>(HIWORD(lp)));
        if (row >= 0) self->on_click_(static_cast<size_t>(row));
      }
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace toraime
