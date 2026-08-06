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
//
// The actual Spotify HTTP calls run on the background worker thread (RunPollJob,
// dispatched via DispatchPollJob) so a slow/laggy connection never blocks rendering
// or input. ApplyPollResult applies the result on the main thread once it's ready —
// its branching mirrors exactly what this code used to do inline.

void MainApplication::FetchAndShowPlayerState() {
    // Skip this tick if a job (poll, skip, ...) is already in flight — the next
    // periodic tick will try again. Prevents piling up redundant poll jobs.
    if (this->jobsOutstanding.load() > 0) return;
    this->DispatchPollJob(JobKind::Poll);
}

void MainApplication::RunPollJob(const PollJob& job, PollResult& out) {
    spotify::Tokens tokens = job.tokens;

    if (job.kind == JobKind::SkipPrev) spotify::skipPrevious(tokens.accessToken);
    else if (job.kind == JobKind::SkipNext) spotify::skipNext(tokens.accessToken);

    if (time(nullptr) + 60 >= tokens.expiresAt) {
        debugLog("APP: refreshing access token");
        out.didPreemptiveRefresh = true;
        out.preemptiveRefreshResult = spotify::refreshAccessToken(tokens.refreshToken);
        tokens = out.preemptiveRefreshResult;
    }
    if (!tokens.valid) return;

    out.didGetPlayerState = true;
    out.playerState = spotify::getPlayerState(tokens.accessToken);

    if (out.playerState.tokenExpired) {
        debugLog("APP: 401 from /me/player — forcing token refresh");
        out.didExpiredRetryRefresh = true;
        out.expiredRetryRefreshResult = spotify::refreshAccessToken(tokens.refreshToken);
        if (out.expiredRetryRefreshResult.valid) {
            out.didExpiredRetryGetPlayerState = true;
            out.expiredRetryPlayerState = spotify::getPlayerState(out.expiredRetryRefreshResult.accessToken);
        }
        return;
    }

    if (!out.playerState.valid) return;
    const auto& player = out.playerState;

    // Download album art only when the art URL changes
    if (!player.albumImageUrl.empty() && player.albumImageUrl != job.currentAlbumUrl) {
        out.newAlbumImageUrl = player.albumImageUrl;
        out.albumArtBytes = spotify::downloadAlbumArt(player.albumImageUrl);
    }

    // Fetch album info only when the album ID changes (separate guard)
    if (!player.albumId.empty() && player.albumId != job.currentAlbumId) {
        out.newAlbumId = player.albumId;
        out.albumInfo = spotify::getAlbumInfo(player.albumId, tokens.accessToken);
    }

    // Fetch artist info only when the artist changes
    if (!player.artistId.empty() && player.artistId != job.currentArtistId) {
        out.newArtistId = player.artistId;
        out.artistInfo = spotify::getArtistInfo(player.artistId, tokens.accessToken);
        if (out.artistInfo.valid && !out.artistInfo.imageUrl.empty()) {
            out.artistImgBytes = spotify::downloadAlbumArt(out.artistInfo.imageUrl);
        }
    }

    // Fetch queue every cycle (changes with each track skip)
    out.queueInfo = spotify::getQueue(tokens.accessToken);
    if (out.queueInfo.valid) {
        for (int i = 0; i < out.queueInfo.trackCount; ++i) {
            const auto& url = out.queueInfo.tracks[i].imageUrl;
            if (!url.empty() && url != job.currentQueueUrls[i]) {
                out.newQueueUrl[i] = url;
                out.queueImgBytes[i] = spotify::downloadAlbumArt(url);
            }
        }
    }
}

void MainApplication::RunPlayPauseJob(const PollJob& job, PollResult&) {
    if (job.playAction) spotify::play(job.tokens.accessToken);
    else spotify::pause(job.tokens.accessToken);
}

