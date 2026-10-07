// 未確定文字列の表示属性 (下線の種類など)
#pragma once

#include "common.h"

namespace toraime {

enum class AttrKind { kInput = 0, kConverted = 1, kFocused = 2 };
constexpr int kAttrCount = 3;

const GUID& AttrGuid(AttrKind kind);
HRESULT CreateDisplayAttributeInfo(const GUID& guid, ITfDisplayAttributeInfo** out);
HRESULT CreateEnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out);

}  // namespace toraime
