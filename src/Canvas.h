#pragma once

#include "raylib.h"

// The game always draws at cfg::kScreenW x kScreenH. Canvas owns an off-screen
// texture of that size and presents it scaled to whatever the real window is:
// 1:1 on the desktop, pillarboxed on a landscape TV in the browser build.
// RAII around the render texture: construct after InitWindow(), destroy before
// CloseWindow().
class Canvas {
public:
    Canvas();
    ~Canvas();
    Canvas(const Canvas&)            = delete;
    Canvas& operator=(const Canvas&) = delete;

    void Begin();           // subsequent draws go to the off-screen texture
    void End();
    void Present() const;   // inside BeginDrawing/EndDrawing: blit to the window, letterboxed

private:
    RenderTexture2D target_ {};
};
