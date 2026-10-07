#include "session.h"

#include <algorithm>

#include "text_util.h"

namespace tora {
namespace {

bool IsPrintableAscii(char16_t c) { return c > 0x20 && c < 0x7F; }

}  // namespace

Session::Session(Engine* engine) : engine_(engine), composer_(&engine->config()) {}

void Session::set_mode(InputMode mode) {
  if (mode == InputMode::kFullWidthAlnum && !engine_->config().full_width_allowed()) {
    mode = InputMode::kHalfWidthAlnum;
  }
  mode_ = mode;
}

void Session::Clear() {
  composer_.Clear();
  segs_.clear();
  input_ = Input();
  focus_ = 0;
  show_hiragana_ = false;
  state_ = State::kEmpty;
}

void Session::Reset() { Clear(); }

bool Session::ConsumesInEmpty(const KeyEvent& key) const {
  if (key.ctrl || key.alt) return false;
  switch (mode_) {
    case InputMode::kHalfWidthAlnum:
      return false;
    case InputMode::kFullWidthAlnum:
      if (!engine_->config().full_width_allowed()) return false;
      return (key.code == KeyCode::kChar && IsPrintableAscii(key.ch)) || key.code == KeyCode::kSpace;
    case InputMode::kHiragana:
      if (key.code == KeyCode::kChar) return key.kana || IsPrintableAscii(key.ch);
      if (key.code == KeyCode::kSpace) {
        // 全角スペースを出すときだけ IME が処理する。それ以外は普通の半角スペース
        return !key.shift && engine_->config().full_width == FullWidthMode::kDefault;
      }
      return false;
  }
  return false;
}

bool Session::WouldConsume(const KeyEvent& key) const {
  if (key.code == KeyCode::kNone) return false;
  if (state_ == State::kEmpty) return ConsumesInEmpty(key);
  return true;  // 入力中・変換中はすべてのキーを IME が処理する
}

Output Session::Process(const KeyEvent& key) {
  if (!WouldConsume(key)) return Output();
  Output out;
  switch (state_) {
    case State::kEmpty:
      out = ProcessEmpty(key);
      break;
    case State::kInput:
      out = ProcessInput(key);
      break;
    case State::kConvert:
      out = ProcessConvert(key);
      break;
  }
  out.consumed = true;
  return out;
}

Output Session::ProcessEmpty(const KeyEvent& key) {
  if (mode_ == InputMode::kFullWidthAlnum) {
    Output out;
    out.commit = key.code == KeyCode::kSpace ? std::u16string(u"　")
                                             : AsciiToFullWidth(std::u16string(1, key.ch));
    return out;
  }
  if (key.code == KeyCode::kSpace) {
    Output out;
    out.commit = u"　";
    return out;
  }
  state_ = State::kInput;
  return ProcessInput(key);
}

void Session::InsertChar(const KeyEvent& key) {
  if (key.kana) {
    composer_.InsertKana(key.ch);
  } else if (key.ch < 0x80) {
    composer_.InsertChar(static_cast<char>(key.ch));
  }
  show_hiragana_ = false;
}

Output Session::ProcessInput(const KeyEvent& key) {
  if (key.ctrl || key.alt) return Render();  // 入力中の Ctrl/Alt の組み合わせは無視する

  switch (key.code) {
    case KeyCode::kChar:
      InsertChar(key);
      break;
    case KeyCode::kSpace:
      if (key.shift) {
        composer_.InsertSpace();
        show_hiragana_ = false;
      } else {
        EnterConvert(false);
      }
      break;
    case KeyCode::kEnter: {
      Output out;
      out.commit = InputPreedit();
      Clear();
      return out;
    }
    case KeyCode::kBackspace:
      composer_.Backspace();
      show_hiragana_ = false;
      if (composer_.empty()) Clear();
      break;
    case KeyCode::kEscape:
      if (LiveActive()) {
        show_hiragana_ = true;  // 1 回目: 変換前のひらがなに戻す
      } else {
        Clear();  // 2 回目: 取り消す
      }
      break;
    case KeyCode::kLeft:
      EnterConvert(true);
      break;
    case KeyCode::kRight:
    case KeyCode::kDown:
      EnterConvert(false);
      break;
    case KeyCode::kF6:
    case KeyCode::kF7:
    case KeyCode::kF8:
    case KeyCode::kF9:
    case KeyCode::kF10:
      EnterConvertWhole();
      Transliterate(key.code);
      break;
    default:
      break;
  }
  return Render();
}

Output Session::ProcessConvert(const KeyEvent& key) {
  if (key.ctrl || key.alt) return Render();

  switch (key.code) {
    case KeyCode::kChar: {
      // 候補ウィンドウの番号で選ぶ
      if (!key.kana && key.ch >= u'1' && key.ch <= u'9') return SelectCandidateOnPage(key.ch - u'1');
      // 確定してから新しく入力を始める
      Output out;
      std::u16string committed = CommitConvert();
      state_ = State::kInput;
      InsertChar(key);
      out = Render();
      out.commit = committed;
      return out;
    }
    case KeyCode::kEnter: {
      Output out;
      out.commit = CommitConvert();
      return out;
    }
    case KeyCode::kSpace:
      if (key.shift) {
        MoveCandidate(-1);
      } else {
        MoveCandidate(1);
      }
      break;
    case KeyCode::kDown:
    case KeyCode::kTab:
      MoveCandidate(key.shift ? -1 : 1);
      break;
    case KeyCode::kUp:
      MoveCandidate(-1);
      break;
    case KeyCode::kPageDown:
      MoveCandidate(static_cast<int>(kPageSize));
      break;
    case KeyCode::kPageUp:
      MoveCandidate(-static_cast<int>(kPageSize));
      break;
    case KeyCode::kLeft:
      if (key.shift) {
        ResizeFocus(-1);
      } else if (focus_ > 0) {
        --focus_;
      }
      break;
    case KeyCode::kRight:
      if (key.shift) {
        ResizeFocus(1);
      } else if (focus_ + 1 < segs_.size()) {
        ++focus_;
      }
      break;
    case KeyCode::kHome:
      focus_ = 0;
      break;
    case KeyCode::kEnd:
      focus_ = segs_.empty() ? 0 : segs_.size() - 1;
      break;
    case KeyCode::kEscape:
    case KeyCode::kBackspace:
      // 入力中に戻る
      segs_.clear();
      state_ = State::kInput;
      show_hiragana_ = false;
      break;
    case KeyCode::kF6:
    case KeyCode::kF7:
    case KeyCode::kF8:
    case KeyCode::kF9:
    case KeyCode::kF10:
      Transliterate(key.code);
      break;
    default:
      break;
  }
  return Render();
}

Output Session::SelectCandidateOnPage(size_t index) {
  if (state_ != State::kConvert || segs_.empty()) return Render();
  Seg& seg = segs_[focus_];
  EnsureCandidates(seg);
  size_t page = seg.selected / kPageSize;
  size_t target = page * kPageSize + index;
  if (target < seg.cands.size()) {
    seg.selected = target;
    seg.changed = true;
    if (focus_ + 1 < segs_.size()) ++focus_;
  }
  return Render();
}

Output Session::Flush() {
  Output out;
  if (state_ == State::kInput) {
    out.commit = InputPreedit();
    Clear();
  } else if (state_ == State::kConvert) {
    out.commit = CommitConvert();
  }
  return out;
}

void Session::EnterConvert(bool focus_last) {
  input_ = Input(composer_.GetUnits());
  segs_.clear();
  if (input_.empty()) return;
  BuildSegmentsFrom(0, 0, 0);
  if (segs_.empty()) return;
  state_ = State::kConvert;
  focus_ = focus_last ? segs_.size() - 1 : 0;
}

void Session::EnterConvertWhole() {
  input_ = Input(composer_.GetUnits());
  segs_.clear();
  if (input_.empty()) return;
  Seg seg;
  seg.begin = 0;
  seg.end = input_.size();
  seg.nodes = engine_->converter().Convert(input_);
  segs_.push_back(std::move(seg));
  state_ = State::kConvert;
  focus_ = 0;
}

void Session::BuildSegmentsFrom(size_t index, uint32_t begin, uint16_t left_rid) {
  segs_.resize(index);
  const Converter& conv = engine_->converter();
  std::vector<Node> path = conv.Convert(input_, begin, input_.size(), left_rid, true);
  for (auto& nodes : conv.Segment(path)) {
    Seg seg;
    seg.begin = nodes.front().begin;
    seg.end = nodes.back().end;
    seg.nodes = std::move(nodes);
    segs_.push_back(std::move(seg));
  }
}

void Session::EnsureCandidates(Seg& seg) const {
  if (!seg.cands.empty()) return;
  seg.cands = engine_->converter().GetCandidates(input_, seg.nodes);
  if (seg.cands.empty()) {
    Candidate c;
    for (const Node& n : seg.nodes) c.surface += n.surface;
    seg.cands.push_back(std::move(c));
  }
}

void Session::MoveCandidate(int delta) {
  if (segs_.empty()) return;
  Seg& seg = segs_[focus_];
  EnsureCandidates(seg);
  const int n = static_cast<int>(seg.cands.size());
  int next = static_cast<int>(seg.selected) + delta;
  if (delta == 1 || delta == -1) {
    next = (next % n + n) % n;  // 1 つずつなら端で一周する
  } else {
    next = std::clamp(next, 0, n - 1);
  }
  seg.selected = static_cast<size_t>(next);
  seg.changed = true;
}

void Session::ResizeFocus(int delta) {
  if (segs_.empty()) return;
  Seg& seg = segs_[focus_];
  uint32_t new_end;
  if (delta > 0) {
    if (seg.end >= input_.size()) return;
    new_end = input_.NextBoundary(seg.end);
  } else {
    new_end = input_.PrevBoundary(seg.end);
    if (new_end <= seg.begin) return;
  }
  const Converter& conv = engine_->converter();
  const uint16_t left_rid =
      focus_ > 0 && !segs_[focus_ - 1].nodes.empty() ? segs_[focus_ - 1].nodes.back().rid : 0;
  Seg resized;
  resized.begin = seg.begin;
  resized.end = new_end;
  resized.nodes = conv.Convert(input_, seg.begin, new_end, left_rid, new_end >= input_.size());
  if (resized.nodes.empty()) return;
  const uint16_t rid = resized.nodes.back().rid;
  segs_[focus_] = std::move(resized);
  if (new_end < input_.size()) {
    BuildSegmentsFrom(focus_ + 1, new_end, rid);
  } else {
    segs_.resize(focus_ + 1);
  }
}

void Session::Transliterate(KeyCode key) {
  if (segs_.empty()) return;
  Seg& seg = segs_[focus_];
  EnsureCandidates(seg);
  const Config& cfg = engine_->config();
  const std::u16string reading = input_.Reading(seg.begin, seg.end);
  const std::u16string raw = input_.Raw(seg.begin, seg.end);

  std::vector<std::u16string> variants;
  switch (key) {
    case KeyCode::kF6:
      variants.push_back(reading);
      break;
    case KeyCode::kF7:
      variants.push_back(HiraganaToKatakana(reading));
      break;
    case KeyCode::kF8:
      if (!cfg.half_width_kana_enabled) return;
      variants.push_back(ToHalfWidthKatakana(reading));
      break;
    case KeyCode::kF9:
    case KeyCode::kF10: {
      std::string r8 = Utf16ToUtf8(raw);
      std::string lower = AsciiLower(r8);
      std::string upper = r8;
      for (char& c : upper) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      }
      std::string capital = lower;
      if (!capital.empty() && capital[0] >= 'a' && capital[0] <= 'z') {
        capital[0] = static_cast<char>(capital[0] - 'a' + 'A');
      }
      const bool full = key == KeyCode::kF9 && cfg.full_width_allowed();
      for (const std::string& v : {lower, upper, capital}) {
        std::u16string s = AsciiToU16(v);
        if (std::find(variants.begin(), variants.end(), full ? AsciiToFullWidth(s) : s) == variants.end()) {
          variants.push_back(full ? AsciiToFullWidth(s) : s);
        }
      }
      break;
    }
    default:
      return;
  }
  if (variants.empty()) return;

