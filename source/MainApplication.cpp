#include <MainApplication.hpp>
#include <LayoutConstants.hpp>
#include <LoginLayout.hpp>
#include <LanguageSelectLayout.hpp>
#include <TokenStorage.hpp>
#include <SpotifyAuth.hpp>
#include <Lang.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <DebugLog.hpp>
#include <cstdio>
#include <ctime>
#include <cmath>

// --- Colors ---

static const pu::ui::Color CLR_BG      {  18,  18,  18, 255 };
static const pu::ui::Color CLR_SIDEBAR {  28,  28,  28, 255 };
static const pu::ui::Color CLR_TAB_SEL {  48,  48,  48, 255 };
static const pu::ui::Color CLR_GREEN   {  29, 185,  84, 255 };
static const pu::ui::Color CLR_WHITE   { 255, 255, 255, 255 };
static const pu::ui::Color CLR_GRAY    { 150, 150, 150, 255 };
static const pu::ui::Color CLR_HINT    {  70,  70,  70, 255 };
static const pu::ui::Color CLR_ART_BG  {  40,  40,  40, 255 };
static const pu::ui::Color CLR_BTN     {  55,  55,  55, 255 };
static const pu::ui::Color CLR_SEP        {  50,  50,  50, 255 };
static const pu::ui::Color CLR_SPINNER_BG {   0,   0,   0, 160 };
static const pu::ui::Color CLR_FOCUS_DARK {  75,  75,  75, 255 };
static const pu::ui::Color CLR_RED_DANGER { 200,  56,  60, 255 };

// --- Local IP helper ---

static std::string getLocalIp() {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return "";

    struct sockaddr_in dest = {};
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(53);
    dest.sin_addr.s_addr = inet_addr("8.8.8.8");

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest)) < 0) {
        close(fd);
        return "";
    }

    struct sockaddr_in local = {};
    socklen_t len = sizeof(local);
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local), &len) < 0) {
        close(fd);
        return "";
    }
    close(fd);

    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

static std::string encodeIp(const std::string& ip) {
    std::string out = ip;
    for (char& c : out) if (c == '.') c = '_';
    return out;
}

static constexpr int SETTINGS_LANG_COUNT = 2;
static const char* const SETTINGS_LANG_CODES[SETTINGS_LANG_COUNT] = { "es", "gb" };

// D-Pad and left-stick tilt share the same directional handling; holding either one
// past DIR_REPEAT_DELAY_FRAMES starts auto-repeating every DIR_REPEAT_INTERVAL_FRAMES
// (frame counts assume the ~60fps cadence OnInput is already driven at elsewhere, e.g. spinnerAngle/barPhase).
static constexpr u64 DIR_UP_MASK    = HidNpadButton_Up    | HidNpadButton_StickLUp;
static constexpr u64 DIR_DOWN_MASK  = HidNpadButton_Down  | HidNpadButton_StickLDown;
static constexpr u64 DIR_LEFT_MASK  = HidNpadButton_Left  | HidNpadButton_StickLLeft;
static constexpr u64 DIR_RIGHT_MASK = HidNpadButton_Right | HidNpadButton_StickLRight;
static constexpr int DIR_REPEAT_DELAY_FRAMES    = 18;
static constexpr int DIR_REPEAT_INTERVAL_FRAMES = 6;

static bool ProcessDirectionRepeat(int& heldFrames, bool pressed, bool held) {
    if (pressed) {
        heldFrames = 0;
        return true;
    }
    if (!held) {
        heldFrames = 0;
        return false;
    }
    heldFrames++;
    if (heldFrames < DIR_REPEAT_DELAY_FRAMES) return false;
    return (heldFrames - DIR_REPEAT_DELAY_FRAMES) % DIR_REPEAT_INTERVAL_FRAMES == 0;
}

static Tab PrevTab(const Tab t) {
    if (t == Tab::Player) return Tab::Settings;
    if (t == Tab::User) return Tab::Player;
    return Tab::User;
}

static Tab NextTab(const Tab t) {
    if (t == Tab::Player) return Tab::User;
    if (t == Tab::User) return Tab::Settings;
    return Tab::Player;
}

// =============================================================================
// MainLayout
// =============================================================================

