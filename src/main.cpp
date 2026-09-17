// Vertical river scroller - raylib + C++20.
//
// Two hosts share this file. On the desktop main() owns the frame loop. In the
// browser build (Emscripten) the browser owns it: emscripten_set_main_loop_arg
// calls WebTick once per display refresh and never returns to main.
#include <cassert>

#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "Canvas.h"
#include "Config.h"
#include "Game.h"

namespace {

// Everything that must exist between InitWindow() and CloseWindow().
struct Session {
    Canvas canvas;   // constructed first: the game draws into it
    Game   game;
};

void Tick(Session& s)
{
    // Cap dt so a debugger pause, a window drag or a background tab can't teleport everything.
    const float dt = (GetFrameTime() < 0.1f) ? GetFrameTime() : 0.1f;
    assert(dt >= 0.0f && dt <= 0.1f);
    s.game.Update(dt);

    s.canvas.Begin();
    ClearBackground(BLACK);
    s.game.Draw();
    s.canvas.End();

    BeginDrawing();
    ClearBackground(BLACK);
    s.canvas.Present();
    EndDrawing();
}

#if defined(__EMSCRIPTEN__)
void WebTick(void* arg)
{
    assert(arg != nullptr);
    Tick(*static_cast<Session*>(arg));
}
#endif

} // namespace

int main()
{
#if defined(__EMSCRIPTEN__)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);   // the canvas follows the browser window and fullscreen
#endif
    InitWindow(cfg::kScreenW, cfg::kScreenH, TextFormat("%s v%s", cfg::kTitle, cfg::kVersion));
    SetExitKey(KEY_NULL);   // Esc pauses; Q on the title screen quits
    assert(IsWindowReady());

#if defined(__EMSCRIPTEN__)
    // Static: the browser loop unwinds main's stack without running destructors,
    // so the session must not live on it. The page never "exits"; closing the tab is quitting.
    static Session session;
    emscripten_set_main_loop_arg(WebTick, &session, 0, 1);   // 0 fps = the display's refresh rate
#else
    SetTargetFPS(cfg::kTargetFps);
    {
        Session session;   // scoped so it is destroyed before CloseWindow()
        while (!WindowShouldClose() && !session.game.WantsQuit()) { Tick(session); }
    }
    CloseWindow();
#endif
    return 0;
}
