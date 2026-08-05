#pragma once
#include <pu/Plutonium>
#include <SpotifyAuth.hpp>
#include <LocalServer.hpp>
#include <memory>
#include <functional>
#include <ctime>
#include <string>

enum class Tab      { Player, User, Settings };
enum class RightTab  { Artist, Queue };
enum class PlayerFocus { Prev, PlayPause, Next };
enum class SettingsFocus { Language, Apply };

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
    pu::ui::elm::TextBlock::Ref settingsHelpText;
    pu::ui::elm::Image::Ref     settingsHelpLeftIcon;
    pu::ui::elm::Image::Ref     settingsHelpRightIcon;
    pu::ui::elm::TextBlock::Ref settingsSavedText;
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

    void OnRenderCallback();
    void SetPlayerTabVisible(bool visible);
    void SetUserTabVisible(bool visible);
    void SetSettingsTabVisible(bool visible);
    void SetRightPanelVisible(bool visible);
    void UpdateSettingsSelectText();
    void UpdatePlayerFocusStyles();
    void UpdateSettingsFocusStyles();

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
    void CycleSettingsLanguage(int delta);
    void MoveSettingsFocus(int delta);
    SettingsFocus GetSettingsFocus() const { return this->settingsFocus; }
    std::string GetSelectedLanguageCode() const;
    void SetSettingsFeedback(const std::string& text);
    void SwitchRightTab(RightTab tab);
    RightTab GetCurrentRightTab() const { return this->currentRightTab; }
    void SetRefreshCallback(std::function<void()> fn);
    void TriggerRefreshNow();
    void SetLoadingSpinner(bool visible);
    void SetBlockingLoading(bool visible);
};

class MainApplication : public pu::ui::Application {
private:
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

    void OnLoad() override;
    void OnLoginSuccess(const spotify::Tokens& tokens);
};