MainLayout::MainLayout() : Layout::Layout(), currentTab(Tab::Player), currentRightTab(RightTab::Artist) {
    this->SetBackgroundColor(CLR_BG);

    // ---- Sidebar ----

    this->sidebarBg = pu::ui::elm::Rectangle::New(0, 0, SIDEBAR_W, SCREEN_H, CLR_SIDEBAR);
    this->Add(this->sidebarBg);

    // "Switch" at Medium font ≈ 90px wide — used only to center the group
    static constexpr s32 LOGO_SIZE      = 48;
    static constexpr s32 LOGO_TEXT_GAP  = 10;
    static constexpr s32 TEXT_W_EST     = 90;
    static constexpr s32 GROUP_W        = LOGO_SIZE + LOGO_TEXT_GAP + TEXT_W_EST;
    static constexpr s32 LOGO_X         = (SIDEBAR_W - GROUP_W) / 2;
    static constexpr s32 LOGO_Y         = 20;
    static constexpr s32 TITLE_X        = LOGO_X + LOGO_SIZE + LOGO_TEXT_GAP;
    static constexpr s32 TITLE_Y        = LOGO_Y + (LOGO_SIZE - 28) / 2; // vertically center text inside logo height

    auto* logoTex = pu::ui::render::LoadImageFromFile("romfs:/spotify.png");
    this->sidebarLogoImg = pu::ui::elm::Image::New(LOGO_X, LOGO_Y, logoTex ? pu::sdl2::TextureHandle::New(logoTex) : nullptr);
    this->sidebarLogoImg->SetWidth(LOGO_SIZE);
    this->sidebarLogoImg->SetHeight(LOGO_SIZE);
    this->Add(this->sidebarLogoImg);

    this->sidebarTitle = pu::ui::elm::TextBlock::New(TITLE_X, TITLE_Y, lang::get("main.sidebar_title"));
    this->sidebarTitle->SetColor(CLR_GREEN);
    this->sidebarTitle->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->sidebarTitle);

    // Tab 1 — Reproductor (selected by default)
    this->tab1Bg = pu::ui::elm::Rectangle::New(0, TAB1_Y, SIDEBAR_W, TAB_H, CLR_TAB_SEL);
    this->Add(this->tab1Bg);
    this->tab1Text = pu::ui::elm::TextBlock::New(28, TAB1_Y + 18, lang::get("main.tab_player"));
    this->tab1Text->SetColor(CLR_WHITE);
    this->tab1Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->tab1Text);

    // Tab 2 — Usuario
    this->tab2Bg = pu::ui::elm::Rectangle::New(0, TAB2_Y, SIDEBAR_W, TAB_H, CLR_SIDEBAR);
    this->Add(this->tab2Bg);
    this->tab2Text = pu::ui::elm::TextBlock::New(28, TAB2_Y + 18, lang::get("main.tab_user"));
    this->tab2Text->SetColor(CLR_GRAY);
    this->tab2Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->tab2Text);

    // Tab 3 — Settings
    this->tab3Bg = pu::ui::elm::Rectangle::New(0, TAB3_Y, SIDEBAR_W, TAB_H, CLR_SIDEBAR);
    this->Add(this->tab3Bg);
    this->tab3Text = pu::ui::elm::TextBlock::New(28, TAB3_Y + 18, lang::get("main.tab_settings"));
    this->tab3Text->SetColor(CLR_GRAY);
    this->tab3Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->tab3Text);

    // Green selection bar on the left edge
    this->tabIndicator = pu::ui::elm::Rectangle::New(0, TAB1_Y, 4, TAB_H, CLR_GREEN);
    this->Add(this->tabIndicator);

    // L / R shoulder-button row — sits above the tab list, one icon at each edge
    static constexpr s32 SHOULDER_ICON_SIZE   = 64;
    static constexpr s32 SHOULDER_ICON_MARGIN = 20;
    static constexpr s32 SHOULDER_ICON_Y      = TAB1_Y - SHOULDER_ICON_SIZE - 16;

    auto* lTex = pu::ui::render::LoadImageFromFile("romfs:/icons/L.png");
    this->lShoulderIcon = pu::ui::elm::Image::New(SHOULDER_ICON_MARGIN, SHOULDER_ICON_Y, lTex ? pu::sdl2::TextureHandle::New(lTex) : nullptr);
    this->lShoulderIcon->SetWidth(SHOULDER_ICON_SIZE);
    this->lShoulderIcon->SetHeight(SHOULDER_ICON_SIZE);
    this->Add(this->lShoulderIcon);

    auto* rTex = pu::ui::render::LoadImageFromFile("romfs:/icons/R.png");
    this->rShoulderIcon = pu::ui::elm::Image::New(SIDEBAR_W - SHOULDER_ICON_SIZE - SHOULDER_ICON_MARGIN, SHOULDER_ICON_Y, rTex ? pu::sdl2::TextureHandle::New(rTex) : nullptr);
    this->rShoulderIcon->SetWidth(SHOULDER_ICON_SIZE);
    this->rShoulderIcon->SetHeight(SHOULDER_ICON_SIZE);
    this->Add(this->rShoulderIcon);

    // Status line (auth state) near bottom of sidebar
    this->statusText = pu::ui::elm::TextBlock::New(12, SCREEN_H - 96, "");
    this->statusText->SetColor(CLR_GRAY);
    this->statusText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->statusText);

    // Device name below status
    this->deviceText = pu::ui::elm::TextBlock::New(12, SCREEN_H - 60, "");
    this->deviceText->SetColor(CLR_GRAY);
    this->deviceText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->deviceText);

    // Bottom hint
    const std::string hintStr = lang::get("common.exit_hint");
    auto hint = pu::ui::elm::TextBlock::New(0, SCREEN_H - 32, hintStr);
    hint->SetColor(CLR_HINT);
    hint->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    hint->SetX((SCREEN_W - hint->GetWidth()) / 2);
    this->Add(hint);

    // ---- Player tab ----

    // Album art placeholder background
    this->albumArtBg = pu::ui::elm::Rectangle::New(ART_X, ART_Y, ART_SIZE, ART_SIZE, CLR_ART_BG, 12);
    this->Add(this->albumArtBg);

    // Album art image (starts empty)
    this->albumArtImage = pu::ui::elm::Image::New(ART_X, ART_Y, nullptr);
    this->albumArtImage->SetWidth(ART_SIZE);
    this->albumArtImage->SetHeight(ART_SIZE);
    this->Add(this->albumArtImage);

    // Track name
    this->trackText = pu::ui::elm::TextBlock::New(ART_X, TRACK_Y, "");
    this->trackText->SetColor(CLR_WHITE);
    this->trackText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->trackText->SetClampWidth(ART_SIZE);
    this->trackText->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
    this->trackText->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->trackText);

    // Artist name
    this->artistText = pu::ui::elm::TextBlock::New(ART_X, ARTIST_Y, "");
    this->artistText->SetColor(CLR_GRAY);
    this->artistText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->artistText->SetClampWidth(ART_SIZE);
    this->artistText->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
    this->artistText->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->artistText);

    // Prev button (circle, centered at PREV_CX)
    this->prevBtnOutline = pu::ui::elm::Rectangle::New(
        PREV_CX - (CTRL_SMALL + 8) / 2, CTRL_Y - 4, CTRL_SMALL + 8, CTRL_SMALL + 8, CLR_GREEN, (CTRL_SMALL + 8) / 2);
    this->Add(this->prevBtnOutline);
    this->prevBtnBg = pu::ui::elm::Rectangle::New(
        PREV_CX - CTRL_SMALL / 2, CTRL_Y, CTRL_SMALL, CTRL_SMALL, CLR_BTN, CTRL_SMALL / 2);
    this->Add(this->prevBtnBg);
    {
        auto* tex = pu::ui::render::LoadImageFromFile("romfs:/player-back.png");
        this->prevBtnImg = pu::ui::elm::Image::New(
            PREV_CX - CTRL_ICON_SM / 2, CTRL_BTN_CY - CTRL_ICON_SM / 2,
            tex ? pu::sdl2::TextureHandle::New(tex) : nullptr);
        this->prevBtnImg->SetWidth(CTRL_ICON_SM);
        this->prevBtnImg->SetHeight(CTRL_ICON_SM);
        this->Add(this->prevBtnImg);
    }

    // Play/Pause button (larger circle, centered at PLAY_CX)
    this->playBtnOutline = pu::ui::elm::Rectangle::New(
        PLAY_CX - (CTRL_LARGE + 8) / 2, CTRL_Y - (CTRL_LARGE - CTRL_SMALL) / 2 - 4,
        CTRL_LARGE + 8, CTRL_LARGE + 8, CLR_GREEN, (CTRL_LARGE + 8) / 2);
    this->Add(this->playBtnOutline);
    this->playBtnBg = pu::ui::elm::Rectangle::New(
        PLAY_CX - CTRL_LARGE / 2, CTRL_Y - (CTRL_LARGE - CTRL_SMALL) / 2,
        CTRL_LARGE, CTRL_LARGE, CLR_BTN, CTRL_LARGE / 2);
    this->Add(this->playBtnBg);
    {
        auto* tex = pu::ui::render::LoadImageFromFile("romfs:/player-play.png");
        this->playBtnImg = pu::ui::elm::Image::New(
            PLAY_CX - CTRL_ICON_LG / 2, CTRL_BTN_CY - CTRL_ICON_LG / 2,
            tex ? pu::sdl2::TextureHandle::New(tex) : nullptr);
        this->playBtnImg->SetWidth(CTRL_ICON_LG);
        this->playBtnImg->SetHeight(CTRL_ICON_LG);
        this->Add(this->playBtnImg);
    }
    {
        auto* tex = pu::ui::render::LoadImageFromFile("romfs:/player-pause.png");
        this->pauseBtnImg = pu::ui::elm::Image::New(
            PLAY_CX - CTRL_ICON_LG / 2, CTRL_BTN_CY - CTRL_ICON_LG / 2,
            tex ? pu::sdl2::TextureHandle::New(tex) : nullptr);
        this->pauseBtnImg->SetWidth(CTRL_ICON_LG);
        this->pauseBtnImg->SetHeight(CTRL_ICON_LG);
        this->pauseBtnImg->SetVisible(false); // hidden until confirmed playing
        this->Add(this->pauseBtnImg);
    }

    // Next button (circle, centered at NEXT_CX)
    this->nextBtnOutline = pu::ui::elm::Rectangle::New(
        NEXT_CX - (CTRL_SMALL + 8) / 2, CTRL_Y - 4, CTRL_SMALL + 8, CTRL_SMALL + 8, CLR_GREEN, (CTRL_SMALL + 8) / 2);
    this->Add(this->nextBtnOutline);
    this->nextBtnBg = pu::ui::elm::Rectangle::New(
        NEXT_CX - CTRL_SMALL / 2, CTRL_Y, CTRL_SMALL, CTRL_SMALL, CLR_BTN, CTRL_SMALL / 2);
    this->Add(this->nextBtnBg);
    {
        auto* tex = pu::ui::render::LoadImageFromFile("romfs:/player-forward.png");
        this->nextBtnImg = pu::ui::elm::Image::New(
            NEXT_CX - CTRL_ICON_SM / 2, CTRL_BTN_CY - CTRL_ICON_SM / 2,
            tex ? pu::sdl2::TextureHandle::New(tex) : nullptr);
        this->nextBtnImg->SetWidth(CTRL_ICON_SM);
        this->nextBtnImg->SetHeight(CTRL_ICON_SM);
        this->Add(this->nextBtnImg);
    }

    // ---- User tab (hidden by default) ----

    this->userAvatarBg = pu::ui::elm::Rectangle::New(UAVATAR_X, UAVATAR_Y, UAVATAR_SIZE, UAVATAR_SIZE, CLR_ART_BG, 10);
    this->userAvatarBg->SetVisible(false);
    this->Add(this->userAvatarBg);

    this->userAvatarImg = pu::ui::elm::Image::New(UAVATAR_X, UAVATAR_Y, nullptr);
    this->userAvatarImg->SetWidth(UAVATAR_SIZE);
    this->userAvatarImg->SetHeight(UAVATAR_SIZE);
    this->userAvatarImg->SetVisible(false);
    this->Add(this->userAvatarImg);

    this->userNameText = pu::ui::elm::TextBlock::New(UINFO_X, UNAME_Y, "");
    this->userNameText->SetColor(CLR_WHITE);
    this->userNameText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->userNameText->SetVisible(false);
    this->Add(this->userNameText);

    // Country flag — positioned dynamically next to the username in SetUserProfile
    this->userFlagImg = pu::ui::elm::Image::New(UINFO_X, UNAME_Y, nullptr);
    this->userFlagImg->SetWidth(20);
    this->userFlagImg->SetHeight(15);
    this->userFlagImg->SetVisible(false);
    this->Add(this->userFlagImg);

    this->userEmailText = pu::ui::elm::TextBlock::New(UINFO_X, UEMAIL_Y, "");
    this->userEmailText->SetColor(CLR_GRAY);
    this->userEmailText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->userEmailText->SetVisible(false);
    this->Add(this->userEmailText);

    this->userPlanText = pu::ui::elm::TextBlock::New(UINFO_X, UPLAN_Y, "");
    this->userPlanText->SetColor(CLR_GRAY);
    this->userPlanText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->userPlanText->SetVisible(false);
    this->Add(this->userPlanText);

    this->userFollowersText = pu::ui::elm::TextBlock::New(UINFO_X, UFOLLOWERS_Y, "");
    this->userFollowersText->SetColor(CLR_GRAY);
    this->userFollowersText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->userFollowersText->SetVisible(false);
    this->Add(this->userFollowersText);

    // ---- Settings tab (hidden by default) ----

    this->settingsTitleText = pu::ui::elm::TextBlock::New(CONTENT_X + 120, ART_Y + 40, lang::get("settings.title"));
    this->settingsTitleText->SetColor(CLR_WHITE);
    this->settingsTitleText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->settingsTitleText->SetVisible(false);
    this->Add(this->settingsTitleText);

    this->settingsLanguageLabel = pu::ui::elm::TextBlock::New(CONTENT_X + 120, ART_Y + 150, lang::get("settings.language_label"));
    this->settingsLanguageLabel->SetColor(CLR_GRAY);
    this->settingsLanguageLabel->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->settingsLanguageLabel->SetVisible(false);
    this->Add(this->settingsLanguageLabel);

    this->settingsSelectOutline = pu::ui::elm::Rectangle::New(CONTENT_X + 116, ART_Y + 192, 568, 86, CLR_GREEN, 10);
    this->settingsSelectOutline->SetVisible(false);
    this->Add(this->settingsSelectOutline);

    this->settingsSelectBg = pu::ui::elm::Rectangle::New(CONTENT_X + 120, ART_Y + 196, 560, 78, CLR_TAB_SEL, 8);
    this->settingsSelectBg->SetVisible(false);
    this->Add(this->settingsSelectBg);

    this->settingsSelectText = pu::ui::elm::TextBlock::New(CONTENT_X + 144, ART_Y + 218, "");
    this->settingsSelectText->SetColor(CLR_WHITE);
    this->settingsSelectText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->settingsSelectText->SetVisible(false);
    this->Add(this->settingsSelectText);

    this->settingsApplyOutline = pu::ui::elm::Rectangle::New(CONTENT_X + 116, ART_Y + 292, 328, 80, CLR_GREEN, 10);
    this->settingsApplyOutline->SetVisible(false);
    this->Add(this->settingsApplyOutline);

    this->settingsApplyBg = pu::ui::elm::Rectangle::New(CONTENT_X + 120, ART_Y + 296, 320, 72, CLR_BTN, 8);
    this->settingsApplyBg->SetVisible(false);
    this->Add(this->settingsApplyBg);

    this->settingsApplyText = pu::ui::elm::TextBlock::New(CONTENT_X + 145, ART_Y + 318, lang::get("settings.apply"));
    this->settingsApplyText->SetColor(CLR_WHITE);
    this->settingsApplyText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->settingsApplyText->SetVisible(false);
    this->Add(this->settingsApplyText);

    this->settingsLogoutOutline = pu::ui::elm::Rectangle::New(CONTENT_X + 116, ART_Y + 386, 328, 80, CLR_GREEN, 10);
    this->settingsLogoutOutline->SetVisible(false);
    this->Add(this->settingsLogoutOutline);

    this->settingsLogoutBg = pu::ui::elm::Rectangle::New(CONTENT_X + 120, ART_Y + 390, 320, 72, CLR_RED_DANGER, 8);
    this->settingsLogoutBg->SetVisible(false);
    this->Add(this->settingsLogoutBg);

    this->settingsLogoutText = pu::ui::elm::TextBlock::New(CONTENT_X + 145, ART_Y + 412, lang::get("settings.logout"));
    this->settingsLogoutText->SetColor(CLR_WHITE);
    this->settingsLogoutText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->settingsLogoutText->SetVisible(false);
    this->Add(this->settingsLogoutText);

    this->settingsHelpText = pu::ui::elm::TextBlock::New(CONTENT_X + 120, ART_Y + 98, lang::get("settings.help"));
    this->settingsHelpText->SetColor(CLR_HINT);
    this->settingsHelpText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->settingsHelpText->SetVisible(false);
    this->Add(this->settingsHelpText);

    {
        static constexpr s32 SETTINGS_HINT_ICON_SIZE = 28;
        const s32 iconY = ART_Y + 92;
        const s32 rightIconX = CONTENT_X + 120 + this->settingsHelpText->GetWidth() + 60;
        const s32 leftIconX = rightIconX - SETTINGS_HINT_ICON_SIZE - 10;

        auto* leftTex = pu::ui::render::LoadImageFromFile("romfs:/icons/JoyCon D-Pad Left.png");
        this->settingsHelpLeftIcon = pu::ui::elm::Image::New(
            leftIconX, iconY,
            leftTex ? pu::sdl2::TextureHandle::New(leftTex) : nullptr);
        this->settingsHelpLeftIcon->SetWidth(SETTINGS_HINT_ICON_SIZE);
        this->settingsHelpLeftIcon->SetHeight(SETTINGS_HINT_ICON_SIZE);
        this->settingsHelpLeftIcon->SetVisible(false);
        this->Add(this->settingsHelpLeftIcon);

        auto* rightTex = pu::ui::render::LoadImageFromFile("romfs:/icons/JoyCon D-Pad Right.png");
        this->settingsHelpRightIcon = pu::ui::elm::Image::New(
            rightIconX, iconY,
            rightTex ? pu::sdl2::TextureHandle::New(rightTex) : nullptr);
        this->settingsHelpRightIcon->SetWidth(SETTINGS_HINT_ICON_SIZE);
        this->settingsHelpRightIcon->SetHeight(SETTINGS_HINT_ICON_SIZE);
        this->settingsHelpRightIcon->SetVisible(false);
        this->Add(this->settingsHelpRightIcon);
    }

    this->settingsSavedText = pu::ui::elm::TextBlock::New(CONTENT_X + 120, ART_Y + 532, "");
    this->settingsSavedText->SetColor(CLR_GREEN);
    this->settingsSavedText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->settingsSavedText->SetVisible(false);
    this->Add(this->settingsSavedText);

    {
        char infoBuf[128];
        snprintf(infoBuf, sizeof(infoBuf), lang::get("settings.app_info_format").c_str(),
            lang::get("login.title").c_str(), APP_VERSION_STR, APP_AUTHOR_STR);
        this->settingsAppInfoText = pu::ui::elm::TextBlock::New(CONTENT_X + 120, SCREEN_H - 100, infoBuf);
    }
    this->settingsAppInfoText->SetColor(CLR_HINT);
    this->settingsAppInfoText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->settingsAppInfoText->SetVisible(false);
    this->Add(this->settingsAppInfoText);

    {
        char attrBuf[192];
        snprintf(attrBuf, sizeof(attrBuf), lang::get("settings.icon_attribution_format").c_str(),
            "Switch Button Icons and Controls - Zacksly", "CC BY 3.0");
        this->settingsAttributionText = pu::ui::elm::TextBlock::New(CONTENT_X + 120, SCREEN_H - 68, attrBuf);
    }
    this->settingsAttributionText->SetColor(CLR_HINT);
    this->settingsAttributionText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->settingsAttributionText->SetVisible(false);
    this->Add(this->settingsAttributionText);

    this->settingsLangIndex = (lang::currentLanguage == "es") ? 0 : 1;
    this->playerFocus = PlayerFocus::PlayPause;
    this->settingsFocus = SettingsFocus::Language;
    this->UpdateSettingsSelectText();
    this->UpdatePlayerFocusStyles();
    this->UpdateSettingsFocusStyles();

    // ---- Right panel ----

    // Borders around the right panel (left, right, bottom)
    this->rightVertSep = pu::ui::elm::Rectangle::New(RIGHT_X - 1, ART_Y, 1, CTRL_Y + CTRL_LARGE - ART_Y + 1, CLR_SEP);
    this->Add(this->rightVertSep);

    this->rightRightBorder = pu::ui::elm::Rectangle::New(RIGHT_X + RIGHT_W, ART_Y, 1, CTRL_Y + CTRL_LARGE - ART_Y + 1, CLR_SEP);
    this->Add(this->rightRightBorder);

    this->rightBottomBorder = pu::ui::elm::Rectangle::New(RIGHT_X - 1, CTRL_Y + CTRL_LARGE, RIGHT_W + 2, 1, CLR_SEP);
    this->Add(this->rightBottomBorder);

    // Tab 1 background (Artista — active by default)
    this->rightTab1Bg = pu::ui::elm::Rectangle::New(RIGHT_X, ART_Y, RIGHT_TAB_W, RIGHT_TAB_H, CLR_TAB_SEL);
    this->Add(this->rightTab1Bg);

    // Tab 2 background (Cola)
    this->rightTab2Bg = pu::ui::elm::Rectangle::New(RIGHT_X + RIGHT_TAB_W, ART_Y, RIGHT_TAB_W, RIGHT_TAB_H, CLR_BG);
    this->Add(this->rightTab2Bg);

    // Tab texts (centered within each 410px half)
    this->rightTab1Text = pu::ui::elm::TextBlock::New(RIGHT_X + RIGHT_TAB_W / 2 - 50, ART_Y + 13, lang::get("main.tab_artist"));
    this->rightTab1Text->SetColor(CLR_WHITE);
    this->rightTab1Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->rightTab1Text);

    this->rightTab2Text = pu::ui::elm::TextBlock::New(RIGHT_X + RIGHT_TAB_W + RIGHT_TAB_W / 2 - 28, ART_Y + 13, lang::get("main.tab_queue"));
    this->rightTab2Text->SetColor(CLR_GRAY);
    this->rightTab2Text->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->Add(this->rightTab2Text);

    // Green underline indicator on the active tab
    this->rightTabIndicator = pu::ui::elm::Rectangle::New(RIGHT_X, ART_Y + RIGHT_TAB_H - 3, RIGHT_TAB_W, 3, CLR_GREEN);
    this->Add(this->rightTabIndicator);

    // Horizontal separator below tab bar
    this->rightHorizSep = pu::ui::elm::Rectangle::New(RIGHT_X, ART_Y + RIGHT_TAB_H, RIGHT_W, 1, CLR_SEP);
    this->Add(this->rightHorizSep);

    // ZL / ZR icons — pinned to the outer edges of the right panel's tab bar
    static constexpr s32 ZTRIGGER_ICON_SIZE   = 64;
    static constexpr s32 ZTRIGGER_ICON_MARGIN = 16;
    static constexpr s32 ZTRIGGER_ICON_Y      = ART_Y + (RIGHT_TAB_H - ZTRIGGER_ICON_SIZE) / 2;

    auto* zlTex = pu::ui::render::LoadImageFromFile("romfs:/icons/ZL.png");
    this->zlShoulderIcon = pu::ui::elm::Image::New(RIGHT_X + ZTRIGGER_ICON_MARGIN, ZTRIGGER_ICON_Y, zlTex ? pu::sdl2::TextureHandle::New(zlTex) : nullptr);
    this->zlShoulderIcon->SetWidth(ZTRIGGER_ICON_SIZE);
    this->zlShoulderIcon->SetHeight(ZTRIGGER_ICON_SIZE);
    this->Add(this->zlShoulderIcon);

    auto* zrTex = pu::ui::render::LoadImageFromFile("romfs:/icons/ZR.png");
    this->zrShoulderIcon = pu::ui::elm::Image::New(RIGHT_X + RIGHT_W - ZTRIGGER_ICON_SIZE - ZTRIGGER_ICON_MARGIN, ZTRIGGER_ICON_Y, zrTex ? pu::sdl2::TextureHandle::New(zrTex) : nullptr);
    this->zrShoulderIcon->SetWidth(ZTRIGGER_ICON_SIZE);
    this->zrShoulderIcon->SetHeight(ZTRIGGER_ICON_SIZE);
    this->Add(this->zrShoulderIcon);

    // Artist tab content
    this->rightArtistImgBg = pu::ui::elm::Rectangle::New(RART_IMG_X, RART_BLOCK_Y, RART_IMG_SIZE, RART_IMG_SIZE, CLR_ART_BG, 10);
    this->Add(this->rightArtistImgBg);

    this->rightArtistImg = pu::ui::elm::Image::New(RART_IMG_X, RART_BLOCK_Y, nullptr);
    this->rightArtistImg->SetWidth(RART_IMG_SIZE);
    this->rightArtistImg->SetHeight(RART_IMG_SIZE);
    this->Add(this->rightArtistImg);

    this->rightArtistName = pu::ui::elm::TextBlock::New(RART_INFO_X, RART_NAME_Y, "");
    this->rightArtistName->SetColor(CLR_WHITE);
    this->rightArtistName->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->rightArtistName->SetClampWidth(RART_INFO_W);
    this->rightArtistName->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
    this->rightArtistName->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->rightArtistName);

    this->rightArtistGenres = pu::ui::elm::TextBlock::New(RART_INFO_X, RART_GENRES_Y, "");
    this->rightArtistGenres->SetColor(CLR_GRAY);
    this->rightArtistGenres->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->rightArtistGenres->SetClampWidth(RART_INFO_W);
    this->rightArtistGenres->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
    this->rightArtistGenres->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->rightArtistGenres);

    this->rightArtistFollowers = pu::ui::elm::TextBlock::New(RART_INFO_X, RART_FOLLOW_Y, "");
    this->rightArtistFollowers->SetColor(CLR_GRAY);
    this->rightArtistFollowers->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->rightArtistFollowers);

    this->rightArtistPopularity = pu::ui::elm::TextBlock::New(RART_INFO_X, RART_POP_Y, "");
    this->rightArtistPopularity->SetColor(CLR_GRAY);
    this->rightArtistPopularity->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->rightArtistPopularity);

    // Album section — separator, thumbnail, header + info lines
    this->rightAlbumSep = pu::ui::elm::Rectangle::New(RART_IMG_X, RALBUM_SEP_Y, RALBUM_FULL_W, 1, CLR_SEP);
    this->Add(this->rightAlbumSep);

    this->rightAlbumImgBg = pu::ui::elm::Rectangle::New(RART_IMG_X, RALBUM_IMG_Y, RALBUM_IMG_SIZE, RALBUM_IMG_SIZE, CLR_ART_BG, 6);
    this->Add(this->rightAlbumImgBg);

    this->rightAlbumImg = pu::ui::elm::Image::New(RART_IMG_X, RALBUM_IMG_Y, nullptr);
    this->rightAlbumImg->SetWidth(RALBUM_IMG_SIZE);
    this->rightAlbumImg->SetHeight(RALBUM_IMG_SIZE);
    this->Add(this->rightAlbumImg);

    this->rightAlbumHeader = pu::ui::elm::TextBlock::New(RART_IMG_X, RALBUM_HDR_Y, lang::get("main.album_header"));
    this->rightAlbumHeader->SetColor(CLR_GRAY);
    this->rightAlbumHeader->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->rightAlbumHeader);

    this->rightAlbumName = pu::ui::elm::TextBlock::New(RALBUM_TEXT_X, RALBUM_NAME_Y, "");
    this->rightAlbumName->SetColor(CLR_WHITE);
    this->rightAlbumName->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->rightAlbumName->SetClampWidth(RALBUM_TEXT_W);
    this->rightAlbumName->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
    this->rightAlbumName->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
    this->Add(this->rightAlbumName);

    this->rightAlbumTypeYear = pu::ui::elm::TextBlock::New(RALBUM_TEXT_X, RALBUM_INFO1_Y, "");
    this->rightAlbumTypeYear->SetColor(CLR_GRAY);
    this->rightAlbumTypeYear->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->rightAlbumTypeYear);

    this->rightAlbumTracks = pu::ui::elm::TextBlock::New(RALBUM_TEXT_X, RALBUM_INFO2_Y, "");
    this->rightAlbumTracks->SetColor(CLR_GRAY);
    this->rightAlbumTracks->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->Add(this->rightAlbumTracks);

    // Queue tab content (5 cards: [0]=currently playing green bg, [1..4]=next in queue)
    for (int i = 0; i < 5; ++i) {
        const s32 cy = RQUEUE_START_Y + i * (RQUEUE_ITEM_H + RQUEUE_GAP);
        const pu::ui::Color cardBgColor = (i == 0) ? CLR_GREEN : CLR_ART_BG;

        this->queueCardBg[i] = pu::ui::elm::Rectangle::New(
            RQUEUE_CARD_X, cy, RQUEUE_CARD_W, RQUEUE_ITEM_H - 2, cardBgColor, 8);
        this->queueCardBg[i]->SetVisible(false);
        this->Add(this->queueCardBg[i]);

        this->queueCardImgBg[i] = pu::ui::elm::Rectangle::New(
            RQUEUE_IMG_X, cy + 15, RQUEUE_IMG_SIZE, RQUEUE_IMG_SIZE, CLR_SIDEBAR, 6);
        this->queueCardImgBg[i]->SetVisible(false);
        this->Add(this->queueCardImgBg[i]);

        this->queueCardImg[i] = pu::ui::elm::Image::New(RQUEUE_IMG_X, cy + 15, nullptr);
        this->queueCardImg[i]->SetWidth(RQUEUE_IMG_SIZE);
        this->queueCardImg[i]->SetHeight(RQUEUE_IMG_SIZE);
        this->queueCardImg[i]->SetVisible(false);
        this->Add(this->queueCardImg[i]);

        this->queueCardTitle[i] = pu::ui::elm::TextBlock::New(RQUEUE_TEXT_X, cy + 22, "");
        this->queueCardTitle[i]->SetColor(CLR_WHITE);
        this->queueCardTitle[i]->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
        this->queueCardTitle[i]->SetClampWidth(RQUEUE_TEXT_W);
        this->queueCardTitle[i]->SetClampSpeed(pu::ui::elm::TextBlock::DefaultClampSpeedSteps / 3);
        this->queueCardTitle[i]->SetClampDelay(pu::ui::elm::TextBlock::DefaultClampStaticDelaySteps);
        this->queueCardTitle[i]->SetVisible(false);
        this->Add(this->queueCardTitle[i]);

        this->queueCardArtist[i] = pu::ui::elm::TextBlock::New(RQUEUE_TEXT_X, cy + 58, "");
        this->queueCardArtist[i]->SetColor(CLR_WHITE);
        this->queueCardArtist[i]->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
        this->queueCardArtist[i]->SetClampWidth(RQUEUE_TEXT_W);
        this->queueCardArtist[i]->SetVisible(false);
        this->Add(this->queueCardArtist[i]);
    }

    // Audio playback bars — right side of first queue card
    for (int j = 0; j < 3; ++j) {
        const s32 bx = BARS_X0 + j * (BAR_W + BAR_GAP);
        this->bars[j] = pu::ui::elm::Rectangle::New(bx, CARD0_CY - BAR_MAX_H / 2, BAR_W, BAR_MAX_H, CLR_WHITE, 2);
        this->bars[j]->SetVisible(false);
        this->Add(this->bars[j]);
    }

    // Loading spinner — centered over album art, shown during skip pending polling
    this->spinnerBackdrop = pu::ui::elm::Rectangle::New(
        ART_X, ART_Y, ART_SIZE, ART_SIZE, CLR_SPINNER_BG, 12);
    this->spinnerBackdrop->SetVisible(false);
    this->Add(this->spinnerBackdrop);

    {
        auto* spinTex = pu::ui::render::LoadImageFromFile("romfs:/loading.png");
        this->spinnerImg = pu::ui::elm::Image::New(
            SPINNER_X, SPINNER_Y,
            spinTex ? pu::sdl2::TextureHandle::New(spinTex) : nullptr);
        this->spinnerImg->SetWidth(SPINNER_SIZE);
        this->spinnerImg->SetHeight(SPINNER_SIZE);
        this->spinnerImg->SetVisible(false);
        this->Add(this->spinnerImg);

        // Full-screen blocking overlay uses the same spinner asset.
        this->blockingOverlayBg = pu::ui::elm::Rectangle::New(0, 0, SCREEN_W, SCREEN_H, CLR_SPINNER_BG);
        this->blockingOverlayBg->SetVisible(false);
        this->Add(this->blockingOverlayBg);

        auto* overlaySpinTex = pu::ui::render::LoadImageFromFile("romfs:/loading.png");

        this->blockingOverlaySpinner = pu::ui::elm::Image::New(
            CONTENT_CX - SPINNER_SIZE / 2,
            SCREEN_H / 2 - SPINNER_SIZE / 2,
            overlaySpinTex ? pu::sdl2::TextureHandle::New(overlaySpinTex) : nullptr);
        this->blockingOverlaySpinner->SetWidth(SPINNER_SIZE);
        this->blockingOverlaySpinner->SetHeight(SPINNER_SIZE);
        this->blockingOverlaySpinner->SetVisible(false);
        this->Add(this->blockingOverlaySpinner);
    }

    // No-playback overlay — shown only when there is no active playback
    this->noPlaybackText = pu::ui::elm::TextBlock::New(
        PLAYER_CX - 290, SCREEN_H / 2 - 20, lang::get("main.no_playback"));
    this->noPlaybackText->SetColor(CLR_GRAY);
    this->noPlaybackText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->noPlaybackText->SetVisible(false);
    this->Add(this->noPlaybackText);

    // Periodic refresh via render callback
    this->AddRenderCallback([this]() { this->OnRenderCallback(); });

    // Spinner rotation — runs every frame, no-op when hidden
    this->AddRenderCallback([this]() {
        if (!this->spinnerVisible && !this->blockingOverlayVisible) return;
        this->spinnerAngle += 4.0f;
        if (this->spinnerAngle >= 360.0f) this->spinnerAngle -= 360.0f;
        if (this->spinnerVisible)
            this->spinnerImg->SetRotationAngle(this->spinnerAngle);
        if (this->blockingOverlayVisible)
            this->blockingOverlaySpinner->SetRotationAngle(this->spinnerAngle);
    });

    // Audio bars animation — sine wave, 120° offset between bars
    this->AddRenderCallback([this]() {
        if (!this->barsVisible) return;
        this->barPhase += 0.07f;
        if (this->barPhase > 2.0f * static_cast<float>(M_PI)) this->barPhase -= 2.0f * static_cast<float>(M_PI);
        constexpr float kOffset = 2.0f * static_cast<float>(M_PI) / 3.0f; // 120°
        for (int j = 0; j < 3; ++j) {
            const float t = (sinf(this->barPhase + j * kOffset) + 1.0f) / 2.0f;
            const s32 h = BAR_MIN_H + static_cast<s32>((BAR_MAX_H - BAR_MIN_H) * t);
            this->bars[j]->SetHeight(h);
            this->bars[j]->SetY(CARD0_CY - h / 2);
        }
    });
}

