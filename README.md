# River Flyer

[![Release][release-badge]][release-latest] [![Build][build-badge]][build-runs] [![License: MIT][license-badge]][license]

[release-badge]: https://img.shields.io/badge/release-v0.7.0-blue
[release-latest]: https://github.com/Flinterpop/River-Flyer/releases/latest
[build-badge]: https://github.com/Flinterpop/River-Flyer/actions/workflows/build.yml/badge.svg
[build-runs]: https://github.com/Flinterpop/River-Flyer/actions/workflows/build.yml
[license-badge]: https://img.shields.io/badge/license-MIT-green
[license]: LICENSE

*Last updated: 25 Sep 2026*

A vertical river scroller in C++20 and raylib, made for three players aged 7 to 12. Fly up a twisting river, dodge the rocks, shoot what is in the way, and top up at the fuel pumps before the tank runs dry. Clipping the shore or an island no longer ends the run: the hull takes damage, throws sparks and bounces the plane back into the channel, and a badly damaged plane trails smoke — fly over a health pack to patch it up. Three planes per game; lose one and the next arrives with a full tank, a full hull and a couple of seconds of grace.

<img width="965" height="1050" alt="image" src="https://github.com/user-attachments/assets/159c47e1-06fc-4ee0-aa22-df71264ec00c" />

## Play

Download `RiverFlyer-<version>-win64.zip` from the [latest release][release-latest], unzip, run `scroller.exe`. It is a single static exe with no installer and nothing else to install.

### On an Xbox, a tablet or a Chromebook

The game also builds to WebAssembly and runs in a browser, which is how it reaches an Xbox: the console's built-in Edge browser, with nothing installed on the console and no developer mode. Unzip `RiverFlyer-<version>-web.zip` from the [latest release][release-latest] (three files) or build it with the `web` preset (see [Browser build](#browser-build)), then:

1. Serve that folder from the PC on the home network, and let it through Windows Firewall for private networks when asked:

   ```powershell
   python -m http.server 8080 --directory build-web     # or the unzipped folder
   ```

2. Find the PC's address (`ipconfig`, the IPv4 Address line). On the Xbox open Edge, go to `http://<that address>:8080/`, and press A on **Play**. That press hands the controller to the game and goes fullscreen.

Notes:

