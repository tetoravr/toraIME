#include "dictionary.h"

#include <algorithm>
#include <cstring>
#include <map>

#include "mapped_file.h"

namespace tora {
namespace {

constexpr char kMagic[8] = {'T', 'O', 'R', 'A', 'D', 'I', 'C', '1'};
constexpr size_t kHeaderSize = 64;
constexpr size_t kReadingRecSize = 16;
constexpr size_t kEntryRecSize = 12;

uint32_t ReadU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t ReadU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
void WriteU32(std::vector<uint8_t>* out, uint32_t v) {
  for (int i = 0; i < 4; ++i) out->push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void WriteU16(std::vector<uint8_t>* out, uint16_t v) {
  out->push_back(static_cast<uint8_t>(v));
  out->push_back(static_cast<uint8_t>(v >> 8));
}

// a と b[0..len) を、a の先頭 len 文字だけで比較する
int ComparePrefix(std::u16string_view reading, std::u16string_view prefix) {
  size_t n = std::min(reading.size(), prefix.size());
  for (size_t i = 0; i < n; ++i) {
    if (reading[i] != prefix[i]) return reading[i] < prefix[i] ? -1 : 1;
  }
  if (reading.size() < prefix.size()) return -1;
  return 0;
}

}  // namespace

SystemDictionary::SystemDictionary() = default;
SystemDictionary::~SystemDictionary() = default;

bool SystemDictionary::OpenFile(const std::filesystem::path& path) {
  auto file = std::make_unique<MappedFile>();
  if (!file->Open(path)) return false;
  if (!Attach(file->data(), file->size())) return false;
  file_ = std::move(file);
  return true;
}

bool SystemDictionary::OpenImage(std::vector<uint8_t> image) {
  image_ = std::move(image);
  return Attach(image_.data(), image_.size());
}

bool SystemDictionary::Attach(const uint8_t* data, size_t size) {
  data_ = nullptr;
  if (size < kHeaderSize || std::memcmp(data, kMagic, sizeof(kMagic)) != 0) return false;
  const uint8_t* h = data + 8;
  uint32_t version = ReadU32(h);
  if (version != 1) return false;
  uint32_t pos_count = ReadU32(h + 4);
  uint32_t reading_count = ReadU32(h + 8);
  uint32_t entry_count = ReadU32(h + 12);
  uint32_t off_readings = ReadU32(h + 16);
  uint32_t off_entries = ReadU32(h + 20);
  uint32_t off_strings = ReadU32(h + 24);
  uint32_t string_units = ReadU32(h + 28);
  uint32_t off_matrix = ReadU32(h + 32);
  uint32_t off_flags = ReadU32(h + 36);
  if (pos_count == 0 || pos_count > 0xFFFF) return false;
  uint64_t matrix_bytes = static_cast<uint64_t>(pos_count) * pos_count * 2;
  if (static_cast<uint64_t>(off_readings) + uint64_t{reading_count} * kReadingRecSize > size ||
      static_cast<uint64_t>(off_entries) + uint64_t{entry_count} * kEntryRecSize > size ||
      static_cast<uint64_t>(off_strings) + uint64_t{string_units} * 2 > size ||
      off_matrix + matrix_bytes > size || uint64_t{off_flags} + pos_count > size ||
      off_strings % 2 != 0 || off_matrix % 2 != 0) {
    return false;
  }
  pos_count_ = static_cast<uint16_t>(pos_count);
  reading_count_ = reading_count;
  entry_count_ = entry_count;
  readings_ = data + off_readings;
  entries_ = data + off_entries;
  strings_ = reinterpret_cast<const char16_t*>(data + off_strings);
  string_units_ = string_units;
  matrix_ = reinterpret_cast<const int16_t*>(data + off_matrix);
  pos_flags_ = data + off_flags;
  id_noun_ = ReadU16(h + 40);
  id_number_ = ReadU16(h + 42);
  id_symbol_ = ReadU16(h + 44);
  id_proper_ = ReadU16(h + 46);
  max_reading_len_ = ReadU32(h + 48);
  data_ = data;
  size_ = size;
  return true;
}

std::u16string_view SystemDictionary::ReadingAt(uint32_t index) const {
  const uint8_t* rec = readings_ + size_t{index} * kReadingRecSize;
  uint32_t off = ReadU32(rec);
  uint32_t len = ReadU32(rec + 4);
  if (uint64_t{off} + len > string_units_) return {};
  return std::u16string_view(strings_ + off, len);
}

void SystemDictionary::EmitEntries(uint32_t reading_index, size_t len, const Callback& fn) const {
  const uint8_t* rec = readings_ + size_t{reading_index} * kReadingRecSize;
  uint32_t first = ReadU32(rec + 8);
  uint32_t count = ReadU32(rec + 12);
  if (uint64_t{first} + count > entry_count_) return;
  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t* e = entries_ + size_t{first + i} * kEntryRecSize;
    uint32_t soff = ReadU32(e);
    uint16_t slen = ReadU16(e + 4);
    if (uint64_t{soff} + slen > string_units_) continue;
    DictEntry entry;
    entry.surface = std::u16string_view(strings_ + soff, slen);
    entry.lid = ReadU16(e + 6);
    entry.rid = ReadU16(e + 8);
    entry.cost = static_cast<int16_t>(ReadU16(e + 10));
    if (entry.lid >= pos_count_ || entry.rid >= pos_count_) continue;
    fn(len, entry);
  }
}

void SystemDictionary::LookupPrefixes(std::u16string_view key, const Callback& fn) const {
  if (!loaded()) return;
  uint32_t lo = 0;
  uint32_t hi = reading_count_;
  size_t max_len = std::min(key.size(), max_reading_len_);
  for (size_t len = 1; len <= max_len && lo < hi; ++len) {
    std::u16string_view prefix = key.substr(0, len);
    // [lo, hi) のうち prefix で始まる範囲に絞り込む
    uint32_t a = lo, b = hi;
    while (a < b) {
      uint32_t mid = a + (b - a) / 2;
      if (ComparePrefix(ReadingAt(mid), prefix) < 0) a = mid + 1; else b = mid;
    }
    uint32_t new_lo = a;
    b = hi;
    while (a < b) {
      uint32_t mid = a + (b - a) / 2;
      if (ComparePrefix(ReadingAt(mid), prefix) <= 0) a = mid + 1; else b = mid;
    }
    lo = new_lo;
    hi = a;
    if (lo < hi && ReadingAt(lo).size() == len) EmitEntries(lo, len, fn);
  }
}

void SystemDictionary::LookupExact(std::u16string_view reading, const Callback& fn) const {
  if (!loaded() || reading.empty()) return;
  uint32_t a = 0, b = reading_count_;
  while (a < b) {
    uint32_t mid = a + (b - a) / 2;
    if (ReadingAt(mid) < reading) a = mid + 1; else b = mid;
  }
  if (a < reading_count_ && ReadingAt(a) == reading) EmitEntries(a, reading.size(), fn);
}

int SystemDictionary::Connection(uint16_t rid, uint16_t lid) const {
  if (!loaded() || rid >= pos_count_ || lid >= pos_count_) return 0;
  int16_t v;
  std::memcpy(&v, matrix_ + size_t{rid} * pos_count_ + lid, sizeof(v));
  return v;
}

uint8_t SystemDictionary::PosFlags(uint16_t id) const {
  if (!loaded() || id >= pos_count_) return 0;
  return pos_flags_[id];
}

std::vector<uint8_t> SystemDictionary::BuildImage(std::vector<DictSourceEntry> entries,
                                                  uint16_t pos_count,
                                                  const std::vector<int16_t>& matrix,
                                                  const std::vector<uint8_t>& pos_flags,
                                                  uint16_t id_noun, uint16_t id_number,
                                                  uint16_t id_symbol, uint16_t id_proper) {
  std::map<std::u16string, std::vector<DictSourceEntry>> by_reading;
  for (auto& e : entries) by_reading[e.reading].push_back(std::move(e));

  std::vector<uint8_t> readings, recs;
  std::u16string strings;
  uint32_t entry_count = 0;
  uint32_t max_len = 0;
  for (auto& [reading, list] : by_reading) {
    std::stable_sort(list.begin(), list.end(),
                     [](const DictSourceEntry& a, const DictSourceEntry& b) { return a.cost < b.cost; });
    WriteU32(&readings, static_cast<uint32_t>(strings.size()));
    WriteU32(&readings, static_cast<uint32_t>(reading.size()));
    strings += reading;
    WriteU32(&readings, entry_count);
    WriteU32(&readings, static_cast<uint32_t>(list.size()));
    max_len = std::max(max_len, static_cast<uint32_t>(reading.size()));
    for (const auto& e : list) {
      WriteU32(&recs, static_cast<uint32_t>(strings.size()));
      WriteU16(&recs, static_cast<uint16_t>(e.surface.size()));
      strings += e.surface;
      WriteU16(&recs, e.lid);
      WriteU16(&recs, e.rid);
      WriteU16(&recs, static_cast<uint16_t>(e.cost));
      ++entry_count;
    }
  }

  uint32_t off_readings = kHeaderSize;
  uint32_t off_entries = off_readings + static_cast<uint32_t>(readings.size());
  uint32_t off_strings = off_entries + static_cast<uint32_t>(recs.size());
  uint32_t off_matrix = off_strings + static_cast<uint32_t>(strings.size() * 2);
  uint32_t off_flags = off_matrix + static_cast<uint32_t>(pos_count) * pos_count * 2;

  std::vector<uint8_t> out(kMagic, kMagic + 8);
  WriteU32(&out, 1);
  WriteU32(&out, pos_count);
  WriteU32(&out, static_cast<uint32_t>(by_reading.size()));
  WriteU32(&out, entry_count);
  WriteU32(&out, off_readings);
  WriteU32(&out, off_entries);
  WriteU32(&out, off_strings);
  WriteU32(&out, static_cast<uint32_t>(strings.size()));
  WriteU32(&out, off_matrix);
  WriteU32(&out, off_flags);
  WriteU16(&out, id_noun);
  WriteU16(&out, id_number);
  WriteU16(&out, id_symbol);
  WriteU16(&out, id_proper);
  WriteU32(&out, max_len);
  out.resize(kHeaderSize, 0);
  out.insert(out.end(), readings.begin(), readings.end());
  out.insert(out.end(), recs.begin(), recs.end());
  for (char16_t c : strings) WriteU16(&out, c);
  for (size_t i = 0; i < size_t{pos_count} * pos_count; ++i) {
    WriteU16(&out, static_cast<uint16_t>(i < matrix.size() ? matrix[i] : 0));
  }
  for (size_t i = 0; i < pos_count; ++i) out.push_back(i < pos_flags.size() ? pos_flags[i] : 0);
  return out;
}

}  // namespace tora
