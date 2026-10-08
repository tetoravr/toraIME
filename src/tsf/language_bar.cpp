#include "language_bar.h"

#include <shellapi.h>

#include <cwchar>
#include <string>

#include "settings.h"
#include "text_service.h"

namespace toraime {
namespace {

constexpr DWORD kSinkCookie = 0x746f7261;  // 1 つしか持たないので固定値

enum MenuId : UINT {
  kMenuHiragana = 1,
  kMenuFullAlnum,
  kMenuHalfAlnum,
  kMenuReadingHint = 10,
  kMenuEnglish,
  kMenuKanaInput,
  kMenuHalfKana,
  kMenuLearning,
  kMenuConvertKeys,
  kMenuCapsLock,
  kMenuFullWidthOff = 20,
  kMenuFullWidthCandidates,
  kMenuFullWidthDefault,
  kMenuPunct0 = 30,
  kMenuPunct1,
  kMenuPunct2,
  kMenuPunct3,
  kMenuLiveFull = 35,
  kMenuLiveKeepLast,
  kMenuLiveOff,
  kMenuUserDict = 40,
  kMenuUserEnglish,
  kMenuClearHistory,
  kMenuSettings,
};

// ITfMenu を Win32 のメニューに作るアダプター。
// Windows 8 以降のタスクバーの入力モード表示は右クリックで OnClick を呼ぶだけなので、
// メニューは IME が自分で TrackPopupMenu で出す必要がある。
class Win32Menu : public ITfMenu {
 public:
  explicit Win32Menu(HMENU menu) : menu_(menu) {}
  virtual ~Win32Menu() = default;

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfMenu)) {
      *ppv = static_cast<ITfMenu*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return ++ref_; }
  STDMETHODIMP_(ULONG) Release() override {
    ULONG r = --ref_;
    if (r == 0) delete this;
    return r;
  }
  STDMETHODIMP AddMenuItem(UINT id, DWORD flags, HBITMAP, HBITMAP, const WCHAR* text, ULONG cch,
                           ITfMenu** sub) override {
    if (sub != nullptr) *sub = nullptr;
    const std::wstring label(text != nullptr ? text : L"", cch);
    if (flags & TF_LBMENUF_SEPARATOR) {
      AppendMenuW(menu_, MF_SEPARATOR, 0, nullptr);
      return S_OK;
    }
    MENUITEMINFOW mii{};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_STRING;
    mii.fType = (flags & TF_LBMENUF_RADIOCHECKED) ? MFT_RADIOCHECK : MFT_STRING;
    mii.fState = ((flags & (TF_LBMENUF_CHECKED | TF_LBMENUF_RADIOCHECKED)) ? MFS_CHECKED : 0) |
                 ((flags & TF_LBMENUF_GRAYED) ? MFS_GRAYED : 0);
    mii.wID = id;
    mii.dwTypeData = const_cast<wchar_t*>(label.c_str());
    if (flags & TF_LBMENUF_SUBMENU) {
      HMENU popup = CreatePopupMenu();
      mii.fMask |= MIIM_SUBMENU;
      mii.hSubMenu = popup;
      if (sub != nullptr) *sub = new Win32Menu(popup);
    }
    InsertMenuItemW(menu_, GetMenuItemCount(menu_), TRUE, &mii);
    return S_OK;
  }

 private:
  std::atomic<ULONG> ref_{1};
  HMENU menu_;
};

constexpr wchar_t kMenuOwnerClass[] = L"toraIME.MenuOwner";

HWND MenuOwnerWindow() {
  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = g_instance;
    wc.lpszClassName = kMenuOwnerClass;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return nullptr;
    registered = true;
  }
  return CreateWindowExW(WS_EX_TOOLWINDOW, kMenuOwnerClass, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                         g_instance, nullptr);
}

void AddItem(ITfMenu* menu, UINT id, const wchar_t* text, DWORD flags = 0, ITfMenu** sub = nullptr) {
  menu->AddMenuItem(id, flags, nullptr, nullptr, text, static_cast<ULONG>(wcslen(text)), sub);
}

DWORD Check(bool on) { return on ? TF_LBMENUF_CHECKED : 0; }
DWORD Radio(bool on) { return on ? TF_LBMENUF_RADIOCHECKED : 0; }

bool TaskbarUsesLightTheme() {
  DWORD value = 0;
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) {
    return value != 0;
  }
  return false;
}

void OpenWithNotepad(const std::filesystem::path& path) {
  if (path.empty()) return;
  ShellExecuteW(nullptr, L"open", L"notepad.exe", (L"\"" + path.wstring() + L"\"").c_str(), nullptr,
                SW_SHOWNORMAL);
}

}  // namespace