  // 今の表記が variants にあれば次のものへ (F10 を続けて押すと 小文字 -> 大文字 -> 先頭だけ大文字)
  const std::u16string current = SegSurface(seg);
  size_t next = 0;
  for (size_t i = 0; i < variants.size(); ++i) {
    if (variants[i] == current) {
      next = (i + 1) % variants.size();
      break;
    }
  }
  const std::u16string& target = variants[next];
  for (size_t i = 0; i < seg.cands.size(); ++i) {
    if (seg.cands[i].surface == target) {
      seg.selected = i;
      seg.changed = true;
      return;
    }
  }
  Candidate c;
  c.surface = target;
  c.learn_reading = reading;
  c.learn_surface = target;
  seg.cands.push_back(std::move(c));
  seg.selected = seg.cands.size() - 1;
  seg.changed = true;
}

std::u16string Session::SegSurface(const Seg& seg) const {
  if (!seg.cands.empty() && seg.selected < seg.cands.size()) return seg.cands[seg.selected].surface;
  std::u16string s;
  for (const Node& n : seg.nodes) s += n.surface;
  return s;
}

std::u16string Session::CommitConvert() {
  std::u16string text;
  for (const Seg& seg : segs_) {
    text += SegSurface(seg);
    if (seg.changed && seg.selected < seg.cands.size()) {
      const Candidate& c = seg.cands[seg.selected];
      if (!c.learn_reading.empty() && !c.learn_surface.empty()) {
        engine_->Learn(c.learn_reading, c.learn_surface);
      }
    }
  }
  Clear();
  return text;
}

