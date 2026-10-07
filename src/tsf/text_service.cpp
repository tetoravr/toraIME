#include "text_service.h"

#include <new>

#include "display_attribute.h"
#include "edit_session.h"
#include "key_translator.h"
#include "language_bar.h"
#include "settings.h"

namespace toraime {
namespace {

std::wstring ToWide(const std::u16string& s) { return std::wstring(s.begin(), s.end()); }

ComPtr<ITfCategoryMgr> CreateCategoryMgr() {
  ComPtr<ITfCategoryMgr> mgr;
  CoCreateInstance(kClsid_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, kIID_ITfCategoryMgr,
                   mgr.PutVoid());
  return mgr;
}

}  // namespace

TextService::TextService() { DllAddRef(); }

TextService::~TextService() { DllRelease(); }

// ---------------------------------------------------------------- IUnknown

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppv) {
  if (ppv == nullptr) return E_INVALIDARG;
  *ppv = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, kIID_ITfTextInputProcessor) ||
      IsEqualIID(riid, kIID_ITfTextInputProcessorEx)) {
    *ppv = static_cast<ITfTextInputProcessorEx*>(this);
  } else if (IsEqualIID(riid, kIID_ITfThreadMgrEventSink)) {
    *ppv = static_cast<ITfThreadMgrEventSink*>(this);
  } else if (IsEqualIID(riid, kIID_ITfKeyEventSink)) {
    *ppv = static_cast<ITfKeyEventSink*>(this);
  } else if (IsEqualIID(riid, kIID_ITfCompositionSink)) {
    *ppv = static_cast<ITfCompositionSink*>(this);
  } else if (IsEqualIID(riid, kIID_ITfDisplayAttributeProvider)) {
    *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
  } else if (IsEqualIID(riid, kIID_ITfThreadFocusSink)) {
    *ppv = static_cast<ITfThreadFocusSink*>(this);
  }
  if (*ppv == nullptr) return E_NOINTERFACE;
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return ++ref_; }

STDMETHODIMP_(ULONG) TextService::Release() {
  ULONG r = --ref_;
  if (r == 0) delete this;
  return r;
}

// ---------------------------------------------------------------- 有効化 / 無効化

