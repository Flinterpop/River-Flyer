# River Flyer for iPad and iPhone

*Last updated: 20 Sep 2026*

This folder builds River Flyer as a native iPadOS / iOS app: the same C++ game as the Windows, browser and Android builds, compiled for arm64 with Xcode. Everything runs on a Mac.

## How it works

raylib has no iOS platform of its own, so the game runs on raylib's SDL backend over [SDL3](https://libsdl.org), which supplies the UIKit window, multi-touch, the Game Controller framework (MFi and Xbox / PlayStation pads) and the app life-cycle events, and draws with OpenGL ES 3.0. Apple deprecated OpenGL ES in 2018 but still ships it (it runs on Metal underneath); if it ever goes, the swap is to ANGLE, below the game code. There is no Swift or Objective-C in the app; miniaudio (raylib's audio) is compiled as Objective-C because its iOS backend uses `AVAudioSession`.

The build is the root `CMakeLists.txt` under `IOS`, configured with CMake's own iOS support and the Xcode generator (preset `ios` in `../CMakePresets.json`), so the result is an ordinary Xcode project you can open, sign and run from Xcode like any other. raylib and SDL3 are fetched as source tarballs on the first configure and built into the app; raylib's sources are compiled directly (its own CMake hard-codes SDL2 for the SDL backend), with one patch.

| File | Holds |
|---|---|
| `make-ios.sh` | The script: checks tools, configures, builds with `xcodebuild`, and installs on a simulator or a device |
| `raylib-sdl-ios.patch` | raylib 6.0 on iOS: Retina scaling, EAGL's default framebuffer, the audio session category, and the Simulator's audio server (see its header) |
| `apply-patch.cmake` | Applies the patch to the fetched raylib source, once |
| `Info.plist.in` | The app's `Info.plist`; CMake fills in the version (from `../src/Config.h`), bundle id `ca.rabidfox.riverflyer`, portrait only, iOS 15 or newer |
| `glshim/` | `<GLES3/gl3.h>` and `<GLES2/gl2ext.h>` as rlgl includes them, mapped to Apple's `<OpenGLES/...>` headers |
| `Assets.xcassets` | App icon (drawn by `../android/make_icon.py`, 1024 px) |

