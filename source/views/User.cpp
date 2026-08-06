#include <MainApplication.hpp>
#include <LayoutConstants.hpp>
#include <SpotifyAuth.hpp>
#include <DebugLog.hpp>
#include <Lang.hpp>
#include <cstdio>

static std::string capitalizeFirst(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(toupper(static_cast<unsigned char>(out[0])));
    return out;
}

static std::string formatFollowers(long n) {
    char buf[32];
    if (n >= 1000000)
        snprintf(buf, sizeof(buf), lang::get("player.followers_millions_format").c_str(), (float)n / 1000000.0f);
    else if (n >= 1000)
        snprintf(buf, sizeof(buf), lang::get("player.followers_thousands_format").c_str(), (float)n / 1000.0f);
    else
        snprintf(buf, sizeof(buf), lang::get("player.followers_count_format").c_str(), n);
    return buf;
}

// --- MainLayout user content setters ---

void MainLayout::SetUserProfile(const spotify::UserProfile& profile) {
    if (!profile.valid) return;
    this->userNameText->SetText(profile.displayName);
    // Scale flag to match name text height (3:2 aspect ratio covers most flags)
    const s32 nameH = this->userNameText->GetHeight();
    this->userFlagImg->SetWidth(nameH * 3 / 2);
    this->userFlagImg->SetHeight(nameH);
    this->userFlagImg->SetX(UINFO_X + this->userNameText->GetWidth() + 12);
    this->userFlagImg->SetY(UNAME_Y);
    this->userEmailText->SetText(profile.email.empty() ? "" : profile.email);
    if (profile.product.empty()) {
        this->userPlanText->SetText("");
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), lang::get("user.plan_format").c_str(), capitalizeFirst(profile.product).c_str());
        this->userPlanText->SetText(buf);
    }
    this->userFollowersText->SetText(formatFollowers(profile.followers));
}

void MainLayout::SetUserFlag(pu::sdl2::TextureHandle::Ref handle) {
    this->userFlagImg->SetImage(handle);
    if (this->currentTab == Tab::User)
        this->userFlagImg->SetVisible(true);
}

void MainLayout::SetUserAvatar(pu::sdl2::TextureHandle::Ref handle) {
    this->userAvatarImg->SetImage(handle);
    this->userAvatarImg->SetWidth(UAVATAR_SIZE);
    this->userAvatarImg->SetHeight(UAVATAR_SIZE);
}

// --- MainApplication user methods ---
//
// Runs on the background worker thread — network only, no pu::ui/SDL calls (those
// happen in ApplyUserProfileResult, back on the main thread).
void MainApplication::RunUserProfileJob(const PollJob& job, PollResult& out) {
    debugLog("USER: fetching user profile");
    out.userProfile = spotify::getUserProfile(job.tokens.accessToken);
    if (!out.userProfile.valid) {
        debugLog("USER: getUserProfile returned invalid");
        return;
    }
    debugLogf("USER: profile ok — name=%s email=%s country=%s product=%s followers=%ld imageUrl=%s",
        out.userProfile.displayName.c_str(),
        out.userProfile.email.c_str(),
        out.userProfile.country.c_str(),
        out.userProfile.product.c_str(),
        out.userProfile.followers,
        out.userProfile.imageUrl.c_str());

    if (out.userProfile.imageUrl.empty()) {
        debugLog("USER: no avatar URL — skipping avatar download");
    } else {
        debugLogf("USER: downloading avatar from %s", out.userProfile.imageUrl.c_str());
        out.userAvatarBytes = spotify::downloadAlbumArt(out.userProfile.imageUrl);
        debugLog(out.userAvatarBytes.empty() ? "USER: avatar download failed (empty data)" : "USER: avatar download ok");
    }

    if (out.userProfile.country.empty()) {
        debugLog("USER: no country — skipping flag download");
    } else {
        std::string cc = out.userProfile.country;
        for (char& c : cc) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        const std::string flagUrl = "https://flagcdn.com/w80/" + cc + ".png";
        debugLogf("USER: downloading flag from %s", flagUrl.c_str());
        out.userFlagBytes = spotify::downloadAlbumArt(flagUrl);
        debugLog(out.userFlagBytes.empty() ? "USER: flag download failed (empty data)" : "USER: flag download ok");
    }
}

void MainApplication::ApplyUserProfileResult(const PollResult& result) {
    this->userProfileJobInFlight = false;
    if (!result.userProfile.valid) return;

    this->userProfileFetched = true;
    this->mainLayout->SetUserProfile(result.userProfile);

    if (!result.userAvatarBytes.empty()) {
        auto* rawTex = pu::ui::render::LoadImageFromBuffer(
            static_cast<const void*>(result.userAvatarBytes.data()), result.userAvatarBytes.size());
        if (rawTex) {
            this->mainLayout->SetUserAvatar(pu::sdl2::TextureHandle::New(rawTex));
            debugLog("USER: avatar texture loaded");
        } else {
            debugLog("USER: LoadImageFromBuffer failed for avatar");
        }
    }

    if (!result.userFlagBytes.empty()) {
        auto* rawTex = pu::ui::render::LoadImageFromBuffer(
            static_cast<const void*>(result.userFlagBytes.data()), result.userFlagBytes.size());
        if (rawTex) {
            this->mainLayout->SetUserFlag(pu::sdl2::TextureHandle::New(rawTex));
            debugLog("USER: flag texture loaded");
        } else {
            debugLog("USER: LoadImageFromBuffer failed for flag");
        }
    }
}

void MainApplication::FetchUserProfile() {
    if (this->userProfileFetched || this->userProfileJobInFlight) return;
    this->userProfileJobInFlight = true;
    PollJob job;
    job.kind = JobKind::UserProfile;
    job.tokens = this->currentTokens;
    this->EnqueueJob(std::move(job));
}
