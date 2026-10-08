// toraIME が自前で定義している TSF の GUID が Windows SDK の値と一致するかを確かめる (MSVC のみ)
// (GUID_LBI_INPUTMODE は SDK のヘッダーに宣言が無いので対象外)
#include <cstdio>

#include "../src/tsf/common.h"

using namespace toraime;

static int g_failures = 0;

static void Check(const GUID& mine, const GUID& sdk, const char* name) {
  if (!IsEqualGUID(mine, sdk)) {
    std::printf("MISMATCH: %s\n", name);
    ++g_failures;
  }
}

#define CHECK_GUID(mine, sdk) Check(mine, sdk, #sdk)

int main() {
  CHECK_GUID(kIID_ITfTextInputProcessor, IID_ITfTextInputProcessor);
  CHECK_GUID(kIID_ITfTextInputProcessorEx, IID_ITfTextInputProcessorEx);
  CHECK_GUID(kIID_ITfThreadMgrEventSink, IID_ITfThreadMgrEventSink);
  CHECK_GUID(kIID_ITfKeyEventSink, IID_ITfKeyEventSink);
  CHECK_GUID(kIID_ITfCompositionSink, IID_ITfCompositionSink);
  CHECK_GUID(kIID_ITfDisplayAttributeProvider, IID_ITfDisplayAttributeProvider);
  CHECK_GUID(kIID_ITfCompartmentEventSink, IID_ITfCompartmentEventSink);
  CHECK_GUID(kIID_ITfThreadFocusSink, IID_ITfThreadFocusSink);
  CHECK_GUID(kIID_ITfEditSession, IID_ITfEditSession);
  CHECK_GUID(kIID_ITfDisplayAttributeInfo, IID_ITfDisplayAttributeInfo);
  CHECK_GUID(kIID_IEnumTfDisplayAttributeInfo, IID_IEnumTfDisplayAttributeInfo);
  CHECK_GUID(kIID_ITfLangBarItem, IID_ITfLangBarItem);
  CHECK_GUID(kIID_ITfLangBarItemButton, IID_ITfLangBarItemButton);
  CHECK_GUID(kIID_ITfLangBarItemSink, IID_ITfLangBarItemSink);
  CHECK_GUID(kIID_ITfMenu, IID_ITfMenu);
  CHECK_GUID(kIID_ITfSource, IID_ITfSource);
  CHECK_GUID(kIID_ITfKeystrokeMgr, IID_ITfKeystrokeMgr);
  CHECK_GUID(kIID_ITfLangBarItemMgr, IID_ITfLangBarItemMgr);
  CHECK_GUID(kIID_ITfCompartmentMgr, IID_ITfCompartmentMgr);
  CHECK_GUID(kIID_ITfInsertAtSelection, IID_ITfInsertAtSelection);
  CHECK_GUID(kIID_ITfContextComposition, IID_ITfContextComposition);
  CHECK_GUID(kIID_ITfCategoryMgr, IID_ITfCategoryMgr);
  CHECK_GUID(kIID_ITfInputProcessorProfileMgr, IID_ITfInputProcessorProfileMgr);
  CHECK_GUID(kIID_ITfThreadMgr, IID_ITfThreadMgr);
  CHECK_GUID(kClsid_TF_CategoryMgr, CLSID_TF_CategoryMgr);
  CHECK_GUID(kClsid_TF_InputProcessorProfiles, CLSID_TF_InputProcessorProfiles);
  CHECK_GUID(kGuid_TFCAT_TIP_KEYBOARD, GUID_TFCAT_TIP_KEYBOARD);
  CHECK_GUID(kGuid_TFCAT_DISPLAYATTRIBUTEPROVIDER, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER);
  CHECK_GUID(kGuid_TFCAT_TIPCAP_SECUREMODE, GUID_TFCAT_TIPCAP_SECUREMODE);
  CHECK_GUID(kGuid_TFCAT_TIPCAP_COMLESS, GUID_TFCAT_TIPCAP_COMLESS);
  CHECK_GUID(kGuid_TFCAT_TIPCAP_INPUTMODECOMPARTMENT, GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT);
  CHECK_GUID(kGuid_TFCAT_TIPCAP_IMMERSIVESUPPORT, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT);
  CHECK_GUID(kGuid_TFCAT_TIPCAP_SYSTRAYSUPPORT, GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT);
  CHECK_GUID(kGuid_COMPARTMENT_KEYBOARD_OPENCLOSE, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
  CHECK_GUID(kGuid_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
  CHECK_GUID(kGuid_COMPARTMENT_KEYBOARD_DISABLED, GUID_COMPARTMENT_KEYBOARD_DISABLED);
  CHECK_GUID(kGuid_COMPARTMENT_EMPTYCONTEXT, GUID_COMPARTMENT_EMPTYCONTEXT);
  CHECK_GUID(kGuid_PROP_ATTRIBUTE, GUID_PROP_ATTRIBUTE);
  std::printf("%d mismatches\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
