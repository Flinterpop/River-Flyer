#!/usr/bin/env bash
# Build the River Flyer iPad / iPhone app on a Mac. Read README.md first.
#
#   ./make-ios.sh                 simulator build: proves the project compiles, no Apple account needed
#   ./make-ios.sh --open          ... then open the project in Xcode to run it on a plugged-in iPad
#   TEAM_ID=ABCDE12345 ./make-ios.sh --device [--install]
#                                 signed device build; --install pushes it to the connected iPad
#
# What it does: takes the version from ../src/Config.h, copies the three files
# of the browser build into RiverFlyer/web (from ../build-web if present, else
# from the matching GitHub release), fills in project.yml, runs XcodeGen to
# make RiverFlyer.xcodeproj, and runs xcodebuild.
set -euo pipefail
cd "$(dirname "$0")"

REPO="Flinterpop/River-Flyer"
DEVICE=0; INSTALL=0; OPEN=0
for arg in "$@"; do
    case "$arg" in
        --device)  DEVICE=1 ;;
        --install) INSTALL=1 ;;
        --open)    OPEN=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

die() { echo "make-ios: $*" >&2; exit 1; }

check_tools() {
    [[ "$(uname)" == "Darwin" ]] || die "this runs on a Mac (Xcode is macOS-only)"
    command -v xcodebuild >/dev/null || die "Xcode is not installed: get it from the App Store, open it once, then re-run"
    xcodebuild -version | head -1
    if ! command -v xcodegen >/dev/null; then
        command -v brew >/dev/null || die "XcodeGen is needed: install Homebrew (https://brew.sh) then re-run, or 'brew install xcodegen'"
        brew install xcodegen
    fi
    [[ -f ../src/Config.h ]] || die "run from the repo's ios/ folder (../src/Config.h not found)"
}

read_version() {
    VERSION="$(sed -n 's/.*kVersion *= *"\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)".*/\1/p' ../src/Config.h)"
    [[ -n "$VERSION" ]] || die "kVersion not found in ../src/Config.h"
    IFS=. read -r MAJOR MINOR PATCH <<< "$VERSION"
    BUILD=$(( MAJOR * 10000 + MINOR * 100 + PATCH ))
    echo "version $VERSION (build $BUILD)"
}

fetch_web() {
    mkdir -p RiverFlyer/web
    if [[ -f ../build-web/index.wasm && -f ../build-web/index.js && -f ../build-web/index.html ]]; then
        echo "using the browser build from ../build-web"
        cp ../build-web/index.html ../build-web/index.js ../build-web/index.wasm RiverFlyer/web/
        return
    fi
    local zip="RiverFlyer-v${VERSION}-web.zip"
    local url="https://github.com/${REPO}/releases/download/v${VERSION}/${zip}"
    local tmp; tmp="$(mktemp -d)"
    echo "downloading ${url}"
    curl -fsSL -o "${tmp}/${zip}" "$url" || die "no browser build in ../build-web and the download failed; build the 'web' preset or check the release exists"
    unzip -q -o "${tmp}/${zip}" -d "${tmp}"
    cp "${tmp}/index.html" "${tmp}/index.js" "${tmp}/index.wasm" RiverFlyer/web/
    rm -rf "$tmp"
}

generate_project() {
    local team="${TEAM_ID:-}"
    sed -e "s/__VERSION__/${VERSION}/" -e "s/__BUILD__/${BUILD}/" -e "s/__TEAM__/${team}/" project.yml > project.generated.yml
    xcodegen generate --spec project.generated.yml --quiet
    [[ -d RiverFlyer.xcodeproj ]] || die "xcodegen produced no project"
    echo "generated RiverFlyer.xcodeproj"
}

build_simulator() {
    echo "building for the iOS Simulator (no signing)"
    xcodebuild -project RiverFlyer.xcodeproj -scheme RiverFlyer -configuration Debug \
        -destination 'generic/platform=iOS Simulator' -derivedDataPath build \
        CODE_SIGNING_ALLOWED=NO build | grep -E "error:|warning:|BUILD" || true
    [[ -d "build/Build/Products/Debug-iphonesimulator/River Flyer.app" ]] || die "simulator build failed (run xcodebuild without the grep to see why)"
    echo "ok: build/Build/Products/Debug-iphonesimulator/River Flyer.app"
}

build_device() {
    [[ -n "${TEAM_ID:-}" ]] || die "--device needs TEAM_ID=<your 10-character Apple team id> (Xcode > Settings > Accounts, or developer.apple.com/account)"
    echo "building for a real iPad / iPhone, team ${TEAM_ID}"
    xcodebuild -project RiverFlyer.xcodeproj -scheme RiverFlyer -configuration Release \
        -destination 'generic/platform=iOS' -derivedDataPath build -allowProvisioningUpdates build \
        | grep -E "error:|warning:|BUILD" || true
    APP="build/Build/Products/Release-iphoneos/River Flyer.app"
    [[ -d "$APP" ]] || die "device build failed; open the project in Xcode (./make-ios.sh --open) and fix Signing & Capabilities"
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
read_version
fetch_web
generate_project
if (( DEVICE )); then
    build_device
    if (( INSTALL )); then install_device; fi
else
    build_simulator
fi
if (( OPEN )); then open RiverFlyer.xcodeproj; fi
echo "done"
