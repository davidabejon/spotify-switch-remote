#pragma once
#include <pu/Plutonium>
#include <SpotifyAuth.hpp>
#include <LocalServer.hpp>
#include <memory>
#include <functional>
#include <ctime>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

enum class Tab      { Player, User, Settings };
enum class RightTab  { Artist, Queue };
enum class PlayerFocus { Prev, PlayPause, Next };
enum class SettingsFocus { Language, Apply, Logout };

class MainLayout : public pu::ui::Layout {
private:
    // Sidebar
    pu::ui::elm::Rectangle::Ref sidebarBg;
    pu::ui::elm::Image::Ref     sidebarLogoImg;
    pu::ui::elm::TextBlock::Ref sidebarTitle;
    pu::ui::elm::Rectangle::Ref tabIndicator;
    pu::ui::elm::Rectangle::Ref tab1Bg;
    pu::ui::elm::TextBlock::Ref tab1Text;
    pu::ui::elm::Rectangle::Ref tab2Bg;
    pu::ui::elm::TextBlock::Ref tab2Text;
    pu::ui::elm::Rectangle::Ref tab3Bg;
    pu::ui::elm::TextBlock::Ref tab3Text;
    pu::ui::elm::Image::Ref     lShoulderIcon;
    pu::ui::elm::Image::Ref     rShoulderIcon;
    pu::ui::elm::TextBlock::Ref statusText;
    pu::ui::elm::TextBlock::Ref deviceText;

    // Player tab
    pu::ui::elm::Rectangle::Ref albumArtBg;
    pu::ui::elm::Image::Ref albumArtImage;
    pu::ui::elm::TextBlock::Ref trackText;
    pu::ui::elm::TextBlock::Ref artistText;
    pu::ui::elm::Rectangle::Ref prevBtnBg;
    pu::ui::elm::Image::Ref     prevBtnImg;
    pu::ui::elm::Rectangle::Ref playBtnBg;
    pu::ui::elm::Image::Ref     playBtnImg;
    pu::ui::elm::Image::Ref     pauseBtnImg;
    pu::ui::elm::Rectangle::Ref nextBtnBg;
    pu::ui::elm::Image::Ref     nextBtnImg;
    pu::ui::elm::Rectangle::Ref prevBtnOutline;
    pu::ui::elm::Rectangle::Ref playBtnOutline;
    pu::ui::elm::Rectangle::Ref nextBtnOutline;
    bool isPlayingState = false;

    // User tab
    pu::ui::elm::Rectangle::Ref userAvatarBg;
    pu::ui::elm::Image::Ref     userAvatarImg;
    pu::ui::elm::TextBlock::Ref userNameText;
    pu::ui::elm::Image::Ref     userFlagImg;
    pu::ui::elm::TextBlock::Ref userEmailText;
    pu::ui::elm::TextBlock::Ref userPlanText;
    pu::ui::elm::TextBlock::Ref userFollowersText;

    // Settings tab
    pu::ui::elm::TextBlock::Ref settingsTitleText;
    pu::ui::elm::TextBlock::Ref settingsLanguageLabel;
    pu::ui::elm::Rectangle::Ref settingsSelectOutline;
    pu::ui::elm::Rectangle::Ref settingsSelectBg;
    pu::ui::elm::TextBlock::Ref settingsSelectText;
    pu::ui::elm::Rectangle::Ref settingsApplyOutline;
    pu::ui::elm::Rectangle::Ref settingsApplyBg;
    pu::ui::elm::TextBlock::Ref settingsApplyText;
    pu::ui::elm::Rectangle::Ref settingsLogoutOutline;
    pu::ui::elm::Rectangle::Ref settingsLogoutBg;
    pu::ui::elm::TextBlock::Ref settingsLogoutText;
    pu::ui::elm::TextBlock::Ref settingsHelpText;
    pu::ui::elm::Image::Ref     settingsHelpLeftIcon;
    pu::ui::elm::Image::Ref     settingsHelpRightIcon;
    pu::ui::elm::TextBlock::Ref settingsSavedText;
    pu::ui::elm::TextBlock::Ref settingsAppInfoText;
    pu::ui::elm::TextBlock::Ref settingsAttributionText;
    int settingsLangIndex = 0;
    PlayerFocus playerFocus = PlayerFocus::PlayPause;
    SettingsFocus settingsFocus = SettingsFocus::Language;