// --- Tab switching ---

void MainLayout::SetPlayerTabVisible(bool visible) {
    const bool showContent  = visible && this->playbackActive;
    const bool showNoPlay   = visible && !this->playbackActive;
    this->albumArtBg->SetVisible(showContent);
    this->albumArtImage->SetVisible(showContent);
    this->trackText->SetVisible(showContent);
    this->artistText->SetVisible(showContent);
    this->prevBtnBg->SetVisible(showContent);
    this->prevBtnImg->SetVisible(showContent);
    this->playBtnBg->SetVisible(showContent);
    this->playBtnImg->SetVisible(showContent && !this->isPlayingState);
    this->pauseBtnImg->SetVisible(showContent && this->isPlayingState);
    this->nextBtnBg->SetVisible(showContent);
    this->nextBtnImg->SetVisible(showContent);
    this->prevBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::Prev);
    this->playBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::PlayPause);
    this->nextBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::Next);
    this->spinnerBackdrop->SetVisible(showContent && this->spinnerVisible);
    this->spinnerImg->SetVisible(showContent && this->spinnerVisible);
    this->noPlaybackText->SetVisible(showNoPlay);
}

void MainLayout::SetUserTabVisible(bool visible) {
    this->userAvatarBg->SetVisible(visible);
    this->userAvatarImg->SetVisible(visible);
    this->userNameText->SetVisible(visible);
    this->userFlagImg->SetVisible(visible && this->userFlagImg->IsImageValid());
    this->userEmailText->SetVisible(visible);
    this->userPlanText->SetVisible(visible);
    this->userFollowersText->SetVisible(visible);
}

