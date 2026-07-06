#pragma once
#include <string>

namespace lang {

// Active language code (e.g. "es"). Static for now — always "es".
extern std::string currentLanguage;

// Loads romfs:/lang/<currentLanguage>.json into memory. Call once after romfsInit().
void load();

// Returns the translated string for 'key', or 'key' itself if missing.
const std::string& get(const std::string& key);

} // namespace lang