STDMETHODIMP TextService::Activate(ITfThreadMgr* thread_mgr, TfClientId client_id) {
  return ActivateEx(thread_mgr, client_id, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* thread_mgr, TfClientId client_id, DWORD) {
  if (thread_mgr == nullptr) return E_INVALIDARG;
  thread_mgr_ = thread_mgr;
  client_id_ = client_id;

  tora::Engine* engine = GetEngine();
  ReloadConfig();
  session_ = std::make_unique<tora::Session>(engine);
  candidate_window_ = std::make_unique<CandidateWindow>([this](size_t i) { OnCandidateClicked(i); });

  // スレッドのイベント
  if (auto source = QueryAs<ITfSource>(thread_mgr, kIID_ITfSource)) {
    source->AdviseSink(kIID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this),
                       &thread_mgr_cookie_);
    source->AdviseSink(kIID_ITfThreadFocusSink, static_cast<ITfThreadFocusSink*>(this),
                       &thread_focus_cookie_);
  }
  // キー入力
  if (auto keystroke = QueryAs<ITfKeystrokeMgr>(thread_mgr, kIID_ITfKeystrokeMgr)) {
    key_sink_advised_ = SUCCEEDED(
        keystroke->AdviseKeyEventSink(client_id, static_cast<ITfKeyEventSink*>(this), TRUE));
  }
  // 表示属性
  if (auto cat = CreateCategoryMgr()) {
    cat->RegisterGUID(kGuidAttrInput, &attr_atoms_[0]);
    cat->RegisterGUID(kGuidAttrConverted, &attr_atoms_[1]);
    cat->RegisterGUID(kGuidAttrFocused, &attr_atoms_[2]);
  }
  // 入力モード表示
  langbar_ = new (std::nothrow) LanguageBarButton(this);
  if (langbar_ != nullptr) {
    if (auto mgr = QueryAs<ITfLangBarItemMgr>(thread_mgr, kIID_ITfLangBarItemMgr)) {
      mgr->AddItem(static_cast<ITfLangBarItemButton*>(langbar_));
    }
  }
  // IME のオン/オフと入力モード
  DWORD open = 0;
  if (!GetCompartmentDword(thread_mgr, client_id, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE, &open)) {
    open = 1;  // 初めてなら IME オンで始める
  }
  syncing_ = true;
  SetCompartmentDword(thread_mgr, client_id, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE, open);
  syncing_ = false;
  SyncCompartments();
  auto on_change = [this](const GUID& guid) { OnCompartmentChanged(guid); };
  open_watcher_ = new (std::nothrow) CompartmentWatcher(on_change);
  if (open_watcher_) open_watcher_->Advise(thread_mgr, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE);
  mode_watcher_ = new (std::nothrow) CompartmentWatcher(on_change);
  if (mode_watcher_) mode_watcher_->Advise(thread_mgr, kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
  return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
  // 未確定の文字列は確定しておく (非同期になった場合は編集セッションの中で composition_ を片付ける)
  if (composition_ && composition_context_) FlushComposition(composition_context_.Get(), true);
  if (candidate_window_) candidate_window_->Hide();
  candidate_window_.reset();

  if (open_watcher_) {
    open_watcher_->Unadvise();
    open_watcher_->Release();
    open_watcher_ = nullptr;
  }
  if (mode_watcher_) {
    mode_watcher_->Unadvise();
    mode_watcher_->Release();
    mode_watcher_ = nullptr;
  }
  if (langbar_ != nullptr) {
    if (auto mgr = QueryAs<ITfLangBarItemMgr>(thread_mgr_.Get(), kIID_ITfLangBarItemMgr)) {
      mgr->RemoveItem(static_cast<ITfLangBarItemButton*>(langbar_));
    }
    langbar_->Detach();
    langbar_->Release();
    langbar_ = nullptr;
  }
  if (key_sink_advised_) {
    if (auto keystroke = QueryAs<ITfKeystrokeMgr>(thread_mgr_.Get(), kIID_ITfKeystrokeMgr)) {
      keystroke->UnadviseKeyEventSink(client_id_);
    }
    key_sink_advised_ = false;
  }
  if (auto source = QueryAs<ITfSource>(thread_mgr_.Get(), kIID_ITfSource)) {
    if (thread_mgr_cookie_ != TF_INVALID_COOKIE) source->UnadviseSink(thread_mgr_cookie_);
    if (thread_focus_cookie_ != TF_INVALID_COOKIE) source->UnadviseSink(thread_focus_cookie_);
  }
  thread_mgr_cookie_ = TF_INVALID_COOKIE;
  thread_focus_cookie_ = TF_INVALID_COOKIE;
  session_.reset();
  thread_mgr_.Reset();
  client_id_ = TF_CLIENTID_NULL;
  return S_OK;
}

// ---------------------------------------------------------------- スレッドのイベント

STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP TextService::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP TextService::OnPopContext(ITfContext*) { return S_OK; }

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) {
  // 別の入力欄に移ったら、未確定の文字列は確定する
  if (composition_ && composition_context_) FlushComposition(composition_context_.Get());
  if (candidate_window_) candidate_window_->Hide();
  return S_OK;
}

STDMETHODIMP TextService::OnSetThreadFocus() {
  // 他のプロセスで設定が変わっているかもしれないので読み直す
  ReloadConfig();
  OnConfigChanged();
  return S_OK;
}

STDMETHODIMP TextService::OnKillThreadFocus() {
  if (candidate_window_) candidate_window_->Hide();
  return S_OK;
}

// ---------------------------------------------------------------- キー入力

STDMETHODIMP TextService::OnSetFocus(BOOL) { return S_OK; }

STDMETHODIMP TextService::OnTestKeyDown(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) {
  if (eaten == nullptr) return E_INVALIDARG;
  *eaten = HandleKey(ctx, wp, lp, true) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* ctx, WPARAM wp, LPARAM lp, BOOL* eaten) {
  if (eaten == nullptr) return E_INVALIDARG;
  *eaten = HandleKey(ctx, wp, lp, false) ? TRUE : FALSE;
  return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) {
  if (eaten) *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) {
  if (eaten) *eaten = FALSE;
  return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) {
  if (eaten) *eaten = FALSE;
  return S_OK;
}