void MainLayout::SetSettingsTabVisible(bool visible) {
    this->settingsTitleText->SetVisible(visible);
    this->settingsLanguageLabel->SetVisible(visible);
    this->settingsSelectOutline->SetVisible(visible && this->settingsFocus == SettingsFocus::Language);
    this->settingsSelectBg->SetVisible(visible);
    this->settingsSelectText->SetVisible(visible);
    this->settingsApplyOutline->SetVisible(visible && this->settingsFocus == SettingsFocus::Apply);
    this->settingsApplyBg->SetVisible(visible);
    this->settingsApplyText->SetVisible(visible);
    this->settingsLogoutOutline->SetVisible(visible && this->settingsFocus == SettingsFocus::Logout);
    this->settingsLogoutBg->SetVisible(visible);
    this->settingsLogoutText->SetVisible(visible);
    this->settingsHelpText->SetVisible(visible);
    this->settingsHelpLeftIcon->SetVisible(visible);
    this->settingsHelpRightIcon->SetVisible(visible);
    this->settingsSavedText->SetVisible(visible);
    this->settingsAppInfoText->SetVisible(visible);
    this->settingsAttributionText->SetVisible(visible);
}

void MainLayout::UpdateSettingsSelectText() {
    const std::string label =
        (this->settingsLangIndex == 0)
            ? lang::get("settings.option_es")
            : lang::get("settings.option_gb");
    this->settingsSelectText->SetText("< " + label + " >");
}

