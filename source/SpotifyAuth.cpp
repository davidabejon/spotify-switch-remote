#include <SpotifyAuth.hpp>
#include <DebugLog.hpp>
#include <curl/curl.h>
#include <mbedtls/sha256.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>

namespace spotify {

static const char CLIENT_ID[]    = "2979d578384e470a9a8440f7a495f611";
static const char REDIRECT_URI[] = "https://davidabejon.github.io/spotify-switch-remote";
static const char SCOPES[]       =
    "user-read-playback-state "
    "user-modify-playback-state "
    "user-read-currently-playing "
    "user-read-private "
    "user-read-email";

// --- Base64url (no padding, URL-safe alphabet) ---

static const char B64URL_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static std::string base64url(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16);
        if (i + 1 < len) triple |= (static_cast<uint32_t>(data[i + 1]) << 8);
        if (i + 2 < len) triple |=  static_cast<uint32_t>(data[i + 2]);

        out += B64URL_TABLE[(triple >> 18) & 0x3F];
        out += B64URL_TABLE[(triple >> 12) & 0x3F];
        if (i + 1 < len) out += B64URL_TABLE[(triple >> 6) & 0x3F];
        if (i + 2 < len) out += B64URL_TABLE[ triple       & 0x3F];
    }
    return out;
}

// --- Percent-encoding for URL query parameters ---

static std::string urlEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

// --- Minimal JSON field extraction ---

// Returns index of the start of the value for 'key', or npos.
// Handles optional whitespace between ':' and value ("key": "v" and "key":"v" both work).
// Search starts at 'offset'.
static size_t jsonFindValue(const std::string& json, const std::string& key, size_t offset = 0) {
    const std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle, offset);
    if (pos == std::string::npos) return std::string::npos;
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) ++pos;
    if (pos >= json.size() || json[pos] != ':') return std::string::npos;
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) ++pos;
    return pos;
}

