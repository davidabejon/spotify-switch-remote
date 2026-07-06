#include <MainApplication.hpp>
#include <LayoutConstants.hpp>
#include <SpotifyAuth.hpp>
#include <TokenStorage.hpp>
#include <DebugLog.hpp>
#include <Lang.hpp>
#include <cstdio>
#include <cmath>
#include <ctime>

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

static std::string formatAlbumType(const std::string& t) {
    if (t == "album")       return lang::get("player.album_type_album");
    if (t == "single")      return lang::get("player.album_type_single");
    if (t == "compilation") return lang::get("player.album_type_compilation");
    return t;
}

// --- MainLayout player content setters ---

void MainLayout::UpdatePlayButton(bool isPlaying) {
    this->isPlayingState = isPlaying;
    const bool showContent = (this->currentTab == Tab::Player) && this->playbackActive;
    this->playBtnImg->SetVisible(showContent && !isPlaying);
    this->pauseBtnImg->SetVisible(showContent && isPlaying);
}

void MainLayout::SetTrack(const std::string& trackName, const std::string& artistName, bool isPlaying) {
    this->trackText->SetText(trackName.empty() ? lang::get("player.no_track") : trackName);
    this->artistText->SetText(artistName);
    this->UpdatePlayButton(isPlaying);
}

void MainLayout::SetAlbumArt(pu::sdl2::TextureHandle::Ref handle) {
    this->albumArtImage->SetImage(handle);
    // SetImage resets render dimensions to the texture's natural size — re-apply explicitly.
    this->albumArtImage->SetWidth(ART_SIZE);
    this->albumArtImage->SetHeight(ART_SIZE);
}

void MainLayout::SetPlaybackActive(bool active) {
    this->playbackActive = active;
    if (this->currentTab == Tab::Player)
        this->SetPlayerTabVisible(true);
    // Right panel only makes sense when there is active playback
    if (this->currentTab == Tab::Player)
        this->SetRightPanelVisible(active);
}

void MainLayout::SetArtistInfo(const spotify::ArtistInfo& info) {
    if (!info.valid) return;
    this->rightArtistName->SetText(info.name);
    this->rightArtistGenres->SetText(info.genres);
    this->rightArtistFollowers->SetText(formatFollowers(info.followers));
    char popBuf[32];
    snprintf(popBuf, sizeof(popBuf), lang::get("player.popularity_format").c_str(), info.popularity);
    this->rightArtistPopularity->SetText(popBuf);
}

void MainLayout::SetArtistImage(pu::sdl2::TextureHandle::Ref handle) {
    this->rightArtistImg->SetImage(handle);
    this->rightArtistImg->SetWidth(RART_IMG_SIZE);
    this->rightArtistImg->SetHeight(RART_IMG_SIZE);
}

void MainLayout::SetAlbumThumbnail(pu::sdl2::TextureHandle::Ref handle) {
    this->rightAlbumImg->SetImage(handle);
    this->rightAlbumImg->SetWidth(RALBUM_IMG_SIZE);
    this->rightAlbumImg->SetHeight(RALBUM_IMG_SIZE);
}

void MainLayout::SetAlbumInfo(const spotify::AlbumInfo& info) {
    if (!info.valid) return;
    this->rightAlbumName->SetText(info.name);
    const auto year = info.releaseDate.size() >= 4 ? info.releaseDate.substr(0, 4) : info.releaseDate;
    char yearBuf[192];
    snprintf(yearBuf, sizeof(yearBuf), lang::get("player.album_type_year_format").c_str(),
        formatAlbumType(info.albumType).c_str(), year.c_str());
    this->rightAlbumTypeYear->SetText(yearBuf);
    char buf[128];
    if (info.label.empty())
        snprintf(buf, sizeof(buf), lang::get("player.tracks_count_format").c_str(), info.totalTracks);
    else
        snprintf(buf, sizeof(buf), lang::get("player.tracks_count_with_label_format").c_str(), info.totalTracks, info.label.c_str());
    this->rightAlbumTracks->SetText(buf);
}