void MainLayout::CycleSettingsLanguage(const int delta) {
    if (delta == 0) return;
    const int next = (this->settingsLangIndex + delta + SETTINGS_LANG_COUNT) % SETTINGS_LANG_COUNT;
    this->settingsLangIndex = next;
    this->UpdateSettingsSelectText();
    this->settingsSavedText->SetText("");
}

void MainLayout::UpdatePlayerFocusStyles() {
    const bool showContent = (this->currentTab == Tab::Player) && this->playbackActive;
    this->prevBtnBg->SetColor(this->playerFocus == PlayerFocus::Prev ? CLR_FOCUS_DARK : CLR_BTN);
    this->playBtnBg->SetColor(this->playerFocus == PlayerFocus::PlayPause ? CLR_FOCUS_DARK : CLR_BTN);
    this->nextBtnBg->SetColor(this->playerFocus == PlayerFocus::Next ? CLR_FOCUS_DARK : CLR_BTN);
    this->prevBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::Prev);
    this->playBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::PlayPause);
    this->nextBtnOutline->SetVisible(showContent && this->playerFocus == PlayerFocus::Next);
}

void MainLayout::MovePlayerFocus(const int delta) {
    if (delta == 0) return;
    int index = 1;
    if (this->playerFocus == PlayerFocus::Prev) index = 0;
    else if (this->playerFocus == PlayerFocus::Next) index = 2;
    index = (index + delta + 3) % 3;
    this->playerFocus = (index == 0) ? PlayerFocus::Prev : (index == 1 ? PlayerFocus::PlayPause : PlayerFocus::Next);
    this->UpdatePlayerFocusStyles();
}