bool TextService::IsKeyboardDisabled(ITfContext* ctx) const {
  if (ctx == nullptr) return true;
  DWORD v = 0;
  if (GetCompartmentDword(ctx, client_id_, kGuid_COMPARTMENT_KEYBOARD_DISABLED, &v) && v != 0) return true;
  if (GetCompartmentDword(ctx, client_id_, kGuid_COMPARTMENT_EMPTYCONTEXT, &v) && v != 0) return true;
  return false;
}

bool TextService::KanaInputActive() const {
  return kana_input_ && GetEngine()->config().kana_input_enabled;
}

bool TextService::HandleKey(ITfContext* ctx, WPARAM vk, LPARAM lp, bool test) {
  if (!session_ || IsKeyboardDisabled(ctx)) return false;
  const tora::Config& config = GetEngine()->config();
  const bool open = IsOpen();
  const bool composing = session_->IsComposing();
  const bool alt = GetKeyState(VK_MENU) < 0;
  const bool ctrl = GetKeyState(VK_CONTROL) < 0;

  // 半角/全角 (JIS キーボードでは VK_OEM_AUTO / VK_OEM_ENLW、US キーボードでは Alt+`)
  if (vk == kVkKanji || vk == kVkOemAuto || vk == kVkOemEnlw || (vk == VK_OEM_3 && alt && !ctrl)) {
    if (!test) {
      // 1 回の打鍵で複数の仮想キーが届いても 2 回切り替えないようにする
      const ULONGLONG now = GetTickCount64();
      if (!(vk != last_toggle_vk_ && now - last_toggle_tick_ < 80)) {
        if (composing) FlushComposition(ctx);
        SetOpen(!open);
      }
      last_toggle_tick_ = now;
      last_toggle_vk_ = vk;
    }
    return true;
  }
  if (vk == kVkImeOn || vk == kVkImeOff) {
    if (!test) {
      if (composing && vk == kVkImeOff) FlushComposition(ctx);
      SetOpen(vk == kVkImeOn);
    }
    return true;
  }
  // カタカナ/ひらがな: Alt と一緒ならかな入力の切り替え (無効なら何もしない)、単独なら IME オン
  if (vk == kVkDbeHiragana || vk == kVkDbeKatakana) {
    if (!test) {
      if (alt) {
        if (config.kana_input_enabled) {
          if (composing) FlushComposition(ctx);
          kana_input_ = !kana_input_;
          SyncCompartments();
        }
      } else if (!open) {
        SetOpen(true);
      }
    }
    return true;
  }
  // 変換 / 無変換
  tora::KeyEvent key;
  bool translated = false;
  if (vk == kVkConvert) {
    if (!open) {
      if (!config.convert_keys_on_off) return false;
      if (!test) SetOpen(true);
      return true;
    }
    if (!composing) return true;  // オンのときは何もしない (アプリに渡さない)
    key = tora::KeyEvent::Of(tora::KeyCode::kSpace);
    translated = true;
  } else if (vk == kVkNonConvert) {
    if (!open) return false;
    if (!composing) {
      if (!config.convert_keys_on_off) return false;
      if (!test) SetOpen(false);
      return true;
    }
    key = tora::KeyEvent::Of(tora::KeyCode::kF7);  // 入力中ならカタカナにする
    translated = true;
  }

  if (!open) return false;
  if (!translated && !TranslateKey(vk, lp, KanaInputActive(), &key)) return false;
  if (!session_->WouldConsume(key)) return false;
  if (test) return true;
  tora::Output out = session_->Process(key);
  ApplyOutput(ctx, out);
  return true;
}

// ---------------------------------------------------------------- オン/オフと入力モード

bool TextService::IsOpen() const {
  if (!thread_mgr_) return false;
  DWORD open = 0;
  GetCompartmentDword(thread_mgr_.Get(), client_id_, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE, &open);
  return open != 0;
}

void TextService::SetOpen(bool open) {
  if (!thread_mgr_) return;
  syncing_ = true;
  SetCompartmentDword(thread_mgr_.Get(), client_id_, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE, open ? 1 : 0);
  syncing_ = false;
  SyncCompartments();
}

void TextService::ToggleOpen() {
  if (composition_ && composition_context_) FlushComposition(composition_context_.Get());
  SetOpen(!IsOpen());
}