void MainLayout::SetQueueInfo(const spotify::QueueInfo& info) {
    for (int i = 0; i < 5; ++i) {
        if (i < info.trackCount) {
            this->queueCardTitle[i]->SetText(info.tracks[i].name);
            this->queueCardArtist[i]->SetText(info.tracks[i].artistName);
        } else {
            this->queueCardTitle[i]->SetText("");
            this->queueCardArtist[i]->SetText("");
        }
    }
}

void MainLayout::SetQueueImage(int index, pu::sdl2::TextureHandle::Ref handle) {
    if (index < 0 || index >= 5) return;
    this->queueCardImg[index]->SetImage(handle);
    this->queueCardImg[index]->SetWidth(RQUEUE_IMG_SIZE);
    this->queueCardImg[index]->SetHeight(RQUEUE_IMG_SIZE);
}

// --- MainApplication player methods ---

void MainApplication::FetchAndShowPlayerState() {
    if (time(nullptr) + 60 >= this->currentTokens.expiresAt) {
        debugLog("APP: refreshing access token");
        this->currentTokens = spotify::refreshAccessToken(this->currentTokens.refreshToken);
        if (this->currentTokens.valid) TokenStorage::saveTokens(this->currentTokens);
    }
    if (!this->currentTokens.valid) {
        this->mainLayout->SetStatus(lang::get("player.token_refresh_error"));
        this->actionsBlocked = false;
        this->mainLayout->SetLoadingSpinner(false);
        return;
    }

    const auto player = spotify::getPlayerState(this->currentTokens.accessToken);

    if (player.tokenExpired) {
        debugLog("APP: 401 from /me/player — forcing token refresh");
        this->currentTokens = spotify::refreshAccessToken(this->currentTokens.refreshToken);
        if (this->currentTokens.valid) {
            TokenStorage::saveTokens(this->currentTokens);
            // Retry once with the new token
            const auto retried = spotify::getPlayerState(this->currentTokens.accessToken);
            this->mainLayout->SetPlaybackActive(retried.valid);
            if (!retried.valid) { this->isPlaying = false; this->actionsBlocked = false; this->mainLayout->SetLoadingSpinner(false); return; }
            this->isPlaying = retried.isPlaying;
            this->currentTrackName = retried.trackName;
            this->mainLayout->SetTrack(retried.trackName, retried.artistName, retried.isPlaying);
            this->mainLayout->SetDevice(retried.deviceName);
        } else {
            debugLog("APP: refresh failed — forcing re-login");
            this->currentTokens = spotify::Tokens();
            TokenStorage::saveTokens(this->currentTokens);
            this->mainLayout->SetStatus(lang::get("player.session_expired"));
        }
        if (!this->actionsBlocked || this->currentTrackName != this->blockedFromTrackName) {
            this->actionsBlocked = false;
            this->mainLayout->SetLoadingSpinner(false);
        }
        return;
    }

    this->mainLayout->SetPlaybackActive(player.valid);

    if (!player.valid) {
        this->isPlaying = false;
        this->actionsBlocked = false;
        this->mainLayout->SetLoadingSpinner(false);
        return;
    }

    this->isPlaying = player.isPlaying;
    const bool trackChanged = (player.trackName != this->blockedFromTrackName);
    this->currentTrackName = player.trackName;
    this->mainLayout->SetTrack(player.trackName, player.artistName, player.isPlaying);
    this->mainLayout->SetDevice(player.deviceName);

    // Download album art only when the art URL changes
    if (!player.albumImageUrl.empty() && player.albumImageUrl != this->currentAlbumUrl) {
        this->currentAlbumUrl = player.albumImageUrl;
        const auto artData = spotify::downloadAlbumArt(player.albumImageUrl);
        if (!artData.empty()) {
            auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                static_cast<const void*>(artData.data()), artData.size());
            if (rawTex) {
                auto handle = pu::sdl2::TextureHandle::New(rawTex);
                this->mainLayout->SetAlbumArt(handle);
                this->mainLayout->SetAlbumThumbnail(handle);
            }
        }
    }

    // Fetch album info only when the album ID changes (separate guard)
    if (!player.albumId.empty() && player.albumId != this->currentAlbumId) {
        this->currentAlbumId = player.albumId;
        const auto albumInfo = spotify::getAlbumInfo(player.albumId, this->currentTokens.accessToken);
        if (albumInfo.valid)
            this->mainLayout->SetAlbumInfo(albumInfo);
    }

    // Fetch artist info only when the artist changes
    if (!player.artistId.empty() && player.artistId != this->currentArtistId) {
        this->currentArtistId = player.artistId;
        const auto info = spotify::getArtistInfo(player.artistId, this->currentTokens.accessToken);
        if (info.valid) {
            this->mainLayout->SetArtistInfo(info);
            if (!info.imageUrl.empty()) {
                const auto imgData = spotify::downloadAlbumArt(info.imageUrl);
                if (!imgData.empty()) {
                    auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                        static_cast<const void*>(imgData.data()), imgData.size());
                    if (rawTex)
                        this->mainLayout->SetArtistImage(pu::sdl2::TextureHandle::New(rawTex));
                }
            }
        }
    }

    // Fetch queue every cycle (changes with each track skip)
    const auto queueInfo = spotify::getQueue(this->currentTokens.accessToken);
    if (queueInfo.valid) {
        this->mainLayout->SetQueueInfo(queueInfo);
        for (int i = 0; i < queueInfo.trackCount; ++i) {
            const auto& url = queueInfo.tracks[i].imageUrl;
            if (!url.empty() && url != this->currentQueueUrls[i]) {
                this->currentQueueUrls[i] = url;
                const auto imgData = spotify::downloadAlbumArt(url);
                if (!imgData.empty()) {
                    auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                        static_cast<const void*>(imgData.data()), imgData.size());
                    if (rawTex)
                        this->mainLayout->SetQueueImage(i, pu::sdl2::TextureHandle::New(rawTex));
                }
            }
        }
    }
    if (!this->actionsBlocked || trackChanged) {
        this->actionsBlocked = false;
        this->mainLayout->SetLoadingSpinner(false);
    }
}

