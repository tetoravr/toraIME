#include "engine.h"

namespace tora {

Engine::Engine() : converter_(&dict_, &user_, &history_, &english_, &config_) {}

bool Engine::LoadSystemDictionary(const std::filesystem::path& path) {
  return dict_.OpenFile(path);
}

bool Engine::LoadSystemDictionaryImage(std::vector<uint8_t> image) {
  return dict_.OpenImage(std::move(image));
}

bool Engine::LoadEnglishWords(const std::filesystem::path& path) { return english_.LoadFile(path); }

bool Engine::LoadUserDictionary(const std::filesystem::path& path) { return user_.LoadFile(path); }

bool Engine::LoadUserEnglishWords(const std::filesystem::path& path) {
  return english_.LoadFile(path);
}

void Engine::SetHistoryPath(const std::filesystem::path& path) {
  history_.SetPath(path);
  history_.Load();
}

void Engine::Learn(std::u16string_view reading, std::u16string_view surface) {
  if (!config_.learning) return;
  history_.Learn(reading, surface);
  history_.Save();
}

}  // namespace tora
