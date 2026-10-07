#include "mapped_file.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tora {

MappedFile::~MappedFile() { Close(); }

#ifdef _WIN32

bool MappedFile::Open(const std::filesystem::path& path) {
  Close();
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size;
  if (!GetFileSizeEx(file, &size) || size.QuadPart == 0) {
    CloseHandle(file);
    return false;
  }
  HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (mapping == nullptr) {
    CloseHandle(file);
    return false;
  }
  void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
  if (view == nullptr) {
    CloseHandle(mapping);
    CloseHandle(file);
    return false;
  }
  file_ = file;
  mapping_ = mapping;
  data_ = static_cast<const uint8_t*>(view);
  size_ = static_cast<size_t>(size.QuadPart);
  return true;
}

void MappedFile::Close() {
  if (data_ != nullptr) UnmapViewOfFile(data_);
  if (mapping_ != nullptr) CloseHandle(mapping_);
  if (file_ != nullptr) CloseHandle(file_);
  data_ = nullptr;
  mapping_ = nullptr;
  file_ = nullptr;
  size_ = 0;
}

#else

bool MappedFile::Open(const std::filesystem::path& path) {
  Close();
  int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) return false;
  struct stat st;
  if (fstat(fd, &st) != 0 || st.st_size == 0) {
    ::close(fd);
    return false;
  }
  void* view = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_SHARED, fd, 0);
  ::close(fd);
  if (view == MAP_FAILED) return false;
  data_ = static_cast<const uint8_t*>(view);
  size_ = static_cast<size_t>(st.st_size);
  return true;
}

void MappedFile::Close() {
  if (data_ != nullptr) munmap(const_cast<uint8_t*>(data_), size_);
  data_ = nullptr;
  size_ = 0;
}

#endif

}  // namespace tora