bool Session::LiveActive() const {
  return engine_->config().live_conversion != LiveConversion::kOff && !show_hiragana_;
}

std::u16string Session::InputPreedit() const {
  const Config& cfg = engine_->config();
  Input input(composer_.GetUnits());
  if (!LiveActive()) return UnitsAsReading(input.units());

  const Converter& conv = engine_->converter();
  std::vector<Node> path = conv.Convert(input);
  std::u16string s;
  if (cfg.live_conversion == LiveConversion::kKeepLastSegment) {
    // 最後の文節 (いま打っている文節) は変換せずに読みのまま見せる
    auto segments = conv.Segment(path);
    for (size_t i = 0; i < segments.size(); ++i) {
      const bool last = i + 1 == segments.size();
      for (const Node& n : segments[i]) {
        const bool kana_word = n.kind == NodeKind::kWord || n.kind == NodeKind::kUser ||
                               n.kind == NodeKind::kHistory || n.kind == NodeKind::kUnknownKana;
        s += last && kana_word ? n.reading : n.surface;
      }
    }
    return s;
  }
  for (const Node& n : path) s += n.surface;
  return s;
}

std::u16string Session::UnitsAsReading(const Units& units) const {
  // 変換しない表示: かなはそのまま、英数字は設定に従った幅
  const Config& cfg = engine_->config();
  std::u16string s;
  for (const Unit& u : units) {
    switch (u.kind) {
      case UnitKind::kKana:
      case UnitKind::kSymbol:
        s += u.text;
        break;
      case UnitKind::kLetter:
      case UnitKind::kDigit:
        s += engine_->converter().ApplyWidth(AsciiToU16(u.raw));
        break;
      case UnitKind::kSpace:
        s += cfg.full_width == FullWidthMode::kDefault ? u"　" : u" ";
        break;
    }
  }
  return s;
}

