cmake_minimum_required(VERSION 3.25)
project(scroller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT to match the x64-windows-static vcpkg triplet.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)   # vcpkg's static raylib target does not carry its glfw dependency

add_executable(scroller
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/Terrain.cpp
    src/Bullets.cpp
    src/Sprites.cpp
    src/Effects.cpp
    src/Audio.cpp
)
target_include_directories(scroller PRIVATE src)
target_link_libraries(scroller PRIVATE raylib glfw $<$<PLATFORM_ID:Windows>:winmm gdi32 opengl32 shell32>)

if(MSVC)
    target_compile_options(scroller PRIVATE /W4 /WX /permissive-)
    # Console window off in Release; keep it in Debug for TraceLog output.
    set_target_properties(scroller PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_options(scroller PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
else()
    target_compile_options(scroller PRIVATE -Wall -Wextra -Werror -pedantic)
endif()
