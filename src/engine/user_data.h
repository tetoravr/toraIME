// ユーザー辞書・学習履歴・英単語リスト
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tora {

// ユーザー辞書。ファイル形式 (UTF-8, タブ区切り): 読み<TAB>表記[<TAB>コスト]
class UserDictionary {
 public:
  struct Entry {
    std::u16string surface;
    int cost;
  };

  bool LoadFile(const std::filesystem::path& path);
  void Add(std::u16string_view reading, std::u16string_view surface, int cost);
  const std::vector<Entry>* Find(std::u16string_view reading) const;
  size_t max_reading_len() const { return max_reading_len_; }
  bool empty() const { return map_.empty(); }

 private:
  std::unordered_map<std::u16string, std::vector<Entry>> map_;
  size_t max_reading_len_ = 0;
};

// 変換の学習履歴。スレッドセーフ。
// ファイル形式 (UTF-8, タブ区切り): 読み<TAB>表記<TAB>回数
class History {
 public:
  void SetPath(const std::filesystem::path& path);
  bool Load();
  bool Save() const;

  void Learn(std::u16string_view reading, std::u16string_view surface);
  void Forget(std::u16string_view reading, std::u16string_view surface);
  // 学習回数 (0 なら未学習)
  int Count(std::u16string_view reading, std::u16string_view surface) const;
  // reading の学習済み表記を新しい順に列挙する
  void ForEach(std::u16string_view reading,
               const std::function<void(const std::u16string& surface, int count)>& fn) const;
  size_t max_reading_len() const;
  void Clear();

 private:
  struct Item {
    std::u16string surface;
    int count;
  };
  mutable std::mutex mu_;
  std::unordered_map<std::u16string, std::vector<Item>> map_;
  std::filesystem::path path_;
  size_t max_reading_len_ = 0;
};

// 英単語リスト。1 行 1 語、# から行末まではコメント。比較は小文字で行う。
class EnglishWords {
 public:
  bool LoadFile(const std::filesystem::path& path);
  void Add(std::string_view word);
  bool Contains(std::string_view lower_word) const;
  size_t max_len() const { return max_len_; }

 private:
  std::unordered_set<std::string> words_;
  size_t max_len_ = 0;
};

}  // namespace tora
