// 読み取り専用のメモリマップファイル。辞書を全プロセスで共有するために使う。
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace tora {

class MappedFile {
 public:
  MappedFile() = default;
  ~MappedFile();
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  bool Open(const std::filesystem::path& path);
  void Close();
  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  const uint8_t* data_ = nullptr;
  size_t size_ = 0;
#ifdef _WIN32
  void* file_ = nullptr;
  void* mapping_ = nullptr;
#endif
};

}  // namespace tora
