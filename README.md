# spotify-switch

> **DISCLAIMER**
>
> This app is an **unofficial, personal-use homebrew project** and is not affiliated with, endorsed by, or approved by Spotify AB.
>
> **What this app does NOT do:** it does not play, stream, or decode any audio; it does not extract or bypass DRM-protected content; it does not simulate or replace the official Spotify client; it does not replicate any core Spotify experience. It only reads playback metadata and sends playback-control commands through the official Spotify Web API, exactly as documented.
>[https://developer.spotify.com/documentation/web-api](https://developer.spotify.com/documentation/web-api).
>

---

A Nintendo Switch homebrew app that shows your Spotify playback in real time — album art, track info, artist details and album metadata — all from your couch.

Built with [Plutonium](https://github.com/XorTroll/Plutonium) (SDL2) and the Spotify Web API.

---

## Features

- Real-time playback state (track, artist, album art)
- Artist panel — photo, genres, followers, popularity
- Album panel — thumbnail, type, release year, track count, label
- Play / pause, previous and next controls
- OAuth 2.0 PKCE login flow via QR code (no credentials stored in plaintext)
- Auto token refresh in the background

## Requirements

- A Nintendo Switch running a CFW that supports homebrew (e.g. Atmosphere)
- A Spotify account (Free or Premium)

No Spotify developer account needed — the app uses a built-in client ID.

## Installation

Download the latest `spotify-switch.nro` from the [Releases](../../releases) page, copy it to `/switch/spotify-switch/` on your SD card and launch it via the Homebrew Menu.

## Building from source

If you prefer to compile it yourself:

- [devkitPro](https://devkitpro.org/) with the `switch-dev` group installed
- Pacman packages: `switch-curl`, `switch-mbedtls`, `switch-sdl2`, `switch-sdl2_image`, `switch-sdl2_ttf`, `switch-sdl2_mixer`

```sh
git clone --recurse-submodules https://github.com/davidabejon/spotify-switch
cd spotify-switch
export DEVKITPRO=/opt/devkitpro
make
```

## First run

On first launch the app displays a QR code. Scan it with your phone, log in to Spotify and grant permissions. The token is saved to the SD card — subsequent launches skip the login screen.

## Project structure

```
source/          C++ source files
include/         Headers
libs/Plutonium/  UI framework (git submodule)
```

## License

MIT
