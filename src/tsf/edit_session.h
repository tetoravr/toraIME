// 関数オブジェクトを実行するだけの ITfEditSession
#pragma once

#include <functional>

#include "common.h"

namespace toraime {

using EditFunction = std::function<HRESULT(TfEditCookie)>;

// ctx の編集セッションを要求する。flags には TF_ES_READWRITE などを指定する。
// TF_ES_ASYNCDONTCARE を指定すると非同期に実行されることがあるので、fn は値でキャプチャすること。
HRESULT RequestEdit(ITfContext* ctx, TfClientId client_id, DWORD flags, EditFunction fn);

}  // namespace toraime
