#include <LoginLayout.hpp>
#include <SpotifyAuth.hpp>
#include <DebugLog.hpp>
#include <Lang.hpp>
#include <qrcodegen.h>
#include <switch.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>

static constexpr s32 W       = 1920;
static constexpr s32 H       = 1080;
static constexpr s32 MARGIN  = 100;
static constexpr s32 QR_X    = 1280;
static constexpr s32 LEFT_W  = QR_X - MARGIN * 2;

static const pu::ui::Color CLR_BG    {  18,  18,  18, 255 };
static const pu::ui::Color CLR_WHITE { 255, 255, 255, 255 };
static const pu::ui::Color CLR_GRAY  { 150, 150, 150, 255 };
static const pu::ui::Color CLR_GREEN {  29, 185,  84, 255 };
static const pu::ui::Color CLR_RED   { 220,  50,  50, 255 };

pu::sdl2::TextureHandle::Ref LoginLayout::buildQrTexture(const std::string& url, s32* outSize) {
    if (outSize) *outSize = 0;

    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuf[qrcodegen_BUFFER_LEN_MAX];

    const bool ok = qrcodegen_encodeText(
        url.c_str(), tempBuf, qrcode,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true);

    if (!ok) return nullptr;

    const int modules = qrcodegen_getSize(qrcode);
    const int quiet   = 4;
    const int total   = modules + quiet * 2;
    const int scale   = std::max(1, 580 / total);
    const int side    = total * scale;

    SDL_Surface* surface = SDL_CreateRGBSurface(0, side, side, 32,
        0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
    if (!surface) return nullptr;

    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 255, 255, 255, 255));

    const int offset = quiet * scale;
    for (int row = 0; row < modules; row++) {
        for (int col = 0; col < modules; col++) {
            if (qrcodegen_getModule(qrcode, col, row)) {
                SDL_Rect rect = { offset + col * scale, offset + row * scale, scale, scale };
                SDL_FillRect(surface, &rect, SDL_MapRGBA(surface->format, 0, 0, 0, 255));
            }
        }
    }

    auto* tex = pu::ui::render::ConvertToTexture(surface);
    if (!tex) return nullptr;
    if (outSize) *outSize = static_cast<s32>(side);
    return pu::sdl2::TextureHandle::New(tex);
}

LoginLayout::LoginLayout(const std::string& authUrl,
                         const std::string& verifier,
                         LocalServer* srv,
                         OnLoginSuccessCallback cb)
    : Layout::Layout(), codeVerifier(verifier), onLoginSuccess(cb),
      server(srv), state(State::Waiting)
{
    this->SetBackgroundColor(CLR_BG);

    this->titleText = pu::ui::elm::TextBlock::New(0, 50, lang::get("login.title"));
    this->titleText->SetColor(CLR_GREEN);
    this->titleText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->titleText->SetX((W / 2) - 180);
    this->Add(this->titleText);

    this->step1Text = pu::ui::elm::TextBlock::New(MARGIN, 190,
        lang::get("login.step1"));
    this->step1Text->SetColor(CLR_WHITE);
    this->step1Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->step1Text);

    this->urlText = pu::ui::elm::TextBlock::New(MARGIN, 250, authUrl);
    this->urlText->SetColor(CLR_GREEN);
    this->urlText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->urlText->SetClampWidth(LEFT_W);
    this->urlText->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps);
    this->urlText->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->urlText);

    this->step2Text = pu::ui::elm::TextBlock::New(MARGIN, 330,
        lang::get("login.step2"));
    this->step2Text->SetColor(CLR_WHITE);
    this->step2Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->step2Text);

    this->statusText = pu::ui::elm::TextBlock::New(MARGIN, 410, lang::get("login.waiting_auth"));
    this->statusText->SetColor(CLR_GRAY);
    this->statusText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->statusText);

    auto hint = pu::ui::elm::TextBlock::New((W / 2) - 120, 1030, lang::get("common.exit_hint"));
    hint->SetColor(CLR_GRAY);
    hint->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(hint);

    s32 qrSide = 0;
    auto texHandle = buildQrTexture(authUrl, &qrSide);
    if (texHandle && qrSide > 0) {
        const s32 panelW = W - QR_X;
        const s32 qrX    = QR_X + (panelW - qrSide) / 2;
        const s32 qrY    = (H - qrSide) / 2;

        this->qrImage = pu::ui::elm::Image::New(qrX, qrY, texHandle);
        this->qrImage->SetWidth(qrSide);
        this->qrImage->SetHeight(qrSide);
        this->Add(this->qrImage);

        this->scanHintText = pu::ui::elm::TextBlock::New(0, qrY + qrSide + 16,
            lang::get("login.scan_hint"));
        this->scanHintText->SetColor(CLR_GRAY);
        this->scanHintText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
        this->scanHintText->SetX(qrX + (qrSide / 2) - 160);
        this->Add(this->scanHintText);
    }

    // Render callback: polls the server and — when a code arrives — performs the
    // token exchange inline (blocking ~1 s). The UI freezes briefly but the Switch
    // OS will not kill a homebrew app for a pause this short.
    // LoadLayout is NOT called here; that happens in the input callback.
    this->AddRenderCallback([this]() { this->OnRenderCallback(); });

    // Input callback: the only place where LoadLayout is called (safe during input phase).
    this->SetOnInput([this](const u64 keys_down, const u64 keys_up,
                            const u64 keys_held, const pu::ui::TouchPoint touch_pos) {
        (void)keys_down; (void)keys_up; (void)keys_held; (void)touch_pos;
        this->OnInputCallback();
    });
}

// Called ~60 fps during the RENDER phase.
// When a code arrives from the server it exchanges it for tokens inline (blocks ~1 s).
// Must NOT call LoadLayout here.
void LoginLayout::OnRenderCallback() {
    if (this->state != State::Waiting) return;
    if (!this->server) return;

    std::string code, error;
    if (!this->server->tick(code, error)) return;

    debugLog("RENDER: code received");

    if (!error.empty()) {
        char buf[512];
        snprintf(buf, sizeof(buf), lang::get("login.auth_error_format").c_str(), error.c_str());
        this->statusText->SetText(buf);
        this->statusText->SetColor(CLR_RED);
        this->state = State::Failed;
        return;
    }
    if (code.empty()) return;

    this->statusText->SetText(lang::get("login.code_received"));
    this->statusText->SetColor(CLR_GREEN);

    // Blocking token exchange — renders freeze for ~1 s, which is acceptable.
    debugLog("RENDER: calling exchangeCode");
    this->resultTokens = spotify::exchangeCode(code, this->codeVerifier);
    debugLog(this->resultTokens.valid ? "RENDER: tokens valid" : "RENDER: tokens invalid");

    if (this->resultTokens.valid) {
        this->state = State::Succeeded;
    } else {
        this->state = State::Failed;
        this->statusText->SetText(lang::get("login.token_error"));
        this->statusText->SetColor(CLR_RED);
    }
}

// Called ~60 fps during the INPUT phase. Safe to call LoadLayout from here.
void LoginLayout::OnInputCallback() {
    if (this->state == State::Succeeded) {
        debugLog("INPUT: calling onLoginSuccess");
        this->state = State::Handled;
        this->onLoginSuccess(this->resultTokens);
        debugLog("INPUT: onLoginSuccess returned");
    }
}