void TextService::SetMode(bool open, tora::InputMode mode) {
  if (composition_ && composition_context_) FlushComposition(composition_context_.Get());
  if (session_) session_->set_mode(mode);
  SetOpen(open);
}

tora::InputMode TextService::CurrentMode() const {
  return session_ ? session_->mode() : tora::InputMode::kHiragana;
}

const wchar_t* TextService::ModeLabel() const {
  if (!IsOpen()) return L"A";
  switch (CurrentMode()) {
    case tora::InputMode::kHiragana: return KanaInputActive() ? L"か" : L"あ";
    case tora::InputMode::kFullWidthAlnum: return L"Ａ";
    case tora::InputMode::kHalfWidthAlnum: return L"A";
  }
  return L"A";
}

std::wstring TextService::ModeDescription() const {
  if (!IsOpen()) return L"半角英数";
  switch (CurrentMode()) {
    case tora::InputMode::kHiragana: return KanaInputActive() ? L"ひらがな (かな入力)" : L"ひらがな";
    case tora::InputMode::kFullWidthAlnum: return L"全角英数";
    case tora::InputMode::kHalfWidthAlnum: return L"半角英数";
  }
  return L"";
}

void TextService::SyncCompartments() {
  if (!thread_mgr_) return;
  DWORD conv = kConversionAlphanumeric;
  if (IsOpen()) {
    switch (CurrentMode()) {
      case tora::InputMode::kHiragana:
        conv = kConversionNative | kConversionFullShape | (KanaInputActive() ? 0 : kConversionRoman);
        break;
      case tora::InputMode::kFullWidthAlnum:
        conv = kConversionFullShape;
        break;
      case tora::InputMode::kHalfWidthAlnum:
        conv = kConversionAlphanumeric;
        break;
    }
  }
  DWORD current = 0;
  if (!GetCompartmentDword(thread_mgr_.Get(), client_id_, kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                           &current) ||
      current != conv) {
    syncing_ = true;
    SetCompartmentDword(thread_mgr_.Get(), client_id_, kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, conv);
    syncing_ = false;
  }
  if (langbar_) langbar_->Update();
}

