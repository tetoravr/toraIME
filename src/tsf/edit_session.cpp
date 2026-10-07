#include "edit_session.h"

#include <new>

namespace toraime {
namespace {

class EditSession : public ITfEditSession {
 public:
  explicit EditSession(EditFunction fn) : fn_(std::move(fn)) { DllAddRef(); }
  virtual ~EditSession() { DllRelease(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) return E_INVALIDARG;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfEditSession)) {
      *ppv = static_cast<ITfEditSession*>(this);
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
  STDMETHODIMP DoEditSession(TfEditCookie ec) override { return fn_ ? fn_(ec) : S_OK; }

 private:
  std::atomic<ULONG> ref_{1};
  EditFunction fn_;
};

}  // namespace

HRESULT RequestEdit(ITfContext* ctx, TfClientId client_id, DWORD flags, EditFunction fn) {
  if (ctx == nullptr) return E_INVALIDARG;
  EditSession* session = new (std::nothrow) EditSession(std::move(fn));
  if (session == nullptr) return E_OUTOFMEMORY;
  HRESULT session_hr = S_OK;
  HRESULT hr = ctx->RequestEditSession(client_id, session, flags, &session_hr);
  session->Release();
  return FAILED(hr) ? hr : session_hr;
}

}  // namespace toraime
