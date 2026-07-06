#include <LocalServer.hpp>
#include <Lang.hpp>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

// ── URL decoding ─────────────────────────────────────────────────────────────

static void urlDecode(const char* src, char* dst, size_t dstSize) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[j++] = static_cast<char>(strtol(hex, nullptr, 16));
            i += 2;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

// Finds key= in a query string (e.g. "code=ABC&state=XYZ") and writes the
// URL-decoded value into dst. Returns true if found.
static bool queryParam(const char* query, const char* key,
                       char* dst, size_t dstSize) {
    const size_t keyLen = strlen(key);
    const char* p = query;
    while (*p) {
        const char* eq = strchr(p, '=');
        if (!eq) break;
        if (static_cast<size_t>(eq - p) == keyLen &&
                strncmp(p, key, keyLen) == 0) {
            const char* amp = strchr(eq + 1, '&');
            size_t valLen = amp ? static_cast<size_t>(amp - eq - 1)
                                : strlen(eq + 1);
            if (valLen >= dstSize) valLen = dstSize - 1;
            char raw[512] = {};
            memcpy(raw, eq + 1, valLen);
            urlDecode(raw, dst, dstSize);
            return true;
        }
        const char* amp = strchr(eq + 1, '&');
        if (!amp) break;
        p = amp + 1;
    }
    dst[0] = '\0';
    return false;
}

// ── HTML responses ────────────────────────────────────────────────────────────

static std::string buildSuccessPage() {
    return
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>" + lang::get("webauth.title") + "</title>"
        "<style>body{margin:0;display:flex;align-items:center;justify-content:center;"
        "min-height:100vh;background:#121212;color:#fff;font-family:system-ui}"
        "h1{color:#1DB954}.card{text-align:center;padding:40px}</style></head>"
        "<body><div class='card'><h1>" + lang::get("webauth.success_heading") + "</h1>"
        "<p>" + lang::get("webauth.success_body") + "</p></div></body></html>\r\n";
}

static std::string buildErrorPage() {
    return
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><html><body style='background:#121212;color:#fff;"
        "font-family:system-ui;text-align:center;padding:40px'>"
        "<h1 style='color:#e74c3c'>" + lang::get("webauth.error_heading") + "</h1>"
        "<p>" + lang::get("webauth.error_body") + "</p></body></html>\r\n";
}

// ── LocalServer ───────────────────────────────────────────────────────────────

LocalServer::LocalServer(int port) : port(port), serverFd(-1) {}

LocalServer::~LocalServer() { stop(); }

bool LocalServer::start() {
    this->serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (this->serverFd < 0) return false;

    int opt = 1;
    setsockopt(this->serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<uint16_t>(this->port));
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(this->serverFd,
             reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(this->serverFd); this->serverFd = -1; return false;
    }
    if (listen(this->serverFd, 1) < 0) {
        close(this->serverFd); this->serverFd = -1; return false;
    }

    // Non-blocking so tick() never stalls the render loop on accept()
    const int flags = fcntl(this->serverFd, F_GETFL, 0);
    fcntl(this->serverFd, F_SETFL, flags | O_NONBLOCK);

    return true;
}

void LocalServer::stop() {
    if (this->serverFd >= 0) {
        close(this->serverFd);
        this->serverFd = -1;
    }
}

bool LocalServer::tick(std::string& code, std::string& error) {
    if (this->serverFd < 0) return false;

    // Non-blocking accept — returns immediately if no one is connecting
    const int client = accept(this->serverFd, nullptr, nullptr);
    if (client < 0) return false; // EAGAIN / EWOULDBLOCK = nothing yet

    // Wait up to 200 ms for the browser to send its request
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(client, &fds);
    struct timeval tv = { 0, 200000 };
    if (select(client + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        close(client);
        return false;
    }

    char buf[4096] = {};
    recv(client, buf, sizeof(buf) - 1, 0);

    char codeBuf[512]  = {};
    char errorBuf[256] = {};

    const char* getPos = strstr(buf, "GET /");
    if (getPos) {
        const char* pathStart = getPos + 4;
        const char* qMark     = strchr(pathStart, '?');
        const char* httpEnd   = strstr(pathStart, " HTTP/");
        if (qMark && (!httpEnd || qMark < httpEnd)) {
            queryParam(qMark + 1, "code",  codeBuf,  sizeof(codeBuf));
            queryParam(qMark + 1, "error", errorBuf, sizeof(errorBuf));
        }
    }

    const bool ok = codeBuf[0] != '\0';
    const std::string page = ok ? buildSuccessPage() : buildErrorPage();
    send(client, page.c_str(), page.size(), 0);
    close(client);

    code  = codeBuf;
    error = errorBuf;
    return true;
}
