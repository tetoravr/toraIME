// toraIME のテキストサービス本体
#pragma once

#include <memory>
#include <string>

#include "candidate_window.h"
#include "common.h"
#include "compartment.h"
#include "session.h"

namespace toraime {

class LanguageBarButton;

class TextService : public ITfTextInputProcessorEx,
                    public ITfThreadMgrEventSink,
                    public ITfKeyEventSink,
                    public ITfCompositionSink,
                    public ITfDisplayAttributeProvider,
                    public ITfThreadFocusSink {
 public:
  TextService();
  virtual ~TextService();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // ITfTextInputProcessor(Ex)
  STDMETHODIMP Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) override;
  STDMETHODIMP ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id, DWORD flags) override;
  STDMETHODIMP Deactivate() override;

  // ITfThreadMgrEventSink
  STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* dim) override;
  STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* dim) override;
  STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* prev) override;
  STDMETHODIMP OnPushContext(ITfContext* ctx) override;
  STDMETHODIMP OnPopContext(ITfContext* ctx) override;

  // ITfKeyEventSink
  STDMETHODIMP OnSetFocus(BOOL foreground) override;
  STDMETHODIMP OnTestKeyDown(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) override;
  STDMETHODIMP OnKeyDown(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) override;
  STDMETHODIMP OnTestKeyUp(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) override;
  STDMETHODIMP OnKeyUp(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) override;
  STDMETHODIMP OnPreservedKey(ITfContext* ctx, REFGUID guid, BOOL* eaten) override;

  // ITfCompositionSink
  STDMETHODIMP OnCompositionTerminated(TfEditCookie ec, ITfComposition* composition) override;

  // ITfDisplayAttributeProvider
  STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) override;
  STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** out) override;

  // ITfThreadFocusSink
  STDMETHODIMP OnSetThreadFocus() override;
  STDMETHODIMP OnKillThreadFocus() override;

  // 言語バーから使う
  bool IsOpen() const;
  void ToggleOpen();
  void SetMode(bool open, tora::InputMode mode);
  tora::InputMode CurrentMode() const;
  const wchar_t* ModeLabel() const;
  std::wstring ModeDescription() const;
  void OnConfigChanged();

 private:
  bool HandleKey(ITfContext* ctx, WPARAM vk, LPARAM lp, bool test);
  void TurnOffCapsLockIfNeeded();
  bool IsKeyboardDisabled(ITfContext* ctx) const;
  bool KanaInputActive() const;
  void SetOpen(bool open);
  void SyncCompartments();
  void OnCompartmentChanged(const GUID& guid);
  void FlushComposition(ITfContext* ctx, bool prefer_sync = false);

  // 編集セッションの中での処理
  void ApplyOutput(ITfContext* ctx, const tora::Output& out, bool prefer_sync = false);
  HRESULT DoApply(TfEditCookie ec, ITfContext* ctx, const tora::Output& out);
  bool StartComposition(TfEditCookie ec, ITfContext* ctx);
  void SetCompositionText(TfEditCookie ec, ITfContext* ctx, const std::u16string& text);
  void ApplyAttributes(TfEditCookie ec, ITfContext* ctx, const tora::Output& out);
  void SetCaret(TfEditCookie ec, ITfContext* ctx, ITfRange* range, LONG pos);
  void EndComposition(TfEditCookie ec, ITfContext* ctx);
  void UpdateCandidateWindow(TfEditCookie ec, ITfContext* ctx, const tora::Output& out);
  void OnCandidateClicked(size_t index);

  std::atomic<ULONG> ref_{1};
  ComPtr<ITfThreadMgr> thread_mgr_;
  TfClientId client_id_ = TF_CLIENTID_NULL;
  DWORD thread_mgr_cookie_ = TF_INVALID_COOKIE;
  DWORD thread_focus_cookie_ = TF_INVALID_COOKIE;
  bool key_sink_advised_ = false;

  ComPtr<ITfComposition> composition_;
  ComPtr<ITfContext> composition_context_;
  std::unique_ptr<tora::Session> session_;
  std::unique_ptr<CandidateWindow> candidate_window_;
  LanguageBarButton* langbar_ = nullptr;
  CompartmentWatcher* open_watcher_ = nullptr;
  CompartmentWatcher* mode_watcher_ = nullptr;
  TfGuidAtom attr_atoms_[3] = {TF_INVALID_GUIDATOM, TF_INVALID_GUIDATOM, TF_INVALID_GUIDATOM};
  bool kana_input_ = false;
  bool syncing_ = false;  // 自分でコンパートメントを書いている最中
  ULONGLONG last_toggle_tick_ = 0;
  WPARAM last_toggle_vk_ = 0;
  ULONGLONG last_caps_off_tick_ = 0;
};

}  // namespace toraime
