cmake_minimum_required(VERSION 3.25)
project(scroller LANGUAGES CXX)
if(IOS)
    enable_language(C OBJC)   # raylib is C; miniaudio's iOS backend must be compiled as Objective-C
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EMSCRIPTEN OR ANDROID)
    # Browser build (preset "web") and Android build (android/ Gradle project):
    # raylib is compiled from the same 6.0 source tarball vcpkg uses, for
    # PLATFORM=Web or PLATFORM=Android. vcpkg has no triplet for either, and
    # building upstream directly also sidesteps the vcpkg port's de-vendored
    # miniaudio (see vcpkg-overlays).
    include(FetchContent)
    if(EMSCRIPTEN)
        # raylib is pinned by hash; the compiler that builds it is not, because
        # it lives in C:/emsdk outside this tree. Say which version the released
        # build was made with, so an emsdk upgrade is noticed rather than
        # silently shipping a differently-built wasm. Warn, never fail: a newer
        # toolchain usually works, and -DRF_EMSDK_VERSION=<v> retunes the check.
        set(RF_EMSDK_VERSION "6.0.9" CACHE STRING "Emscripten version the web build is tested with")
        if(NOT EMSCRIPTEN_VERSION VERSION_EQUAL RF_EMSDK_VERSION)
            message(WARNING "Emscripten ${EMSCRIPTEN_VERSION} is not the tested ${RF_EMSDK_VERSION}. "
                            "Test the page before releasing, then update RF_EMSDK_VERSION in CMakeLists.txt.")
        endif()
        set(PLATFORM       "Web"    CACHE STRING "raylib platform" FORCE)
        set(OPENGL_VERSION "ES 3.0" CACHE STRING "raylib GL"       FORCE)   # WebGL2: mipmaps on the NPOT sprites
    else()
        set(PLATFORM       "Android" CACHE STRING "raylib platform" FORCE)
    endif()
    set(BUILD_EXAMPLES OFF      CACHE BOOL   "raylib examples" FORCE)
    FetchContent_Declare(raylib
        URL      https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
        URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    )
    FetchContent_MakeAvailable(raylib)
    if(EMSCRIPTEN)
        # raylib exports only ccall to the page. The shell also needs callMain (the
        # game starts on the Play press). A later -sEXPORTED_RUNTIME_METHODS replaces
        # an earlier one rather than adding to it, so patch raylib's flag in place.
        get_target_property(RF_RAYLIB_LINK raylib INTERFACE_LINK_OPTIONS)
        list(TRANSFORM RF_RAYLIB_LINK REPLACE "^-sEXPORTED_RUNTIME_METHODS=.*$" "-sEXPORTED_RUNTIME_METHODS=ccall,callMain")
        set_target_properties(raylib PROPERTIES INTERFACE_LINK_OPTIONS "${RF_RAYLIB_LINK}")
    endif()
