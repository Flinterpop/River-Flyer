# River Flyer

[![Release][release-badge]][release-latest] [![License: MIT][license-badge]][license]

[release-badge]: https://img.shields.io/badge/release-v0.3.0-blue
[release-latest]: https://github.com/Flinterpop/River-Flyer/releases/latest
[license-badge]: https://img.shields.io/badge/license-MIT-green
[license]: LICENSE

*Last updated: 16 Sep 2026*

A vertical river scroller in C++20 and raylib, made for three players aged 7 to 12. Fly up a twisting river, dodge the rocks, shoot what is in the way, and top up at the fuel pumps before the tank runs dry. Three planes per game; lose one and the next arrives with a full tank and a couple of seconds of grace.


<img width="642" height="846" alt="image" src="https://github.com/user-attachments/assets/6c0fc587-461e-4f39-81a9-b7c3d45606c8" />

## Play

Download `RiverFlyer-<version>-win64.zip` from the [latest release][release-latest], unzip, run `scroller.exe`. It is a single static exe with no installer and nothing else to install.

The title screen picks the game: **1 or 2 players**, a **pilot name** for each (Left/Right cycles the saved names, Enter renames), and **Easy / Normal / Hard**. Space, or A on a gamepad, starts. High scores are recorded under the pilot name automatically (both names joined for co-op).

| Key | Action |
|---|---|
| Pilot 1: W A S D + Space | Fly and shoot. In a one-player game the arrow keys work too |
| Pilot 2: arrows + Right Ctrl (or Right Shift) | Second pilot in a two-player game |
| Gamepad | Left stick or d-pad flies, A or the right trigger shoots; pad 1 is pilot 1, pad 2 is pilot 2. Menus work from any pad |
| Up | Afterburner |
| Down | Drag chute: slows the river for everyone |
| Esc / P / Start | Pause. From the pause panel Esc resumes, Q returns to the title |
| Tab (hold) | Peek at the top-ten table mid-game |
| M | Music on / off |
| F12 | Save `screenshotNNN.png` next to the exe |
| Q (title screen) | Quit |

Scoring: distance plus 100 per rock or fuel pump shot, 200 per boat, 300 per island gun, 500 per bridge, 50 per star.

Notes:

- Fuel burns steadily and the bar goes red below a quarter. Fly over a red **FUEL** pump to refill; a hose runs from the pump while you are taking fuel on.
- Difficulty sets planes (5 on Easy, 3 otherwise), how many rocks and boats spawn, whether islands have guns, shell speed, fuel burn and how fast the river ramps up.
- The scenery changes every 6000 px: forest, farmland, canyon, snow. Night falls and lifts once every 14000 px.
- The river sometimes splits around an island; either channel works, but both banks of the island are as solid as the shore. Islands may carry gun emplacements that swivel to follow you and lob slow shells; dodge them or shoot the gun.
- Boats cross the river back and forth; bridges block the whole channel and take three hits to open. Hitting a rock, boat, gun, bridge or shell rolls the plane into the water; hitting a bank or running dry spirals it in. Either way it costs one plane.
- Pickups float on the water: a **star** for points, a **shield** bubble that pops anything solid it touches for 7 s (banks still count), a **spread** that fires three-way for 10 s, and an **extra plane**.
- Wildlife is harmless: ducks, jumping fish, deer on the banks and the occasional otter. Fly close to an otter to spot it; the game-over panel keeps count.
- In a two-player game the score is shared, each pilot has their own planes and fuel, guns aim at whoever is nearer, and the game ends when both are out.
- Top-ten scores are kept in `highscores.txt`, pilot names in `profiles.txt`, both next to the exe (delete either to start fresh). `BEST` at the top of the screen is the current record.

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

## Layout

| File | Holds |
|---|---|
| `src/Config.h` | Every tunable: sizes, speeds, spawn chances, difficulty presets, stage palettes, animation and sound parameters, version |
| `src/Game.*` | State machine (Title, EnterName, Playing, Paused, GameOver), per-pilot play, collisions, pickups, scoring, HUD and panels |
| `src/Pilot.h` | One player: plane, controls, planes left, fuel, timers, power-ups |
| `src/Input.*` | Keyboard and gamepad bindings per pilot; menu navigation |
| `src/Player.*` | The plane: movement, afterburner, drag chute, spiral and roll crash animations |
| `src/Terrain.*` | River strips that scroll down with random drift and periodically split around an island; banks interpolated between strips, sand shoreline, shallows, trees with reflections; stage palettes; pools of rocks, fuel pumps, boats, guns, bridges, pickups and critters |
| `src/Bullets.*` | Fixed bullet pool (with sideways velocity for the spread shot) |
| `src/Shells.*` | Fixed pool of enemy shells fired by island guns |
| `src/Effects.*` | Fixed pools of particle bursts (rock, fuel, plane, splash) and wake foam |
| `src/Sprites.*` | Textures generated at start-up: 2x sprites with shading, seamless Perlin water and grass tiles, trees, boat, gun, pickups; swap the `Gen*` bodies for `LoadTexture()` when real art exists |
| `src/Audio.*` | Sound bank synthesised at start-up: slurp, shoot, pop, crunch, whine, splash, brake, fanfare, thud, ding, and the music loop |
| `src/HighScores.*` | Top-ten table with names, saved as `highscores.txt` beside the exe |
| `src/Profiles.*` | Pilot names for the title screen, saved as `profiles.txt` beside the exe |

Notes:

- No heap allocation after start-up: pools are `std::array`, every loop has a fixed bound, and each function carries assertions on its inputs and results. Debug builds keep the assertions and a console for raylib's log.
- There are no binary assets in the repo; sprites and sounds are generated in code so the whole game is the source tree.

## Releasing

Bump the version in `src/Config.h`, `vcpkg.json` and the badge above together, build Release, then tag and publish:

```powershell
git tag -a v0.3.0 -m "v0.3.0"
git push origin main v0.3.0
Compress-Archive build\Release\scroller.exe RiverFlyer-v0.3.0-win64.zip
gh release create v0.3.0 RiverFlyer-v0.3.0-win64.zip --title "v0.3.0" --notes-file notes.md
```

## License

[MIT](LICENSE). raylib itself is zlib/libpng licensed; the overlay port under `vcpkg-overlays/raylib` derives from the MIT-licensed vcpkg port.
