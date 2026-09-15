// Vertical river scroller skeleton - raylib + C++20.
#include <cassert>

#include "raylib.h"

#include "Config.h"
#include "Game.h"

int main()
{
    InitWindow(cfg::kScreenW, cfg::kScreenH, "River Flyer");
    SetTargetFPS(cfg::kTargetFps);
    assert(IsWindowReady());

    {
        Game game;   // scoped so it is destroyed before CloseWindow()
        while (!WindowShouldClose()) {
            // Cap dt so a debugger pause or window drag can't teleport everything.
            const float dt = (GetFrameTime() < 0.1f) ? GetFrameTime() : 0.1f;
            assert(dt >= 0.0f && dt <= 0.1f);
            game.Update(dt);
            if (IsKeyPressed(KEY_F12)) { TakeScreenshot("screenshot.png"); }   // saved next to the exe

            BeginDrawing();
            ClearBackground(BLACK);
            game.Draw();
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}
