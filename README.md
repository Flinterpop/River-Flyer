# River Flyer

[![Release][release-badge]][release-latest] [![License: MIT][license-badge]][license]

[release-badge]: https://img.shields.io/badge/release-v0.1.1-blue
[release-latest]: https://github.com/Flinterpop/River-Flyer/releases/latest
[license-badge]: https://img.shields.io/badge/license-MIT-green
[license]: LICENSE

*Last updated: 15 Sep 2026*

A vertical river scroller in C++20 and raylib, made for three players aged 7 to 12. Fly up a twisting river, dodge the rocks, shoot what is in the way, and top up at the fuel pumps before the tank runs dry. Three planes per game; lose one and the next arrives with a full tank and a couple of seconds of grace.


<img width="642" height="846" alt="image" src="https://github.com/user-attachments/assets/6c0fc587-461e-4f39-81a9-b7c3d45606c8" />

## Play

Download `RiverFlyer-<version>-win64.zip` from the [latest release][release-latest], unzip, run `scroller.exe`. It is a single static exe with no installer and nothing else to install.

| Key | Action |
|---|---|
| W A S D / arrows | Fly. Up lights the afterburner, down pops a drag chute that slows the river |
| Space | Shoot. Rocks and fuel pumps both go down for 100 points |
| Space / R / Enter | Fly again after losing the last plane |
| F12 | Save `screenshotNNN.png` next to the exe |
| Esc | Quit |

Notes:

- Fuel burns steadily and the bar top-left goes red below a quarter. Fly over a red **FUEL** pump to refill; you keep refilling as long as you sit on it.
- Score is distance plus 100 per kill. The river speeds up gently with distance, to at most double speed.
- Hitting a rock rolls the plane into the water; hitting the bank or running dry spirals it in. Either way it costs one plane.

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
| `src/Config.h` | Every tunable: sizes, speeds, spawn chances, animation and sound parameters, version |
| `src/Game.*` | State machine (Playing, Crashing, GameOver), collisions, fuel, score, HUD |
| `src/Player.*` | The plane: movement, afterburner, drag chute, spiral and roll crash animations |
| `src/Terrain.*` | River strips that scroll down with random drift; rock and fuel-pump pool |
| `src/Bullets.*` | Fixed bullet pool |
| `src/Effects.*` | Fixed pool of particle bursts (rock, fuel, plane, splash) |
| `src/Sprites.*` | Textures drawn procedurally at start-up; swap for `LoadTexture()` when real art exists |
| `src/Audio.*` | Sound bank synthesised at start-up: slurp, shoot, pop, crunch, whine, splash, brake |

Notes:

- No heap allocation after start-up: pools are `std::array`, every loop has a fixed bound, and each function carries assertions on its inputs and results. Debug builds keep the assertions and a console for raylib's log.
- There are no binary assets in the repo; sprites and sounds are generated in code so the whole game is the source tree.

## Releasing

Bump the version in `src/Config.h`, `vcpkg.json` and the badge above together, build Release, then tag and publish:

```powershell
git tag -a v0.1.1 -m "v0.1.1"
git push origin main v0.1.1
Compress-Archive build\Release\scroller.exe RiverFlyer-v0.1.1-win64.zip
gh release create v0.1.1 RiverFlyer-v0.1.1-win64.zip --title "v0.1.1" --notes-file notes.md
```

## License

[MIT](LICENSE). raylib itself is zlib/libpng licensed; the overlay port under `vcpkg-overlays/raylib` derives from the MIT-licensed vcpkg port.
