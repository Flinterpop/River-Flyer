# Applies raylib-sdl-ios.patch to the fetched raylib source, once. CMake runs
# it as the FetchContent PATCH_COMMAND, which can be re-run on an already
# patched tree (e.g. after this file changes), so it checks first.
#   cmake -DSOURCE=<raylib dir> -DPATCH=<patch file> -P apply-patch.cmake
file(READ "${SOURCE}/src/platforms/rcore_desktop_sdl.c" RF_SDL)
file(READ "${SOURCE}/src/rlgl.h" RF_RLGL)
file(READ "${SOURCE}/src/raudio.c" RF_AUDIO)
if(RF_SDL MATCHES "UpdateWindowSizeSDL" AND RF_RLGL MATCHES "RL_DEFAULT_FRAMEBUFFER" AND RF_AUDIO MATCHES "RAYLIB_SIMULATOR_AUDIO")
    message(STATUS "raylib: iOS patch already applied")
    return()
elseif(RF_SDL MATCHES "UpdateWindowSizeSDL" OR RF_RLGL MATCHES "RL_DEFAULT_FRAMEBUFFER" OR RF_AUDIO MATCHES "RAYLIB_SIMULATOR_AUDIO")
    message(FATAL_ERROR "raylib: an older iOS patch is applied; delete build-ios/_deps/raylib-* and configure again")
endif()
execute_process(
    COMMAND patch -p1 -i "${PATCH}"
    WORKING_DIRECTORY "${SOURCE}"
    RESULT_VARIABLE RF_RESULT
)
if(NOT RF_RESULT EQUAL 0)
    message(FATAL_ERROR "raylib: iOS patch failed to apply (${RF_RESULT})")
endif()