elseif(IOS)
    # iPad / iPhone build (ios/ folder, CMake's own iOS toolchain with the
    # Xcode generator). raylib has no iOS platform, so it runs on its SDL
    # backend over SDL3 (UIKit window, multi-touch, Game Controller framework)
    # drawing with OpenGL ES 3.0. raylib's CMake hard-codes find_package(SDL2)
    # for that backend, so its sources are compiled here directly, from the
    # same 6.0 tarball, with one patch: the SDL backend never fills raylib's
    # screenScale matrix, so on a Retina display it would draw in the bottom
    # left third of the screen. ios/raylib-sdl-ios.patch reads the point and
    # pixel sizes from the window and sets it (see the patch header).
    include(FetchContent)
    FetchContent_Declare(raylib
        URL      https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
        URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
        PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE=<SOURCE_DIR> -DPATCH=${CMAKE_SOURCE_DIR}/ios/raylib-sdl-ios.patch -P ${CMAKE_SOURCE_DIR}/ios/apply-patch.cmake
        SOURCE_SUBDIR none   # populate only: raylib's CMakeLists.txt is not used
    )
    FetchContent_Declare(SDL3
        URL      https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.16.tar.gz
        URL_HASH SHA256=c2ee715e42ec520c4d11fd8d249ef1d2b2baf4ad31148b72b3b000276c0b3633
    )
    set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib SDL3)

    add_library(raylib STATIC
        ${raylib_SOURCE_DIR}/src/rcore.c
        ${raylib_SOURCE_DIR}/src/rshapes.c
        ${raylib_SOURCE_DIR}/src/rtextures.c
        ${raylib_SOURCE_DIR}/src/rtext.c
        ${raylib_SOURCE_DIR}/src/rmodels.c
        ${raylib_SOURCE_DIR}/src/raudio.c
    )
    set_source_files_properties(${raylib_SOURCE_DIR}/src/raudio.c PROPERTIES LANGUAGE OBJC)
    target_include_directories(raylib
        PUBLIC  ${raylib_SOURCE_DIR}/src
        PRIVATE ${CMAKE_SOURCE_DIR}/ios/glshim   # <GLES3/gl3.h> -> <OpenGLES/ES3/gl.h>
    )
    target_compile_definitions(raylib
        PUBLIC  PLATFORM_DESKTOP_SDL GRAPHICS_API_OPENGL_ES3 USING_SDL3_PROJECT
        PRIVATE MA_NO_RUNTIME_LINKING   # miniaudio: link Core Audio directly (App Store notarisation)
    )
    target_compile_options(raylib PRIVATE -Wno-deprecated-declarations)   # OpenGL ES is deprecated by Apple but present
    target_link_libraries(raylib PUBLIC SDL3::SDL3-static
        "-framework OpenGLES" "-framework AVFoundation" "-framework AudioToolbox" "-framework CoreAudio"
    )
else()
    # Static CRT to match the x64-windows-static vcpkg triplet.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    find_package(raylib CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)   # vcpkg's static raylib target does not carry its glfw dependency
endif()

set(RF_SOURCES
    src/main.cpp
    src/Canvas.cpp
    src/Screen.cpp
    src/Haptics.cpp
    src/Game.cpp
    src/Player.cpp
    src/Terrain.cpp
    src/Bullets.cpp
    src/Boss.cpp
    src/Sprites.cpp
    src/Effects.cpp
    src/Audio.cpp
    src/HighScores.cpp
    src/Shells.cpp
    src/Input.cpp
    src/Profiles.cpp
    src/Missiles.cpp
    src/Storage.cpp
    src/Touch.cpp
)

if(ANDROID)
    # NativeActivity loads the game as libscroller.so (android.app.lib_name in
    # the manifest) and calls ANativeActivity_onCreate, which lives in the NDK's
    # native_app_glue compiled into raylib: -u keeps it from being dropped as
    # unreferenced. raylib's android_main() then calls our main().
    add_library(scroller SHARED ${RF_SOURCES})
    target_include_directories(scroller PRIVATE "${ANDROID_NDK}/sources/android/native_app_glue")   # Storage.cpp: android_app
    # 16 KB page alignment: Android 15+ warns (and Google Play refuses) otherwise.
    target_link_options(scroller PRIVATE "-u" "ANativeActivity_onCreate" "-Wl,-z,max-page-size=16384")
