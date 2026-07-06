#pragma once
#include <string>

namespace lang {

// Active language code (e.g. "es"). Static for now — always "es".
extern std::string currentLanguage;

// Loads romfs:/lang/<currentLanguage>.json into memory. Call once after romfsInit().
void load();

// Sets currentLanguage from the saved preference file, if one exists.
void loadPreference();

// Sets currentLanguage, persists it as the new preference, and reloads strings.
void setLanguage(const std::string& code);

// Returns the translated string for 'key', or 'key' itself if missing.
const std::string& get(const std::string& key);

} // namespace lang
