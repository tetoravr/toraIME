// TSF テキストサービス共通の定義
#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <msctf.h>

#include <atomic>
#include <utility>

// ---- MinGW の msctf.h に無い宣言 (MSVC では SDK の定義を使う) ----
#if !defined(_MSC_VER)
#include <ctfutb.h>

#ifndef TF_LBI_STYLE_BTN_BUTTON
#define TF_LBI_STYLE_HIDDENSTATUSCONTROL 0x00000001
#define TF_LBI_STYLE_SHOWNINTRAY 0x00000002
#define TF_LBI_STYLE_BTN_BUTTON 0x00010000
#define TF_LBI_STYLE_BTN_MENU 0x00020000
#define TF_LBI_STYLE_BTN_TOGGLE 0x00040000
#define TF_LBI_ICON 0x00000001
#define TF_LBI_TEXT 0x00000002
#define TF_LBI_TOOLTIP 0x00000004
#define TF_LBI_STATUS 0x00010000
#define TF_LBMENUF_CHECKED 0x00000001
#define TF_LBMENUF_SUBMENU 0x00000002
#define TF_LBMENUF_SEPARATOR 0x00000004
#define TF_LBMENUF_RADIOCHECKED 0x00000008
#define TF_LBMENUF_GRAYED 0x00000010
typedef enum { TF_LBI_CLK_RIGHT = 1, TF_LBI_CLK_LEFT = 2 } TfLBIClick;
#endif

struct ITfMenu : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE AddMenuItem(UINT uId, DWORD dwFlags, HBITMAP hbmp,
                                                HBITMAP hbmpMask, const WCHAR* pch, ULONG cch,
                                                ITfMenu** ppMenu) = 0;
};

struct ITfLangBarItemButton : public ITfLangBarItem {
  virtual HRESULT STDMETHODCALLTYPE OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) = 0;
  virtual HRESULT STDMETHODCALLTYPE InitMenu(ITfMenu* pMenu) = 0;
  virtual HRESULT STDMETHODCALLTYPE OnMenuSelect(UINT wID) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetIcon(HICON* phIcon) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetText(BSTR* pbstrText) = 0;
};

struct ITfTextInputProcessorEx : public ITfTextInputProcessor {
  virtual HRESULT STDMETHODCALLTYPE ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) = 0;
};

struct ITfDisplayAttributeProvider : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) = 0;
};
#endif  // !_MSC_VER

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL 0
#endif
#ifndef TF_INVALID_GUIDATOM
#define TF_INVALID_GUIDATOM 0
#endif

namespace toraime {

// ---- GUID (SDK のシンボルに依存しないよう自前で定義する) ----
extern const CLSID kClsidTextService;
extern const GUID kGuidProfile;
extern const GUID kGuidAttrInput;
extern const GUID kGuidAttrConverted;
extern const GUID kGuidAttrFocused;

extern const IID kIID_ITfTextInputProcessor;
extern const IID kIID_ITfTextInputProcessorEx;
extern const IID kIID_ITfThreadMgrEventSink;
extern const IID kIID_ITfKeyEventSink;
extern const IID kIID_ITfCompositionSink;
extern const IID kIID_ITfDisplayAttributeProvider;
extern const IID kIID_ITfCompartmentEventSink;
extern const IID kIID_ITfThreadFocusSink;
extern const IID kIID_ITfEditSession;
extern const IID kIID_ITfDisplayAttributeInfo;
extern const IID kIID_IEnumTfDisplayAttributeInfo;
extern const IID kIID_ITfLangBarItem;
extern const IID kIID_ITfLangBarItemButton;
extern const IID kIID_ITfMenu;
extern const IID kIID_ITfLangBarItemSink;
extern const IID kIID_ITfSource;
extern const IID kIID_ITfKeystrokeMgr;
extern const IID kIID_ITfLangBarItemMgr;
extern const IID kIID_ITfCompartmentMgr;
extern const IID kIID_ITfInsertAtSelection;
extern const IID kIID_ITfContextComposition;
extern const IID kIID_ITfCategoryMgr;
extern const IID kIID_ITfInputProcessorProfileMgr;
extern const IID kIID_ITfThreadMgr;

extern const CLSID kClsid_TF_CategoryMgr;
extern const CLSID kClsid_TF_InputProcessorProfiles;

extern const GUID kGuid_TFCAT_TIP_KEYBOARD;
extern const GUID kGuid_TFCAT_DISPLAYATTRIBUTEPROVIDER;
extern const GUID kGuid_TFCAT_TIPCAP_SECUREMODE;
extern const GUID kGuid_TFCAT_TIPCAP_COMLESS;
extern const GUID kGuid_TFCAT_TIPCAP_INPUTMODECOMPARTMENT;
extern const GUID kGuid_TFCAT_TIPCAP_IMMERSIVESUPPORT;
extern const GUID kGuid_TFCAT_TIPCAP_SYSTRAYSUPPORT;
extern const GUID kGuid_LBI_INPUTMODE;
extern const GUID kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE;
extern const GUID kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION;
extern const GUID kGuid_COMPARTMENT_KEYBOARD_DISABLED;
extern const GUID kGuid_COMPARTMENT_EMPTYCONTEXT;
extern const GUID kGuid_PROP_ATTRIBUTE;

// 入力モードコンパートメントの値 (IME_CMODE_* と同じ)
constexpr DWORD kConversionAlphanumeric = 0x0000;
constexpr DWORD kConversionNative = 0x0001;
constexpr DWORD kConversionKatakana = 0x0002;
constexpr DWORD kConversionFullShape = 0x0008;
constexpr DWORD kConversionRoman = 0x0010;

constexpr LANGID kLangJapanese = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
constexpr wchar_t kDisplayName[] = L"toraIME";

// ---- DLL 全体 ----
extern HINSTANCE g_instance;
void DllAddRef();
void DllRelease();

// ---- 最小限の COM スマートポインター ----
template <class T>
class ComPtr {
 public:
  ComPtr() = default;
  ComPtr(T* p) : p_(p) {  // NOLINT: 生ポインターから AddRef して受け取る
    if (p_) p_->AddRef();
  }
  ComPtr(const ComPtr& o) : ComPtr(o.p_) {}
  ComPtr(ComPtr&& o) noexcept : p_(std::exchange(o.p_, nullptr)) {}
  ~ComPtr() { Reset(); }
  ComPtr& operator=(const ComPtr& o) {
    if (this != &o) {
      Reset();
      p_ = o.p_;
      if (p_) p_->AddRef();
    }
    return *this;
  }
  ComPtr& operator=(ComPtr&& o) noexcept {
    if (this != &o) {
      Reset();
      p_ = std::exchange(o.p_, nullptr);
    }
    return *this;
  }
  void Reset() {
    if (p_) std::exchange(p_, nullptr)->Release();
  }
  T* Get() const { return p_; }
  T* operator->() const { return p_; }
  explicit operator bool() const { return p_ != nullptr; }
  // 出力引数用 (中身を解放してからアドレスを返す)
  T** Put() {
    Reset();
    return &p_;
  }
  void** PutVoid() { return reinterpret_cast<void**>(Put()); }
  // AddRef 済みのポインターを引き取る
  static ComPtr Attach(T* p) {
    ComPtr c;
    c.p_ = p;
    return c;
  }
  T* Detach() { return std::exchange(p_, nullptr); }

 private:
  T* p_ = nullptr;
};

template <class T, class U>
ComPtr<T> QueryAs(U* from, const IID& iid) {
  ComPtr<T> to;
  if (from) from->QueryInterface(iid, to.PutVoid());
  return to;
}

}  // namespace toraime