    // Right panel
    pu::ui::elm::Rectangle::Ref rightVertSep;
    pu::ui::elm::Rectangle::Ref rightRightBorder;
    pu::ui::elm::Rectangle::Ref rightBottomBorder;
    pu::ui::elm::Rectangle::Ref rightTab1Bg;
    pu::ui::elm::TextBlock::Ref rightTab1Text;
    pu::ui::elm::Rectangle::Ref rightTab2Bg;
    pu::ui::elm::TextBlock::Ref rightTab2Text;
    pu::ui::elm::Rectangle::Ref rightTabIndicator;
    pu::ui::elm::Rectangle::Ref rightHorizSep;
    pu::ui::elm::Image::Ref     zlShoulderIcon;
    pu::ui::elm::Image::Ref     zrShoulderIcon;

    // Artist tab content
    pu::ui::elm::Rectangle::Ref rightArtistImgBg;
    pu::ui::elm::Image::Ref     rightArtistImg;
    pu::ui::elm::TextBlock::Ref rightArtistName;
    pu::ui::elm::TextBlock::Ref rightArtistGenres;
    pu::ui::elm::TextBlock::Ref rightArtistFollowers;
    pu::ui::elm::TextBlock::Ref rightArtistPopularity;

    // Album section (inside Artist tab, below artist image)
    pu::ui::elm::Rectangle::Ref rightAlbumSep;
    pu::ui::elm::Rectangle::Ref rightAlbumImgBg;
    pu::ui::elm::Image::Ref     rightAlbumImg;
    pu::ui::elm::TextBlock::Ref rightAlbumHeader;
    pu::ui::elm::TextBlock::Ref rightAlbumName;
    pu::ui::elm::TextBlock::Ref rightAlbumTypeYear;
    pu::ui::elm::TextBlock::Ref rightAlbumTracks;

    // Queue tab content (5 cards: [0]=currently playing green bg, [1..4]=next)
    pu::ui::elm::Rectangle::Ref queueCardBg[5];
    pu::ui::elm::Rectangle::Ref queueCardImgBg[5];
    pu::ui::elm::Image::Ref     queueCardImg[5];
    pu::ui::elm::TextBlock::Ref queueCardTitle[5];
    pu::ui::elm::TextBlock::Ref queueCardArtist[5];

    // No-playback overlay (shown instead of player content)
    pu::ui::elm::TextBlock::Ref noPlaybackText;

    // Audio playback bars (first queue card, animated when queue tab is visible)
    pu::ui::elm::Rectangle::Ref bars[3];
    float barPhase = 0.0f;
    bool barsVisible = false;

    // Loading spinner (shown while waiting for next polling after a skip)
    pu::ui::elm::Rectangle::Ref spinnerBackdrop;
    pu::ui::elm::Image::Ref spinnerImg;
    float spinnerAngle = 0.0f;
    bool spinnerVisible = false;

    // Full-screen blocking loading overlay (used for language apply and other blocking flows)
    pu::ui::elm::Rectangle::Ref blockingOverlayBg;
    pu::ui::elm::Image::Ref blockingOverlaySpinner;
    bool blockingOverlayVisible = false;

    // State
    Tab currentTab;
    RightTab currentRightTab;
    bool playbackActive = true;
    std::function<void()> refreshCallback;
    time_t lastRefresh = 0;

    // Whether controller button hints (L/R, ZL/ZR, D-Pad icons) are currently shown;
    // hidden while the last interaction was touch, shown again once a controller input is used.
    bool controllerHintsEnabled = true;

    // Touch hover state for tappable buttons (mirrors pu::ui::elm::Button's press/release tracking)
    bool prevTapHovering = false;
    bool playPauseTapHovering = false;
    bool nextTapHovering = false;
    bool tab1TapHovering = false;
    bool tab2TapHovering = false;
    bool tab3TapHovering = false;
    bool rightTab1TapHovering = false;
    bool rightTab2TapHovering = false;
    bool settingsSelectTapHovering = false;
    bool settingsApplyTapHovering = false;
    bool settingsLogoutTapHovering = false;

