#include "converter.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "text_util.h"

namespace tora {
namespace {

constexpr size_t kMaxKanaKey = 64;
constexpr int32_t kInfinity = std::numeric_limits<int32_t>::max() / 2;
constexpr size_t kMaxHeadCandidates = 60;
constexpr size_t kMaxWholeCandidates = 20;
constexpr size_t kMaxChainPieces = 80;

}  // namespace

std::u16string Converter::ApplyWidth(std::u16string_view ascii) const {
  if (config_->full_width == FullWidthMode::kDefault) return AsciiToFullWidth(ascii);
  return std::u16string(ascii);
}

int Converter::Connection(uint16_t rid, uint16_t lid) const {
  return dict_ != nullptr ? dict_->Connection(rid, lid) : 0;
}

uint8_t Converter::PosFlags(uint16_t id) const {
  return dict_ != nullptr ? dict_->PosFlags(id) : 0;
}

int Converter::HistoryBonus(std::u16string_view reading, std::u16string_view surface) const {
  if (history_ == nullptr || !config_->learning) return 0;
  int count = history_->Count(reading, surface);
  if (count <= 0) return 0;
  return 1500 + 500 * std::min(count, 6);
}

void Converter::AddWordNodes(const std::vector<Piece>& chain, std::vector<Node>* out) const {
  const uint32_t p = chain.front().begin;
  std::u16string key;
  std::vector<std::pair<size_t, uint32_t>> bounds;  // (読みの長さ, 終端オフセット)
  for (const Piece& piece : chain) {
    if (piece.unit.kind != UnitKind::kKana) break;
    key += piece.unit.text;
    bounds.emplace_back(key.size(), piece.end);
    if (key.size() >= kMaxKanaKey) break;
  }
  if (key.empty()) return;

  auto unit_end_for = [&bounds](size_t len) -> uint32_t {
    for (const auto& b : bounds) {
      if (b.first == len) return b.second;
      if (b.first > len) break;
    }
    return 0;
  };

  // 学習履歴を読みの長さごとに集めておく
  std::unordered_map<size_t, std::vector<std::pair<std::u16string, int>>> learned;
  if (history_ != nullptr && config_->learning) {
    size_t max_len = history_->max_reading_len();
    for (const auto& b : bounds) {
      if (b.first > max_len) break;
      history_->ForEach(key.substr(0, b.first), [&](const std::u16string& s, int count) {
        learned[b.first].emplace_back(s, count);
      });
    }
  }
  auto bonus_for = [&learned](size_t len, std::u16string_view surface) -> int {
    auto it = learned.find(len);
    if (it == learned.end()) return 0;
    for (const auto& [s, count] : it->second) {
      if (s == surface) return 1500 + 500 * std::min(count, 6);
    }
    return 0;
  };

  std::unordered_set<std::u16string> seen;  // 学習のみのノードとの重複除け (終端 + 表記)
  if (dict_ != nullptr) {
    dict_->LookupPrefixes(key, [&](size_t len, const DictEntry& e) {
      uint32_t q = unit_end_for(len);
      if (q == 0) return;
      Node n;
      n.begin = p;
      n.end = q;
      n.surface = std::u16string(e.surface);
      n.reading = key.substr(0, len);
      n.lid = e.lid;
      n.rid = e.rid;
      int bonus = bonus_for(len, e.surface);
      n.cost = e.cost - bonus;
      n.kind = NodeKind::kWord;
      if (bonus > 0) seen.insert(std::u16string(1, static_cast<char16_t>(q)) + n.surface);
      out->push_back(std::move(n));
    });
  }

  uint16_t noun = dict_ != nullptr ? dict_->id_noun() : 0;
  if (user_ != nullptr && !user_->empty()) {
    for (const auto& b : bounds) {
      if (b.first > user_->max_reading_len()) break;
      std::u16string reading = key.substr(0, b.first);
      const auto* list = user_->Find(reading);
      if (list == nullptr) continue;
      for (const auto& e : *list) {
        Node n;
        n.begin = p;
        n.end = b.second;
        n.surface = e.surface;
        n.reading = reading;
        n.lid = n.rid = noun;
        n.cost = e.cost - bonus_for(b.first, e.surface);
        n.kind = NodeKind::kUser;
        out->push_back(std::move(n));
      }
    }
  }

  for (const auto& [len, items] : learned) {
    uint32_t q = unit_end_for(len);
    if (q == 0) continue;
    for (const auto& [surface, count] : items) {
      if (seen.count(std::u16string(1, static_cast<char16_t>(q)) + surface) > 0) continue;
      Node n;
      n.begin = p;
      n.end = q;
      n.surface = surface;
      n.reading = key.substr(0, len);
      n.lid = n.rid = noun;
      n.cost = std::max(500, kHistoryOnlyCost - (1500 + 500 * std::min(count, 6)));
      n.kind = NodeKind::kHistory;
      out->push_back(std::move(n));
    }
  }
}

void Converter::AddAsciiNodes(const Input& input, const std::vector<Piece>& chain,
                              std::vector<Node>* out) const {
  const Unit& first = chain.front().unit;
  if (!Input::IsAlphaUnit(first)) return;
  const bool upper = first.kind == UnitKind::kLetter && IsAsciiUpper(first.raw[0]);
  const bool detect = config_->english_detection;
  if (!upper && !detect) return;

  // 英字の並び (ローマ字として読めたかどうかを 1 文字ずつ記録する)
  const uint32_t p = chain.front().begin;
  std::string run;
  std::vector<bool> junk;
  for (const Piece& piece : chain) {
    if (!Input::IsAlphaUnit(piece.unit)) break;
    for (char ch : piece.unit.raw) {
      run.push_back(ch);
      junk.push_back(piece.unit.kind == UnitKind::kLetter);
    }
    if (run.size() >= static_cast<size_t>(kMaxAsciiSpan)) break;
  }
  const uint32_t run_end = p + static_cast<uint32_t>(run.size());
  const bool run_start = p == 0 || !IsAsciiAlpha(input.RawCharAt(p - 1));
  const bool run_continues = IsAsciiAlpha(input.RawCharAt(run_end));
  const uint16_t noun = dict_ != nullptr ? dict_->id_noun() : 0;

  bool has_junk = false;
  int span_cost = kUpperSpanBase;
  for (size_t len = 1; len <= run.size() && len <= static_cast<size_t>(kMaxAsciiSpan); ++len) {
    const char ch = run[len - 1];
    const bool is_junk = junk[len - 1] && !IsAsciiUpper(ch);
    has_junk = has_junk || is_junk;
    span_cost += IsAsciiUpper(ch) ? kUpperSpanPerUpper : (is_junk ? kUpperSpanPerJunk : kUpperSpanPerLower);

    const uint32_t q = p + static_cast<uint32_t>(len);
    const std::string raw = run.substr(0, len);
    const std::u16string raw16 = AsciiToU16(raw);
    const std::string lower = AsciiLower(raw);
    const std::u16string lower16 = AsciiToU16(lower);
    const bool whole = run_start && q == run_end && !run_continues;

    auto add = [&](int cost, NodeKind kind) {
      Node n;
      n.begin = p;
      n.end = q;
      n.surface = ApplyWidth(raw16);
      n.reading = lower16;
      n.lid = n.rid = noun;
      n.cost = cost;
      n.kind = kind;
      out->push_back(std::move(n));
    };

    if (detect && len >= 2) {
      const int bonus = HistoryBonus(lower16, raw16);
      const bool known = english_ != nullptr && english_->Contains(lower);
      if ((known && (len >= 3 || whole)) || bonus > 0) {
        int cost = kEnglishBase - kEnglishPerChar * static_cast<int>(std::min<size_t>(len, 10));
        if (!whole) cost += len == 3 ? kEnglishMidRunPenalty3 : kEnglishMidRunPenalty;
        add(cost - bonus, NodeKind::kEnglish);
      }
    }
    if (upper) {
      add(span_cost, NodeKind::kAsciiSpan);
    } else if (detect && has_junk) {
      add(kAsciiSpanBase + kAsciiSpanPerChar * static_cast<int>(len), NodeKind::kAsciiSpan);
    }
  }
}

void Converter::AddFallbackNode(const std::vector<Piece>& chain, std::vector<Node>* out) const {
  const Unit& u = chain.front().unit;
  const uint16_t noun = dict_ != nullptr ? dict_->id_noun() : 0;
  Node n;
  n.begin = chain.front().begin;
  n.end = chain.front().end;
  n.lid = n.rid = noun;
  switch (u.kind) {
    case UnitKind::kKana:
      n.surface = u.text;
      n.reading = u.text;
      n.cost = kUnknownKanaCost;
      n.kind = NodeKind::kUnknownKana;
      break;
    case UnitKind::kLetter:
      n.surface = ApplyWidth(AsciiToU16(u.raw));
      n.reading = AsciiToU16(AsciiLower(u.raw));
      n.cost = IsAsciiUpper(u.raw[0]) ? kUnknownUpperCost : kUnknownLetterCost;
      n.kind = NodeKind::kUnknownLetter;
      break;
    case UnitKind::kDigit: {
      std::u16string digits;
      for (const Piece& piece : chain) {
        if (piece.unit.kind != UnitKind::kDigit) break;
        digits += AsciiToU16(piece.unit.raw);
        n.end = piece.end;
      }
      n.surface = ApplyWidth(digits);
      n.reading = digits;
      n.lid = n.rid = dict_ != nullptr ? dict_->id_number() : 0;
      n.cost = kNumberCost;
      n.kind = NodeKind::kNumber;
      break;
    }
    case UnitKind::kSymbol: {
      n.surface = u.text;
      n.reading = u.text;
      n.lid = n.rid = dict_ != nullptr ? dict_->id_symbol() : 0;
      n.cost = kSymbolCost;
      n.kind = NodeKind::kSymbol;
      if (dict_ != nullptr) {
        bool found = false;
        dict_->LookupExact(u.text, [&](size_t, const DictEntry& e) {
          if (found || e.surface != u.text) return;
          found = true;
          n.lid = e.lid;
          n.rid = e.rid;
          n.cost = e.cost;
        });
      }
      break;
    }
    case UnitKind::kSpace:
      n.surface = config_->full_width == FullWidthMode::kDefault ? u"　" : u" ";
      n.reading = u" ";
      n.lid = n.rid = dict_ != nullptr ? dict_->id_symbol() : 0;
      n.cost = kSpaceCost;
      n.kind = NodeKind::kSpace;
      break;
  }
  out->push_back(std::move(n));
}

void Converter::AddNodesAt(const Input& input, uint32_t p, uint32_t end,
                           std::vector<Node>* out) const {
  const std::vector<Piece> chain = input.Slice(p, end, kMaxChainPieces);
  if (chain.empty()) return;
  std::vector<Node> nodes;
  AddWordNodes(chain, &nodes);
  AddAsciiNodes(input, chain, &nodes);
  AddFallbackNode(chain, &nodes);
  // 経路探索には (終端, 左文脈, 右文脈) ごとに最小コストのノードだけあればよい
  std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) {
    if (a.end != b.end) return a.end < b.end;
    if (a.lid != b.lid) return a.lid < b.lid;
    if (a.rid != b.rid) return a.rid < b.rid;
    return a.cost < b.cost;
  });
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (i > 0 && nodes[i].end == nodes[i - 1].end && nodes[i].lid == nodes[i - 1].lid &&
        nodes[i].rid == nodes[i - 1].rid) {
      continue;
    }
    out->push_back(std::move(nodes[i]));
  }
}

