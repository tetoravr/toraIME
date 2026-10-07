// プロセス内で共有する変換エンジン (辞書・設定・学習履歴)
#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

#include "config.h"
#include "converter.h"
#include "dictionary.h"
#include "user_data.h"

namespace tora {

class Engine {
 public:
  Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  bool LoadSystemDictionary(const std::filesystem::path& path);
  bool LoadSystemDictionaryImage(std::vector<uint8_t> image);  // テスト用
  bool LoadEnglishWords(const std::filesystem::path& path);
  bool LoadUserDictionary(const std::filesystem::path& path);
  bool LoadUserEnglishWords(const std::filesystem::path& path);
  void SetHistoryPath(const std::filesystem::path& path);

  Config& config() { return config_; }
  const Config& config() const { return config_; }
  const Converter& converter() const { return converter_; }
  const SystemDictionary& dictionary() const { return dict_; }
  History& history() { return history_; }
  EnglishWords& english() { return english_; }
  UserDictionary& user_dictionary() { return user_; }

  // 選ばれた候補を学習して保存する
  void Learn(std::u16string_view reading, std::u16string_view surface);

 private:
  Config config_;
  SystemDictionary dict_;
  UserDictionary user_;
  History history_;
  EnglishWords english_;
  Converter converter_;
};

}  // namespace tora