HICON CreateLabelIcon(const wchar_t* label) {
  const int size = GetSystemMetrics(SM_CXSMICON);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = size;
  bmi.bmiHeader.biHeight = -size;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  HBITMAP color = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, screen);
  if (color == nullptr || bits == nullptr) {
    DeleteDC(dc);
    return nullptr;
  }
  // 黒地に白で文字を描き、明るさをアルファ値にする
  HGDIOBJ old_bmp = SelectObject(dc, color);
  RECT rc = {0, 0, size, size};
  FillRect(dc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  HFONT font = CreateFontW(-size * 7 / 8, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
  HGDIOBJ old_font = SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));
  DrawTextW(dc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(dc, old_font);
  DeleteObject(font);
  SelectObject(dc, old_bmp);
  DeleteDC(dc);
  GdiFlush();

  const bool light = TaskbarUsesLightTheme();
  const BYTE ink = light ? 0 : 255;
  auto* px = static_cast<BYTE*>(bits);
  for (int i = 0; i < size * size; ++i) {
    BYTE a = px[i * 4 + 1];  // 緑の値を明るさとして使う
    // 乗算済みアルファ
    BYTE c = static_cast<BYTE>(ink * a / 255);
    px[i * 4 + 0] = c;
    px[i * 4 + 1] = c;
    px[i * 4 + 2] = c;
    px[i * 4 + 3] = a;
  }
  HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
  ICONINFO ii{};
  ii.fIcon = TRUE;
  ii.hbmColor = color;
  ii.hbmMask = mask;
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(color);
  DeleteObject(mask);
  return icon;
}

void UnregisterMenuOwnerClass() { UnregisterClassW(kMenuOwnerClass, g_instance); }

LanguageBarButton::LanguageBarButton(TextService* service) : service_(service) { DllAddRef(); }

LanguageBarButton::~LanguageBarButton() { DllRelease(); }

void LanguageBarButton::Update() {
  if (sink_) sink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_TOOLTIP);
}

STDMETHODIMP LanguageBarButton::QueryInterface(REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfLangBarItem) ||
      IsEqualIID(riid, kIID_ITfLangBarItemButton)) {
    *ppv = static_cast<ITfLangBarItemButton*>(this);
  } else if (IsEqualIID(riid, kIID_ITfSource)) {
    *ppv = static_cast<ITfSource*>(this);
  } else {
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) LanguageBarButton::AddRef() { return ++ref_; }

STDMETHODIMP_(ULONG) LanguageBarButton::Release() {
  ULONG r = --ref_;
  if (r == 0) delete this;
  return r;
}

STDMETHODIMP LanguageBarButton::GetInfo(TF_LANGBARITEMINFO* info) {
  if (info == nullptr) return E_INVALIDARG;
  info->clsidService = kClsidTextService;
  info->guidItem = kGuid_LBI_INPUTMODE;
  info->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
  info->ulSort = 0;
  lstrcpynW(info->szDescription, L"toraIME 入力モード", TF_LBI_DESC_MAXLEN);
  return S_OK;
}

STDMETHODIMP LanguageBarButton::GetStatus(DWORD* status) {
  if (status == nullptr) return E_INVALIDARG;
  *status = 0;
  return S_OK;
}

STDMETHODIMP LanguageBarButton::Show(BOOL) { return E_NOTIMPL; }

STDMETHODIMP LanguageBarButton::GetTooltipString(BSTR* tooltip) {
  if (tooltip == nullptr) return E_INVALIDARG;
  std::wstring text = L"toraIME - ";
  text += service_ ? service_->ModeDescription() : L"";
  *tooltip = SysAllocString(text.c_str());
  return *tooltip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LanguageBarButton::OnClick(TfLBIClick click, POINT pt, const RECT*) {
  if (service_ == nullptr) return S_OK;
  if (click == TF_LBI_CLK_LEFT) {
    service_->ToggleOpen();
  } else if (click == TF_LBI_CLK_RIGHT) {
    ShowPopupMenu(pt);
  }
  return S_OK;
}

void LanguageBarButton::ShowPopupMenu(POINT pt) {
  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) return;
  Win32Menu* adapter = new Win32Menu(menu);
  InitMenu(adapter);
  adapter->Release();
  HWND owner = MenuOwnerWindow();
  if (owner != nullptr) {
    ComPtr<LanguageBarButton> self(this);  // メニュー表示中に解放されないように
    SetForegroundWindow(owner);
    UINT cmd = static_cast<UINT>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y,
        owner, nullptr));
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyWindow(owner);
    if (cmd != 0) OnMenuSelect(cmd);
  }
  DestroyMenu(menu);  // サブメニューもまとめて破棄される
}