elseif(IOS)
    # The version is read from src/Config.h like the Android APK does, so
    # every build carries the same number. CFBundleVersion = MMmmpp.
    file(STRINGS src/Config.h RF_VERSION_LINE REGEX "kVersion *= *\"[0-9]+\\.[0-9]+\\.[0-9]+\"")
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" RF_VERSION "${RF_VERSION_LINE}")
    if(NOT RF_VERSION)
        message(FATAL_ERROR "kVersion not found in src/Config.h")
    endif()
    math(EXPR RF_BUILD_DEFAULT "${CMAKE_MATCH_1} * 10000 + ${CMAKE_MATCH_2} * 100 + ${CMAKE_MATCH_3}")
    # App Store Connect refuses a second upload with the same build number, so
    # -DRF_BUILD=<n> can bump it without changing the game's version.
    set(RF_BUILD "${RF_BUILD_DEFAULT}" CACHE STRING "CFBundleVersion for the iOS build")
    set(RF_TEAM_ID "" CACHE STRING "Apple developer team id for signing a device build (empty: simulator only)")

    add_executable(scroller MACOSX_BUNDLE ${RF_SOURCES} src/Haptics.mm ios/Assets.xcassets)
    set_source_files_properties(ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)   # Xcode compiles the icon into Assets.car
    set_target_properties(scroller PROPERTIES
        OUTPUT_NAME                                 "River Flyer"
        MACOSX_BUNDLE_INFO_PLIST                    "${CMAKE_SOURCE_DIR}/ios/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME                   "River Flyer"
        MACOSX_BUNDLE_GUI_IDENTIFIER                ca.rabidfox.riverflyer
        MACOSX_BUNDLE_SHORT_VERSION_STRING          "${RF_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION                "${RF_BUILD}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER   ca.rabidfox.riverflyer
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY      "1,2"                 # iPhone and iPad
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE             Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM            "${RF_TEAM_ID}"
        XCODE_ATTRIBUTE_ENABLE_BITCODE              NO
        XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC       NO                    # miniaudio manages its own retain/release
        # Without these an `xcodebuild archive` produces an empty archive, so
        # the app can't be exported for TestFlight or the App Store.
        XCODE_ATTRIBUTE_SKIP_INSTALL                NO
        XCODE_ATTRIBUTE_INSTALL_PATH                "$(LOCAL_APPS_DIR)"
    )
else()
    add_executable(scroller ${RF_SOURCES})
endif()
target_include_directories(scroller PRIVATE src)

if(EMSCRIPTEN)
    target_link_libraries(scroller PRIVATE raylib)
    # index.html + index.js + index.wasm, so the game is the site root when served.
    set_target_properties(scroller PROPERTIES OUTPUT_NAME index SUFFIX ".html")
    set(RF_SHELL "${CMAKE_SOURCE_DIR}/web/shell.html")
    set_property(TARGET scroller APPEND PROPERTY LINK_DEPENDS "${RF_SHELL}")
    target_link_options(scroller PRIVATE
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "--shell-file=${RF_SHELL}"
    )
elseif(ANDROID)
    target_link_libraries(scroller PRIVATE raylib EGL)   # EGL: main.cpp checks for a live context
elseif(IOS)
    target_link_libraries(scroller PRIVATE raylib)       # SDL3 and the frameworks come with it
    target_compile_definitions(scroller PRIVATE RF_IOS)  # the game's own host switch (clang has no single iOS macro)
else()
    target_link_libraries(scroller PRIVATE raylib glfw $<$<PLATFORM_ID:Windows>:winmm gdi32 opengl32 shell32>)
endif()

if(MSVC)
    target_compile_options(scroller PRIVATE /W4 /WX /permissive-)
    # Console window off in Release; keep it in Debug for TraceLog output.
    set_target_properties(scroller PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_options(scroller PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
else()
    target_compile_options(scroller PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

# ---------------------------------------------------------------------------
# Tests: headless checks of the river generator and the two saved records
# (ctest, or run build/tests/<config>/rf_tests directly). Desktop hosts only -
# the phone and browser builds have no test runner to speak of, and the game
# sources they share are the ones being covered here.
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT IOS)
    enable_testing()
    set(RF_TEST_SOURCES ${RF_SOURCES})
    list(REMOVE_ITEM RF_TEST_SOURCES src/main.cpp)   # the test binary brings its own main()
    add_executable(rf_tests tests/rf_tests.cpp ${RF_TEST_SOURCES})
    target_include_directories(rf_tests PRIVATE src)
    target_link_libraries(rf_tests PRIVATE raylib glfw $<$<PLATFORM_ID:Windows>:winmm gdi32 opengl32 shell32>)
    # The game's own assertions are half of what these tests check, so keep them
    # in Release too: -U undefines NDEBUG after CMake's config flags have set it.
    # MSVC says "D9025: overriding '/DNDEBUG' with '/UNDEBUG'" for this target;
    # that is the intent. The shipped exe keeps NDEBUG and its assertions off.
    target_compile_options(rf_tests PRIVATE $<IF:$<BOOL:${MSVC}>,/UNDEBUG,-UNDEBUG>)
    if(MSVC)
        target_compile_options(rf_tests PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(rf_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    endif()
    # Its own folder: the records it writes must not land on the game's.
    set_target_properties(rf_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME rf_tests COMMAND rf_tests)
endif()