void MainApplication::OnPlayPause() {
    if (this->isPlaying) {
        spotify::pause(this->currentTokens.accessToken);
    } else {
        spotify::play(this->currentTokens.accessToken);
    }
    // Optimistic UI update — periodic refresh will confirm the real state
    this->isPlaying = !this->isPlaying;
    this->mainLayout->UpdatePlayButton(this->isPlaying);
}

void MainApplication::OnPrev() {
    this->blockedFromTrackName = this->currentTrackName;
    this->actionsBlocked = true;
    this->mainLayout->SetLoadingSpinner(true);
    spotify::skipPrevious(this->currentTokens.accessToken);
    // Brief wait then refresh so the new track info appears quickly
    this->mainLayout->SetRefreshCallback(nullptr); // prevent double-refresh race
    this->FetchAndShowPlayerState();
    this->mainLayout->SetRefreshCallback([this]() { this->FetchAndShowPlayerState(); });
}

void MainApplication::OnNext() {
    this->blockedFromTrackName = this->currentTrackName;
    this->actionsBlocked = true;
    this->mainLayout->SetLoadingSpinner(true);
    spotify::skipNext(this->currentTokens.accessToken);
    this->mainLayout->SetRefreshCallback(nullptr);
    this->FetchAndShowPlayerState();
    this->mainLayout->SetRefreshCallback([this]() { this->FetchAndShowPlayerState(); });
}