    void OnRenderCallback();
    void SetPlayerTabVisible(bool visible);
    void SetUserTabVisible(bool visible);
    void SetSettingsTabVisible(bool visible);
    void SetRightPanelVisible(bool visible);
    void UpdateSettingsSelectText();
    void UpdatePlayerFocusStyles();
    void UpdateSettingsFocusStyles();
    static bool UpdateTapZone(bool& hovering, const pu::ui::TouchPoint& touch, s32 x, s32 y, s32 w, s32 h);

public:
    MainLayout();
    PU_SMART_CTOR(MainLayout)

    void SetStatus(const std::string& text);
    void SetTrack(const std::string& trackName, const std::string& artistName, bool isPlaying);
    void SetDevice(const std::string& deviceName);
    void SetAlbumArt(pu::sdl2::TextureHandle::Ref handle);
    void UpdatePlayButton(bool isPlaying);
    void SetArtistInfo(const spotify::ArtistInfo& info);
    void SetArtistImage(pu::sdl2::TextureHandle::Ref handle);
    void SetAlbumInfo(const spotify::AlbumInfo& info);
    void SetAlbumThumbnail(pu::sdl2::TextureHandle::Ref handle);
    void SetPlaybackActive(bool active);
    void SetUserProfile(const spotify::UserProfile& profile);
    void SetUserFlag(pu::sdl2::TextureHandle::Ref handle);
    void SetUserAvatar(pu::sdl2::TextureHandle::Ref handle);
    void SetQueueInfo(const spotify::QueueInfo& info);
    void SetQueueImage(int index, pu::sdl2::TextureHandle::Ref handle);
    void SwitchToTab(Tab tab);
    Tab GetCurrentTab() const { return this->currentTab; }
    bool GetPlaybackActive() const { return this->playbackActive; }
    void MovePlayerFocus(int delta);
    PlayerFocus GetPlayerFocus() const { return this->playerFocus; }
    void SetPlayerFocus(PlayerFocus focus);
    void CycleSettingsLanguage(int delta);
    void MoveSettingsFocus(int delta);
    SettingsFocus GetSettingsFocus() const { return this->settingsFocus; }
    void SetSettingsFocus(SettingsFocus focus);
    std::string GetSelectedLanguageCode() const;
    void SetSettingsFeedback(const std::string& text);
    void SwitchRightTab(RightTab tab);
    RightTab GetCurrentRightTab() const { return this->currentRightTab; }
    void SetRefreshCallback(std::function<void()> fn);
    void TriggerRefreshNow();
    void SetLoadingSpinner(bool visible);
    void SetBlockingLoading(bool visible);
    void SetControllerHintsVisible(bool visible);

    // Touch tap detection — each returns true once, on the frame the touch is released
    // after having been pressed down inside that button's bounds.
    bool TapPrev(const pu::ui::TouchPoint& touch);
    bool TapPlayPause(const pu::ui::TouchPoint& touch);
    bool TapNext(const pu::ui::TouchPoint& touch);
    bool TapSidebarTab(const pu::ui::TouchPoint& touch, Tab tab);
    bool TapRightTab(const pu::ui::TouchPoint& touch, RightTab tab);
    bool TapSettingsSelect(const pu::ui::TouchPoint& touch);
    bool TapSettingsApply(const pu::ui::TouchPoint& touch);
    bool TapSettingsLogout(const pu::ui::TouchPoint& touch);
};

class MainApplication : public pu::ui::Application {
private:
    // ---- Background networking (all Spotify HTTP calls run off the main/render thread;
    // see WorkerLoop) ----
    enum class JobKind { Poll, SkipPrev, SkipNext, PlayPause, UserProfile };

    // Inputs snapshotted on the main thread when a job is dispatched, so the worker
    // thread never touches MainApplication's mutable state directly.
    struct PollJob {
        JobKind kind = JobKind::Poll;
        u64 generation = 0;
        spotify::Tokens tokens;
        std::string currentAlbumUrl;
        std::string currentAlbumId;
        std::string currentArtistId;
        std::string currentQueueUrls[5];
        bool playAction = false; // JobKind::PlayPause: true = call play(), false = call pause()
    };

