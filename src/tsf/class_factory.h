#pragma once

#include "common.h"

namespace toraime {

class ClassFactory : public IClassFactory {
 public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  // IClassFactory
  STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
  STDMETHODIMP LockServer(BOOL lock) override;
};

}  // namespace toraime
