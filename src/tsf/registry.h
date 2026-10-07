// COM サーバーと TSF のプロファイル・カテゴリの登録
#pragma once

#include "common.h"

namespace toraime {

HRESULT RegisterServer();
HRESULT UnregisterServer();
HRESULT RegisterProfile();
HRESULT UnregisterProfile();
HRESULT RegisterCategories();
HRESULT UnregisterCategories();

}  // namespace toraime
