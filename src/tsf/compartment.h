// コンパートメント (スレッドやコンテキストごとの共有値) の読み書きと変更通知
#pragma once

#include <functional>

#include "common.h"

namespace toraime {

// punk は ITfThreadMgr か ITfContext
bool GetCompartmentDword(IUnknown* punk, TfClientId client_id, const GUID& guid, DWORD* value);
bool SetCompartmentDword(IUnknown* punk, TfClientId client_id, const GUID& guid, DWORD value);

// 値が変わったときに callback(guid) を呼ぶ
class CompartmentWatcher : public ITfCompartmentEventSink {
 public:
  using Callback = std::function<void(const GUID&)>;
  explicit CompartmentWatcher(Callback callback);
  virtual ~CompartmentWatcher();

  bool Advise(IUnknown* punk, const GUID& guid);
  void Unadvise();

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP OnChange(REFGUID guid) override;

 private:
  std::atomic<ULONG> ref_{1};
  Callback callback_;
  ComPtr<ITfCompartment> compartment_;
  DWORD cookie_ = TF_INVALID_COOKIE;
};

}  // namespace toraime