void MainLayout::UpdateSettingsFocusStyles() {
    const bool showContent = (this->currentTab == Tab::Settings);
    this->settingsSelectBg->SetColor(this->settingsFocus == SettingsFocus::Language ? CLR_FOCUS_DARK : CLR_TAB_SEL);
    this->settingsApplyBg->SetColor(this->settingsFocus == SettingsFocus::Apply ? CLR_FOCUS_DARK : CLR_BTN);
    this->settingsLogoutBg->SetColor(CLR_RED_DANGER);
    this->settingsSelectOutline->SetVisible(showContent && this->settingsFocus == SettingsFocus::Language);
    this->settingsApplyOutline->SetVisible(showContent && this->settingsFocus == SettingsFocus::Apply);
    this->settingsLogoutOutline->SetVisible(showContent && this->settingsFocus == SettingsFocus::Logout);
}

void MainLayout::MoveSettingsFocus(const int delta) {
    if (delta == 0) return;
    static constexpr int FOCUS_COUNT = 3;
    int index = 0;
    if (this->settingsFocus == SettingsFocus::Apply) index = 1;
    else if (this->settingsFocus == SettingsFocus::Logout) index = 2;
    const int next = (index + delta + FOCUS_COUNT) % FOCUS_COUNT;
    this->settingsFocus = (next == 0) ? SettingsFocus::Language : (next == 1 ? SettingsFocus::Apply : SettingsFocus::Logout);
    this->UpdateSettingsFocusStyles();
}

std::string MainLayout::GetSelectedLanguageCode() const {
    return SETTINGS_LANG_CODES[this->settingsLangIndex];
}

void MainLayout::SetSettingsFeedback(const std::string& text) {
    this->settingsSavedText->SetText(text);
}

