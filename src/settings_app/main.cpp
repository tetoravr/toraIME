// toraIME の設定アプリ (toraime_settings.exe)
// 設定は HKCU\Software\toraIME に保存する。IME は入力欄にフォーカスが戻ったときに読み直す。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <vector>

#include "../tsf/settings.h"
#include "resource.h"

namespace toraime {
HINSTANCE g_instance = nullptr;
}

using namespace toraime;

namespace {

enum ControlId : int {
  kIdLive = 100,
  kIdReadingHint,
  kIdEnglish,
  kIdLearning,
  kIdConvertKeys,
  kIdCapsLock,
  kIdKanaInput,
  kIdFullWidth,
  kIdHalfKana,
  kIdPunctuation,
  kIdUserDict,
  kIdUserEnglish,
  kIdClearHistory,
  kIdOk = IDOK,
  kIdCancel = IDCANCEL,
  kIdApply = 200,
};

struct App {
  HWND hwnd = nullptr;
  HFONT font = nullptr;
  UINT dpi = 96;
  int Scale(int v) const { return MulDiv(v, static_cast<int>(dpi), 96); }
};
App g_app;

HWND Item(int id) { return GetDlgItem(g_app.hwnd, id); }

HWND AddControl(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
  HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, g_app.Scale(x), g_app.Scale(y),
                           g_app.Scale(w), g_app.Scale(h), g_app.hwnd,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
  SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.font), TRUE);
  return c;
}

void AddCheck(int id, const wchar_t* text, int x, int y, int w = 400) {
  AddControl(L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, x, y, w, 22, id);
}

void AddCombo(int id, const wchar_t* label, const std::vector<const wchar_t*>& items, int x, int y) {
  AddControl(L"STATIC", label, SS_LEFT | SS_CENTERIMAGE, x, y, 120, 24, 0);
  HWND combo = AddControl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, x + 124, y, 260, 200, id);
  for (const wchar_t* item : items) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
}

void AddGroup(const wchar_t* text, int x, int y, int w, int h) {
  AddControl(L"BUTTON", text, BS_GROUPBOX, x, y, w, h, 0);
}

