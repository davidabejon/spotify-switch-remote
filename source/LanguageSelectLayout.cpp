#include <LanguageSelectLayout.hpp>
#include <LayoutConstants.hpp>
#include <SpotifyAuth.hpp>

static const pu::ui::Color CLR_BG      {  18,  18,  18, 255 };
static const pu::ui::Color CLR_WHITE   { 255, 255, 255, 255 };
static const pu::ui::Color CLR_GRAY    { 150, 150, 150, 255 };
static const pu::ui::Color CLR_GREEN   {  29, 185,  84, 255 };
static const pu::ui::Color CLR_OPT_BG  {  40,  40,  40, 255 };

// Codes/labels are hardcoded (not looked up via lang::get) since no language
// has been chosen yet when this screen is shown. Codes double as ISO country
// codes for the flagcdn lookup below.
static const char* const CODES[2]  = { "es", "gb" };
static const char* const LABELS[2] = { "Español", "English" };

static constexpr s32 OPT_W   = 420;
static constexpr s32 OPT_H   = 140;
static constexpr s32 OPT_GAP = 60;
static constexpr s32 OPT_Y   = (SCREEN_H - OPT_H) / 2;
static constexpr s32 OPTS_X0 = (SCREEN_W - (OPT_W * 2 + OPT_GAP)) / 2;

static constexpr s32 FLAG_W   = 48;
static constexpr s32 FLAG_H   = 32;
static constexpr s32 FLAG_GAP = 16;

LanguageSelectLayout::LanguageSelectLayout(OnLanguageSelectedCallback cb) : Layout::Layout(), onSelected(cb) {
    this->SetBackgroundColor(CLR_BG);

    this->titleText = pu::ui::elm::TextBlock::New(0, OPT_Y - 170, "Selecciona idioma");
    this->titleText->SetColor(CLR_WHITE);
    this->titleText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));
    this->titleText->SetX((SCREEN_W - this->titleText->GetWidth()) / 2);
    this->Add(this->titleText);

    this->subtitleText = pu::ui::elm::TextBlock::New(0, OPT_Y - 110, "Select language");
    this->subtitleText->SetColor(CLR_GRAY);
    this->subtitleText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Medium));
    this->subtitleText->SetX((SCREEN_W - this->subtitleText->GetWidth()) / 2);
    this->Add(this->subtitleText);

    for (int i = 0; i < 2; ++i) {
        const s32 x = OPTS_X0 + i * (OPT_W + OPT_GAP);

        this->optionBg[i] = pu::ui::elm::Rectangle::New(x, OPT_Y, OPT_W, OPT_H, CLR_OPT_BG, 14);
        this->Add(this->optionBg[i]);

        this->optionText[i] = pu::ui::elm::TextBlock::New(0, 0, LABELS[i]);
        this->optionText[i]->SetColor(CLR_WHITE);
        this->optionText[i]->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Large));

        // Text + flag are centered together as a group within the box.
        const s32 textW    = this->optionText[i]->GetWidth();
        const s32 groupW   = textW + FLAG_GAP + FLAG_W;
        const s32 groupX   = x + (OPT_W - groupW) / 2;
        const s32 centerY  = OPT_Y + OPT_H / 2;

        this->optionText[i]->SetX(groupX);
        this->optionText[i]->SetY(centerY - this->optionText[i]->GetHeight() / 2);
        this->Add(this->optionText[i]);

        this->flagImg[i] = pu::ui::elm::Image::New(groupX + textW + FLAG_GAP, centerY - FLAG_H / 2, nullptr);
        this->flagImg[i]->SetWidth(FLAG_W);
        this->flagImg[i]->SetHeight(FLAG_H);
        this->Add(this->flagImg[i]);
    }

    this->hintText = pu::ui::elm::TextBlock::New(0, OPT_Y + OPT_H + 60,
        "Usa < > y pulsa A para confirmar  /  Use < > and press A to confirm");
    this->hintText->SetColor(CLR_GRAY);
    this->hintText->SetFont(pu::ui::GetDefaultFont(pu::ui::DefaultFontSize::Small));
    this->hintText->SetX((SCREEN_W - this->hintText->GetWidth()) / 2);
    this->Add(this->hintText);

    this->UpdateSelection();

    // Flags are fetched over the network — deferred to the first render tick so
    // this screen paints immediately even if that takes a while (e.g. no WiFi yet).
    this->AddRenderCallback([this]() {
        if (this->flagsLoaded) return;
        this->flagsLoaded = true;
        this->LoadFlags();
    });

    this->SetOnInput([this](const u64 keys_down, const u64 keys_up, const u64 keys_held,
                            const pu::ui::TouchPoint touch_pos) {
        (void)keys_up; (void)keys_held; (void)touch_pos;
        this->OnInputCallback(keys_down);
    });
}

void LanguageSelectLayout::LoadFlags() {
    for (int i = 0; i < 2; ++i) {
        const std::string url = "https://flagcdn.com/w80/" + std::string(CODES[i]) + ".png";
        const auto data = spotify::downloadAlbumArt(url);
        if (data.empty()) continue;

        auto* rawTex = pu::ui::render::LoadImageFromBuffer(static_cast<const void*>(data.data()), data.size());
        if (!rawTex) continue;

        this->flagImg[i]->SetImage(pu::sdl2::TextureHandle::New(rawTex));
        this->flagImg[i]->SetWidth(FLAG_W);
        this->flagImg[i]->SetHeight(FLAG_H);
    }
}

void LanguageSelectLayout::UpdateSelection() {
    for (int i = 0; i < 2; ++i) {
        const bool selected = (i == this->selectedIndex);
        this->optionBg[i]->SetColor(selected ? CLR_GREEN : CLR_OPT_BG);
        this->optionText[i]->SetColor(selected ? CLR_WHITE : CLR_GRAY);
    }
}

void LanguageSelectLayout::OnInputCallback(const u64 keys_down) {
    if (keys_down & (HidNpadButton_Left | HidNpadButton_L)) {
        this->selectedIndex = 0;
        this->UpdateSelection();
    }
    if (keys_down & (HidNpadButton_Right | HidNpadButton_R)) {
        this->selectedIndex = 1;
        this->UpdateSelection();
    }
    if (keys_down & HidNpadButton_A) {
        this->onSelected(CODES[this->selectedIndex]);
    }
}
