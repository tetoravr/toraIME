#include "composer.h"

#include "romaji.h"
#include "text_util.h"

namespace tora {
namespace {

bool IsDigitUnit(const Units& units) {
  return !units.empty() && units.back().kind == UnitKind::kDigit;
}

// かな + 濁点/半濁点 の合成
char16_t Compose(char16_t base, char16_t mark) {
  if (mark == u'゛') {
    if (base == u'う') return u'ゔ';
    if ((base >= u'か' && base <= u'ち' && (base - u'か') % 2 == 0) ||
        base == u'つ' || base == u'て' || base == u'と') {
      return static_cast<char16_t>(base + 1);
    }
    if (base >= u'は' && base <= u'ほ' && (base - u'は') % 3 == 0) return static_cast<char16_t>(base + 1);
  } else if (mark == u'゜') {
    if (base >= u'は' && base <= u'ほ' && (base - u'は') % 3 == 0) return static_cast<char16_t>(base + 2);
  }
  return 0;
}

}  // namespace

void Composer::AddUnit(UnitKind kind, std::string raw, std::u16string text) {
  Unit u;
  u.kind = kind;
  u.raw = std::move(raw);
  u.text = std::move(text);
  units_.push_back(std::move(u));
}

void Composer::FlushPending(bool final) {
  std::vector<RomajiChunk> chunks;
  Romaji::Resolve(&pending_, final, &chunks);
  for (auto& c : chunks) {
    if (c.kana.empty()) {
      AddUnit(UnitKind::kLetter, c.raw, AsciiToU16(c.raw));
    } else {
      AddUnit(UnitKind::kKana, std::move(c.raw), std::move(c.kana));
    }
  }
}

std::u16string Composer::MapSymbol(char c) const {
  const bool after_digit = IsDigitUnit(units_);
  const PunctuationStyle p = config_->punctuation;
  switch (c) {
    case ',':
      if (after_digit) return u",";
      return (p == PunctuationStyle::kComma || p == PunctuationStyle::kCommaKuten) ? u"，" : u"、";
    case '.':
      if (after_digit) return u".";
      return (p == PunctuationStyle::kComma || p == PunctuationStyle::kToutenPeriod) ? u"．" : u"。";
    case '[':
      return u"「";
    case ']':
      return u"」";
    case '/':
      return u"・";
    default:
      break;
  }
  std::u16string s(1, static_cast<char16_t>(c));
  if (config_->full_width == FullWidthMode::kDefault) return AsciiToFullWidth(s);
  return s;
}

void Composer::InsertChar(char c) {
  if (c >= 'a' && c <= 'z') {
    pending_.push_back(c);
    FlushPending(false);
    return;
  }
  if (c == '\'' && pending_ == "n") {
    pending_.push_back(c);
    FlushPending(false);
    return;
  }
  FlushPending(true);
  if (c >= 'A' && c <= 'Z') {
    AddUnit(UnitKind::kLetter, std::string(1, c), std::u16string(1, static_cast<char16_t>(c)));
  } else if (c >= '0' && c <= '9') {
    AddUnit(UnitKind::kDigit, std::string(1, c), std::u16string(1, static_cast<char16_t>(c)));
  } else if (c == '-' && !IsDigitUnit(units_)) {
    AddUnit(UnitKind::kKana, "-", u"ー");
  } else if (c > 0x20 && c < 0x7F) {
    AddUnit(UnitKind::kSymbol, std::string(1, c), MapSymbol(c));
  }
}

void Composer::InsertKana(char16_t c) {
  FlushPending(true);
  if ((c == u'゛' || c == u'゜') && !units_.empty() && units_.back().kind == UnitKind::kKana &&
      !units_.back().text.empty()) {
    char16_t composed = Compose(units_.back().text.back(), c);
    if (composed != 0) {
      units_.back().text.back() = composed;
      return;
    }
  }
  switch (c) {
    case u'、':
    case u'。':
    case u'「':
    case u'」':
    case u'・':
    case u'゛':
    case u'゜':
      AddUnit(UnitKind::kSymbol, "", std::u16string(1, c));
      break;
    default:
      AddUnit(UnitKind::kKana, "", std::u16string(1, c));
      break;
  }
}

void Composer::InsertSpace() {
  FlushPending(true);
  AddUnit(UnitKind::kSpace, " ", u" ");
}

bool Composer::Backspace() {
  if (!pending_.empty()) {
    pending_.pop_back();
    return true;
  }
  if (units_.empty()) return false;
  Unit& u = units_.back();
  if (u.kind == UnitKind::kKana && u.text.size() > 1) {
    // 「きゃ」->「き」のように最後の 1 文字だけ消す
    u.text.pop_back();
    if (!u.raw.empty()) u.raw = Romaji::KanaToRomaji(u.text);
    return true;
  }
  units_.pop_back();
  return true;
}

void Composer::Clear() {
  units_.clear();
  pending_.clear();
}

Units Composer::GetUnits() const {
  Composer copy = *this;
  copy.FlushPending(true);
  return copy.units_;
}

}  // namespace tora
