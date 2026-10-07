// システム辞書 (tools/build_dict.py が生成する toraime.dic) の読み込み
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tora {

// 品詞 ID ごとのフラグ (build_dict.py と同じ値)
enum PosFlag : uint8_t {
  kPosFunctional = 1,  // 助詞・助動詞・接尾辞など。直前の文節にくっつく
  kPosPrefix = 2,      // 接頭詞。直後の語と同じ文節になる
  kPosSymbol = 4,
};

struct DictEntry {
  std::u16string_view surface;
  uint16_t lid;
  uint16_t rid;
  int16_t cost;
};

// テスト用に辞書イメージをメモリ上で作るための入力
struct DictSourceEntry {
  std::u16string reading;
  std::u16string surface;
  uint16_t lid;
  uint16_t rid;
  int16_t cost;
};

class MappedFile;

class SystemDictionary {
 public:
  SystemDictionary();
  ~SystemDictionary();
  SystemDictionary(const SystemDictionary&) = delete;
  SystemDictionary& operator=(const SystemDictionary&) = delete;

  bool OpenFile(const std::filesystem::path& path);
  bool OpenImage(std::vector<uint8_t> image);
  bool loaded() const { return data_ != nullptr; }

  using Callback = std::function<void(size_t reading_len, const DictEntry& entry)>;
  // key の先頭と一致する読みを、短い順にすべて列挙する
  void LookupPrefixes(std::u16string_view key, const Callback& fn) const;
  // 読みが完全一致するものを列挙する (コストの小さい順)
  void LookupExact(std::u16string_view reading, const Callback& fn) const;

  int Connection(uint16_t rid, uint16_t lid) const;
  uint8_t PosFlags(uint16_t id) const;

  uint16_t pos_count() const { return pos_count_; }
  uint16_t id_noun() const { return id_noun_; }
  uint16_t id_number() const { return id_number_; }
  uint16_t id_symbol() const { return id_symbol_; }
  uint16_t id_proper() const { return id_proper_; }
  size_t max_reading_len() const { return max_reading_len_; }

  // build_dict.py と同じ形式のイメージを作る (主にテスト用)
  static std::vector<uint8_t> BuildImage(std::vector<DictSourceEntry> entries, uint16_t pos_count,
                                         const std::vector<int16_t>& matrix,
                                         const std::vector<uint8_t>& pos_flags,
                                         uint16_t id_noun, uint16_t id_number,
                                         uint16_t id_symbol, uint16_t id_proper);

 private:
  bool Attach(const uint8_t* data, size_t size);
  std::u16string_view ReadingAt(uint32_t index) const;
  void EmitEntries(uint32_t reading_index, size_t len, const Callback& fn) const;

  std::unique_ptr<MappedFile> file_;
  std::vector<uint8_t> image_;
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;

  uint16_t pos_count_ = 0;
  uint32_t reading_count_ = 0;
  uint32_t entry_count_ = 0;
  const uint8_t* readings_ = nullptr;
  const uint8_t* entries_ = nullptr;
  const char16_t* strings_ = nullptr;
  uint32_t string_units_ = 0;
  const int16_t* matrix_ = nullptr;
  const uint8_t* pos_flags_ = nullptr;
  uint16_t id_noun_ = 0;
  uint16_t id_number_ = 0;
  uint16_t id_symbol_ = 0;
  uint16_t id_proper_ = 0;
  size_t max_reading_len_ = 0;
};

}  // namespace tora
