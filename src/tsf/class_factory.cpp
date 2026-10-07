#include "class_factory.h"

#include <new>

#include "text_service.h"

namespace toraime {

STDMETHODIMP ClassFactory::QueryInterface(REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
    *ppv = static_cast<IClassFactory*>(this);
    AddRef();
    return S_OK;
  }
  *ppv = nullptr;
  return E_NOINTERFACE;
}

// 静的オブジェクトなので参照カウントは DLL のロックとして扱う
STDMETHODIMP_(ULONG) ClassFactory::AddRef() {
  DllAddRef();
  return 2;
}

STDMETHODIMP_(ULONG) ClassFactory::Release() {
  DllRelease();
  return 1;
}

STDMETHODIMP ClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  *ppv = nullptr;
  if (outer != nullptr) return CLASS_E_NOAGGREGATION;
  TextService* service = new (std::nothrow) TextService();
  if (service == nullptr) return E_OUTOFMEMORY;
  HRESULT hr = service->QueryInterface(riid, ppv);
  service->Release();
  return hr;
}

STDMETHODIMP ClassFactory::LockServer(BOOL lock) {
  if (lock) {
    DllAddRef();
  } else {
    DllRelease();
  }
  return S_OK;
}

}  // namespace toraime
