// Vertical river scroller skeleton - raylib + C++20.
#include <cassert>

#include "raylib.h"

#include "Config.h"
#include "Game.h"

int main()
{
    InitWindow(cfg::kScreenW, cfg::kScreenH, TextFormat("%s v%s", cfg::kTitle, cfg::kVersion));
    SetTargetFPS(cfg::kTargetFps);
    SetExitKey(KEY_NULL);   // Esc pauses; Q on the title screen quits
    assert(IsWindowReady());

    {
        Game game;   // scoped so it is destroyed before CloseWindow()
        while (!WindowShouldClose() && !game.WantsQuit()) {
            // Cap dt so a debugger pause or window drag can't teleport everything.
            const float dt = (GetFrameTime() < 0.1f) ? GetFrameTime() : 0.1f;
            assert(dt >= 0.0f && dt <= 0.1f);
            game.Update(dt);

            BeginDrawing();
            ClearBackground(BLACK);
            game.Draw();
            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}
