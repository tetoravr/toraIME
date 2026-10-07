#include "display_attribute.h"

#include <new>

namespace toraime {
namespace {

struct AttrDef {
  const GUID* guid;
  const wchar_t* description;
  TF_DISPLAYATTRIBUTE attr;
};

TF_DISPLAYATTRIBUTE MakeAttr(TF_DA_LINESTYLE line, BOOL bold, TF_DA_ATTR_INFO info, bool highlight) {
  TF_DISPLAYATTRIBUTE a{};
  a.crText.type = highlight ? TF_CT_SYSCOLOR : TF_CT_NONE;
  a.crText.nIndex = COLOR_HIGHLIGHTTEXT;
  a.crBk.type = highlight ? TF_CT_SYSCOLOR : TF_CT_NONE;
  a.crBk.nIndex = COLOR_HIGHLIGHT;
  a.lsStyle = line;
  a.fBoldLine = bold;
  a.crLine.type = TF_CT_NONE;
  a.bAttr = info;
  return a;
}

const AttrDef& Def(int index) {
  static const AttrDef defs[kAttrCount] = {
      {&kGuidAttrInput, L"toraIME Input", MakeAttr(TF_LS_DOT, FALSE, TF_ATTR_INPUT, false)},
      {&kGuidAttrConverted, L"toraIME Converted", MakeAttr(TF_LS_SOLID, FALSE, TF_ATTR_CONVERTED, false)},
      {&kGuidAttrFocused, L"toraIME Focused", MakeAttr(TF_LS_SOLID, TRUE, TF_ATTR_TARGET_CONVERTED, true)},
  };
  return defs[index];
}

class DisplayAttributeInfo : public ITfDisplayAttributeInfo {
 public:
  explicit DisplayAttributeInfo(int index) : index_(index) { DllAddRef(); }
  virtual ~DisplayAttributeInfo() { DllRelease(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfDisplayAttributeInfo)) {
      *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
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

  STDMETHODIMP GetGUID(GUID* guid) override {
    if (guid == nullptr) return E_INVALIDARG;
    *guid = *Def(index_).guid;
    return S_OK;
  }
  STDMETHODIMP GetDescription(BSTR* desc) override {
    if (desc == nullptr) return E_INVALIDARG;
    *desc = SysAllocString(Def(index_).description);
    return *desc ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* attr) override {
    if (attr == nullptr) return E_INVALIDARG;
    *attr = Def(index_).attr;
    return S_OK;
  }
  STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }
  STDMETHODIMP Reset() override { return S_OK; }

 private:
  std::atomic<ULONG> ref_{1};
  int index_;
};

class EnumDisplayAttributeInfo : public IEnumTfDisplayAttributeInfo {
 public:
  explicit EnumDisplayAttributeInfo(int pos = 0) : pos_(pos) { DllAddRef(); }
  virtual ~EnumDisplayAttributeInfo() { DllRelease(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_IEnumTfDisplayAttributeInfo)) {
      *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
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

  STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** out) override {
    if (out == nullptr) return E_INVALIDARG;
    *out = new (std::nothrow) EnumDisplayAttributeInfo(pos_);
    return *out ? S_OK : E_OUTOFMEMORY;
  }
  STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** infos, ULONG* fetched) override {
    if (infos == nullptr) return E_INVALIDARG;
    ULONG n = 0;
    while (n < count && pos_ < kAttrCount) {
      infos[n] = new (std::nothrow) DisplayAttributeInfo(pos_);
      if (infos[n] == nullptr) break;
      ++n;
      ++pos_;
    }
    if (fetched) *fetched = n;
    return n == count ? S_OK : S_FALSE;
  }
  STDMETHODIMP Reset() override {
    pos_ = 0;
    return S_OK;
  }
  STDMETHODIMP Skip(ULONG count) override {
    int next = pos_ + static_cast<int>(count);
    pos_ = next > kAttrCount ? kAttrCount : next;
    return next <= kAttrCount ? S_OK : S_FALSE;
  }

 private:
  std::atomic<ULONG> ref_{1};
  int pos_;
};

}  // namespace

const GUID& AttrGuid(AttrKind kind) { return *Def(static_cast<int>(kind)).guid; }

HRESULT CreateDisplayAttributeInfo(const GUID& guid, ITfDisplayAttributeInfo** out) {
  if (out == nullptr) return E_INVALIDARG;
  *out = nullptr;
  for (int i = 0; i < kAttrCount; ++i) {
    if (IsEqualGUID(guid, *Def(i).guid)) {
      *out = new (std::nothrow) DisplayAttributeInfo(i);
      return *out ? S_OK : E_OUTOFMEMORY;
    }
  }
  return E_INVALIDARG;
}

HRESULT CreateEnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) {
  if (out == nullptr) return E_INVALIDARG;
  *out = new (std::nothrow) EnumDisplayAttributeInfo();
  return *out ? S_OK : E_OUTOFMEMORY;
}

}  // namespace toraime