static std::string jsonGetString(const std::string& json, const std::string& key) {
    const auto pos = jsonFindValue(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') return "";
    const auto start = pos + 1;
    const auto end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

// Like jsonGetString but searches starting from 'offset' — used to skip nested objects.
static std::string jsonGetStringFrom(const std::string& json, const std::string& key, size_t offset) {
    const auto pos = jsonFindValue(json, key, offset);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') return "";
    const auto start = pos + 1;
    const auto end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

static long jsonGetLong(const std::string& json, const std::string& key) {
    const auto pos = jsonFindValue(json, key);
    if (pos == std::string::npos) return 0;
    return strtol(json.c_str() + pos, nullptr, 10);
}

// --- curl write callback ---

static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// --- JSON object/array helpers ---

// Returns index one past the closing bracket of the object/array starting at 'start'
static size_t jsonSkipValue(const std::string& json, size_t start) {
    if (start >= json.size()) return start;
    const char open  = json[start];
    const char close = (open == '{') ? '}' : (open == '[') ? ']' : '\0';
    if (!close) return start + 1;
    int depth = 0;
    for (size_t i = start; i < json.size(); ++i) {
        if (json[i] == open)  ++depth;
        else if (json[i] == close) {
            if (--depth == 0) return i + 1;
        }
    }
    return json.size();
}

// Returns inner content of the first matching JSON object value for 'key'.
// Tolerates whitespace around ':' ("key": { and "key":{ both work).
static std::string jsonGetObject(const std::string& json, const std::string& key) {
    const auto pos = jsonFindValue(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '{') return "";
    const auto end = jsonSkipValue(json, pos);
    return json.substr(pos + 1, end - pos - 2);
}

// Returns up to maxItems string values from a JSON string array, comma-separated.
static std::string jsonGetStringArray(const std::string& json, const std::string& key, int maxItems = 3) {
    const auto pos = jsonFindValue(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') return "";
    std::string result;
    int count = 0;
    size_t i = pos + 1;
    while (i < json.size() && json[i] != ']' && count < maxItems) {
        while (i < json.size() && json[i] != '"' && json[i] != ']') ++i;
        if (i >= json.size() || json[i] == ']') break;
        const auto start = i + 1;
        const auto end = json.find('"', start);
        if (end == std::string::npos) break;
        if (count > 0) result += ", ";
        result += json.substr(start, end - start);
        ++count;
        i = end + 1;
    }
    return result;
}

// Returns a specific field from the first object in a JSON array for 'key'.
static std::string jsonFirstArrayItemField(const std::string& json,
                                           const std::string& arrayKey,
                                           const std::string& fieldKey,
                                           size_t offset = 0) {
    const auto pos = jsonFindValue(json, arrayKey, offset);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') return "";
    size_t arrStart = pos + 1;
    while (arrStart < json.size() && json[arrStart] != '{' && json[arrStart] != ']') ++arrStart;
    if (arrStart >= json.size() || json[arrStart] == ']') return "";
    const auto end = jsonSkipValue(json, arrStart);
    const auto firstObj = json.substr(arrStart + 1, end - arrStart - 2);
    return jsonGetString(firstObj, fieldKey);
}

// Returns the "name" field of the first object in a JSON array for 'key'.
// Searches from 'offset' so callers can skip over nested arrays with the same key name.
static std::string jsonFirstArrayItemName(const std::string& json,
                                          const std::string& key,
                                          size_t offset = 0) {
    const auto pos = jsonFindValue(json, key, offset);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') return "";
    size_t arrStart = pos + 1;
    while (arrStart < json.size() && json[arrStart] != '{' && json[arrStart] != ']') ++arrStart;
    if (arrStart >= json.size() || json[arrStart] == ']') return "";
    const auto end = jsonSkipValue(json, arrStart);
    const auto firstObj = json.substr(arrStart + 1, end - arrStart - 2);
    return jsonGetString(firstObj, "name");
}

// Returns inner content of the object at 'index' in the JSON array for 'key'.
static std::string jsonGetArrayItem(const std::string& json, const std::string& key, int index) {
    const auto pos = jsonFindValue(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') return "";
    size_t cur = pos + 1;
    int count = 0;
    while (cur < json.size()) {
        while (cur < json.size() && json[cur] != '{' && json[cur] != ']') ++cur;
        if (cur >= json.size() || json[cur] == ']') break;
        const auto end = jsonSkipValue(json, cur);
        if (count == index) return json.substr(cur + 1, end - cur - 2);
        ++count;
        cur = end;
    }
    return "";
}

// --- HTTP POST to Spotify API ---
// curl_global_init() is called once from main() before any threads start.
// Do NOT call it again here — it is not thread-safe.

static std::string httpPost(const std::string& url, const std::string& body) {
    debugLog("HTTP: curl_easy_init");
    CURL* curl = curl_easy_init();
    if (!curl) { debugLog("HTTP: curl_easy_init returned null"); return ""; }

    debugLog("HTTP: setopt");
    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    debugLog("HTTP: curl_easy_perform");
    const CURLcode res = curl_easy_perform(curl);
    char resBuf[64];
    snprintf(resBuf, sizeof(resBuf), "HTTP: res=%d len=%d", (int)res, (int)response.size());
    debugLog(resBuf);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    debugLog("HTTP: done");
    return response;
}

// --- HTTP GET to Spotify API ---

static std::string httpGet(const std::string& url, const std::string& accessToken, long* outHttpCode = nullptr) {
    CURL* curl = curl_easy_init();
    if (!curl) { debugLog("HTTP GET: curl_easy_init returned null"); return ""; }

    std::string response;
    struct curl_slist* headers = nullptr;
    const std::string authHeader = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    const CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    if (outHttpCode) *outHttpCode = httpCode;

    char buf[80];
    snprintf(buf, sizeof(buf), "HTTP GET: res=%d http=%ld len=%d", (int)res, httpCode, (int)response.size());
    debugLog(buf);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

// --- Random bytes from /dev/urandom ---

static void randomBytes(uint8_t* buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, len);
        (void)n;
        close(fd);
    }
}

// --- Public API ---

std::string generateCodeVerifier() {
    uint8_t bytes[32] = {};
    randomBytes(bytes, sizeof(bytes));
    return base64url(bytes, sizeof(bytes));
}

std::string generateCodeChallenge(const std::string& verifier) {
    uint8_t hash[32] = {};
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0 /* SHA-256, not SHA-224 */);
    mbedtls_sha256_update(&ctx,
        reinterpret_cast<const unsigned char*>(verifier.c_str()),
        verifier.size());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    return base64url(hash, sizeof(hash));
}

std::string generateState() {
    uint8_t bytes[12] = {};
    randomBytes(bytes, sizeof(bytes));
    return base64url(bytes, sizeof(bytes));
}

std::string buildAuthUrl(const std::string& challenge, const std::string& state) {
    return std::string("https://accounts.spotify.com/authorize")
        + "?response_type=code"
        + "&client_id="             + urlEncode(CLIENT_ID)
        + "&scope="                 + urlEncode(SCOPES)
        + "&redirect_uri="          + urlEncode(REDIRECT_URI)
        + "&state="                 + urlEncode(state)
        + "&code_challenge_method=S256"
        + "&code_challenge="        + urlEncode(challenge);
}

Tokens exchangeCode(const std::string& code, const std::string& verifier) {
    const std::string body =
        std::string("grant_type=authorization_code")
        + "&code="          + urlEncode(code)
        + "&redirect_uri="  + urlEncode(REDIRECT_URI)
        + "&client_id="     + urlEncode(CLIENT_ID)
        + "&code_verifier=" + urlEncode(verifier);

    const auto response = httpPost("https://accounts.spotify.com/api/token", body);
    if (response.empty()) return Tokens();

    const auto at = jsonGetString(response, "access_token");
    if (at.empty()) return Tokens();

    const auto rt = jsonGetString(response, "refresh_token");
    const auto expiresIn = jsonGetLong(response, "expires_in");
    return Tokens(at, rt, static_cast<long>(time(nullptr)) + expiresIn);
}

Tokens refreshAccessToken(const std::string& existingRefreshToken) {
    const std::string body =
        std::string("grant_type=refresh_token")
        + "&refresh_token=" + urlEncode(existingRefreshToken)
        + "&client_id="     + urlEncode(CLIENT_ID);

    const auto response = httpPost("https://accounts.spotify.com/api/token", body);
    if (response.empty()) return Tokens();

    const auto at = jsonGetString(response, "access_token");
    if (at.empty()) return Tokens();

    auto rt = jsonGetString(response, "refresh_token");
    if (rt.empty()) rt = existingRefreshToken; // Spotify may reuse the same token

    const auto expiresIn = jsonGetLong(response, "expires_in");
    return Tokens(at, rt, static_cast<long>(time(nullptr)) + expiresIn);
}

PlayerState getPlayerState(const std::string& accessToken) {
    debugLog("PLAYER: fetching /me/player");
    long httpCode = 0;
    const auto resp = httpGet("https://api.spotify.com/v1/me/player", accessToken, &httpCode);
    PlayerState state;
    if (httpCode == 401) {
        debugLog("PLAYER: 401 Unauthorized — token expired or invalid");
        state.tokenExpired = true;
        return state;
    }
    if (resp.size() < 10) {
        // HTTP 204 = no active device
        debugLog("PLAYER: 204 no active device");
        return state;
    }

    const auto deviceObj = jsonGetObject(resp, "device");
    state.deviceName = jsonGetString(deviceObj, "name");

    const auto itemObj = jsonGetObject(resp, "item");

    // The item object has this structure (order matters):
    //   "album": { ..., "artists": [...], "name": "Album Name", ... },
    //   "artists": [ {"name": "Artist"}, ... ],   <- track artists
    //   ...
    //   "name": "Track Name",                     <- track name (comes last)
    //
    // We must skip past album (and its nested artists) before searching for
    // the track-level "artists" and "name" fields.

    size_t afterAlbum = 0;
    {
        const auto albumValuePos = jsonFindValue(itemObj, "album");
        if (albumValuePos != std::string::npos && albumValuePos < itemObj.size() && itemObj[albumValuePos] == '{')
            afterAlbum = jsonSkipValue(itemObj, albumValuePos);
    }

    state.artistName = jsonFirstArrayItemName(itemObj, "artists", afterAlbum);
    state.artistId   = jsonFirstArrayItemField(itemObj, "artists", "id", afterAlbum);

    // Track name comes after both album and artists — skip artists array too.
    size_t afterArtists = afterAlbum;
    {
        const auto artistsValuePos = jsonFindValue(itemObj, "artists", afterAlbum);
        if (artistsValuePos != std::string::npos && artistsValuePos < itemObj.size() && itemObj[artistsValuePos] == '[')
            afterArtists = jsonSkipValue(itemObj, artistsValuePos);
    }
    state.trackName = jsonGetStringFrom(itemObj, "name", afterArtists);

    // Album art and ID from item.album.
    // Must skip the nested "artists" array inside the album object — it contains its own
    // "id" fields that would be found before the album-level "id" if we search from the start.
    const auto albumObj = jsonGetObject(itemObj, "album");
    state.albumImageUrl = jsonFirstArrayItemField(albumObj, "images", "url");
    {
        size_t afterAlbumArtists = 0;
        const auto ap = jsonFindValue(albumObj, "artists");
        if (ap != std::string::npos && ap < albumObj.size() && albumObj[ap] == '[')
            afterAlbumArtists = jsonSkipValue(albumObj, ap);
        state.albumId = jsonGetStringFrom(albumObj, "id", afterAlbumArtists);
    }

    const auto isPlayingPos = jsonFindValue(resp, "is_playing");
    state.isPlaying = (isPlayingPos != std::string::npos &&
                       isPlayingPos + 4 <= resp.size() &&
                       resp.substr(isPlayingPos, 4) == "true");
    state.valid = !state.trackName.empty();

    char buf[256];
    snprintf(buf, sizeof(buf), "PLAYER: track='%s' artist='%s' device='%s' playing=%d",
             state.trackName.c_str(), state.artistName.c_str(),
             state.deviceName.c_str(), (int)state.isPlaying);
    debugLog(buf);

    return state;
}

// --- HTTP helpers for playback control ---

static void httpPut(const std::string& url, const std::string& accessToken) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = nullptr;
    const std::string auth = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Length: 0");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

static void httpPostNoBody(const std::string& url, const std::string& accessToken) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = nullptr;
    const std::string auth = "Authorization: Bearer " + accessToken;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Content-Length: 0");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

// --- Playback control ---

void play(const std::string& accessToken) {
    debugLog("CTRL: play");
    httpPut("https://api.spotify.com/v1/me/player/play", accessToken);
}

void pause(const std::string& accessToken) {
    debugLog("CTRL: pause");
    httpPut("https://api.spotify.com/v1/me/player/pause", accessToken);
}

void skipNext(const std::string& accessToken) {
    debugLog("CTRL: next");
    httpPostNoBody("https://api.spotify.com/v1/me/player/next", accessToken);
}

void skipPrevious(const std::string& accessToken) {
    debugLog("CTRL: previous");
    httpPostNoBody("https://api.spotify.com/v1/me/player/previous", accessToken);
}

// --- Artist info ---

ArtistInfo getArtistInfo(const std::string& artistId, const std::string& accessToken) {
    ArtistInfo info;
    if (artistId.empty()) return info;
    const auto resp = httpGet("https://api.spotify.com/v1/artists/" + artistId, accessToken);
    if (resp.size() < 10) return info;

    info.name       = jsonGetString(resp, "name");
    info.genres     = jsonGetStringArray(resp, "genres", 3);
    info.popularity = (int)jsonGetLong(resp, "popularity");
    info.imageUrl   = jsonFirstArrayItemField(resp, "images", "url");

    const auto followersObj = jsonGetObject(resp, "followers");
    info.followers  = jsonGetLong(followersObj, "total");

    info.valid = !info.name.empty();
    return info;
}

// --- Album info ---

AlbumInfo getAlbumInfo(const std::string& albumId, const std::string& accessToken) {
    AlbumInfo info;
    if (albumId.empty()) return info;
    const auto resp = httpGet("https://api.spotify.com/v1/albums/" + albumId, accessToken);
    if (resp.size() < 10) return info;

    info.name        = jsonGetString(resp, "name");
    info.albumType   = jsonGetString(resp, "album_type");
    info.releaseDate = jsonGetString(resp, "release_date");
    info.label       = jsonGetString(resp, "label");
    info.totalTracks = (int)jsonGetLong(resp, "total_tracks");
    info.popularity  = (int)jsonGetLong(resp, "popularity");
    info.valid       = !info.name.empty();
    return info;
}

// --- User profile ---

UserProfile getUserProfile(const std::string& accessToken) {
    UserProfile profile;
    const auto resp = httpGet("https://api.spotify.com/v1/me", accessToken);
    if (resp.size() < 10) return profile;

    profile.displayName = jsonGetString(resp, "display_name");
    profile.email       = jsonGetString(resp, "email");
    profile.country     = jsonGetString(resp, "country");
    profile.product     = jsonGetString(resp, "product");
    profile.imageUrl    = jsonFirstArrayItemField(resp, "images", "url");

    const auto followersObj = jsonGetObject(resp, "followers");
    profile.followers = jsonGetLong(followersObj, "total");

    profile.valid = !profile.displayName.empty();
    return profile;
}

// --- Queue ---

QueueInfo getQueue(const std::string& accessToken) {
    QueueInfo info;
    const auto resp = httpGet("https://api.spotify.com/v1/me/player/queue", accessToken);
    if (resp.size() < 10) return info;

    // Extracts name, artistName and imageUrl from a track object inner string.
    // Uses the afterAlbum offset pattern (same as getPlayerState) to avoid
    // matching album-level "artists" before the track-level array.
    auto extractTrack = [](const std::string& obj, QueueInfo::Track& t) {
        size_t afterAlbum = 0;
        const auto albumPos = jsonFindValue(obj, "album");
        if (albumPos != std::string::npos && albumPos < obj.size() && obj[albumPos] == '{')
            afterAlbum = jsonSkipValue(obj, albumPos);

        t.artistName = jsonFirstArrayItemName(obj, "artists", afterAlbum);

        size_t afterArtists = afterAlbum;
        const auto artPos = jsonFindValue(obj, "artists", afterAlbum);
        if (artPos != std::string::npos && artPos < obj.size() && obj[artPos] == '[')
            afterArtists = jsonSkipValue(obj, artPos);
        t.name = jsonGetStringFrom(obj, "name", afterArtists);

        const auto albumObj = jsonGetObject(obj, "album");
        t.imageUrl = jsonFirstArrayItemField(albumObj, "images", "url");
    };

    // Currently playing track
    const auto nowObj = jsonGetObject(resp, "currently_playing");
    if (!nowObj.empty()) {
        extractTrack(nowObj, info.tracks[0]);
        if (!info.tracks[0].name.empty()) info.trackCount = 1;
    }

    // Next 4 queue items
    for (int i = 0; i < 4; ++i) {
        const auto item = jsonGetArrayItem(resp, "queue", i);
        if (item.empty()) break;
        extractTrack(item, info.tracks[i + 1]);
        info.trackCount = i + 2;
    }

    info.valid = info.trackCount > 0;
    return info;
}

// --- Album art download (Spotify CDN, no auth required) ---

std::string downloadAlbumArt(const std::string& url) {
    if (url.empty()) return "";
    debugLog("ART: downloading");
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string data;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    const CURLcode res = curl_easy_perform(curl);
    char buf[64];
    snprintf(buf, sizeof(buf), "ART: res=%d size=%d", (int)res, (int)data.size());
    debugLog(buf);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? data : "";
}

} // namespace spotify
