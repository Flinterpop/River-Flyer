#pragma once

#include "raylib.h"

// The game draws at cfg::kScreenW x screen::H() (the design height on the
// desktop, taller on a phone or tablet; see Screen.h). Canvas owns an
// off-screen texture of that size and presents it scaled to whatever the real
// window is: 1:1 on the desktop, pillarboxed on a landscape TV in the browser
// build, filling or nearly filling a phone or tablet.
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

    // Where the canvas lands in the window this frame (window pixels). The
    // touch layer uses it to map fingers back into canvas coordinates.
    static Rectangle Placement();

private:
    RenderTexture2D target_ {};
};