void MainLayout::SetRightPanelVisible(bool visible) {
    this->rightVertSep->SetVisible(visible);
    this->rightRightBorder->SetVisible(visible);
    this->rightBottomBorder->SetVisible(visible);
    this->rightTab1Bg->SetVisible(visible);
    this->rightTab2Bg->SetVisible(visible);
    this->rightTab1Text->SetVisible(visible);
    this->rightTab2Text->SetVisible(visible);
    this->rightTabIndicator->SetVisible(visible);
    this->rightHorizSep->SetVisible(visible);
    this->zlShoulderIcon->SetVisible(visible);
    this->zrShoulderIcon->SetVisible(visible);
    const bool showArtist = visible && this->currentRightTab == RightTab::Artist;
    this->rightArtistImgBg->SetVisible(showArtist);
    this->rightArtistImg->SetVisible(showArtist);
    this->rightArtistName->SetVisible(showArtist);
    this->rightArtistGenres->SetVisible(showArtist);
    this->rightArtistFollowers->SetVisible(showArtist);
    this->rightArtistPopularity->SetVisible(showArtist);
    this->rightAlbumSep->SetVisible(showArtist);
    this->rightAlbumImgBg->SetVisible(showArtist);
    this->rightAlbumImg->SetVisible(showArtist);
    this->rightAlbumHeader->SetVisible(showArtist);
    this->rightAlbumName->SetVisible(showArtist);
    this->rightAlbumTypeYear->SetVisible(showArtist);
    this->rightAlbumTracks->SetVisible(showArtist);
    const bool showQueue = visible && this->currentRightTab == RightTab::Queue;
    for (int i = 0; i < 5; ++i) {
        this->queueCardBg[i]->SetVisible(showQueue);
        this->queueCardImgBg[i]->SetVisible(showQueue);
        this->queueCardImg[i]->SetVisible(showQueue);
        this->queueCardTitle[i]->SetVisible(showQueue);
        this->queueCardArtist[i]->SetVisible(showQueue);
    }
    this->barsVisible = showQueue;
    for (int j = 0; j < 3; ++j) this->bars[j]->SetVisible(showQueue);
}

void MainLayout::SwitchToTab(Tab tab) {
    this->currentTab = tab;
    const bool isPlayer = (tab == Tab::Player);
    const bool isUser = (tab == Tab::User);
    const bool isSettings = (tab == Tab::Settings);

    this->tab1Bg->SetColor(isPlayer ? CLR_TAB_SEL : CLR_SIDEBAR);
    this->tab2Bg->SetColor(isUser ? CLR_TAB_SEL : CLR_SIDEBAR);
    this->tab3Bg->SetColor(isSettings ? CLR_TAB_SEL : CLR_SIDEBAR);
    this->tab1Text->SetColor(isPlayer ? CLR_WHITE : CLR_GRAY);
    this->tab2Text->SetColor(isUser ? CLR_WHITE : CLR_GRAY);
    this->tab3Text->SetColor(isSettings ? CLR_WHITE : CLR_GRAY);
    this->tabIndicator->SetY(isPlayer ? TAB1_Y : (isUser ? TAB2_Y : TAB3_Y));

    this->SetPlayerTabVisible(isPlayer);
    this->SetUserTabVisible(isUser);
    this->SetSettingsTabVisible(isSettings);
    this->SetRightPanelVisible(isPlayer && this->playbackActive);
}

void MainLayout::SwitchRightTab(RightTab tab) {
    this->currentRightTab = tab;
    const bool isArtist = (tab == RightTab::Artist);

    this->rightTab1Bg->SetColor(isArtist ? CLR_TAB_SEL : CLR_BG);
    this->rightTab2Bg->SetColor(isArtist ? CLR_BG : CLR_TAB_SEL);
    this->rightTab1Text->SetColor(isArtist ? CLR_WHITE : CLR_GRAY);
    this->rightTab2Text->SetColor(isArtist ? CLR_GRAY : CLR_WHITE);
    this->rightTabIndicator->SetX(isArtist ? RIGHT_X : RIGHT_X + RIGHT_TAB_W);
    this->rightArtistImgBg->SetVisible(isArtist);
    this->rightArtistImg->SetVisible(isArtist);
    this->rightArtistName->SetVisible(isArtist);
    this->rightArtistGenres->SetVisible(isArtist);
    this->rightArtistFollowers->SetVisible(isArtist);
    this->rightArtistPopularity->SetVisible(isArtist);
    this->rightAlbumSep->SetVisible(isArtist);
    this->rightAlbumImgBg->SetVisible(isArtist);
    this->rightAlbumImg->SetVisible(isArtist);
    this->rightAlbumHeader->SetVisible(isArtist);
    this->rightAlbumName->SetVisible(isArtist);
    this->rightAlbumTypeYear->SetVisible(isArtist);
    this->rightAlbumTracks->SetVisible(isArtist);
    const bool showQueue = !isArtist;
    for (int i = 0; i < 5; ++i) {
        this->queueCardBg[i]->SetVisible(showQueue);
        this->queueCardImgBg[i]->SetVisible(showQueue);
        this->queueCardImg[i]->SetVisible(showQueue);
        this->queueCardTitle[i]->SetVisible(showQueue);
        this->queueCardArtist[i]->SetVisible(showQueue);
    }
    this->barsVisible = showQueue;
    for (int j = 0; j < 3; ++j) this->bars[j]->SetVisible(showQueue);
}

// --- Periodic refresh ---

void MainLayout::OnRenderCallback() {
    if (!this->refreshCallback) return;
    const time_t now = time(nullptr);
    if (now - this->lastRefresh < REFRESH_INTERVAL_SECS) return;
    this->lastRefresh = now;
    this->refreshCallback();
}

void MainLayout::SetRefreshCallback(std::function<void()> fn) {
    this->refreshCallback = std::move(fn);
    this->lastRefresh = time(nullptr);
}

void MainLayout::TriggerRefreshNow() {
    this->lastRefresh = 0;
}

void MainLayout::SetLoadingSpinner(bool visible) {
    this->spinnerVisible = visible;
    if (!visible) this->spinnerAngle = 0.0f;
    const bool canShow = (this->currentTab == Tab::Player) && this->playbackActive;
    this->spinnerBackdrop->SetVisible(visible && canShow);
    this->spinnerImg->SetVisible(visible && canShow);
}

void MainLayout::SetBlockingLoading(bool visible) {
    this->blockingOverlayVisible = visible;
    if (!visible) this->spinnerAngle = 0.0f;
    this->blockingOverlayBg->SetVisible(visible);
    this->blockingOverlaySpinner->SetVisible(visible);
}

// --- Content setters ---

void MainLayout::SetStatus(const std::string& text) {
    this->statusText->SetText(text);
}

void MainLayout::SetDevice(const std::string& deviceName) {
    this->deviceText->SetText(deviceName.empty() ? "" : deviceName);
}

// =============================================================================
// MainApplication
// =============================================================================

