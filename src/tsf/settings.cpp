#include "settings.h"

#include <shlobj.h>

#include <fstream>
#include <iterator>
#include <mutex>

#include "common.h"

namespace toraime {
namespace {

constexpr wchar_t kRegKey[] = L"Software\\toraIME";

DWORD ReadDword(HKEY key, const wchar_t* name, DWORD def) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) ==
          ERROR_SUCCESS &&
      type == REG_DWORD) {
    return value;
  }
  return def;
}

void WriteDword(HKEY key, const wchar_t* name, DWORD value) {
  RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

bool IsAppContainer() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  DWORD is_container = 0;
  DWORD size = 0;
  BOOL ok = GetTokenInformation(token, TokenIsAppContainer, &is_container, sizeof(is_container), &size);
  CloseHandle(token);
  return ok && is_container != 0;
}

}  // namespace

tora::Config LoadConfig() {
  tora::Config c;
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return c;
  DWORD fw = ReadDword(key, L"FullWidth", static_cast<DWORD>(c.full_width));
  if (fw <= 2) c.full_width = static_cast<tora::FullWidthMode>(fw);
  DWORD punct = ReadDword(key, L"Punctuation", static_cast<DWORD>(c.punctuation));
  if (punct <= 3) c.punctuation = static_cast<tora::PunctuationStyle>(punct);
  c.kana_input_enabled = ReadDword(key, L"KanaInput", c.kana_input_enabled) != 0;
  c.half_width_kana_enabled = ReadDword(key, L"HalfWidthKana", c.half_width_kana_enabled) != 0;
  c.english_detection = ReadDword(key, L"EnglishDetection", c.english_detection) != 0;
  DWORD live = ReadDword(key, L"LiveConversion", static_cast<DWORD>(c.live_conversion));
  if (live <= 2) c.live_conversion = static_cast<tora::LiveConversion>(live);
  c.reading_hint = ReadDword(key, L"ReadingHint", c.reading_hint) != 0;
  c.learning = ReadDword(key, L"Learning", c.learning) != 0;
  c.convert_keys_on_off = ReadDword(key, L"ConvertKeysOnOff", c.convert_keys_on_off) != 0;
  RegCloseKey(key);
  return c;
}

void SaveConfig(const tora::Config& c) {
  HKEY key;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return;
  }
  WriteDword(key, L"FullWidth", static_cast<DWORD>(c.full_width));
  WriteDword(key, L"Punctuation", static_cast<DWORD>(c.punctuation));
  WriteDword(key, L"KanaInput", c.kana_input_enabled);
  WriteDword(key, L"HalfWidthKana", c.half_width_kana_enabled);
  WriteDword(key, L"EnglishDetection", c.english_detection);
  WriteDword(key, L"LiveConversion", static_cast<DWORD>(c.live_conversion));
  WriteDword(key, L"ReadingHint", c.reading_hint);
  WriteDword(key, L"Learning", c.learning);
  WriteDword(key, L"ConvertKeysOnOff", c.convert_keys_on_off);
  RegCloseKey(key);
}

std::filesystem::path ModuleDirectory() {
  wchar_t buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameW(g_instance, buf, static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf)) return {};
  return std::filesystem::path(buf).parent_path();
}

std::filesystem::path DataDirectory() {
  const std::filesystem::path dir = ModuleDirectory();
  std::error_code ec;
  if (std::filesystem::exists(dir / L"toraime.dic", ec)) return dir;
  return dir.parent_path();
}

std::filesystem::path UserDataDirectory() {
  static const std::filesystem::path dir = []() -> std::filesystem::path {
    if (IsAppContainer()) return {};
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) return {};
    std::filesystem::path p = std::filesystem::path(path) / L"toraIME";
    CoTaskMemFree(path);
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
  }();
  return dir;
}

std::filesystem::path EnsureUserFile(const wchar_t* name) {
  std::filesystem::path dir = UserDataDirectory();
  if (dir.empty()) return {};
  std::filesystem::path path = dir / name;
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    std::ofstream out(path, std::ios::binary);
    out << "\xEF\xBB\xBF";  // メモ帳で UTF-8 として開けるように BOM を付ける
    if (std::wstring(name) == L"user_dict.txt") {
      out << "# toraIME ユーザー辞書 (UTF-8)\r\n"
             "# 1 行に 1 語、「読み<TAB>表記」で書きます。読みはひらがなです。\r\n"
             "# 3 列目にコスト (小さいほど優先、既定 3000) を書くこともできます。\r\n"
             "# 変更は次に IME を使い始めたとき (アプリの再起動後など) に反映されます。\r\n"
             "# 例:\r\n"
             "# とらいめ\ttoraIME\r\n";
    } else if (std::wstring(name) == L"user_english.txt") {
      out << "# toraIME ユーザー英単語リスト (1 行に 1 語)\r\n"
             "# ここに書いた単語は、ローマ字として読めても英字のまま入力されやすくなります。\r\n"
             "# 例:\r\n"
             "# kotlin\r\n";
    }
  }
  return path;
}

tora::Engine* GetEngine() {
  static tora::Engine* engine = nullptr;
  static std::once_flag once;
  std::call_once(once, [] {
    engine = new tora::Engine();  // プロセス終了まで使うので解放しない
    engine->config() = LoadConfig();
    const std::filesystem::path data_dir = DataDirectory();
    engine->LoadSystemDictionary(data_dir / L"toraime.dic");
    engine->LoadEnglishWords(data_dir / L"english_words.txt");
    const std::filesystem::path user_dir = UserDataDirectory();
    if (!user_dir.empty()) {
      engine->LoadUserDictionary(user_dir / L"user_dict.txt");
      engine->LoadUserEnglishWords(user_dir / L"user_english.txt");
      engine->SetHistoryPath(user_dir / L"history.tsv");
    }
  });
  return engine;
}

void ReloadConfig() { GetEngine()->config() = LoadConfig(); }

}  // namespace toraime