void TextService::OnCompartmentChanged(const GUID& guid) {
  if (syncing_ || !thread_mgr_ || !session_) return;
  if (IsEqualGUID(guid, kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
    // システム (タスクバーなど) からオン/オフが切り替えられた
    if (!IsOpen() && composition_ && composition_context_) FlushComposition(composition_context_.Get());
    SyncCompartments();
  } else if (IsEqualGUID(guid, kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION)) {
    DWORD conv = 0;
    GetCompartmentDword(thread_mgr_.Get(), client_id_, kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, &conv);
    tora::InputMode mode = tora::InputMode::kHalfWidthAlnum;
    if (conv & kConversionNative) {
      mode = tora::InputMode::kHiragana;
      if (GetEngine()->config().kana_input_enabled) kana_input_ = (conv & kConversionRoman) == 0;
    } else if (conv & kConversionFullShape) {
      mode = tora::InputMode::kFullWidthAlnum;
    }
    if (composition_ && composition_context_) FlushComposition(composition_context_.Get());
    session_->set_mode(mode);
    SyncCompartments();  // 全角が無効なら半角に戻すなど、実際のモードに合わせる
  }
}

void TextService::OnConfigChanged() {
  if (!session_) return;
  const tora::Config& c = GetEngine()->config();
  if (!c.kana_input_enabled) kana_input_ = false;
  if (session_->mode() == tora::InputMode::kFullWidthAlnum && !c.full_width_allowed()) {
    session_->set_mode(tora::InputMode::kHiragana);
  }
  SyncCompartments();
}

// ---------------------------------------------------------------- 文字列の更新

void TextService::FlushComposition(ITfContext* ctx, bool prefer_sync) {
  if (!session_) return;
  tora::Output out = session_->Flush();
  ApplyOutput(ctx, out, prefer_sync);
}

void TextService::ApplyOutput(ITfContext* ctx, const tora::Output& out, bool prefer_sync) {
  if (ctx == nullptr) return;
  ComPtr<TextService> self(this);
  ComPtr<ITfContext> context(ctx);
  auto fn = [self, context, out](TfEditCookie ec) { return self->DoApply(ec, context.Get(), out); };
  if (prefer_sync && SUCCEEDED(RequestEdit(ctx, client_id_, TF_ES_SYNC | TF_ES_READWRITE, fn))) return;
  RequestEdit(ctx, client_id_, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, fn);
}

HRESULT TextService::DoApply(TfEditCookie ec, ITfContext* ctx, const tora::Output& out) {
  if (!out.commit.empty()) {
    // 確定: 未確定文字列を確定文字列で置き換えて終える
    if (composition_ || StartComposition(ec, ctx)) {
      SetCompositionText(ec, ctx, out.commit);
      ComPtr<ITfRange> range;
      if (SUCCEEDED(composition_->GetRange(range.Put()))) {
        if (ComPtr<ITfProperty> prop; SUCCEEDED(ctx->GetProperty(kGuid_PROP_ATTRIBUTE, prop.Put()))) {
          prop->Clear(ec, range.Get());
        }
        range->Collapse(ec, TF_ANCHOR_END);
        SetCaret(ec, ctx, range.Get(), 0);
      }
      composition_->EndComposition(ec);
      composition_.Reset();
      composition_context_.Reset();
    }
  }
  if (!out.preedit.empty()) {
    if (composition_ || StartComposition(ec, ctx)) {
      SetCompositionText(ec, ctx, out.preedit);
      ApplyAttributes(ec, ctx, out);
      ComPtr<ITfRange> range;
      if (SUCCEEDED(composition_->GetRange(range.Put()))) {
        SetCaret(ec, ctx, range.Get(), static_cast<LONG>(out.caret));
      }
    }
  } else if (composition_) {
    EndComposition(ec, ctx);
  }
  UpdateCandidateWindow(ec, ctx, out);
  return S_OK;
}

bool TextService::StartComposition(TfEditCookie ec, ITfContext* ctx) {
  auto insert = QueryAs<ITfInsertAtSelection>(ctx, kIID_ITfInsertAtSelection);
  auto context_composition = QueryAs<ITfContextComposition>(ctx, kIID_ITfContextComposition);
  if (!insert || !context_composition) return false;
  ComPtr<ITfRange> range;
  if (FAILED(insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, range.Put())) || !range) {
    return false;
  }
  ComPtr<ITfComposition> composition;
  if (FAILED(context_composition->StartComposition(ec, range.Get(), static_cast<ITfCompositionSink*>(this),
                                                   composition.Put())) ||
      !composition) {
    return false;
  }
  composition_ = composition;
  composition_context_ = ctx;
  return true;
}

void TextService::SetCompositionText(TfEditCookie ec, ITfContext*, const std::u16string& text) {
  ComPtr<ITfRange> range;
  if (FAILED(composition_->GetRange(range.Put()))) return;
  const std::wstring w = ToWide(text);
  range->SetText(ec, 0, w.c_str(), static_cast<LONG>(w.size()));
}

void TextService::ApplyAttributes(TfEditCookie ec, ITfContext* ctx, const tora::Output& out) {
  ComPtr<ITfRange> range;
  ComPtr<ITfProperty> prop;
  if (FAILED(composition_->GetRange(range.Put())) ||
      FAILED(ctx->GetProperty(kGuid_PROP_ATTRIBUTE, prop.Put()))) {
    return;
  }
  prop->Clear(ec, range.Get());
  for (const tora::AttrSpan& span : out.attrs) {
    if (span.length == 0) continue;
    ComPtr<ITfRange> r;
    if (FAILED(range->Clone(r.Put()))) continue;
    LONG shifted = 0;
    r->Collapse(ec, TF_ANCHOR_START);
    r->ShiftEnd(ec, static_cast<LONG>(span.begin + span.length), &shifted, nullptr);
    r->ShiftStart(ec, static_cast<LONG>(span.begin), &shifted, nullptr);
    TfGuidAtom atom = attr_atoms_[0];
    if (span.type == tora::AttrType::kConverted) atom = attr_atoms_[1];
    if (span.type == tora::AttrType::kFocused) atom = attr_atoms_[2];
    if (atom == TF_INVALID_GUIDATOM) continue;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = static_cast<LONG>(atom);
    prop->SetValue(ec, r.Get(), &v);
  }
}