void SetCheck(int id, bool on) { SendMessageW(Item(id), BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
bool GetCheck(int id) { return SendMessageW(Item(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
void SetCombo(int id, int index) { SendMessageW(Item(id), CB_SETCURSEL, static_cast<WPARAM>(index), 0); }
int GetCombo(int id) { return static_cast<int>(SendMessageW(Item(id), CB_GETCURSEL, 0, 0)); }

// 表示の順番と設定値の対応
const tora::LiveConversion kLiveOrder[] = {tora::LiveConversion::kFull,
                                           tora::LiveConversion::kKeepLastSegment,
                                           tora::LiveConversion::kOff};

void Load() {
  const tora::Config c = LoadConfig();
  for (int i = 0; i < 3; ++i) {
    if (kLiveOrder[i] == c.live_conversion) SetCombo(kIdLive, i);
  }
  SetCheck(kIdReadingHint, c.reading_hint);
  SetCheck(kIdEnglish, c.english_detection);
  SetCheck(kIdLearning, c.learning);
  SetCheck(kIdConvertKeys, c.convert_keys_on_off);
  SetCheck(kIdCapsLock, c.caps_lock_disabled);
  SetCheck(kIdKanaInput, c.kana_input_enabled);
  SetCombo(kIdFullWidth, static_cast<int>(c.full_width));
  SetCheck(kIdHalfKana, c.half_width_kana_enabled);
  SetCombo(kIdPunctuation, static_cast<int>(c.punctuation));
}

void Save() {
  tora::Config c = LoadConfig();
  int live = GetCombo(kIdLive);
  if (live >= 0 && live < 3) c.live_conversion = kLiveOrder[live];
  c.reading_hint = GetCheck(kIdReadingHint);
  c.english_detection = GetCheck(kIdEnglish);
  c.learning = GetCheck(kIdLearning);
  c.convert_keys_on_off = GetCheck(kIdConvertKeys);
  c.caps_lock_disabled = GetCheck(kIdCapsLock);
  c.kana_input_enabled = GetCheck(kIdKanaInput);
  int fw = GetCombo(kIdFullWidth);
  if (fw >= 0 && fw <= 2) c.full_width = static_cast<tora::FullWidthMode>(fw);
  c.half_width_kana_enabled = GetCheck(kIdHalfKana);
  int punct = GetCombo(kIdPunctuation);
  if (punct >= 0 && punct <= 3) c.punctuation = static_cast<tora::PunctuationStyle>(punct);
  SaveConfig(c);
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
  if (MessageBoxW(g_app.hwnd, L"学習履歴をすべて消去しますか?", L"toraIME",
                  MB_OKCANCEL | MB_ICONQUESTION) != IDOK) {
    return;
  }
  const std::filesystem::path dir = UserDataDirectory();
  std::error_code ec;
  if (!dir.empty()) std::filesystem::remove(dir / L"history.tsv", ec);
  BumpHistoryGeneration();  // 起動中のアプリの IME にも消去を伝える
  MessageBoxW(g_app.hwnd, L"学習履歴を消去しました。", L"toraIME", MB_ICONINFORMATION);
}

void CreateControls() {
  int y = 12;
  AddGroup(L"入力", 12, y, 420, 132);
  AddCombo(kIdLive, L"入力中の表示",
           {L"すべて自動で変換する", L"入力中の文節はひらがなのまま", L"変換しない (Space で変換)"}, 24, y + 22);
  AddCheck(kIdReadingHint, L"入力した読みを下に表示する", 24, y + 52);
  AddCheck(kIdEnglish, L"英単語は英字のまま入力する", 24, y + 76);
  AddCheck(kIdLearning, L"変換を学習する", 24, y + 100);
  y += 144;
  AddGroup(L"キー", 12, y, 420, 104);
  AddCheck(kIdConvertKeys, L"変換キーでオン / 無変換キーでオフ", 24, y + 24);
  AddCheck(kIdCapsLock, L"CapsLock を無効にする", 24, y + 48);
  AddCheck(kIdKanaInput, L"かな入力を使えるようにする (Alt+カタカナひらがな)", 24, y + 72);
  y += 116;
  AddGroup(L"文字", 12, y, 420, 112);
  AddCombo(kIdFullWidth, L"全角英数字",
           {L"使わない (常に半角)", L"候補と F9 だけで使う", L"入力した英数字を全角にする"}, 24, y + 22);
  AddCheck(kIdHalfKana, L"半角カタカナを使う", 24, y + 52);
  AddCombo(kIdPunctuation, L"句読点", {L"、。", L"，．", L"，。", L"、．"}, 24, y + 78);
  y += 124;
  AddGroup(L"辞書", 12, y, 420, 60);
  AddControl(L"BUTTON", L"ユーザー辞書を開く", BS_PUSHBUTTON | WS_TABSTOP, 24, y + 22, 128, 28, kIdUserDict);
  AddControl(L"BUTTON", L"英単語リストを開く", BS_PUSHBUTTON | WS_TABSTOP, 160, y + 22, 128, 28, kIdUserEnglish);
  AddControl(L"BUTTON", L"学習履歴を消去", BS_PUSHBUTTON | WS_TABSTOP, 296, y + 22, 124, 28, kIdClearHistory);
  y += 72;
  AddControl(L"BUTTON", L"OK", BS_DEFPUSHBUTTON | WS_TABSTOP, 172, y, 82, 28, kIdOk);
  AddControl(L"BUTTON", L"キャンセル", BS_PUSHBUTTON | WS_TABSTOP, 262, y, 82, 28, kIdCancel);
  AddControl(L"BUTTON", L"適用", BS_PUSHBUTTON | WS_TABSTOP, 352, y, 80, 28, kIdApply);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case kIdOk:
          Save();
          DestroyWindow(hwnd);
          return 0;
        case kIdCancel:
          DestroyWindow(hwnd);
          return 0;
        case kIdApply:
          Save();
          return 0;
        case kIdUserDict:
          OpenWithNotepad(EnsureUserFile(L"user_dict.txt"));
          return 0;
        case kIdUserEnglish:
          OpenWithNotepad(EnsureUserFile(L"user_english.txt"));
          return 0;
        case kIdClearHistory:
          ClearHistory();
          return 0;
        default:
          break;
      }
      break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
      SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
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
  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);

  // 二重起動しない
  HWND existing = FindWindowW(L"toraIME.Settings", nullptr);
  if (existing != nullptr) {
    SetForegroundWindow(existing);
    return 0;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
  wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_TORAIME));
  wc.lpszClassName = L"toraIME.Settings";
  RegisterClassExW(&wc);

  g_app.dpi = GetDpiForSystem();
  g_app.font = CreateFontW(-g_app.Scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
  RECT rc = {0, 0, g_app.Scale(444), g_app.Scale(536)};
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRectExForDpi(&rc, style, FALSE, 0, g_app.dpi);
  g_app.hwnd = CreateWindowExW(0, wc.lpszClassName, L"toraIME の設定", style, CW_USEDEFAULT, CW_USEDEFAULT,
                               rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, instance, nullptr);
  if (g_app.hwnd == nullptr) return 1;
  CreateControls();
  Load();
  ShowWindow(g_app.hwnd, show);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (IsDialogMessageW(g_app.hwnd, &msg)) continue;
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  DeleteObject(g_app.font);
  return 0;
}
