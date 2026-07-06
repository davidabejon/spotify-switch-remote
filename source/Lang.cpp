#include <Lang.hpp>
#include <cstdio>
#include <sys/stat.h>
#include <unordered_map>

namespace lang {

std::string currentLanguage = "gb";

static const char CONFIG_DIR[]  = "/config/spotify-switch";
static const char LANG_PATH[]   = "/config/spotify-switch/language.txt";

static std::unordered_map<std::string, std::string> strings;

static std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                default:   out += s[i + 1]; break;
            }
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

// Minimal flat {"key":"value", ...} JSON parser — no nesting, string values only.
static void parse(const std::string& json) {
    size_t i = 0;
    const size_t n = json.size();
    while (i < n) {
        while (i < n && json[i] != '"') ++i;
        if (i >= n) break;
        ++i;
        std::string key;
        while (i < n && json[i] != '"') {
            if (json[i] == '\\' && i + 1 < n) { key += json[i]; key += json[i + 1]; i += 2; continue; }
            key += json[i++];
        }
        if (i >= n) break;
        ++i; // closing quote of key

        while (i < n && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) ++i;
        if (i >= n || json[i] != ':') continue;
        ++i;
        while (i < n && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) ++i;
        if (i >= n || json[i] != '"') continue; // only string values are supported
        ++i;

        std::string val;
        while (i < n && json[i] != '"') {
            if (json[i] == '\\' && i + 1 < n) { val += json[i]; val += json[i + 1]; i += 2; continue; }
            val += json[i++];
        }
        if (i >= n) break;
        ++i; // closing quote of value

        strings[unescape(key)] = unescape(val);
    }
}

void load() {
    strings.clear();

    const std::string path = "romfs:/lang/" + currentLanguage + ".json";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > 65536) {
        fclose(f);
        return;
    }

    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = fread(content.data(), 1, static_cast<size_t>(size), f);
    fclose(f);
    content.resize(read);

    parse(content);
}

void loadPreference() {
    FILE* f = fopen(LANG_PATH, "r");
    if (!f) return;

    char buf[8] = {};
    const size_t read = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);

    if (read > 0) currentLanguage = buf;
}

void setLanguage(const std::string& code) {
    currentLanguage = code;

    mkdir(CONFIG_DIR, 0777);
    FILE* f = fopen(LANG_PATH, "w");
    if (f) {
        fputs(code.c_str(), f);
        fclose(f);
    }

    load();
}

const std::string& get(const std::string& key) {
    const auto it = strings.find(key);
    if (it != strings.end()) return it->second;
    return key;
}

} // namespace lang
