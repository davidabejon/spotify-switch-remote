#include <TokenStorage.hpp>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace TokenStorage {

static const char CONFIG_DIR[]   = "/config/spotify-switch";
static const char TOKENS_PATH[]  = "/config/spotify-switch/tokens.json";

static std::string jsonGetString(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    const auto start = pos + needle.size();
    const auto end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

static long jsonGetLong(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    return strtol(json.c_str() + pos + needle.size(), nullptr, 10);
}

bool saveTokens(const spotify::Tokens& tokens) {
    mkdir(CONFIG_DIR, 0777);

    FILE* f = fopen(TOKENS_PATH, "w");
    if (!f) return false;

    fprintf(f,
        "{\"access_token\":\"%s\","
        "\"refresh_token\":\"%s\","
        "\"expires_at\":%ld}",
        tokens.accessToken.c_str(),
        tokens.refreshToken.c_str(),
        tokens.expiresAt);

    fclose(f);
    return true;
}

spotify::Tokens loadTokens() {
    FILE* f = fopen(TOKENS_PATH, "r");
    if (!f) return spotify::Tokens();

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);

    if (size <= 0 || size > 4096) {
        fclose(f);
        return spotify::Tokens();
    }

    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = fread(content.data(), 1, static_cast<size_t>(size), f);
    fclose(f);

    content.resize(read);

    const auto at = jsonGetString(content, "access_token");
    const auto rt = jsonGetString(content, "refresh_token");
    const auto ea = jsonGetLong(content, "expires_at");

    if (at.empty() || rt.empty()) return spotify::Tokens();
    return spotify::Tokens(at, rt, ea);
}

void clearTokens() {
    remove(TOKENS_PATH);
}

} // namespace TokenStorage