std::vector<Node> Converter::Convert(const Input& input, uint32_t begin, uint32_t end,
                                     uint16_t left_rid, bool eos) const {
  end = std::min(end, input.size());
  if (begin >= end) return {};
  const uint32_t width = end - begin;

  std::vector<Node> nodes;
  std::vector<int32_t> total;
  std::vector<int32_t> prev;
  std::vector<std::vector<uint32_t>> ends(width + 1);

  for (uint32_t p = begin; p < end; ++p) {
    // 直前のノードを右文脈 ID ごとに最良のものだけにまとめる
    std::vector<std::pair<uint16_t, uint32_t>> groups;
    if (p != begin) {
      const auto& list = ends[p - begin];
      if (list.empty()) continue;
      std::unordered_map<uint16_t, uint32_t> best;
      for (uint32_t idx : list) {
        auto it = best.find(nodes[idx].rid);
        if (it == best.end() || total[idx] < total[it->second]) best[nodes[idx].rid] = idx;
      }
      groups.assign(best.begin(), best.end());
    }

    std::vector<Node> local;
    AddNodesAt(input, p, end, &local);
    for (Node& n : local) {
      int32_t best_cost = kInfinity;
      int32_t best_prev = -1;
      if (p == begin) {
        best_cost = Connection(left_rid, n.lid);
      } else {
        for (const auto& [rid, idx] : groups) {
          int32_t c = total[idx] + Connection(rid, n.lid);
          if (c < best_cost) {
            best_cost = c;
            best_prev = static_cast<int32_t>(idx);
          }
        }
      }
      const uint32_t idx = static_cast<uint32_t>(nodes.size());
      total.push_back(best_cost + n.cost);
      prev.push_back(best_prev);
      ends[n.end - begin].push_back(idx);
      nodes.push_back(std::move(n));
    }
  }

  int32_t best_cost = kInfinity;
  int32_t best = -1;
  for (uint32_t idx : ends[width]) {
    int32_t c = total[idx] + (eos ? Connection(nodes[idx].rid, 0) : 0);
    if (c < best_cost) {
      best_cost = c;
      best = static_cast<int32_t>(idx);
    }
  }
  std::vector<Node> path;
  for (int32_t i = best; i >= 0; i = prev[i]) path.push_back(nodes[i]);
  std::reverse(path.begin(), path.end());
  return path;
}