void TextService::SetCaret(TfEditCookie ec, ITfContext* ctx, ITfRange* range, LONG pos) {
  ComPtr<ITfRange> r;
  if (FAILED(range->Clone(r.Put()))) return;
  LONG shifted = 0;
  r->Collapse(ec, TF_ANCHOR_START);
  if (pos > 0) {
    r->ShiftEnd(ec, pos, &shifted, nullptr);
    r->ShiftStart(ec, pos, &shifted, nullptr);
  }
  TF_SELECTION sel{};
  sel.range = r.Get();
  sel.style.ase = TF_AE_NONE;
  sel.style.fInterimChar = FALSE;
  ctx->SetSelection(ec, 1, &sel);
}

void TextService::EndComposition(TfEditCookie ec, ITfContext* ctx) {
  ComPtr<ITfRange> range;
  if (SUCCEEDED(composition_->GetRange(range.Put()))) {
    range->SetText(ec, 0, L"", 0);
    if (ComPtr<ITfProperty> prop; SUCCEEDED(ctx->GetProperty(kGuid_PROP_ATTRIBUTE, prop.Put()))) {
      prop->Clear(ec, range.Get());
    }
  }
  composition_->EndComposition(ec);
  composition_.Reset();
  composition_context_.Reset();
}

void TextService::UpdateCandidateWindow(TfEditCookie ec, ITfContext* ctx, const tora::Output& out) {
  if (!candidate_window_) return;
  if (!out.candidates_visible || !composition_) {
    candidate_window_->Hide();
    return;
  }
  RECT rc{};
  bool have_rect = false;
  ComPtr<ITfContextView> view;
  ComPtr<ITfRange> range;
  if (SUCCEEDED(ctx->GetActiveView(view.Put())) && SUCCEEDED(composition_->GetRange(range.Put()))) {
    ComPtr<ITfRange> r;
    if (SUCCEEDED(range->Clone(r.Put()))) {
      LONG shifted = 0;
      r->Collapse(ec, TF_ANCHOR_START);
      r->ShiftEnd(ec, static_cast<LONG>(out.focus_begin + (out.focus_length > 0 ? 1 : 0)), &shifted, nullptr);
      r->ShiftStart(ec, static_cast<LONG>(out.focus_begin), &shifted, nullptr);
      BOOL clipped = FALSE;
      have_rect = SUCCEEDED(view->GetTextExt(ec, r.Get(), &rc, &clipped));
    }
  }
  if (!have_rect) {
    // 文字の位置が取れないアプリではキャレットの位置を使う
    GUITHREADINFO gti{};
    gti.cbSize = sizeof(gti);
    if (!GetGUIThreadInfo(GetCurrentThreadId(), &gti) || gti.hwndCaret == nullptr) {
      candidate_window_->Hide();
      return;
    }
    rc = gti.rcCaret;
    MapWindowPoints(gti.hwndCaret, nullptr, reinterpret_cast<POINT*>(&rc), 2);
  }
  std::vector<std::wstring> items, notes;
  for (const auto& c : out.candidates) items.push_back(ToWide(c));
  for (const auto& n : out.annotations) notes.push_back(ToWide(n));
  candidate_window_->Show(rc, items, notes, out.selected, out.page, out.page_count);
}

void TextService::OnCandidateClicked(size_t index) {
  if (!session_ || !composition_context_) return;
  tora::Output out = session_->SelectCandidateOnPage(index);
  ApplyOutput(composition_context_.Get(), out);
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie ec, ITfComposition* composition) {
  // アプリ側で未確定文字列が終了された (表示されている文字列はそのまま残る)
  if (composition_ && composition == composition_.Get()) {
    ComPtr<ITfRange> range;
    if (composition_context_ && SUCCEEDED(composition_->GetRange(range.Put()))) {
      if (ComPtr<ITfProperty> prop;
          SUCCEEDED(composition_context_->GetProperty(kGuid_PROP_ATTRIBUTE, prop.Put()))) {
        prop->Clear(ec, range.Get());
      }
    }
    composition_.Reset();
    composition_context_.Reset();
  }
  if (session_) session_->Reset();
  if (candidate_window_) candidate_window_->Hide();
  return S_OK;
}

// ---------------------------------------------------------------- 表示属性

STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) {
  return CreateEnumDisplayAttributeInfo(out);
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** out) {
  return CreateDisplayAttributeInfo(guid, out);
}

}  // namespace toraime
