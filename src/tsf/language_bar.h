// タスクバーの入力モード表示 (「あ」「A」) と右クリックメニュー
#pragma once

#include "common.h"

namespace toraime {

class TextService;

class LanguageBarButton : public ITfLangBarItemButton, public ITfSource {
 public:
  explicit LanguageBarButton(TextService* service);
  virtual ~LanguageBarButton();

  // テキストサービスが終了するときに呼ぶ
  void Detach() { service_ = nullptr; }
  // 表示を更新する
  void Update();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  // ITfLangBarItem
  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* info) override;
  STDMETHODIMP GetStatus(DWORD* status) override;
  STDMETHODIMP Show(BOOL show) override;
  STDMETHODIMP GetTooltipString(BSTR* tooltip) override;
  // ITfLangBarItemButton
  STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* area) override;
  STDMETHODIMP InitMenu(ITfMenu* menu) override;
  STDMETHODIMP OnMenuSelect(UINT id) override;
  STDMETHODIMP GetIcon(HICON* icon) override;
  STDMETHODIMP GetText(BSTR* text) override;
  // ITfSource
  STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* cookie) override;
  STDMETHODIMP UnadviseSink(DWORD cookie) override;

 private:
  std::atomic<ULONG> ref_{1};
  TextService* service_;
  void ShowPopupMenu(POINT pt);

  ComPtr<ITfLangBarItemSink> sink_;
};

// DLL のアンロード時に呼ぶ
void UnregisterMenuOwnerClass();

// 1 文字のラベルからタスクバー用のアイコンを作る
HICON CreateLabelIcon(const wchar_t* label);

}  // namespace toraime
