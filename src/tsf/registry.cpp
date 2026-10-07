#include "registry.h"

#include <iterator>
#include <string>

#include "resource.h"

namespace toraime {
namespace {

const GUID* const kCategories[] = {
    &kGuid_TFCAT_TIP_KEYBOARD,
    &kGuid_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    &kGuid_TFCAT_TIPCAP_SECUREMODE,
    &kGuid_TFCAT_TIPCAP_COMLESS,
    &kGuid_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    &kGuid_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &kGuid_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

std::wstring GuidString(const GUID& guid) {
  wchar_t buf[64];
  StringFromGUID2(guid, buf, 64);
  return buf;
}

std::wstring ModulePath() {
  wchar_t buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameW(g_instance, buf, static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf)) return {};
  return std::wstring(buf, n);
}

bool SetStringValue(HKEY key, const wchar_t* name, const std::wstring& value) {
  return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

}  // namespace

HRESULT RegisterServer() {
  const std::wstring clsid_key = L"CLSID\\" + GuidString(kClsidTextService);
  const std::wstring path = ModulePath();
  if (path.empty()) return E_FAIL;
  HKEY key;
  if (RegCreateKeyExW(HKEY_CLASSES_ROOT, clsid_key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return E_ACCESSDENIED;
  }
  bool ok = SetStringValue(key, nullptr, kDisplayName);
  HKEY inproc;
  if (ok && RegCreateKeyExW(key, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inproc,
                            nullptr) == ERROR_SUCCESS) {
    ok = SetStringValue(inproc, nullptr, path) && SetStringValue(inproc, L"ThreadingModel", L"Apartment");
    RegCloseKey(inproc);
  } else {
    ok = false;
  }
  RegCloseKey(key);
  return ok ? S_OK : E_FAIL;
}

HRESULT UnregisterServer() {
  const std::wstring clsid_key = L"CLSID\\" + GuidString(kClsidTextService);
  LSTATUS s = RegDeleteTreeW(HKEY_CLASSES_ROOT, clsid_key.c_str());
  return (s == ERROR_SUCCESS || s == ERROR_FILE_NOT_FOUND) ? S_OK : E_FAIL;
}

HRESULT RegisterProfile() {
  ComPtr<ITfInputProcessorProfileMgr> mgr;
  HRESULT hr = CoCreateInstance(kClsid_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_ITfInputProcessorProfileMgr, mgr.PutVoid());
  if (FAILED(hr)) return hr;
  const std::wstring path = ModulePath();
  const std::wstring name = kDisplayName;
  // アイコンはリソースの順番 (0 番目) で指定する
  return mgr->RegisterProfile(kClsidTextService, kLangJapanese, kGuidProfile, name.c_str(),
                              static_cast<ULONG>(name.size()), path.c_str(),
                              static_cast<ULONG>(path.size()), 0, nullptr, 0, TRUE, 0);
}

HRESULT UnregisterProfile() {
  ComPtr<ITfInputProcessorProfileMgr> mgr;
  HRESULT hr = CoCreateInstance(kClsid_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_ITfInputProcessorProfileMgr, mgr.PutVoid());
  if (FAILED(hr)) return hr;
  return mgr->UnregisterProfile(kClsidTextService, kLangJapanese, kGuidProfile, 0);
}

HRESULT RegisterCategories() {
  ComPtr<ITfCategoryMgr> mgr;
  HRESULT hr = CoCreateInstance(kClsid_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_ITfCategoryMgr, mgr.PutVoid());
  if (FAILED(hr)) return hr;
  for (const GUID* cat : kCategories) {
    hr = mgr->RegisterCategory(kClsidTextService, *cat, kClsidTextService);
    if (FAILED(hr)) return hr;
  }
  return S_OK;
}

HRESULT UnregisterCategories() {
  ComPtr<ITfCategoryMgr> mgr;
  HRESULT hr = CoCreateInstance(kClsid_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_ITfCategoryMgr, mgr.PutVoid());
  if (FAILED(hr)) return hr;
  for (const GUID* cat : kCategories) mgr->UnregisterCategory(kClsidTextService, *cat, kClsidTextService);
  return S_OK;
}

}  // namespace toraime
