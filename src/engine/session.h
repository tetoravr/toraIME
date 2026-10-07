// 1 つの入力コンテキストの状態機械 (入力中 / 変換中)。OS に依存しない。
//
//   空       --文字-->        入力中 (打つたびに自動で変換して表示する)
//   入力中   --Enter-->       確定
//   入力中   --Space/←/→-->  変換中 (文節ごとに候補を選べる)
//   変換中   --Enter-->       確定 (選んだ候補を学習する)
//   変換中   --Esc/BS-->      入力中に戻る
//   変換中   --文字-->        確定して、その文字から新しく入力を始める
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "composer.h"
#include "converter.h"
#include "engine.h"
#include "input.h"

namespace tora {

enum class KeyCode {
  kNone,
  kChar,  // ch に文字
  kSpace,
  kEnter,
  kBackspace,
  kDelete,
  kEscape,
  kTab,
  kLeft,
  kRight,
  kUp,
  kDown,
  kHome,
  kEnd,
  kPageUp,
  kPageDown,
  kF6,   // ひらがな
  kF7,   // 全角カタカナ
  kF8,   // 半角カタカナ
  kF9,   // 全角英数
  kF10,  // 半角英数
};

struct KeyEvent {
  KeyCode code = KeyCode::kNone;
  char16_t ch = 0;    // kChar のとき: ローマ字入力なら ASCII、かな入力ならかな
  bool kana = false;  // ch がかな入力によるかな
  bool shift = false;
  bool ctrl = false;
  bool alt = false;

  static KeyEvent Char(char16_t c) {
    KeyEvent k;
    k.code = KeyCode::kChar;
    k.ch = c;
    return k;
  }
  static KeyEvent Of(KeyCode code, bool shift = false) {
    KeyEvent k;
    k.code = code;
    k.shift = shift;
    return k;
  }
};

enum class InputMode {
  kHiragana,        // 日本語入力 (自動変換)
  kFullWidthAlnum,  // 全角英数 (全角が有効なときだけ)
  kHalfWidthAlnum,  // 半角英数 (直接入力)
};

enum class AttrType { kInput, kConverted, kFocused };

struct AttrSpan {
  size_t begin;
  size_t length;
  AttrType type;
};

struct Output {
  bool consumed = false;
  std::u16string commit;  // 確定する文字列 (preedit より先に確定する)
  std::u16string preedit;
  std::vector<AttrSpan> attrs;
  size_t caret = 0;

  bool candidates_visible = false;
  std::vector<std::u16string> candidates;  // 現在のページ
  std::vector<std::u16string> annotations;
  int selected = -1;  // ページ内のインデックス
  size_t page = 0;
  size_t page_count = 0;
  size_t focus_begin = 0;  // 注目文節の preedit 内の位置 (候補ウィンドウの位置合わせ用)
  size_t focus_length = 0;

  // 入力中に表示する読み (表示が読みと同じときや設定で無効なときは空)
  std::u16string reading;
};

class Session {
 public:
  static constexpr size_t kPageSize = 9;

  explicit Session(Engine* engine);

  bool IsComposing() const { return state_ != State::kEmpty; }
  InputMode mode() const { return mode_; }
  void set_mode(InputMode mode);

  // このキーを IME が処理するか (状態は変えない)
  bool WouldConsume(const KeyEvent& key) const;
  Output Process(const KeyEvent& key);
  // 未確定の文字列をすべて確定する (フォーカスが移るときなど)
  Output Flush();
  void Reset();
  // 現在の表示
  Output Render() const;

  // 候補ウィンドウのクリックなどで候補を直接選ぶ (ページ内インデックス)
  Output SelectCandidateOnPage(size_t index);

 private:
  enum class State { kEmpty, kInput, kConvert };

  struct Seg {
    uint32_t begin = 0;
    uint32_t end = 0;
    std::vector<Node> nodes;
    std::vector<Candidate> cands;
    size_t selected = 0;
    bool changed = false;  // ユーザーが候補を選び直した
  };

  bool ConsumesInEmpty(const KeyEvent& key) const;

  Output ProcessEmpty(const KeyEvent& key);
  Output ProcessInput(const KeyEvent& key);
  Output ProcessConvert(const KeyEvent& key);

  void InsertChar(const KeyEvent& key);
  void EnterConvert(bool focus_last);
  void EnterConvertWhole();
  void BuildSegmentsFrom(size_t index, uint32_t begin, uint16_t left_rid);
  void EnsureCandidates(Seg& seg) const;
  void MoveCandidate(int delta);
  void ResizeFocus(int delta);
  void Transliterate(KeyCode key);
  std::u16string CommitConvert();
  std::u16string SegSurface(const Seg& seg) const;
  std::u16string InputPreedit() const;
  std::u16string UnitsAsReading(const Units& units) const;
  bool LiveActive() const;
  void Clear();

  Engine* engine_;
  InputMode mode_ = InputMode::kHiragana;
  State state_ = State::kEmpty;
  Composer composer_;
  bool show_hiragana_ = false;  // Esc で変換前の表示に戻している

  Input input_;  // 変換中の入力
  mutable std::vector<Seg> segs_;  // 候補は表示するときに遅延して作る
  size_t focus_ = 0;
};

}  // namespace tora
