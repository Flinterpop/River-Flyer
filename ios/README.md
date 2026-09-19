# River Flyer for iPad and iPhone

*Last updated: 19 Sep 2026*

This folder turns River Flyer into a real iPadOS / iOS app. It was written on a Windows PC, where nothing Apple can be built or run, so it is a handover: everything here is meant to be run on a Mac, and the first run there is the first test. The notes below say what to expect and what to check.

## How it works

The app is a full-screen `WKWebView` that hosts the game's browser build (the same three files as `RiverFlyer-<version>-web.zip`: `index.html`, `index.js`, `index.wasm`). The touch controls added in v0.6.1 are the ones it uses. WebAssembly in a `WKWebView` runs JIT-compiled at native speed, audio and multi-touch go through Safari's engine, and the result installs with an icon like any other app. There is no raylib-native iOS build: raylib has no iOS platform, and a port would need an SDL backend, a cross-compiled raylib and an Xcode project that cannot be checked from a PC. This route is the same game with the same controls and none of that risk.

The files are served to the web view from the app bundle through a custom `riverflyer://app/` URL scheme (`GameViewController.swift`) rather than `file://`, because the Emscripten loader fetches `index.wasm` with `fetch()`, which WebKit refuses for `file://`, and `WebAssembly.instantiateStreaming` insists on the `application/wasm` content type; the scheme handler supplies both.

| File | Holds |
|---|---|
| `make-ios.sh` | The script: checks tools, copies in the browser build, generates the Xcode project, builds |
| `project.yml` | XcodeGen spec; `make-ios.sh` fills in the version, build number and team |
| `RiverFlyer/AppDelegate.swift` | Window and root view controller |
| `RiverFlyer/GameViewController.swift` | The web view, full-screen behaviour, and the bundle scheme handler |
| `RiverFlyer/Assets.xcassets` | App icon (drawn by `../android/make_icon.py`, 1024 px, opaque) |
| `RiverFlyer/web/` | The three browser-build files, copied in by the script (git-ignored) |

Notes:

- The version and build number come from `../src/Config.h`, like the Android APK, so all builds stay in lockstep.
- Portrait only, status bar hidden, home indicator auto-hides, bottom-edge swipes are deferred (a game owns the whole screen). Bundle id `ca.rabidfox.riverflyer`, deployment target iOS 15.

## On the Mac

Prerequisites, all free:

1. **Xcode** from the App Store (about 10 GB). Open it once so it installs its command-line tools and the iOS Simulator, and accept the licence.
2. **Homebrew** (https://brew.sh) so the script can `brew install xcodegen`, the tool that writes the Xcode project from `project.yml`. Or install XcodeGen yourself and skip Homebrew.
3. This repo: `git clone https://github.com/Flinterpop/River-Flyer.git` then `cd River-Flyer/ios`.

Then:

```bash
./make-ios.sh              # simulator build: proves the project compiles; needs no Apple account
./make-ios.sh --open       # the same, then opens RiverFlyer.xcodeproj in Xcode
```

To put it on an iPad, the app has to be signed. In Xcode: plug the iPad in (unlock it, tap Trust), pick it as the run destination at the top, open the project's **Signing & Capabilities** tab, tick *Automatically manage signing* and choose your Team, then press **Run**. Any Apple ID works as a team: a free account signs an app that runs for 7 days and must be re-installed after that; the $99/year developer account signs for a year and unlocks TestFlight, which is how the girls could install it on their own iPads without a cable.

From the command line, once a team exists (Xcode > Settings > Accounts shows its 10-character id):

```bash
TEAM_ID=ABCDE12345 ./make-ios.sh --device --install
```

The first launch of a free-account build is blocked until the iPad trusts the developer: **Settings > General > VPN & Device Management**, tap the account, Trust.

## What to check on the first run

Nothing in this folder has been executed on a Mac. The Swift is small and the pattern (scheme handler serving a WebAssembly app from the bundle) is well travelled, but these are the places it could still go wrong:

1. **Blank screen after the Play overlay.** The wasm did not load. Look at Xcode's console for `River Flyer: no such bundled file`: the `web` folder is missing from the bundle (in the project navigator it should be a blue folder reference, not a yellow group) or the three files were not copied in.
2. **No sound.** The Play tap is the user gesture that unlocks audio. If it stays silent, the `mediaTypesRequiringUserActionForPlayback = []` line in `GameViewController.swift` is the thing to look at, and whether iPad silent mode is on.
3. **Scores not kept between launches.** The game stores them in `localStorage`. WebKit keeps that for custom-scheme origins, but if a restart shows an empty table, say so: the fallback is to serve from `file://` with the WebKit preference `allowFileAccessFromFileURLs`.
4. **Page scrolls or zooms under a finger.** The shell sets `touch-action: none` and the view disables scrolling and bounce; if a drag still moves the page, the scroll view settings in `viewDidLoad` are the place.
5. **Rotation.** Portrait is enforced from both `Info.plist` and the view controller. On an iPad in landscape the app should turn the content to portrait rather than letterbox sideways.

If the simulator build fails inside `xcodegen` or `xcodebuild`, run the failing command by hand without the `grep` in the script to see the whole message, and send that back.

## Updating

When a new River Flyer version is released, `git pull` and run the script again: it picks up the new version number and downloads that release's browser build (or uses `../build-web` if it has been built on the Mac with Emscripten). Nothing else in this folder needs to change for a game update.
