// toraIME の設定アプリ (toraime_settings.exe)
// 設定は HKCU\Software\toraIME に保存する (変更するとすぐ保存)。IME は入力欄にフォーカスが戻ったときに読み直す。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../tsf/settings.h"
#include "canvas.h"
#include "resource.h"
#include "settings_view.h"
#include "theme.h"

namespace toraime {
HINSTANCE g_instance = nullptr;
}

using namespace toraime;

namespace {

constexpr wchar_t kClassName[] = L"toraIME.Settings";
constexpr wchar_t kFont[] = L"Yu Gothic UI";

struct App {
  HWND hwnd = nullptr;
  UINT dpi = 96;
  tora::Config config;
  bool dark = false;
  std::unique_ptr<toraui::SettingsView> view;
  std::unique_ptr<toraui::Canvas> background;  // 背景はキャッシュする
  std::vector<uint32_t> icon;
  int icon_size = 0;
  bool tracking_mouse = false;
};
App g_app;

float Scale() { return static_cast<float>(g_app.dpi) / 96.0f; }

// アイコンをストレートアルファの ARGB にする
std::vector<uint32_t> IconPixels(HICON icon, int size) {
  std::vector<uint32_t> out(static_cast<size_t>(size) * size, 0);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size;
  bmi.bmiHeader.biHeight = -size;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dc = CreateCompatibleDC(nullptr);
  HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp != nullptr && bits != nullptr) {
    std::memset(bits, 0, out.size() * 4);
    HGDIOBJ old = SelectObject(dc, bmp);
    DrawIconEx(dc, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);
    GdiFlush();
    const auto* src = static_cast<const uint32_t*>(bits);
    for (size_t i = 0; i < out.size(); ++i) {
      const uint32_t p = src[i];
      const uint32_t a = p >> 24;
      if (a == 0) continue;
      auto un = [a](uint32_t c) { return std::min<uint32_t>(255, c * 255 / a); };
      out[i] = (a << 24) | (un((p >> 16) & 0xFF) << 16) | (un((p >> 8) & 0xFF) << 8) | un(p & 0xFF);
    }
    SelectObject(dc, old);
    DeleteObject(bmp);
  }
  DeleteDC(dc);
  return out;
}