What differs from the other builds is in `src/`: `main.cpp` includes `SDL_main.h` (SDL's app delegate calls `main()` once the app has launched) and stops drawing while the app is in the background, since iOS ends an app that touches the GPU there; `Storage.cpp` writes the two records to the app's Documents folder; `Game.cpp` has no Q-to-quit, as an iOS app is not meant to quit itself.

## On the Mac

Prerequisites, all free:

1. **Xcode** from the App Store (about 10 GB). Open it once so it installs its command-line tools and an iOS Simulator, and accept the licence.
2. **CMake** 3.25 or newer: `brew install cmake`, or with no Homebrew `pip3 install --user cmake` (the script finds either).
3. This repo: `git clone https://github.com/Flinterpop/River-Flyer.git` then `cd River-Flyer`.

Then, from the repo root or this folder:

```bash
./ios/make-ios.sh              # simulator build: proves everything compiles; needs no Apple account
./ios/make-ios.sh --run        # the same, then installs and launches it on the booted simulator (boots an iPad if none is)
./ios/make-ios.sh --open       # opens build-ios/scroller.xcodeproj in Xcode
```

The first configure takes a few minutes (SDL's configure checks, and the two downloads); after that a rebuild is seconds. The simulator runs headless from the command line; Xcode's Simulator app (Device Hub in Xcode 27) shows its screen, and `xcrun simctl io booted screenshot x.png` grabs one. The simulator has **no sound**: its audio server hangs on the RemoteIO unit and then aborts the app, so simulator builds do not open an audio device (set `RAYLIB_SIMULATOR_AUDIO=1` in the app's environment to try, e.g. `SIMCTL_CHILD_RAYLIB_SIMULATOR_AUDIO=1 xcrun simctl launch ...`). It is also slow: Apple's software GL renderer draws the river at a few frames a second on the title screen, which says nothing about a real iPad.

### On an iPad

The app has to be signed with an Apple team. Any Apple ID works: a free account signs an app that runs for 7 days and must be re-installed after that; the $99/year developer account signs for a year and unlocks TestFlight, which is how the girls could install it on their own iPads without a cable. Xcode > Settings > Accounts shows the account and its 10-character team id.

From Xcode: `./ios/make-ios.sh --open`, plug the iPad in (unlock it, tap Trust), pick it as the run destination at the top, select the `scroller` scheme, open the project's **Signing & Capabilities** tab, tick *Automatically manage signing* and choose your Team, then press **Run**. Note that CMake regenerates the project on every configure, so a team chosen in Xcode is forgotten by the next `make-ios.sh`; pass it in instead:

```bash
TEAM_ID=ABCDE12345 ./ios/make-ios.sh --device --install
```

This needs Xcode signed in to the account (it creates the provisioning profile). The first launch of a free-account build is blocked until the iPad trusts the developer: **Settings > General > VPN & Device Management**, tap the account, Trust.

## What to check on the first run on a device

The simulator build has been run (title screen, layout on an iPhone 17 and an iPad Pro 13", Retina scaling, storage folder). These have not, because they need a real device:

1. **Sound.** The audio session is opened as Ambient (playback only, obeys the mute switch, mixes with other audio). If it stays silent, check the side switch / Control Centre mute first; then look in Xcode's console for `AUDIO:` lines.
2. **Touch.** Drag to steer, FIRE / CHAFF / JAM buttons, taps in the menus, the on-screen keyboard for a pilot name. This is the same touch layer as Android and the browser, reached through SDL's finger events.
3. **Leaving and returning.** Press Home or take a call mid-game: the game should pause and be there, paused, on return, with no crash (iOS kills an app that draws in the background; `main.cpp` stops drawing on `SDL_EVENT_WILL_ENTER_BACKGROUND`).
4. **Scores kept between launches.** They live in the app's Documents folder; the Files app shows them under *On My iPad > River Flyer* if the check is wanted.
5. **A paired gamepad.** Same buttons as on the Xbox.
6. **Frame rate.** 60 fps is expected on any supported iPad; the game asks for vsync.

If something is wrong, Xcode's console (View > Debug Area) shows raylib's log; the lines starting `INFO: DISPLAY:` give the screen and render sizes, which on a Retina device should differ by the scale factor.

## Updating

When the game changes, `git pull` and run the script again: nothing in this folder depends on the version except through `../src/Config.h`, which it reads. To move to a newer raylib, update the tarball URL and hash in `../CMakeLists.txt` (both the web/Android block and the iOS block use the same one) and check that `raylib-sdl-ios.patch` still applies.

## TestFlight / App Store

The app is on TestFlight as `ca.rabidfox.riverflyer` (App Store Connect app
id 6814993158). A distribution build:

```sh
cmake --preset ios -DRF_TEAM_ID=KU9VFDK3Z4 -DRF_BUILD=<n>     # n must beat the last upload
xcodebuild -project build-ios/scroller.xcodeproj -scheme scroller -configuration Release \
  -destination 'generic/platform=iOS' -archivePath /tmp/RiverFlyer.xcarchive \
  CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="Apple Distribution" \
  PROVISIONING_PROFILE_SPECIFIER="RF AppStore ca.rabidfox.riverflyer" DEVELOPMENT_TEAM=KU9VFDK3Z4 archive
xcodebuild -exportArchive -archivePath /tmp/RiverFlyer.xcarchive \
  -exportOptionsPlist ios/ExportOptions.plist -exportPath /tmp/rf-export
xcrun altool --upload-app -f "/tmp/rf-export/River Flyer.ipa" -t ios --apple-id 6814993158 \
  --apiKey <key id> --apiIssuer <issuer id>
```

Notes learned the hard way:

- `SKIP_INSTALL NO` and `INSTALL_PATH` on the iOS target (in `CMakeLists.txt`)
  are what make `xcodebuild archive` produce a non-empty archive.
- Apple rejects a binary whose `Info.plist` lacks purpose strings for APIs the
  *libraries* reference — SDL touches camera and Bluetooth, miniaudio the
  microphone — even though the game never asks for them. Those strings are in
  `ios/Info.plist.in`.
- `-DRF_BUILD=<n>` bumps `CFBundleVersion` without changing the game version,
  which App Store Connect requires for a replacement upload.

## iPhone and iPad layout

The game draws to an off-screen canvas that is always 960 px wide; its
**height follows the display** (`src/Screen.h`), so an iPad fills the glass
instead of showing a 4:3 letterbox and a tall phone gets more river than
black bars. `screen::Fit()` picks the height at start-up from the window
size, clamped to `[cfg::kScreenH, cfg::kScreenHMax]` (1000–1500); every
fixed-size pool is built for the maximum, so nothing allocates at run time.

| Device | Canvas |
|---|---|
| iPad Pro 13" (4:3) | 960 x 1280, exact fit |
| iPad Pro 11" | 960 x 1392, exact fit |
| iPhone 17 (19.5:9) | 960 x 1500 (clamped; small bars remain) |
| Desktop window | 960 x 1000 as before |

A taller canvas shows more river ahead, which makes the game slightly more
forgiving on an iPad — the scroll speed is unchanged, there is just more
warning. Raise `kScreenHMax` to let phones go taller still.