STDMETHODIMP LanguageBarButton::InitMenu(ITfMenu* menu) {
  if (menu == nullptr || service_ == nullptr) return E_INVALIDARG;
  const tora::Config& c = GetEngine()->config();
  const tora::InputMode mode = service_->CurrentMode();
  const bool open = service_->IsOpen();

  AddItem(menu, kMenuHiragana, L"ひらがな", Radio(open && mode == tora::InputMode::kHiragana));
  AddItem(menu, kMenuFullAlnum, L"全角英数",
          Radio(open && mode == tora::InputMode::kFullWidthAlnum) |
              (c.full_width_allowed() ? 0 : TF_LBMENUF_GRAYED));
  AddItem(menu, kMenuHalfAlnum, L"半角英数 (IME オフ)", Radio(!open || mode == tora::InputMode::kHalfWidthAlnum));
  AddItem(menu, 0, L"", TF_LBMENUF_SEPARATOR);

  ITfMenu* live = nullptr;
  AddItem(menu, 0, L"入力中の表示", TF_LBMENUF_SUBMENU, &live);
  if (live != nullptr) {
    AddItem(live, kMenuLiveFull, L"すべて自動で変換する",
            Radio(c.live_conversion == tora::LiveConversion::kFull));
    AddItem(live, kMenuLiveKeepLast, L"入力中の文節はひらがなのまま",
            Radio(c.live_conversion == tora::LiveConversion::kKeepLastSegment));
    AddItem(live, kMenuLiveOff, L"変換しない (Space で変換)",
            Radio(c.live_conversion == tora::LiveConversion::kOff));
    live->Release();
  }
  AddItem(menu, kMenuReadingHint, L"入力した読みを下に表示する",
          Check(c.reading_hint) | (c.live_conversion == tora::LiveConversion::kOff ? TF_LBMENUF_GRAYED : 0));
  AddItem(menu, kMenuEnglish, L"英単語は英字のまま入力する", Check(c.english_detection));
  AddItem(menu, kMenuLearning, L"変換を学習する", Check(c.learning));
  AddItem(menu, kMenuConvertKeys, L"変換キーでオン / 無変換キーでオフ", Check(c.convert_keys_on_off));
  AddItem(menu, kMenuCapsLock, L"CapsLock を無効にする", Check(c.caps_lock_disabled));
  AddItem(menu, 0, L"", TF_LBMENUF_SEPARATOR);

  ITfMenu* sub = nullptr;
  AddItem(menu, 0, L"全角英数字", TF_LBMENUF_SUBMENU, &sub);
  if (sub != nullptr) {
    AddItem(sub, kMenuFullWidthOff, L"使わない (常に半角)", Radio(c.full_width == tora::FullWidthMode::kDisabled));
    AddItem(sub, kMenuFullWidthCandidates, L"候補と F9 だけで使う",
            Radio(c.full_width == tora::FullWidthMode::kCandidatesOnly));
    AddItem(sub, kMenuFullWidthDefault, L"入力した英数字を全角にする",
            Radio(c.full_width == tora::FullWidthMode::kDefault));
    sub->Release();
  }
  AddItem(menu, kMenuKanaInput, L"かな入力を使えるようにする (Alt+カタカナひらがな)", Check(c.kana_input_enabled));
  AddItem(menu, kMenuHalfKana, L"半角カタカナを使う", Check(c.half_width_kana_enabled));
  sub = nullptr;
  AddItem(menu, 0, L"句読点", TF_LBMENUF_SUBMENU, &sub);
  if (sub != nullptr) {
    const wchar_t* labels[] = {L"、。", L"，．", L"，。", L"、．"};
    for (int i = 0; i < 4; ++i) {
      AddItem(sub, kMenuPunct0 + i, labels[i], Radio(static_cast<int>(c.punctuation) == i));
    }
    sub->Release();
  }
  AddItem(menu, 0, L"", TF_LBMENUF_SEPARATOR);
  const bool has_user_dir = !UserDataDirectory().empty();
  const DWORD user_flags = has_user_dir ? 0 : TF_LBMENUF_GRAYED;
  AddItem(menu, kMenuUserDict, L"ユーザー辞書を開く", user_flags);
  AddItem(menu, kMenuUserEnglish, L"英単語リストを開く", user_flags);
  AddItem(menu, kMenuClearHistory, L"学習履歴を消去", user_flags);
  AddItem(menu, 0, L"", TF_LBMENUF_SEPARATOR);
  AddItem(menu, kMenuSettings, L"設定を開く...");
  return S_OK;
}

