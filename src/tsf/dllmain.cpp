// DLL のエントリーポイントと COM サーバーとしての公開関数
#include "candidate_window.h"
#include "class_factory.h"
#include "language_bar.h"
#include "common.h"
#include "registry.h"

namespace toraime {

HINSTANCE g_instance = nullptr;
namespace {
std::atomic<long> g_dll_refs{0};
ClassFactory g_factory;
}  // namespace

void DllAddRef() { ++g_dll_refs; }
void DllRelease() { --g_dll_refs; }

}  // namespace toraime

using namespace toraime;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_instance = instance;
    DisableThreadLibraryCalls(instance);
  } else if (reason == DLL_PROCESS_DETACH) {
    // DLL が登録したウィンドウクラスはアンロード時に自動では消えないので消しておく
    CandidateWindow::UnregisterWindowClass();
    UnregisterMenuOwnerClass();
  }
  return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  *ppv = nullptr;
  if (!IsEqualCLSID(clsid, kClsidTextService)) return CLASS_E_CLASSNOTAVAILABLE;
  return g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return g_dll_refs.load() <= 0 ? S_OK : S_FALSE; }

STDAPI DllRegisterServer() {
  HRESULT hr = RegisterServer();
  if (SUCCEEDED(hr)) hr = RegisterProfile();
  if (SUCCEEDED(hr)) hr = RegisterCategories();
  if (FAILED(hr)) {
    UnregisterCategories();
    UnregisterProfile();
    UnregisterServer();
  }
  return hr;
}

STDAPI DllUnregisterServer() {
  UnregisterCategories();
  UnregisterProfile();
  UnregisterServer();
  return S_OK;
}
