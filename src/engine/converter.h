// かな漢字変換 (ラティス + Viterbi)
//
// 入力単位 (Unit) の境界をノードの境界としてラティスを作る。ノードは
//   - システム辞書・ユーザー辞書・学習履歴の単語 (読みがかなの並びと一致するもの)
//   - 英単語リストにある英単語 (打鍵したローマ字の綴りと一致するもの)
//   - ローマ字にならなかった英字を含む英字列 (英字のまま)
//   - 大文字で始まる英字列 (英字のまま)
//   - 数字列・記号・未知語
// で、単語コスト + 連接コストが最小になる経路を選ぶ。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "config.h"
#include "dictionary.h"
#include "input.h"
#include "user_data.h"

namespace tora {

enum class NodeKind : unsigned char {
  kWord,
  kUser,
  kHistory,
  kEnglish,
  kAsciiSpan,
  kUnknownKana,
  kUnknownLetter,
  kNumber,
  kSymbol,
  kSpace,
};

struct Node {
  uint32_t begin = 0;  // Input のオフセット
  uint32_t end = 0;
  std::u16string surface;
  std::u16string reading;  // 学習のキー (かなの読み、英字の場合は小文字の綴り)
  uint16_t lid = 0;
  uint16_t rid = 0;
  int32_t cost = 0;
  NodeKind kind = NodeKind::kWord;
};

struct Candidate {
  std::u16string surface;
  std::u16string learn_reading;  // 選ばれたときに学習する読み (空なら学習しない)
  std::u16string learn_surface;
  std::u16string annotation;     // 候補ウィンドウに添える説明 ([カタカナ] など)
};

class Converter {
 public:
  Converter(const SystemDictionary* dict, const UserDictionary* user, const History* history,
            const EnglishWords* english, const Config* config)
      : dict_(dict), user_(user), history_(history), english_(english), config_(config) {}

  // 大文字で始めた語の区切りに使う大きな英単語リスト (固有名詞を含む)
  void set_english_large(const EnglishWords* words) { english_large_ = words; }

  // input の [begin, end) を変換して最小コストの経路を返す。
  // left_rid は直前の文節の右文脈 ID (先頭なら 0 = BOS)。
  std::vector<Node> Convert(const Input& input, uint32_t begin, uint32_t end,
                            uint16_t left_rid = 0, bool eos = true) const;
  std::vector<Node> Convert(const Input& input) const { return Convert(input, 0, input.size()); }

  // 経路を文節 (自立語 + 付属語) に区切る
  std::vector<std::vector<Node>> Segment(const std::vector<Node>& path) const;

  // 文節の候補一覧。先頭は現在の表記。
  std::vector<Candidate> GetCandidates(const Input& input, const std::vector<Node>& segment) const;

  // 付属語 (直前の文節にくっつく語) か
  bool IsFunctional(const Node& node) const;

  // 英数字を設定に従った幅にする
  std::u16string ApplyWidth(std::u16string_view ascii) const;

  // コスト (Mozc の辞書と同じスケール)
  static constexpr int kUnknownKanaCost = 9000;
  static constexpr int kUnknownLetterCost = 9000;
  static constexpr int kUnknownUpperCost = 12000;
  static constexpr int kNumberCost = 1500;
  static constexpr int kSymbolCost = 1000;
  static constexpr int kSpaceCost = 500;
  static constexpr int kEnglishBase = 5200;
  static constexpr int kEnglishPerChar = 300;
  static constexpr int kEnglishMidRunPenalty3 = 1500;  // 文中の 3 文字の英単語
  static constexpr int kEnglishMidRunPenalty = 0;      // 文中の 4 文字以上の英単語
  static constexpr int kAsciiSpanBase = 3000;
  static constexpr int kAsciiSpanPerChar = 1500;
  static constexpr int kUpperSpanBase = 2000;
  static constexpr int kHistoryOnlyCost = 5000;
  static constexpr int kMaxAsciiSpan = 32;

 private:
  void AddNodesAt(const Input& input, uint32_t p, uint32_t end, std::vector<Node>* out) const;
  void AddWordNodes(const std::vector<Piece>& chain, std::vector<Node>* out) const;
  void AddAsciiNodes(const Input& input, const std::vector<Piece>& chain, std::vector<Node>* out) const;
  void AddFallbackNode(const std::vector<Piece>& chain, std::vector<Node>* out) const;
  int HistoryBonus(std::u16string_view reading, std::u16string_view surface) const;
  bool IsKnownEnglish(const std::string& lower, const std::string& raw) const;
  int Connection(uint16_t rid, uint16_t lid) const;
  uint8_t PosFlags(uint16_t id) const;

  const SystemDictionary* dict_;
  const UserDictionary* user_;
  const History* history_;
  const EnglishWords* english_;
  const EnglishWords* english_large_ = nullptr;
  const Config* config_;
};

}  // namespace tora