void MainApplication::ApplyPollResult(const PollResult& result) {
    if (result.didPreemptiveRefresh) {
        this->currentTokens = result.preemptiveRefreshResult;
        if (this->currentTokens.valid) TokenStorage::saveTokens(this->currentTokens);
    }
    if (!this->currentTokens.valid) {
        this->mainLayout->SetStatus(lang::get("player.token_refresh_error"));
        this->actionsBlocked = false;
        this->mainLayout->SetLoadingSpinner(false);
        return;
    }

    if (!result.didGetPlayerState) return; // tokens went invalid after the job was dispatched

    if (result.playerState.tokenExpired) {
        debugLog("APP: 401 from /me/player — forcing token refresh");
        if (result.didExpiredRetryRefresh && result.expiredRetryRefreshResult.valid) {
            this->currentTokens = result.expiredRetryRefreshResult;
            TokenStorage::saveTokens(this->currentTokens);
            const auto& retried = result.expiredRetryPlayerState;
            const bool retriedValid = result.didExpiredRetryGetPlayerState && retried.valid;
            this->mainLayout->SetPlaybackActive(retriedValid);
            if (!retriedValid) {
                this->isPlaying = false;
                this->actionsBlocked = false;
                this->mainLayout->SetLoadingSpinner(false);
                return;
            }
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

    const auto& player = result.playerState;
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

    if (!result.newAlbumImageUrl.empty()) {
        this->currentAlbumUrl = result.newAlbumImageUrl;
        if (!result.albumArtBytes.empty()) {
            auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                static_cast<const void*>(result.albumArtBytes.data()), result.albumArtBytes.size());
            if (rawTex) {
                auto handle = pu::sdl2::TextureHandle::New(rawTex);
                this->mainLayout->SetAlbumArt(handle);
                this->mainLayout->SetAlbumThumbnail(handle);
            }
        }
    }

    if (!result.newAlbumId.empty()) {
        this->currentAlbumId = result.newAlbumId;
        if (result.albumInfo.valid)
            this->mainLayout->SetAlbumInfo(result.albumInfo);
    }

    if (!result.newArtistId.empty()) {
        this->currentArtistId = result.newArtistId;
        if (result.artistInfo.valid) {
            this->mainLayout->SetArtistInfo(result.artistInfo);
            if (!result.artistImgBytes.empty()) {
                auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                    static_cast<const void*>(result.artistImgBytes.data()), result.artistImgBytes.size());
                if (rawTex)
                    this->mainLayout->SetArtistImage(pu::sdl2::TextureHandle::New(rawTex));
            }
        }
    }

    if (result.queueInfo.valid) {
        this->mainLayout->SetQueueInfo(result.queueInfo);
        for (int i = 0; i < result.queueInfo.trackCount; ++i) {
            if (result.newQueueUrl[i].empty()) continue;
            this->currentQueueUrls[i] = result.newQueueUrl[i];
            if (!result.queueImgBytes[i].empty()) {
                auto* rawTex = pu::ui::render::LoadImageFromBuffer(
                    static_cast<const void*>(result.queueImgBytes[i].data()), result.queueImgBytes[i].size());
                if (rawTex)
                    this->mainLayout->SetQueueImage(i, pu::sdl2::TextureHandle::New(rawTex));
            }
        }
    }

    if (!this->actionsBlocked || trackChanged) {
        this->actionsBlocked = false;
        this->mainLayout->SetLoadingSpinner(false);
    }
}

void MainApplication::OnPlayPause() {
    PollJob job;
    job.kind = JobKind::PlayPause;
    job.tokens = this->currentTokens;
    job.playAction = !this->isPlaying;
    this->EnqueueJob(std::move(job));
    // Optimistic UI update — periodic refresh will confirm the real state
    this->isPlaying = !this->isPlaying;
    this->mainLayout->UpdatePlayButton(this->isPlaying);
}

void MainApplication::OnPrev() {
    this->blockedFromTrackName = this->currentTrackName;
    this->actionsBlocked = true;
    this->mainLayout->SetLoadingSpinner(true);
    this->DispatchPollJob(JobKind::SkipPrev);
}

void MainApplication::OnNext() {
    this->blockedFromTrackName = this->currentTrackName;
    this->actionsBlocked = true;
    this->mainLayout->SetLoadingSpinner(true);
    this->DispatchPollJob(JobKind::SkipNext);
}
