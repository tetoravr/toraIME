#include "compartment.h"

namespace toraime {
namespace {

ComPtr<ITfCompartment> GetCompartment(IUnknown* punk, const GUID& guid) {
  ComPtr<ITfCompartment> compartment;
  auto mgr = QueryAs<ITfCompartmentMgr>(punk, kIID_ITfCompartmentMgr);
  if (mgr) mgr->GetCompartment(guid, compartment.Put());
  return compartment;
}

}  // namespace

bool GetCompartmentDword(IUnknown* punk, TfClientId, const GUID& guid, DWORD* value) {
  *value = 0;
  auto compartment = GetCompartment(punk, guid);
  if (!compartment) return false;
  VARIANT var;
  VariantInit(&var);
  if (compartment->GetValue(&var) != S_OK) return false;  // S_FALSE は値なし
  bool ok = var.vt == VT_I4;
  if (ok) *value = static_cast<DWORD>(var.lVal);
  VariantClear(&var);
  return ok;
}

bool SetCompartmentDword(IUnknown* punk, TfClientId client_id, const GUID& guid, DWORD value) {
  auto compartment = GetCompartment(punk, guid);
  if (!compartment) return false;
  VARIANT var;
  VariantInit(&var);
  var.vt = VT_I4;
  var.lVal = static_cast<LONG>(value);
  return SUCCEEDED(compartment->SetValue(client_id, &var));
}

CompartmentWatcher::CompartmentWatcher(Callback callback) : callback_(std::move(callback)) {
  DllAddRef();
}

CompartmentWatcher::~CompartmentWatcher() {
  Unadvise();
  DllRelease();
}

bool CompartmentWatcher::Advise(IUnknown* punk, const GUID& guid) {
  Unadvise();
  compartment_ = GetCompartment(punk, guid);
  if (!compartment_) return false;
  auto source = QueryAs<ITfSource>(compartment_.Get(), kIID_ITfSource);
  if (!source || FAILED(source->AdviseSink(kIID_ITfCompartmentEventSink,
                                           static_cast<ITfCompartmentEventSink*>(this), &cookie_))) {
    cookie_ = TF_INVALID_COOKIE;
    compartment_.Reset();
    return false;
  }
  return true;
}

void CompartmentWatcher::Unadvise() {
  if (compartment_ && cookie_ != TF_INVALID_COOKIE) {
    auto source = QueryAs<ITfSource>(compartment_.Get(), kIID_ITfSource);
    if (source) source->UnadviseSink(cookie_);
  }
  cookie_ = TF_INVALID_COOKIE;
  compartment_.Reset();
}

STDMETHODIMP CompartmentWatcher::QueryInterface(REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfCompartmentEventSink)) {
    *ppv = static_cast<ITfCompartmentEventSink*>(this);
    AddRef();
    return S_OK;
  }
  *ppv = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CompartmentWatcher::AddRef() { return ++ref_; }

STDMETHODIMP_(ULONG) CompartmentWatcher::Release() {
  ULONG r = --ref_;
  if (r == 0) delete this;
  return r;
}

STDMETHODIMP CompartmentWatcher::OnChange(REFGUID guid) {
  if (callback_) callback_(guid);
  return S_OK;
}

}  // namespace toraime