bool Converter::IsFunctional(const Node& node) const {
  return (node.kind == NodeKind::kWord || node.kind == NodeKind::kSymbol) &&
         (PosFlags(node.lid) & kPosFunctional) != 0;
}

std::vector<std::vector<Node>> Converter::Segment(const std::vector<Node>& path) const {
  std::vector<std::vector<Node>> segments;
  for (size_t i = 0; i < path.size(); ++i) {
    bool attach = false;
    if (i > 0) {
      const bool functional = IsFunctional(path[i]);
      const bool after_prefix = path[i - 1].kind == NodeKind::kWord &&
                                (PosFlags(path[i - 1].rid) & kPosPrefix) != 0;
      // ローマ字にならなかった英字は前の文節にくっつける
      const bool stray_letter = path[i].kind == NodeKind::kUnknownLetter;
      attach = functional || after_prefix || stray_letter;
    }
    if (!attach || segments.empty()) segments.emplace_back();
    segments.back().push_back(path[i]);
  }
  return segments;
}

std::vector<Candidate> Converter::GetCandidates(const Input& input,
                                                const std::vector<Node>& segment) const {
  std::vector<Candidate> list;
  if (segment.empty()) return list;
  std::unordered_set<std::u16string> seen;
  auto add = [&](std::u16string surface, std::u16string learn_reading, std::u16string learn_surface,
                 std::u16string annotation = {}) {
    if (surface.empty() || !seen.insert(surface).second) return;
    list.push_back({std::move(surface), std::move(learn_reading), std::move(learn_surface),
                    std::move(annotation)});
  };

  const uint32_t begin = segment.front().begin;
  const uint32_t end = segment.back().end;
  const std::u16string reading = input.Reading(begin, end);

  // 自立語部分 (head) と付属語部分 (tail) に分ける
  size_t h = 1;
  while (h < segment.size() && !IsFunctional(segment[h])) ++h;
  const std::u16string head_reading = input.Reading(begin, segment[h - 1].end);
  std::u16string head_surface, tail_surface;
  for (size_t i = 0; i < segment.size(); ++i) (i < h ? head_surface : tail_surface) += segment[i].surface;
  const bool has_tail = h < segment.size();
  const uint16_t tail_lid = has_tail ? segment[h].lid : 0;

  // 1. 現在の表記
  add(head_surface + tail_surface, head_reading, head_surface);

  // 2. 学習済みの表記
  if (history_ != nullptr && config_->learning) {
    history_->ForEach(head_reading, [&](const std::u16string& s, int) {
      add(s + tail_surface, head_reading, s);
    });
  }
  // 3. ユーザー辞書
  if (user_ != nullptr) {
    if (const auto* entries = user_->Find(head_reading)) {
      for (const auto& e : *entries) add(e.surface + tail_surface, head_reading, e.surface);
    }
  }
  // 4. システム辞書 (自立語部分の読み)
  if (dict_ != nullptr) {
    struct Scored {
      std::u16string surface;
      int cost;
    };
    std::vector<Scored> scored;
    dict_->LookupExact(head_reading, [&](size_t, const DictEntry& e) {
      int cost = e.cost + (has_tail ? Connection(e.rid, tail_lid) : 0);
      scored.push_back({std::u16string(e.surface), cost});
    });
    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored& a, const Scored& b) { return a.cost < b.cost; });
    size_t n = 0;
    for (const auto& s : scored) {
      if (n++ >= kMaxHeadCandidates) break;
      add(s.surface + tail_surface, head_reading, s.surface);
    }
    // 文節全体の読みでも引く
    if (has_tail) {
      n = 0;
      dict_->LookupExact(reading, [&](size_t, const DictEntry& e) {
        if (n++ >= kMaxWholeCandidates) return;
        add(std::u16string(e.surface), reading, std::u16string(e.surface));
      });
    }
  }

  // 5. 文字種の変換
  const std::u16string hira = reading;
  add(hira, reading, hira, u"ひらがな");
  add(HiraganaToKatakana(hira), reading, HiraganaToKatakana(hira), u"カタカナ");
  if (config_->half_width_kana_enabled) {
    add(ToHalfWidthKatakana(hira), reading, ToHalfWidthKatakana(hira), u"半角カナ");
  }
  const std::u16string raw = input.Raw(begin, end);
  bool ascii = !raw.empty();
  for (char16_t c : raw) {
    if (c >= 0x80) ascii = false;
  }
  if (ascii) {
    std::string raw8 = Utf16ToUtf8(raw);
    std::string lower = AsciiLower(raw8);
    std::string upper = raw8;
    for (char& c : upper) {
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    std::string capital = lower;
    if (!capital.empty() && capital[0] >= 'a' && capital[0] <= 'z') {
      capital[0] = static_cast<char>(capital[0] - 'a' + 'A');
    }
    const std::u16string key = AsciiToU16(lower);
    for (const std::string& v : {raw8, lower, capital, upper}) {
      add(AsciiToU16(v), key, AsciiToU16(v), u"半角英数");
    }
    if (config_->full_width_allowed()) {
      for (const std::string& v : {raw8, lower, capital, upper}) {
        add(AsciiToFullWidth(AsciiToU16(v)), key, AsciiToFullWidth(AsciiToU16(v)), u"全角英数");
      }
    }
  }
  return list;
}

}  // namespace tora