void ApplyWindowTheme() {
  // タイトルバーも背景の色に合わせる (Windows 11)
  const BOOL dark = g_app.dark ? TRUE : FALSE;
  DwmSetWindowAttribute(g_app.hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
  const toraui::Theme t = toraui::Theme::ForMode(g_app.dark);
  const COLORREF caption = RGB(static_cast<int>(t.bg_top.r * 255), static_cast<int>(t.bg_top.g * 255),
                               static_cast<int>(t.bg_top.b * 255));
  DwmSetWindowAttribute(g_app.hwnd, 35 /* DWMWA_CAPTION_COLOR */, &caption, sizeof(caption));
  const int round = 2;  // DWMWCP_ROUND
  DwmSetWindowAttribute(g_app.hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &round, sizeof(round));
}

void Rebuild() {
  g_app.view = std::make_unique<toraui::SettingsView>(Scale(), kFont, kFont);
  g_app.icon_size = static_cast<int>(52 * Scale());
  HICON icon = static_cast<HICON>(LoadImageW(g_instance, MAKEINTRESOURCEW(IDI_TORAIME), IMAGE_ICON, 256, 256, 0));
  if (icon != nullptr) {
    g_app.view->SetIcon(IconPixels(icon, 256), 256, 256);
    DestroyIcon(icon);
  }
  g_app.background.reset();
}

void Paint(HDC hdc) {
  const toraui::SizeI size = g_app.view->size();
  const toraui::Theme theme = toraui::Theme::ForMode(g_app.dark);
  if (!g_app.background) {
    g_app.background = std::make_unique<toraui::Canvas>(size.w, size.h);
    g_app.view->DrawBackground(*g_app.background, theme);
  }
  toraui::Canvas canvas = *g_app.background;
  g_app.view->Draw(canvas, theme, g_app.config);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size.w;
  bmi.bmiHeader.biHeight = -size.h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (bmp == nullptr || bits == nullptr) return;
  std::memcpy(bits, canvas.pixels(), static_cast<size_t>(size.w) * size.h * 4);
  HDC mem = CreateCompatibleDC(hdc);
  HGDIOBJ old = SelectObject(mem, bmp);
  BitBlt(hdc, 0, 0, size.w, size.h, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old);
  DeleteDC(mem);
  DeleteObject(bmp);
}

void OpenWithNotepad(const std::filesystem::path& path) {
  if (path.empty()) {
    MessageBoxW(g_app.hwnd, L"ユーザーデータのフォルダーを開けませんでした。", L"toraIME", MB_ICONWARNING);
    return;
  }
  ShellExecuteW(g_app.hwnd, L"open", L"notepad.exe", (L"\"" + path.wstring() + L"\"").c_str(), nullptr,
                SW_SHOWNORMAL);
}

void ClearHistory() {
  if (MessageBoxW(g_app.hwnd, L"学習履歴をすべて消去しますか?", L"toraIME", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) {
    return;
  }
  const std::filesystem::path dir = UserDataDirectory();
  std::error_code ec;
  if (!dir.empty()) std::filesystem::remove(dir / L"history.tsv", ec);
  BumpHistoryGeneration();  // 起動中のアプリの IME にも消去を伝える
  MessageBoxW(g_app.hwnd, L"学習履歴を消去しました。", L"toraIME", MB_ICONINFORMATION);
}

void HandleAction(toraui::SettingsView::Action action) {
  using A = toraui::SettingsView::Action;
  switch (action) {
    case A::kChanged:
      SaveConfig(g_app.config);
      break;
    case A::kOpenUserDict:
      OpenWithNotepad(EnsureUserFile(L"user_dict.txt"));
      break;
    case A::kOpenUserEnglish:
      OpenWithNotepad(EnsureUserFile(L"user_english.txt"));
      break;
    case A::kClearHistory:
      ClearHistory();
      break;
    case A::kClose:
      DestroyWindow(g_app.hwnd);
      return;
    case A::kNone:
      break;
  }
  InvalidateRect(g_app.hwnd, nullptr, FALSE);
}

void ResizeToContent(const RECT* suggested) {
  const toraui::SizeI size = g_app.view->size();
  RECT rc = {0, 0, size.w, size.h};
  const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(g_app.hwnd, GWL_STYLE));
  AdjustWindowRectExForDpi(&rc, style, FALSE, 0, g_app.dpi);
  int x = suggested ? suggested->left : 0, y = suggested ? suggested->top : 0;
  SetWindowPos(g_app.hwnd, nullptr, x, y, rc.right - rc.left, rc.bottom - rc.top,
               SWP_NOZORDER | SWP_NOACTIVATE | (suggested ? 0 : SWP_NOMOVE));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  using Key = toraui::SettingsView::Key;
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (g_app.view) Paint(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_MOUSEMOVE:
      if (!g_app.tracking_mouse) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        g_app.tracking_mouse = true;
      }
      if (g_app.view->Hover(static_cast<float>(GET_X_LPARAM(lp)), static_cast<float>(GET_Y_LPARAM(lp)))) {
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    case WM_MOUSELEAVE:
      g_app.tracking_mouse = false;
      if (g_app.view->Hover(-1, -1)) InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_SETCURSOR:
      if (LOWORD(lp) == HTCLIENT) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return TRUE;
      }
      break;
    case WM_LBUTTONUP:
      HandleAction(g_app.view->Click(static_cast<float>(GET_X_LPARAM(lp)), static_cast<float>(GET_Y_LPARAM(lp)),
                                     &g_app.config));
      return 0;
    case WM_KEYDOWN: {
      const bool shift = GetKeyState(VK_SHIFT) < 0;
      switch (wp) {
        case VK_TAB: HandleAction(g_app.view->KeyPress(shift ? Key::kShiftTab : Key::kTab, &g_app.config)); return 0;
        case VK_SPACE:
        case VK_RETURN: HandleAction(g_app.view->KeyPress(Key::kActivate, &g_app.config)); return 0;
        case VK_LEFT: HandleAction(g_app.view->KeyPress(Key::kLeft, &g_app.config)); return 0;
        case VK_RIGHT: HandleAction(g_app.view->KeyPress(Key::kRight, &g_app.config)); return 0;
        case VK_ESCAPE: DestroyWindow(hwnd); return 0;
        default: break;
      }
      break;
    }
    case WM_DPICHANGED:
      g_app.dpi = HIWORD(wp);
      Rebuild();
      ResizeToContent(reinterpret_cast<const RECT*>(lp));
      InvalidateRect(hwnd, nullptr, FALSE);
      return 0;
    case WM_SETTINGCHANGE:
      // ライト/ダークの切り替え
      if (lp != 0 && wcscmp(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0) {
        g_app.dark = toraui::IsAppDarkMode();
        g_app.background.reset();
        ApplyWindowTheme();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      break;
    case WM_ACTIVATE:
      // 他のアプリで設定が変わっているかもしれない
      if (LOWORD(wp) != WA_INACTIVE) {
        g_app.config = LoadConfig();
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  g_instance = instance;

  // 二重起動しない
  if (HWND existing = FindWindowW(kClassName, nullptr)) {
    ShowWindow(existing, SW_RESTORE);
    SetForegroundWindow(existing);
    return 0;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_TORAIME));
  wc.lpszClassName = kClassName;
  RegisterClassExW(&wc);

  g_app.config = LoadConfig();
  g_app.dark = toraui::IsAppDarkMode();
  g_app.dpi = GetDpiForSystem();
  Rebuild();

  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  g_app.hwnd = CreateWindowExW(0, kClassName, L"toraIME の設定", style, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                               nullptr, nullptr, instance, nullptr);
  if (g_app.hwnd == nullptr) return 1;
  // 表示するモニターの DPI に合わせる
  const UINT dpi = GetDpiForWindow(g_app.hwnd);
  if (dpi != g_app.dpi) {
    g_app.dpi = dpi;
    Rebuild();
  }
  ApplyWindowTheme();
  ResizeToContent(nullptr);
  ShowWindow(g_app.hwnd, show);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}
