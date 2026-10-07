// 設定 (レジストリ) とデータファイルの場所、プロセスで共有するエンジン
#pragma once

#include <filesystem>

#include "config.h"
#include "engine.h"

namespace toraime {

// HKCU\Software\toraIME から設定を読む / 書く
tora::Config LoadConfig();
void SaveConfig(const tora::Config& config);

// DLL のあるフォルダー
std::filesystem::path ModuleDirectory();
// システム辞書・英単語リストのあるフォルダー (DLL と同じか、その親。x64/x86 の DLL で共有するため)
std::filesystem::path DataDirectory();
// %APPDATA%\toraIME (ユーザー辞書・学習履歴)。AppContainer 内では空
std::filesystem::path UserDataDirectory();

// ユーザー辞書などのファイルが無ければ説明付きで作ってから、そのパスを返す
std::filesystem::path EnsureUserFile(const wchar_t* name);

// プロセスで共有するエンジン (初回呼び出し時に辞書を読み込む)
tora::Engine* GetEngine();
// レジストリから設定を読み直してエンジンに反映する
void ReloadConfig();

}  // namespace toraime