Output Session::Render() const {
  Output out;
  out.consumed = true;
  if (state_ == State::kInput) {
    out.preedit = InputPreedit();
    if (!out.preedit.empty()) out.attrs.push_back({0, out.preedit.size(), AttrType::kInput});
    out.caret = out.preedit.size();
    out.focus_begin = 0;
    out.focus_length = out.preedit.size();
    if (engine_->config().reading_hint && LiveActive()) {
      // 打ったとおりの読み (未確定のローマ字もそのまま)。表示と同じなら出さない
      std::u16string reading = UnitsAsReading(composer_.GetUnitsAsTyped());
      if (reading != out.preedit) out.reading = std::move(reading);
    }
    return out;
  }
  if (state_ != State::kConvert) return out;

  for (size_t i = 0; i < segs_.size(); ++i) {
    const std::u16string s = SegSurface(segs_[i]);
    const size_t begin = out.preedit.size();
    out.preedit += s;
    out.attrs.push_back({begin, s.size(), i == focus_ ? AttrType::kFocused : AttrType::kConverted});
    if (i == focus_) {
      out.focus_begin = begin;
      out.focus_length = s.size();
      out.caret = begin + s.size();
    }
  }

  // 候補一覧 (注目文節)
  Seg& seg = segs_[focus_];
  EnsureCandidates(seg);
  const size_t page = seg.selected / kPageSize;
  out.page = page;
  out.page_count = (seg.cands.size() + kPageSize - 1) / kPageSize;
  for (size_t i = page * kPageSize; i < seg.cands.size() && i < (page + 1) * kPageSize; ++i) {
    out.candidates.push_back(seg.cands[i].surface);
    out.annotations.push_back(seg.cands[i].annotation);
  }
  out.selected = static_cast<int>(seg.selected - page * kPageSize);
  out.candidates_visible = !out.candidates.empty();
  return out;
}

}  // namespace tora
