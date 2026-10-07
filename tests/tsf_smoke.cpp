// TSF テキストサービスを実際のスレッドマネージャーで有効化・無効化してみるスモークテスト (MSVC のみ)
#include <cstdio>

#include "../src/tsf/common.h"
#include "../src/tsf/display_attribute.h"
#include "../src/tsf/settings.h"
#include "../src/tsf/text_service.h"

using namespace toraime;

static int g_failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAILED line %d: %s\n", __LINE__, #cond);      \
      ++g_failures;                                              \
    }                                                            \
  } while (0)

int main() {
  g_instance = GetModuleHandleW(nullptr);
  CHECK(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));

  // 表示属性の列挙
  {
    ComPtr<IEnumTfDisplayAttributeInfo> e;
    CHECK(SUCCEEDED(CreateEnumDisplayAttributeInfo(e.Put())));
    ITfDisplayAttributeInfo* infos[4] = {};
    ULONG fetched = 0;
    e->Next(4, infos, &fetched);
    CHECK(fetched == 3);
    for (ULONG i = 0; i < fetched; ++i) infos[i]->Release();
  }

  ComPtr<ITfThreadMgr> tm;
  CHECK(SUCCEEDED(CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
                                   tm.PutVoid())));
  if (!tm) return 1;
  TfClientId tid = TF_CLIENTID_NULL;
  CHECK(SUCCEEDED(tm->Activate(&tid)));

  TextService* service = new TextService();
  CHECK(SUCCEEDED(service->ActivateEx(tm.Get(), tid, 0)));
  CHECK(service->IsOpen());
  CHECK(service->CurrentMode() == tora::InputMode::kHiragana);
  CHECK(wcscmp(service->ModeLabel(), L"あ") == 0);

  // 文書とコンテキスト (テキストストア無し)
  ComPtr<ITfDocumentMgr> dm;
  CHECK(SUCCEEDED(tm->CreateDocumentMgr(dm.Put())));
  ComPtr<ITfContext> ctx;
  TfEditCookie ec = 0;
  CHECK(SUCCEEDED(dm->CreateContext(tid, 0, nullptr, ctx.Put(), &ec)));
  CHECK(SUCCEEDED(dm->Push(ctx.Get())));
  CHECK(SUCCEEDED(tm->SetFocus(dm.Get())));

  // キー入力 (テキストストアが無いので編集は失敗してよい。落ちないことを確かめる)
  BOOL eaten = FALSE;
  service->OnTestKeyDown(ctx.Get(), 'K', 0x00250001, &eaten);
  std::printf("eaten(K) = %d\n", eaten);
  service->OnKeyDown(ctx.Get(), 'K', 0x00250001, &eaten);
  service->OnKeyDown(ctx.Get(), 'A', 0x001E0001, &eaten);
  service->OnKeyDown(ctx.Get(), VK_RETURN, 0x001C0001, &eaten);

  // オン/オフの切り替え
  service->ToggleOpen();
  CHECK(!service->IsOpen());
  CHECK(wcscmp(service->ModeLabel(), L"A") == 0);
  service->OnTestKeyDown(ctx.Get(), 'K', 0x00250001, &eaten);
  CHECK(!eaten);  // オフなら文字キーは処理しない
  service->ToggleOpen();
  CHECK(service->IsOpen());

  // 全角が無効なら全角英数モードにはならない
  service->SetMode(true, tora::InputMode::kFullWidthAlnum);
  if (!GetEngine()->config().full_width_allowed()) {
    CHECK(service->CurrentMode() == tora::InputMode::kHalfWidthAlnum);
  }
  service->SetMode(true, tora::InputMode::kHiragana);

  dm->Pop(TF_POPF_ALL);
  CHECK(SUCCEEDED(service->Deactivate()));
  service->Release();
  CHECK(SUCCEEDED(tm->Deactivate()));
  tm.Reset();
  CoUninitialize();
  std::printf("%d failures\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
