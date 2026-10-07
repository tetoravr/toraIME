#include "user_data.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "text_util.h"

namespace tora {
namespace {

constexpr size_t kMaxHistoryPerReading = 16;

bool ReadLines(const std::filesystem::path& path, const std::function<void(std::string&)>& fn) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::string line;
  bool first = true;
  while (std::getline(in, line)) {
    if (first) {
      first = false;
      if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
          static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);  // UTF-8 BOM
      }
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    fn(line);
  }
  return true;
}

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> cols;
  std::string col;
  std::istringstream ss(line);
  while (std::getline(ss, col, '\t')) cols.push_back(col);
  return cols;
}

}  // namespace

bool UserDictionary::LoadFile(const std::filesystem::path& path) {
  return ReadLines(path, [this](std::string& line) {
    if (line.empty() || line[0] == '#') return;
    auto cols = SplitTab(line);
    if (cols.size() < 2 || cols[0].empty() || cols[1].empty()) return;
    int cost = 3000;
    if (cols.size() >= 3) {
      try {
        cost = std::stoi(cols[2]);
      } catch (...) {
      }
    }
    Add(Utf8ToUtf16(cols[0]), Utf8ToUtf16(cols[1]), cost);
  });
}

void UserDictionary::Add(std::u16string_view reading, std::u16string_view surface, int cost) {
  auto& list = map_[std::u16string(reading)];
  for (auto& e : list) {
    if (e.surface == surface) {
      e.cost = std::min(e.cost, cost);
      return;
    }
  }
  list.push_back({std::u16string(surface), cost});
  max_reading_len_ = std::max(max_reading_len_, reading.size());
}

const std::vector<UserDictionary::Entry>* UserDictionary::Find(std::u16string_view reading) const {
  auto it = map_.find(std::u16string(reading));
  return it == map_.end() ? nullptr : &it->second;
}

void History::SetPath(const std::filesystem::path& path) {
  std::lock_guard<std::mutex> lock(mu_);
  path_ = path;
}

bool History::Load() {
  std::filesystem::path path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
  }
  if (path.empty()) return false;
  std::unordered_map<std::u16string, std::vector<Item>> map;
  size_t max_len = 0;
  bool ok = ReadLines(path, [&](std::string& line) {
    auto cols = SplitTab(line);
    if (cols.size() < 3 || cols[0].empty() || cols[1].empty()) return;
    int count = 1;
    try {
      count = std::max(1, std::stoi(cols[2]));
    } catch (...) {
    }
    std::u16string reading = Utf8ToUtf16(cols[0]);
    auto& list = map[reading];
    if (list.size() < kMaxHistoryPerReading) list.push_back({Utf8ToUtf16(cols[1]), count});
    max_len = std::max(max_len, reading.size());
  });
  std::lock_guard<std::mutex> lock(mu_);
  map_ = std::move(map);
  max_reading_len_ = max_len;
  return ok;
}

bool History::Save() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (path_.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(path_.parent_path(), ec);
  std::filesystem::path tmp = path_;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    for (const auto& [reading, list] : map_) {
      for (const auto& item : list) {
        out << Utf16ToUtf8(reading) << '\t' << Utf16ToUtf8(item.surface) << '\t' << item.count
            << '\n';
      }
    }
    if (!out) return false;
  }
  std::filesystem::rename(tmp, path_, ec);
  return !ec;
}

void History::Learn(std::u16string_view reading, std::u16string_view surface) {
  if (reading.empty() || surface.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  auto& list = map_[std::u16string(reading)];
  int count = 0;
  for (auto it = list.begin(); it != list.end(); ++it) {
    if (it->surface == surface) {
      count = it->count;
      list.erase(it);
      break;
    }
  }
  list.insert(list.begin(), {std::u16string(surface), std::min(count + 1, 1000)});
  if (list.size() > kMaxHistoryPerReading) list.resize(kMaxHistoryPerReading);
  max_reading_len_ = std::max(max_reading_len_, reading.size());
}

void History::Forget(std::u16string_view reading, std::u16string_view surface) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = map_.find(std::u16string(reading));
  if (it == map_.end()) return;
  auto& list = it->second;
  list.erase(std::remove_if(list.begin(), list.end(),
                            [&](const Item& i) { return i.surface == surface; }),
             list.end());
  if (list.empty()) map_.erase(it);
}

int History::Count(std::u16string_view reading, std::u16string_view surface) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = map_.find(std::u16string(reading));
  if (it == map_.end()) return 0;
  for (const auto& item : it->second) {
    if (item.surface == surface) return item.count;
  }
  return 0;
}

void History::ForEach(std::u16string_view reading,
                      const std::function<void(const std::u16string&, int)>& fn) const {
  std::vector<Item> copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = map_.find(std::u16string(reading));
    if (it == map_.end()) return;
    copy = it->second;
  }
  for (const auto& item : copy) fn(item.surface, item.count);
}

size_t History::max_reading_len() const {
  std::lock_guard<std::mutex> lock(mu_);
  return max_reading_len_;
}

void History::Clear() {
  std::lock_guard<std::mutex> lock(mu_);
  map_.clear();
  max_reading_len_ = 0;
}

bool EnglishWords::LoadFile(const std::filesystem::path& path) {
  return ReadLines(path, [this](std::string& line) {
    size_t hash = line.find('#');
    if (hash != std::string::npos) line.erase(hash);
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) return;
    Add(line.substr(start));
  });
}

void EnglishWords::Add(std::string_view word) {
  if (word.empty()) return;
  for (char c : word) {
    if (!IsAsciiAlpha(c)) return;  // 英字だけの語を対象にする
  }
  std::string lower = AsciiLower(word);
  max_len_ = std::max(max_len_, lower.size());
  words_.insert(std::move(lower));
}

bool EnglishWords::Contains(std::string_view lower_word) const {
  return words_.count(std::string(lower_word)) > 0;
}

}  // namespace tora
