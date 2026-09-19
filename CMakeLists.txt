cmake_minimum_required(VERSION 3.25)
project(scroller LANGUAGES CXX)

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
else()
    # Static CRT to match the x64-windows-static vcpkg triplet.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    find_package(raylib CONFIG REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)   # vcpkg's static raylib target does not carry its glfw dependency
endif()

set(RF_SOURCES
    src/main.cpp
    src/Canvas.cpp
    src/Game.cpp
    src/Player.cpp
    src/Terrain.cpp
    src/Bullets.cpp
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
