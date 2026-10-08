#include "key_translator.h"

namespace toraime {
namespace {

bool Down(const BYTE* state, int vk) { return (state[vk] & 0x80) != 0; }

// JIS かな配列: { 仮想キー, 通常, Shift }
struct KanaKey {
  WPARAM vk;
  char16_t normal;
  char16_t shifted;
};

const KanaKey kKanaKeys[] = {
    {'1', u'ぬ', u'ぬ'}, {'2', u'ふ', u'ふ'}, {'3', u'あ', u'ぁ'}, {'4', u'う', u'ぅ'},
    {'5', u'え', u'ぇ'}, {'6', u'お', u'ぉ'}, {'7', u'や', u'ゃ'}, {'8', u'ゆ', u'ゅ'},
    {'9', u'よ', u'ょ'}, {'0', u'わ', u'を'}, {VK_OEM_MINUS, u'ほ', u'ほ'}, {VK_OEM_7, u'へ', u'へ'},
    {VK_OEM_5, u'ー', u'ー'},
    {'Q', u'た', u'た'}, {'W', u'て', u'て'}, {'E', u'い', u'ぃ'}, {'R', u'す', u'す'},
    {'T', u'か', u'か'}, {'Y', u'ん', u'ん'}, {'U', u'な', u'な'}, {'I', u'に', u'に'},
    {'O', u'ら', u'ら'}, {'P', u'せ', u'せ'}, {VK_OEM_3, u'゛', u'゛'}, {VK_OEM_4, u'゜', u'「'},
    {'A', u'ち', u'ち'}, {'S', u'と', u'と'}, {'D', u'し', u'し'}, {'F', u'は', u'は'},
    {'G', u'き', u'き'}, {'H', u'く', u'く'}, {'J', u'ま', u'ま'}, {'K', u'の', u'の'},
    {'L', u'り', u'り'}, {VK_OEM_PLUS, u'れ', u'れ'}, {VK_OEM_1, u'け', u'け'}, {VK_OEM_6, u'む', u'」'},
    {'Z', u'つ', u'っ'}, {'X', u'さ', u'さ'}, {'C', u'そ', u'そ'}, {'V', u'ひ', u'ひ'},
    {'B', u'こ', u'こ'}, {'N', u'み', u'み'}, {'M', u'も', u'も'}, {VK_OEM_COMMA, u'ね', u'、'},
    {VK_OEM_PERIOD, u'る', u'。'}, {VK_OEM_2, u'め', u'・'}, {VK_OEM_102, u'ろ', u'ろ'},
};

}  // namespace

bool TranslateKey(WPARAM vk, LPARAM lparam, bool kana_input, bool ignore_caps_lock, tora::KeyEvent* out) {
  using tora::KeyCode;
  BYTE state[256];
  if (!GetKeyboardState(state)) return false;
  if (ignore_caps_lock) state[VK_CAPITAL] &= static_cast<BYTE>(~1);
  tora::KeyEvent k;
  k.shift = Down(state, VK_SHIFT);
  k.ctrl = Down(state, VK_CONTROL);
  k.alt = Down(state, VK_MENU);

  switch (vk) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_CAPITAL:
      return false;
    case VK_SPACE: k.code = KeyCode::kSpace; break;
    case VK_RETURN: k.code = KeyCode::kEnter; break;
    case VK_BACK: k.code = KeyCode::kBackspace; break;
    case VK_DELETE: k.code = KeyCode::kDelete; break;
    case VK_ESCAPE: k.code = KeyCode::kEscape; break;
    case VK_TAB: k.code = KeyCode::kTab; break;
    case VK_LEFT: k.code = KeyCode::kLeft; break;
    case VK_RIGHT: k.code = KeyCode::kRight; break;
    case VK_UP: k.code = KeyCode::kUp; break;
    case VK_DOWN: k.code = KeyCode::kDown; break;
    case VK_HOME: k.code = KeyCode::kHome; break;
    case VK_END: k.code = KeyCode::kEnd; break;
    case VK_PRIOR: k.code = KeyCode::kPageUp; break;
    case VK_NEXT: k.code = KeyCode::kPageDown; break;
    case VK_F6: k.code = KeyCode::kF6; break;
    case VK_F7: k.code = KeyCode::kF7; break;
    case VK_F8: k.code = KeyCode::kF8; break;
    case VK_F9: k.code = KeyCode::kF9; break;
    case VK_F10: k.code = KeyCode::kF10; break;
    default: {
      if (k.ctrl || k.alt) {
        // ショートカットキー。文字は付けずに渡す (入力中なら無視、そうでなければアプリへ)
        k.code = KeyCode::kChar;
        k.ch = 0;
        break;
      }
      if (kana_input) {
        for (const KanaKey& kk : kKanaKeys) {
          if (kk.vk == vk) {
            k.code = KeyCode::kChar;
            k.ch = k.shift ? kk.shifted : kk.normal;
            k.kana = true;
            *out = k;
            return true;
          }
        }
      }
      // 現在のキーボード配列で文字にする (0x4: キーボードの状態を変えない)
      wchar_t buf[4] = {};
      UINT scan = (static_cast<UINT>(lparam) >> 16) & 0xFF;
      int n = ToUnicodeEx(static_cast<UINT>(vk), scan, state, buf, 4, 0x4, GetKeyboardLayout(0));
      if (n != 1 || buf[0] <= 0x20 || buf[0] >= 0x7F) return false;
      k.code = KeyCode::kChar;
      k.ch = buf[0];
      break;
    }
  }
  *out = k;
  return true;
}

}  // namespace toraime