- The title screen's `Gamepads connected` line shows whether the controller has reached the page. If it reads 0 while Edge's own cursor still moves, the browser has kept the controller: press Play again with the cursor, or plug in a USB keyboard.
- In fullscreen the browser takes Esc for itself to leave fullscreen, so pause with P or Start. With a keyboard, F toggles fullscreen.
- Renaming a pilot shows an on-screen keyboard: the d-pad or stick moves, A types, B erases, Y is a space, Start keeps the name and Back cancels. A real keyboard still types straight in.
- Scores and pilot names are kept in that browser's local storage, so the Xbox has its own table separate from the PC's.
- The same page works on any tablet or laptop browser on the network. Nothing is uploaded anywhere; the PC only serves the files while the command runs.
- **iPad and iPhone**: open the same address in Safari. A touchscreen gets the touch controls described under [Android](#on-an-android-phone-or-tablet): drag to steer, FIRE / CHAFF / JAM buttons, tappable menus. Use Share, then **Add to Home Screen**; opening it from there runs full screen like an app. There is also a native iPad / iPhone app, the same C++ game built for iOS (see [iOS build](#ios-build)); it has to be built on a Mac with Xcode and installed with a cable or TestFlight, since it is not on the App Store.

### Assist

A fifth title-screen row, **ASSIST**, gives one seat an easier ride without
changing the world for both (spawn rates, scroll speed and the guns are
shared by definition): two extra planes, half the hull damage and a slower
fuel burn. With one player it is OFF / ON; with two it cycles
OFF / PILOT 1 / PILOT 2 / BOTH, and the assisted pilot's name carries an
ASSIST tag on the HUD. Useful when a seven-year-old and a twelve-year-old
want the same game.

### Two players on one tablet

Set **PLAYERS** to 2 on the title screen and the touch layer splits down the
middle: pilot one steers in the right half with FIRE / CHAFF / JAM in the
bottom-right corner, pilot two has the mirrored set on the left. Each half
has its own floating stick, so two people can share an iPad. The planes carry
a small **1** and **2** and start on their own player's side.

### On an Android phone or tablet

Install `RiverFlyer-<version>-android.apk` from the [latest release][release-latest] (or build it, see [Android build](#android-build)): copy it to the device or open the link there, allow the install from that source when asked, and tap the icon. The whole game is in the app; nothing is downloaded and no permissions are requested.

Touch controls, drawn in the black bars around the river on a tall phone and over its corners on a squarer tablet:

- **Drag anywhere** to steer: the plane follows the finger's movement, not its position, so it never hides under a thumb. Push up for the afterburner, pull down for the drag chute.
- **FIRE** (hold), **CHAFF** (tap) and **JAM** (hold) sit under the right thumb; the pause button is top right. The phone's Back button also pauses, and quits from the title screen.
- Menus are tapped: **TAP TO FLY** starts, a title row changes when tapped (left half back, right half forward), a pilot's name opens the on-screen keyboard, and the pause panel has FLY ON / QUIT / MUSIC buttons.
- A gamepad paired with the device works too, with the same buttons as on the Xbox.
- Scores and names are kept in the app's own storage; uninstalling clears them. Leaving the app (or a call) pauses the game.

The title screen picks the game: **1 or 2 players**, a **pilot name** for each (Left/Right cycles the saved names, Enter renames), and **Easy / Normal / Hard**. Space, or A on a gamepad, starts. High scores are recorded under the pilot name automatically (both names joined for co-op).

| Key | Action |
|---|---|
| Pilot 1: W A S D + Space | Fly and shoot. In a one-player game the arrow keys work too |
| Pilot 2: arrows + Right Ctrl (or Right Shift) | Second pilot in a two-player game |
| Gamepad | Left stick or d-pad flies, A or the right trigger shoots; pad 1 is pilot 1, pad 2 is pilot 2. Menus work from any pad |
| Up | Afterburner |
| Down | Drag chute: slows the river for everyone |
| C (pilot 2: /) | Drop chaff: every missile chasing you turns to chase the cloud instead. Three per plane. On a pad, B or the left bumper |
| V hold (pilot 2: .) | Jam: missiles lose lock and fly straight while held, but fuel burns three times as fast. On a pad, X or the right bumper |
| Esc / P / Start | Pause. From the pause panel Esc resumes, Q returns to the title |
| Tab (hold) | Peek at the top-ten table mid-game |
| M | Music on / off |
| F12 | Save `screenshotNNN.png` next to the exe |
| Q (title screen) | Quit |

Scoring: distance plus 100 per rock or fuel pump shot, 200 per boat, 300 per island gun, 400 per missile site, 100 per missile shot down, 500 per bridge, 50 per star.

Notes:

- Fuel burns steadily and the bar goes red below a quarter. Fly over a red **FUEL** pump to refill; a hose runs from the pump while you are taking fuel on.
- Difficulty sets planes (5 on Easy, 3 otherwise), how many rocks and boats spawn, whether islands have guns, shell speed, fuel burn and how fast the river ramps up.
- The scenery changes every 6000 px: forest, farmland, canyon, snow. Night falls and lifts once every 14000 px.
- The river sometimes splits around an island; either channel works, but both banks of the island are as solid as the shore. Islands may carry gun emplacements that swivel to follow you and lob slow shells; dodge them or shoot the gun.
- Wider islands on Normal and Hard may also carry a **missile site**. It fires from much further away than a gun, and its missile chases you, trailing smoke, for up to six seconds. A radar-warning beep gets faster as it closes and the HUD flashes **MISSILE!** with a reminder of the keys. Missiles turn slowly, so a hard sidestep at close range makes one overshoot; you can also shoot it, drop chaff, or hold jam. The shield pops a missile harmlessly. Fuel pumps restock chaff.
- Boats cross the river back and forth; bridges block the whole channel and take three hits to open. Hitting a rock, boat, gun, bridge, shell or missile rolls the plane into the water; hitting a bank or running dry spirals it in. Either way it costs one plane.
- Pickups float on the water: a **star** for points, a **shield** bubble that pops anything solid it touches for 7 s (banks still count), a **spread** that fires three-way for 10 s, and an **extra plane**.
- Wildlife is harmless: ducks, jumping fish, deer on the banks and the occasional otter. Fly close to an otter to spot it; the game-over panel keeps count.
- In a two-player game the score is shared, each pilot has their own planes and fuel, guns aim at whoever is nearer, and the game ends when both are out.
- Top-ten scores are kept in `highscores.txt`, pilot names in `profiles.txt`, both next to the exe (delete either to start fresh); the browser build keeps the same two records in the browser's local storage. `BEST` at the top of the screen is the current record.
- Renaming a pilot (Enter or A on the pilot row) opens an on-screen keyboard that works from a gamepad as well as the keys.

## Build

Requirements: Visual Studio 2026 (MSVC 14.5x), CMake 3.25+, and [vcpkg](https://github.com/microsoft/vcpkg) at `C:\vcpkg`. The preset uses vcpkg manifest mode, so the first configure builds raylib into `build\vcpkg_installed` (a few minutes) and later ones are instant.

```powershell
cmake --preset msvc
cmake --build --preset release      # or: debug
build\Release\scroller.exe
```

The build is `/W4 /WX` with a static CRT; the Release exe depends only on `winmm`, `gdi32`, `shell32`, `user32` and `kernel32`.

### The raylib overlay port

`vcpkg-overlays/raylib` is the stock vcpkg raylib 6.0 port with two fixes that matter at run time; `CMakePresets.json` points `VCPKG_OVERLAY_PORTS` at it.

- raylib 6.0's `ParseConfigHeader.cmake` misreads the new `#define SUPPORT_X 0` style in `config.h`, so with the port's `CUSTOMIZE_BUILD=ON` every `SUPPORT_*` flag is compiled in. `SUPPORT_CUSTOM_FRAME_CONTROL` makes `EndDrawing()` skip the buffer swap, event polling and frame pacing, so the window never paints and never responds; `SUPPORT_BUSY_WAIT_LOOP` pins a core. The overlay forces both off.
- The port de-vendors `miniaudio.h`, swapping the 0.11.24 that raylib ships for the vcpkg port's 0.11.25. raylib's mixer is silent on Windows/WASAPI with the newer header even though the device initialises and `IsSoundPlaying` reports playback. The overlay keeps the bundled header.

The port's exported CMake target also carries no link dependencies, so `CMakeLists.txt` links `glfw::glfw` and the Win32 libraries explicitly.

### Browser build

Requirements: the [Emscripten SDK](https://emscripten.org) installed and activated at `C:\emsdk`:

```powershell
git clone https://github.com/emscripten-core/emsdk C:\emsdk
C:\emsdk\emsdk install 6.0.9
C:\emsdk\emsdk activate 6.0.9
```

Emscripten is the one toolchain this repo cannot pin by hash, because it lives outside the tree: the configure warns when `emcc` is not the tested 6.0.9 (`RF_EMSDK_VERSION` in `CMakeLists.txt`). A newer one usually works - test the page, then move the pin. Nothing needs to be on `PATH`: the `web` preset points at the toolchain file directly and uses Visual Studio's bundled Ninja. vcpkg is not involved; the first configure downloads raylib 6.0's source from GitHub (the same tarball vcpkg uses, checked by hash) and builds it for the web with `PLATFORM=Web`, which also sidesteps both overlay-port problems above.

```powershell
cmake --preset web
cmake --build --preset web
python -m http.server 8080 --directory build-web     # then open http://localhost:8080/
```

The output is `build-web\index.html`, `index.js` and `index.wasm`, about 600 KB in all; those three files are the whole deployment. Where it differs from the desktop build:

- `web/shell.html` is the page. The game does not start until **Play** is pressed: that press is the user gesture browsers demand before a page may play sound or go fullscreen, so the audio device is created already unlocked. The shell needs `callMain` exported, and raylib's own `-sEXPORTED_RUNTIME_METHODS` would otherwise replace ours, so `CMakeLists.txt` patches raylib's flag rather than adding a second.
- The browser owns the frame loop (`emscripten_set_main_loop_arg` in `src/main.cpp`) and runs at the display's refresh rate; the desktop build keeps its own 60 fps loop. Movement is time-based, so both play the same.
- `src/Canvas.*` draws the game into a 960 px wide texture and presents it scaled and pillarboxed to whatever the window is, so a 1080p TV shows the whole river. The desktop build goes through the same path at 1:1.
- `src/Screen.*` picks the canvas height once at start-up from the display's shape, clamped to 1000–1500: a desktop window and the browser stay at 1000, while an Android or iOS phone gets 1500 and an iPad fills the glass, so a tall screen shows more river instead of black bars. Every fixed-size pool is built for the 1500 maximum, so nothing allocates at run time.
- `src/Storage.*` keeps the high scores and pilot names in the browser's local storage instead of files next to the exe.
- raylib is built for WebGL2 (`OPENGL_VERSION "ES 3.0"`) so the non-power-of-two sprites keep their mipmaps; WebGL1 cannot mipmap them and the sprites shimmer.
- `src/Touch.*` switches on when the page reports a touchscreen (`navigator.maxTouchPoints`), so an iPad, a phone or a touch laptop gets the phone layout while an Xbox or a desktop browser keeps the keyboard and gamepad panels. The shell sets `touch-action: none` on the canvas so fingers reach the game rather than scrolling or zooming the page, and carries the `apple-mobile-web-app-*` tags for the Home Screen. The mouse never stands in for a finger in the browser: browsers synthesise a click after every tap, which would count twice.
- The game code compiles with the same `-Wall -Wextra -Werror -pedantic` as any non-MSVC build; the deprecation warnings during the build come from raylib's bundled miniaudio and stb, not from the game.

### Android build

Requirements: a JDK 17 or newer, and an Android SDK with platform 36, NDK 28 and CMake 3.31 (the SDK's own CMake bundles the Ninja that Gradle's native build expects). From the SDK's `cmdline-tools`:

```powershell
sdkmanager "platforms;android-36" "build-tools;36.1.0" "ndk;28.2.13676358" "cmake;3.31.6"
```

Tell Gradle where the SDK is with `android\local.properties` containing `sdk.dir=C:/Android/sdk` (forward slashes; the file is git-ignored), then:

```powershell
cd android
.\gradlew assembleDebug        # app\build\outputs\apk\debug\app-debug.apk
.\gradlew assembleRelease      # app\build\outputs\apk\release\app-release.apk
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

The first build downloads the Android Gradle Plugin (9.4.1, matched to the Gradle 9.7.1 wrapper - the two go up together or not at all) and raylib's source tarball; after that it is offline. Where it differs from the other two builds:

- `android/` is a small Gradle project with no Java in it: the manifest declares `android.app.NativeActivity` with `android.app.lib_name = scroller`, and Gradle drives the same `CMakeLists.txt` as the desktop, which under `ANDROID` builds the game as `libscroller.so` against raylib compiled for `PLATFORM=Android`. raylib's `android_main()` calls the game's `main()`.
- `src/Touch.*` is the touch layer: a floating stick and the FIRE / CHAFF / JAM / pause buttons, drawn in window space after the canvas so they land in the letterbox bars. It also turns taps into canvas coordinates for the panels. On the desktop `scroller.exe --touch` shows the same layout with the mouse as a finger, for trying it without a device.
- `src/Storage.*` writes the two records to the app's internal data folder, the only place a native activity may write without asking.
- `src/main.cpp` opens the window at `0 x 0` (the full display) and lets `Canvas` letterbox, and it survives the window being taken away during start-up (a lock screen or a call): raylib 6.0 runs its GL setup twice on Android, and if the surface vanishes between the two passes the second leaves rlgl with no default texture, shader or batch, so the game waits for the surface and runs `rlglInit()` again.
- The library is linked with `-z max-page-size=16384`: Android 15 and later show a warning dialog (and Google Play refuses uploads) for native code that is not 16 KB-page aligned.
- The version is read from `src/Config.h` by `android/app/build.gradle`, so the APK follows the same bump as the desktop and web builds. Release builds are signed with the debug key unless `android/keystore.properties` names a real one; that is enough for sideloading.
- The launcher icon is drawn by `android/make_icon.py` (Pillow), in keeping with the rest of the game generating its own art.

### iOS build

Requirements: a Mac with Xcode (the App Store one; open it once) and CMake 3.25 or newer (`brew install cmake`, or without Homebrew `pip3 install --user cmake`). Then:

```bash
./ios/make-ios.sh --run                                   # simulator build, installed and launched on the booted simulator
TEAM_ID=ABCDE12345 ./ios/make-ios.sh --device --install   # signed build on the plugged-in iPad
```

The first configure downloads raylib's source tarball and SDL 3.4; after that it is offline. [`ios/README.md`](ios/README.md) has the signing steps and what to check. Where it differs from the other builds:

- raylib has no iOS platform, so the game runs on raylib's SDL backend over SDL3, which supplies the UIKit window, multi-touch, the Game Controller framework and app life-cycle events, drawing with OpenGL ES 3.0 (deprecated by Apple but present in the SDK; the swap for ANGLE, if it ever goes, is below the game). The root `CMakeLists.txt` under `IOS` fetches both and compiles raylib's sources directly, since raylib's own CMake hard-codes SDL2 for that backend, with the Xcode generator so the result is an ordinary Xcode project (`build-ios/scroller.xcodeproj`).
- `ios/raylib-sdl-ios.patch` fixes four things in raylib 6.0 for iOS, each explained in its header: Retina scaling (the SDL backend never sets raylib's screen-scale matrix), the default framebuffer (EAGL has none; the view's is used), the audio session category (miniaudio's default opens the microphone path; a game wants Ambient), and the Simulator's audio server, which hangs and aborts the process, so simulator builds run silent.
- `src/main.cpp` on iOS includes `SDL_main.h`, which renames `main()` so SDL's app delegate can call it once the app has launched, and watches the app events: iOS ends an app that touches the GPU in the background, so the loop skips frames (paused) until the app returns.
- `src/Storage.*` writes the two records to the app's Documents folder; the bundle is read-only.
- Scores, names and the icon behave as on Android: kept in the app's own storage, cleared by uninstalling; the icon is the same 1024 px image drawn by `android/make_icon.py`.

### Tests

`tests/rf_tests.cpp` covers the parts of the game that are pure logic: the river generator across every difficulty and a spread of seeds, the high-score table, and the pilot list including their round trip through `Storage`. It opens no window, so it runs anywhere the desktop build runs. The desktop presets build it; the phone and browser presets skip it.

```powershell
cmake --build --preset debug
ctest --test-dir build -C Debug --output-on-failure
```

GitHub Actions runs the same two things on every push to `main` and on pull requests (`.github/workflows/build.yml`): the Windows desktop build with `ctest`, and the browser build, whose toolchain is the unpinned one. The phone and tablet apps are not built there - they need an SDK and a signing identity, and both are tried on a real device before a release.

The generator's own assertions are half of what the tests check, so `rf_tests` keeps `NDEBUG` undefined in Release as well — a Release run still traps a bad clamp. The shipped exe is unaffected and keeps its assertions off. Note what this does *not* cover: the v0.6.0 island-clamp crash depended on ARM fusing a multiply-add, and would not reproduce on x86 even unfixed. The tests catch a generator that is wrong everywhere, not one that is wrong only on one instruction set.

## Layout

| File | Holds |
|---|---|
| `src/Config.h` | Every tunable: sizes, speeds, spawn chances, difficulty presets, stage palettes, animation and sound parameters, version |
| `src/Game.*` | State machine (Title, EnterName, Playing, Paused, GameOver), per-pilot play, collisions, pickups, scoring, HUD and panels |
| `src/Pilot.h` | One player: plane, controls, planes left, fuel, timers, power-ups |
| `src/Input.*` | Keyboard and gamepad bindings per pilot; menu navigation; pilot one also takes the touch layer |
| `src/Touch.*` | Touch controls: floating stick, FIRE / CHAFF / JAM / pause buttons drawn in window space, menu taps in canvas coordinates; the mouse stands in for a finger on the desktop |
| `src/Player.*` | The plane: movement, afterburner, drag chute, spiral and roll crash animations |
| `src/Terrain.*` | River strips that scroll down with random drift and periodically split around an island; banks interpolated between strips, sand shoreline, shallows, trees with reflections; stage palettes; pools of rocks, fuel pumps, boats, guns, missile sites, bridges, pickups and critters |
| `src/Bullets.*` | Fixed bullet pool (with sideways velocity for the spread shot) |
| `src/Shells.*` | Fixed pool of enemy shells fired by island guns |
| `src/Missiles.*` | Fixed pool of homing missiles from the island sites: limited turn rate, chaff decoy, straight flight while jammed |
| `src/Effects.*` | Fixed pools of particle bursts (rock, fuel, plane, splash, chaff), missile smoke and wake foam |
| `src/Sprites.*` | Textures generated at start-up: 2x sprites with shading, seamless Perlin water and grass tiles, trees, boat, gun, missile site, pickups; swap the `Gen*` bodies for `LoadTexture()` when real art exists |
| `src/Audio.*` | Sound bank synthesised at start-up: slurp, shoot, pop, crunch, whine, splash, brake, fanfare, thud, ding, launch, chaff, radar warning, and the music loop |
| `src/HighScores.*` | Top-ten table with names, saved through `Storage` as `highscores.txt` |
| `src/Profiles.*` | Pilot names for the title screen, saved through `Storage` as `profiles.txt` |
| `src/Storage.*` | The two saved records: files beside the exe on the desktop, browser local storage on the web, the app's internal folder on Android, its Documents folder on iOS |
| `src/Canvas.*` | Off-screen 960 px wide render target, presented scaled and pillarboxed to the real window |
| `src/Screen.*` | Canvas height for this display (1000–1500), chosen at start-up |
| `src/main.cpp` | Window and frame loop; on the web the browser drives the loop instead; on Android it also recovers from a window lost during start-up; on iOS it stops drawing while the app is in the background |
| `web/shell.html` | The page around the WebAssembly build: Play button, fullscreen, canvas focus |
| `tests/rf_tests.cpp` | Headless checks of the river generator, the score table and the pilot list; run with ctest |
| `.github/workflows/build.yml` | CI: the Windows build plus ctest, and the browser build, on every push and pull request |
| `android/` | Gradle project for the APK: manifest (NativeActivity, portrait), `build.gradle` (drives the root CMake, version from `Config.h`), icon generator |
| `ios/` | iPad / iPhone app: `make-ios.sh` drives the root CMake for iOS (raylib on SDL3, OpenGL ES 3.0); the raylib patch, `Info.plist` template, GLES header shims and icon; version from `Config.h` |

Notes:

- No heap allocation after start-up: pools are `std::array`, every loop has a fixed bound, and each function carries assertions on its inputs and results. Debug builds keep the assertions and a console for raylib's log.
- There are no binary assets in the repo; sprites and sounds are generated in code so the whole game is the source tree.

## Releasing

Bump the version in `src/Config.h`, `vcpkg.json` and the badge above together (the APK and the iOS app read it from `Config.h`), build Release for the desktop, web and Android, then tag and publish:

```powershell
git tag -a v0.7.0 -m "v0.7.0"
git push origin main v0.7.0
Compress-Archive build\Release\scroller.exe RiverFlyer-v0.7.0-win64.zip
Compress-Archive build-web\index.html, build-web\index.js, build-web\index.wasm RiverFlyer-v0.7.0-web.zip
Copy-Item android\app\build\outputs\apk\release\app-release.apk RiverFlyer-v0.7.0-android.apk
gh release create v0.7.0 RiverFlyer-v0.7.0-win64.zip RiverFlyer-v0.7.0-web.zip RiverFlyer-v0.7.0-android.apk --title "v0.7.0" --notes-file notes.md
```

## License

[MIT](LICENSE). raylib itself is zlib/libpng licensed; the overlay port under `vcpkg-overlays/raylib` derives from the MIT-licensed vcpkg port.
