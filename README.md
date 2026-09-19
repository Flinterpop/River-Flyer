# River Flyer

[![Release][release-badge]][release-latest] [![License: MIT][license-badge]][license]

[release-badge]: https://img.shields.io/badge/release-v0.5.0-blue
[release-latest]: https://github.com/Flinterpop/River-Flyer/releases/latest
[license-badge]: https://img.shields.io/badge/license-MIT-green
[license]: LICENSE

*Last updated: 17 Sep 2026*

A vertical river scroller in C++20 and raylib, made for three players aged 7 to 12. Fly up a twisting river, dodge the rocks, shoot what is in the way, and top up at the fuel pumps before the tank runs dry. Three planes per game; lose one and the next arrives with a full tank and a couple of seconds of grace.

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
C:\emsdk\emsdk install latest
C:\emsdk\emsdk activate latest
```

Nothing needs to be on `PATH`: the `web` preset points at the toolchain file directly and uses Visual Studio's bundled Ninja. vcpkg is not involved; the first configure downloads raylib 6.0's source from GitHub (the same tarball vcpkg uses, checked by hash) and builds it for the web with `PLATFORM=Web`, which also sidesteps both overlay-port problems above.

```powershell
cmake --preset web
cmake --build --preset web
python -m http.server 8080 --directory build-web     # then open http://localhost:8080/
```

The output is `build-web\index.html`, `index.js` and `index.wasm`, about 600 KB in all; those three files are the whole deployment. Where it differs from the desktop build:

- `web/shell.html` is the page. The game does not start until **Play** is pressed: that press is the user gesture browsers demand before a page may play sound or go fullscreen, so the audio device is created already unlocked. The shell needs `callMain` exported, and raylib's own `-sEXPORTED_RUNTIME_METHODS` would otherwise replace ours, so `CMakeLists.txt` patches raylib's flag rather than adding a second.
- The browser owns the frame loop (`emscripten_set_main_loop_arg` in `src/main.cpp`) and runs at the display's refresh rate; the desktop build keeps its own 60 fps loop. Movement is time-based, so both play the same.
- `src/Canvas.*` draws the fixed 960 x 1000 game into a texture and presents it scaled and pillarboxed to whatever the window is, so a 1080p TV shows the whole river. The desktop build goes through the same path at 1:1.
- `src/Storage.*` keeps the high scores and pilot names in the browser's local storage instead of files next to the exe.
- raylib is built for WebGL2 (`OPENGL_VERSION "ES 3.0"`) so the non-power-of-two sprites keep their mipmaps; WebGL1 cannot mipmap them and the sprites shimmer.
- The game code compiles with the same `-Wall -Wextra -Werror -pedantic` as any non-MSVC build; the deprecation warnings during the build come from raylib's bundled miniaudio and stb, not from the game.

## Layout

| File | Holds |
|---|---|
| `src/Config.h` | Every tunable: sizes, speeds, spawn chances, difficulty presets, stage palettes, animation and sound parameters, version |
| `src/Game.*` | State machine (Title, EnterName, Playing, Paused, GameOver), per-pilot play, collisions, pickups, scoring, HUD and panels |
| `src/Pilot.h` | One player: plane, controls, planes left, fuel, timers, power-ups |
| `src/Input.*` | Keyboard and gamepad bindings per pilot; menu navigation |
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
| `src/Storage.*` | The two saved records: files beside the exe on the desktop, browser local storage on the web |
| `src/Canvas.*` | Off-screen 960 x 1000 render target, presented scaled and pillarboxed to the real window |
| `src/main.cpp` | Window and frame loop; on the web the browser drives the loop instead |
| `web/shell.html` | The page around the WebAssembly build: Play button, fullscreen, canvas focus |

Notes:

- No heap allocation after start-up: pools are `std::array`, every loop has a fixed bound, and each function carries assertions on its inputs and results. Debug builds keep the assertions and a console for raylib's log.
- There are no binary assets in the repo; sprites and sounds are generated in code so the whole game is the source tree.

## Releasing

Bump the version in `src/Config.h`, `vcpkg.json` and the badge above together, build Release, then tag and publish:

```powershell
git tag -a v0.5.0 -m "v0.5.0"
git push origin main v0.5.0
Compress-Archive build\Release\scroller.exe RiverFlyer-v0.5.0-win64.zip
Compress-Archive build-web\index.html, build-web\index.js, build-web\index.wasm RiverFlyer-v0.5.0-web.zip
gh release create v0.5.0 RiverFlyer-v0.5.0-win64.zip RiverFlyer-v0.5.0-web.zip --title "v0.5.0" --notes-file notes.md
```

## License

[MIT](LICENSE). raylib itself is zlib/libpng licensed; the overlay port under `vcpkg-overlays/raylib` derives from the MIT-licensed vcpkg port.