STDMETHODIMP LanguageBarButton::OnMenuSelect(UINT id) {
  if (service_ == nullptr) return S_OK;
  tora::Engine* engine = GetEngine();
  tora::Config c = engine->config();
  bool changed = true;
  switch (id) {
    case kMenuHiragana: service_->SetMode(true, tora::InputMode::kHiragana); changed = false; break;
    case kMenuFullAlnum: service_->SetMode(true, tora::InputMode::kFullWidthAlnum); changed = false; break;
    case kMenuHalfAlnum: service_->SetMode(false, tora::InputMode::kHiragana); changed = false; break;
    case kMenuReadingHint: c.reading_hint = !c.reading_hint; break;
    case kMenuLiveFull: c.live_conversion = tora::LiveConversion::kFull; break;
    case kMenuLiveKeepLast: c.live_conversion = tora::LiveConversion::kKeepLastSegment; break;
    case kMenuLiveOff: c.live_conversion = tora::LiveConversion::kOff; break;
    case kMenuEnglish: c.english_detection = !c.english_detection; break;
    case kMenuLearning: c.learning = !c.learning; break;
    case kMenuConvertKeys: c.convert_keys_on_off = !c.convert_keys_on_off; break;
    case kMenuCapsLock: c.caps_lock_disabled = !c.caps_lock_disabled; break;
    case kMenuSettings:
      ShellExecuteW(nullptr, L"open", (DataDirectory() / L"toraime_settings.exe").c_str(), nullptr, nullptr,
                    SW_SHOWNORMAL);
      changed = false;
      break;
    case kMenuKanaInput: c.kana_input_enabled = !c.kana_input_enabled; break;
    case kMenuHalfKana: c.half_width_kana_enabled = !c.half_width_kana_enabled; break;
    case kMenuFullWidthOff: c.full_width = tora::FullWidthMode::kDisabled; break;
    case kMenuFullWidthCandidates: c.full_width = tora::FullWidthMode::kCandidatesOnly; break;
    case kMenuFullWidthDefault: c.full_width = tora::FullWidthMode::kDefault; break;
    case kMenuPunct0:
    case kMenuPunct1:
    case kMenuPunct2:
    case kMenuPunct3:
      c.punctuation = static_cast<tora::PunctuationStyle>(id - kMenuPunct0);
      break;
    case kMenuUserDict: OpenWithNotepad(EnsureUserFile(L"user_dict.txt")); changed = false; break;
    case kMenuUserEnglish: OpenWithNotepad(EnsureUserFile(L"user_english.txt")); changed = false; break;
    case kMenuClearHistory:
      if (MessageBoxW(nullptr, L"学習履歴をすべて消去しますか?", L"toraIME",
                      MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST) == IDOK) {
        ClearHistoryEverywhere();
      }
      changed = false;
      break;
    default:
      changed = false;
      break;
  }
  if (changed) {
    engine->config() = c;
    SaveConfig(c);
    service_->OnConfigChanged();
  }
  return S_OK;
}

STDMETHODIMP LanguageBarButton::GetIcon(HICON* icon) {
  if (icon == nullptr) return E_INVALIDARG;
  *icon = CreateLabelIcon(service_ ? service_->ModeLabel() : L"A");
  return *icon ? S_OK : E_FAIL;
}

STDMETHODIMP LanguageBarButton::GetText(BSTR* text) {
  if (text == nullptr) return E_INVALIDARG;
  *text = SysAllocString(service_ ? service_->ModeLabel() : L"A");
  return *text ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LanguageBarButton::AdviseSink(REFIID riid, IUnknown* punk, DWORD* cookie) {
  if (cookie == nullptr || punk == nullptr) return E_INVALIDARG;
  if (!IsEqualIID(riid, kIID_ITfLangBarItemSink)) return CONNECT_E_CANNOTCONNECT;
  if (sink_) return CONNECT_E_ADVISELIMIT;
  if (FAILED(punk->QueryInterface(kIID_ITfLangBarItemSink, sink_.PutVoid()))) return E_NOINTERFACE;
  *cookie = kSinkCookie;
  return S_OK;
}

STDMETHODIMP LanguageBarButton::UnadviseSink(DWORD cookie) {
  if (cookie != kSinkCookie || !sink_) return CONNECT_E_NOCONNECTION;
  sink_.Reset();
  return S_OK;
}

}  // namespace toraime