void MainApplication::OnLoad() {
    this->SetOnInput([&](const u64 keys_down, const u64 keys_up, const u64 keys_held,
                         const pu::ui::TouchPoint touch_pos) {
        (void)keys_up; (void)touch_pos;

        if (keys_down & HidNpadButton_Plus) {
            this->Close();
            return;
        }

        if (!this->mainLayoutActive) return;

        // D-Pad and left-stick tilt share the same directional handling: a quick tap fires once,
        // holding either one past a short delay starts auto-repeating it.
        const bool dUp    = ProcessDirectionRepeat(this->dirUpHoldFrames,    keys_down & DIR_UP_MASK,    keys_held & DIR_UP_MASK);
        const bool dDown  = ProcessDirectionRepeat(this->dirDownHoldFrames,  keys_down & DIR_DOWN_MASK,  keys_held & DIR_DOWN_MASK);
        const bool dLeft  = ProcessDirectionRepeat(this->dirLeftHoldFrames,  keys_down & DIR_LEFT_MASK,  keys_held & DIR_LEFT_MASK);
        const bool dRight = ProcessDirectionRepeat(this->dirRightHoldFrames, keys_down & DIR_RIGHT_MASK, keys_held & DIR_RIGHT_MASK);

        // L / R → sidebar tab cycling
        if (keys_down & HidNpadButton_L)
            this->mainLayout->SwitchToTab(PrevTab(this->mainLayout->GetCurrentTab()));
        if (keys_down & HidNpadButton_R)
            this->mainLayout->SwitchToTab(NextTab(this->mainLayout->GetCurrentTab()));

        // Settings focus navigation
        if (this->mainLayout->GetCurrentTab() == Tab::Settings) {
            if (dUp)
                this->mainLayout->MoveSettingsFocus(-1);
            if (dDown)
                this->mainLayout->MoveSettingsFocus(1);

            if (this->mainLayout->GetSettingsFocus() == SettingsFocus::Language) {
                if (dLeft)
                    this->mainLayout->CycleSettingsLanguage(-1);
                if (dRight)
                    this->mainLayout->CycleSettingsLanguage(1);
            }

            if (keys_down & HidNpadButton_A) {
                if (this->mainLayout->GetSettingsFocus() == SettingsFocus::Apply)
                    this->ApplyLanguageFromSettings();
                else if (this->mainLayout->GetSettingsFocus() == SettingsFocus::Logout)
                    this->OnLogout();
                else
                    this->mainLayout->SetSettingsFeedback(lang::get("settings.press_apply"));
            }
        }

        // Player focus navigation and action
        if (this->mainLayout->GetCurrentTab() == Tab::Player && !this->actionsBlocked) {
            if (dLeft)
                this->mainLayout->MovePlayerFocus(-1);
            if (dRight)
                this->mainLayout->MovePlayerFocus(1);
            if (keys_down & HidNpadButton_A) {
                if (this->mainLayout->GetPlayerFocus() == PlayerFocus::Prev)
                    this->OnPrev();
                else if (this->mainLayout->GetPlayerFocus() == PlayerFocus::PlayPause)
                    this->OnPlayPause();
                else
                    this->OnNext();
            }
        }

        // ZL / ZR → right panel tab switching (only in Player tab with active playback)
        if (this->mainLayout->GetCurrentTab() == Tab::Player && this->mainLayout->GetPlaybackActive()) {
            if (keys_down & HidNpadButton_ZL)
                this->mainLayout->SwitchRightTab(RightTab::Artist);
            if (keys_down & HidNpadButton_ZR)
                this->mainLayout->SwitchRightTab(RightTab::Queue);
        }
    });

    const auto saved = TokenStorage::loadTokens();
    if (saved.valid) {
        this->currentTokens = saved;
        this->ActivateMainLayout(false, false, false);
        return;
    }

    // No saved session — let the user pick a display language before logging in.
    this->languageLayout = LanguageSelectLayout::New([this](const std::string& code) {
        this->OnLanguageSelected(code);
    });
    this->LoadLayout(this->languageLayout);
}

void MainApplication::OnLanguageSelected(const std::string& code) {
    lang::setLanguage(code);
    this->mainLayout = MainLayout::New();
    this->StartLoginFlow();
}

bool MainApplication::StartLoginFlow() {
    const std::string ip = getLocalIp();
    if (ip.empty()) {
        this->mainLayout->SetStatus(lang::get("main.connect_wifi"));
        this->LoadLayout(this->mainLayout);
        return false;
    }

    const auto verifier     = spotify::generateCodeVerifier();
    const auto challenge    = spotify::generateCodeChallenge(verifier);
    const auto randomState  = spotify::generateState();
    const auto fullState    = randomState + "." + encodeIp(ip);
    const auto authUrl      = spotify::buildAuthUrl(challenge, fullState);

    static constexpr int PORT = 8080;
    this->localServer = std::make_unique<LocalServer>(PORT);
    this->localServer->start();

    this->loginLayout = LoginLayout::New(authUrl, verifier,
        this->localServer.get(),
        [this](const spotify::Tokens& tokens) {
            this->OnLoginSuccess(tokens);
        },
        [this]() {
            this->OnLoginBack();
        });

    this->LoadLayout(this->loginLayout);
    return true;
}

void MainApplication::OnLoginBack() {
    debugLog("APP: OnLoginBack");
    if (this->localServer) {
        this->localServer->stop();
        this->localServer.reset();
    }
    if (!this->languageLayout) {
        this->languageLayout = LanguageSelectLayout::New([this](const std::string& code) {
            this->OnLanguageSelected(code);
        });
    }
    this->LoadLayout(this->languageLayout);
}

void MainApplication::OnLogout() {
    debugLog("APP: OnLogout");
    // Bail out before touching the session if we can't reach the login flow —
    // leaves the user logged in with a "connect wifi" hint instead of stranding them.
    if (!this->StartLoginFlow()) return;

    TokenStorage::clearTokens();
    this->currentTokens = spotify::Tokens();
    this->mainLayoutActive = false;
    this->userProfileFetched = false;
}

void MainApplication::OnLoginSuccess(const spotify::Tokens& tokens) {
    debugLog("APP: OnLoginSuccess");
    if (this->localServer) {
        this->localServer->stop();
        this->localServer.reset();
    }
    this->currentTokens = tokens;
    TokenStorage::saveTokens(tokens);
    this->ActivateMainLayout(false, false, false);
    debugLog("APP: ready");
}

void MainApplication::ResetMainLayoutCaches() {
    this->currentTrackName.clear();
    this->blockedFromTrackName.clear();
    this->currentAlbumUrl.clear();
    this->currentAlbumId.clear();
    this->currentArtistId.clear();
    for (std::string& queueUrl : this->currentQueueUrls) {
        queueUrl.clear();
    }
    this->actionsBlocked = false;
}

void MainApplication::ActivateMainLayout(const bool showSettingsTab, const bool showBlockingLoading, const bool deferInitialFetch) {
    this->mainLayout = MainLayout::New();
    this->mainLayoutActive = true;
    this->userProfileFetched = false;
    this->pendingInitialMainFetch = deferInitialFetch;
    this->pendingInitialMainFetchAfter = deferInitialFetch ? (time(nullptr) + 1) : 0;
    this->ResetMainLayoutCaches();
    this->mainLayout->SetStatus(lang::get("main.session_started"));
    this->LoadLayout(this->mainLayout);
    if (showSettingsTab) {
        this->mainLayout->SwitchToTab(Tab::Settings);
    }
    if (showBlockingLoading) {
        this->mainLayout->SetBlockingLoading(true);
    }

    this->mainLayout->SetRefreshCallback([this]() {
        if (this->pendingInitialMainFetch) {
            if (time(nullptr) < this->pendingInitialMainFetchAfter) return;
            this->pendingInitialMainFetch = false;
            this->FetchUserProfile();
            this->FetchAndShowPlayerState();
            this->mainLayout->SetBlockingLoading(false);
            return;
        }
        this->FetchAndShowPlayerState();
    });

    if (deferInitialFetch) {
        this->mainLayout->TriggerRefreshNow();
        return;
    }

    this->FetchUserProfile();
    this->FetchAndShowPlayerState();
}

void MainApplication::ApplyLanguageFromSettings() {
    const std::string code = this->mainLayout->GetSelectedLanguageCode();
    if (code == lang::currentLanguage) {
        this->mainLayout->SetSettingsFeedback(lang::get("settings.no_change"));
        return;
    }

    this->mainLayout->SetBlockingLoading(true);
    lang::setLanguage(code);
    this->ActivateMainLayout(true, true, true);
    this->mainLayout->SetSettingsFeedback(lang::get("settings.saved"));
}
