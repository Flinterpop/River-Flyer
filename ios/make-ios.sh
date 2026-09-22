#!/usr/bin/env bash
# Build the River Flyer iPad / iPhone app on a Mac. Read README.md first.
#
#   ./make-ios.sh                   simulator build (no Apple account needed)
#   ./make-ios.sh --run             ... and install + launch it on the booted simulator
#   ./make-ios.sh --open            ... then open the Xcode project (run on a plugged-in iPad from there)
#   TEAM_ID=ABCDE12345 ./make-ios.sh --device [--install]
#                                   signed device build; --install pushes it to the connected iPad
#
# What it does: configures the repo's CMake project for iOS with the Xcode
# generator (preset "ios" in ../CMakePresets.json), which fetches raylib 6.0
# and SDL3, then builds build-ios/RiverFlyer.xcodeproj with xcodebuild.
set -euo pipefail
cd "$(dirname "$0")/.."

DEVICE=0; INSTALL=0; OPEN=0; RUN=0; CONFIG=Release
for arg in "$@"; do
    case "$arg" in
        --device)  DEVICE=1 ;;
        --install) INSTALL=1 ;;
        --open)    OPEN=1 ;;
        --run)     RUN=1 ;;
        --debug)   CONFIG=Debug ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

die() { echo "make-ios: $*" >&2; exit 1; }

check_tools() {
    [[ "$(uname)" == "Darwin" ]] || die "this runs on a Mac (Xcode is macOS-only)"
    command -v xcodebuild >/dev/null || die "Xcode is not installed: get it from the App Store, open it once, then re-run"
    xcodebuild -version | head -1
    # CMake: Homebrew, the CMake.app bundle, or a pip install (pip3 install --user cmake).
    for dir in /opt/homebrew/bin /usr/local/bin /Applications/CMake.app/Contents/bin "$HOME/Library/Python/3.9/bin" "$HOME/Library/Python/3.13/bin"; do
        [[ -x "$dir/cmake" ]] && PATH="$dir:$PATH"
    done
    command -v cmake >/dev/null || die "CMake is needed: 'brew install cmake', or 'pip3 install --user cmake' (no Homebrew required)"
    cmake --version | head -1
}

configure() {
    local team="${TEAM_ID:-}"
    cmake --preset ios -DRF_TEAM_ID="$team" > build-ios/configure.log 2>&1 \
        || { tail -30 build-ios/configure.log; die "configure failed (full log: build-ios/configure.log)"; }
    echo "configured build-ios/scroller.xcodeproj"
}

# xcodebuild prints thousands of lines; keep the ones that matter.
run_xcodebuild() {
    set -o pipefail
    xcodebuild "$@" 2>&1 | tee build-ios/build.log | grep -E "error:|warning: .*src/|BUILD (SUCCEEDED|FAILED)" || true
    [[ "${PIPESTATUS[0]}" -eq 0 ]] || die "build failed (full log: build-ios/build.log)"
}

build_simulator() {
    echo "building $CONFIG for the iOS Simulator (no signing)"
    run_xcodebuild -project build-ios/scroller.xcodeproj -scheme scroller -configuration "$CONFIG" \
        -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO -quiet build
    APP="build-ios/$CONFIG-iphonesimulator/River Flyer.app"
    [[ -d "$APP" ]] || die "no app bundle at $APP"
    echo "ok: $APP"
}

# Installs and launches on the booted simulator, booting an iPad if none is.
# The device runs headless from the command line; its window is Xcode's
# Simulator app (Xcode 26 and earlier) or Device Hub (Xcode 27), opened here if
# found. A screenshot works either way: xcrun simctl io <id> screenshot x.png
run_simulator() {
    local uuid='[0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}'
    local booted
    booted="$(xcrun simctl list devices booted | grep -oE "$uuid" | head -1 || true)"
    if [[ -z "$booted" ]]; then
        booted="$(xcrun simctl list devices available | grep -i 'iPad' | grep -oE "$uuid" | head -1 || true)"
        [[ -n "$booted" ]] || die "no iOS simulator available; open Xcode > Settings > Components and install one"
        xcrun simctl boot "$booted"
    fi
    local dev; dev="$(xcode-select -p)"
    for app in "$dev/Applications/Simulator.app" "$dev/../Applications/Simulator.app" "$dev/../Applications/DeviceHub.app"; do
        [[ -d "$app" ]] && { open "$app"; break; }
    done
    xcrun simctl install "$booted" "$APP"
    xcrun simctl launch "$booted" ca.rabidfox.riverflyer
    echo "launched on simulator $booted (the simulator has no audio device; see README)"
}

build_device() {
    [[ -n "${TEAM_ID:-}" ]] || die "--device needs TEAM_ID=<your 10-character Apple team id> (Xcode > Settings > Accounts, or developer.apple.com/account)"
    echo "building $CONFIG for a real iPad / iPhone, team ${TEAM_ID}"
    run_xcodebuild -project build-ios/scroller.xcodeproj -scheme scroller -configuration "$CONFIG" \
        -destination 'generic/platform=iOS' -allowProvisioningUpdates -quiet build
    APP="build-ios/$CONFIG-iphoneos/River Flyer.app"
    [[ -d "$APP" ]] || die "no app bundle at $APP; open the project (./make-ios.sh --open) and check Signing & Capabilities"
    echo "ok: $APP"
}

install_device() {
    local id
    id="$(xcrun devicectl list devices 2>/dev/null | grep -i connected | grep -oiE '[0-9a-f]{8}-([0-9a-f]{4}-){3}[0-9a-f]{12}' | head -1)"
    [[ -n "$id" ]] || die "no connected iPad / iPhone found by devicectl (plug it in, unlock it, and trust this Mac)"
    xcrun devicectl device install app --device "$id" "$APP"
    echo "installed on device $id; the first launch of a free-account build needs Settings > General > VPN & Device Management > trust"
}

check_tools
mkdir -p build-ios
configure
if (( DEVICE )); then
    build_device
    if (( INSTALL )); then install_device; fi
else
    build_simulator
    if (( RUN )); then run_simulator; fi
fi
if (( OPEN )); then open build-ios/scroller.xcodeproj; fi
echo "done"