    // Outputs produced entirely from network data on the worker thread — no pu::ui/SDL
    // calls here, since those aren't safe off the main thread. The main thread applies
    // these via Apply*Result().
    struct PollResult {
        JobKind kind = JobKind::Poll;
        u64 generation = 0;

        bool didPreemptiveRefresh = false;
        spotify::Tokens preemptiveRefreshResult;

        bool didGetPlayerState = false;
        spotify::PlayerState playerState;

        bool didExpiredRetryRefresh = false;
        spotify::Tokens expiredRetryRefreshResult;
        bool didExpiredRetryGetPlayerState = false;
        spotify::PlayerState expiredRetryPlayerState;

        std::string newAlbumImageUrl; // non-empty only when it changed vs. the job's snapshot
        std::string albumArtBytes;

        std::string newAlbumId;
        spotify::AlbumInfo albumInfo;

        std::string newArtistId;
        spotify::ArtistInfo artistInfo;
        std::string artistImgBytes;

        spotify::QueueInfo queueInfo;
        std::string newQueueUrl[5];
        std::string queueImgBytes[5];

        spotify::UserProfile userProfile;
        std::string userAvatarBytes;
        std::string userFlagBytes;
    };

    std::thread workerThread;
    std::mutex jobMutex;
    std::condition_variable jobCv;
    std::queue<PollJob> jobQueue;
    bool workerStop = false;
    std::atomic<int> jobsOutstanding{0};

    std::mutex resultMutex;
    std::queue<PollResult> resultQueue;

    // Bumped whenever the session/layout is torn down and rebuilt (login, logout,
    // language change) so results from a since-invalidated job are discarded instead
    // of being applied onto unrelated state.
    u64 currentGeneration = 0;
    bool userProfileJobInFlight = false;

    // Set to the number of results still awaited (e.g. 2 for user-profile + poll)
    // whenever the blocking overlay must stay up until specific async jobs land;
    // ApplyPendingResults hides the overlay once this reaches zero.
    int pendingBlockingLoadingJobs = 0;

    void WorkerLoop();
    void RunJob(const PollJob& job, PollResult& out);
    void RunPollJob(const PollJob& job, PollResult& out);
    void RunPlayPauseJob(const PollJob& job, PollResult& out);
    void RunUserProfileJob(const PollJob& job, PollResult& out);
    void EnqueueJob(PollJob job);
    void DispatchPollJob(JobKind kind);
    void ApplyPendingResults();
    void ApplyPollResult(const PollResult& result);
    void ApplyUserProfileResult(const PollResult& result);

    MainLayout::Ref mainLayout;
    pu::ui::Layout::Ref languageLayout;
    pu::ui::Layout::Ref loginLayout;
    std::unique_ptr<LocalServer> localServer;
    spotify::Tokens currentTokens;
    bool mainLayoutActive = false;
    bool isPlaying = false;
    bool actionsBlocked = false;
    std::string currentTrackName;
    std::string blockedFromTrackName;
    std::string currentAlbumUrl;
    std::string currentAlbumId;
    std::string currentArtistId;
    bool userProfileFetched = false;
    std::string currentQueueUrls[5];
    bool pendingInitialMainFetch = false;
    time_t pendingInitialMainFetchAfter = 0;
    int dirUpHoldFrames = 0;
    int dirDownHoldFrames = 0;
    int dirLeftHoldFrames = 0;
    int dirRightHoldFrames = 0;

    void FetchAndShowPlayerState();
    void FetchUserProfile();
    void OnPlayPause();
    void OnPrev();
    void OnNext();
    void OnLanguageSelected(const std::string& code);
    void ApplyLanguageFromSettings();
    bool StartLoginFlow();
    void OnLogout();
    void OnLoginBack();
    void ActivateMainLayout(bool showSettingsTab, bool showBlockingLoading, bool deferInitialFetch);
    void ResetMainLayoutCaches();

public:
    using Application::Application;
    PU_SMART_CTOR(MainApplication)
    ~MainApplication() override;

    void OnLoad() override;
    void OnLoginSuccess(const spotify::Tokens& tokens);
};
