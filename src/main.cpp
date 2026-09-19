// Vertical river scroller - raylib + C++20.
//
// Three hosts share this file. On the desktop main() owns the frame loop. In
// the browser build (Emscripten) the browser owns it: emscripten_set_main_loop_arg
// calls WebTick once per display refresh and never returns to main. On Android
// raylib's android_main() calls main() on the activity's thread and the loop
// is ours again; the activity ends when main returns.
#include <cassert>
#include <cstring>

#include "raylib.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#if defined(__ANDROID__)
#include <EGL/egl.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include "rlgl.h"
extern "C" struct android_app* GetAndroidApp(void);   // raylib exports it but does not declare it
#endif

#include "Canvas.h"
#include "Config.h"
#include "Game.h"
#include "Touch.h"

namespace {

// A frame this long means the host was away (backgrounded app, hidden tab,
// debugger): pause rather than drop the player back into the action.
constexpr float kAwaySeconds = 0.5f;

// Everything that must exist between InitWindow() and CloseWindow().
struct Session {
    Canvas canvas;   // constructed first: the game draws into it
    Game   game;
};

void Tick(Session& s)
{
    // Cap dt so a debugger pause, a window drag or a background tab can't teleport everything.
    const float raw = GetFrameTime();
    const float dt  = (raw < 0.1f) ? raw : 0.1f;
    assert(dt >= 0.0f && dt <= 0.1f);
    if (raw > kAwaySeconds) { s.game.Pause(); }

    touch::ShowControls(s.game.Playing());
    touch::Update();
    s.game.Update(dt);

    s.canvas.Begin();
    ClearBackground(BLACK);
    s.game.Draw();
    s.canvas.End();

    BeginDrawing();
    ClearBackground(BLACK);
    s.canvas.Present();
    touch::Draw();
    EndDrawing();
}

#if defined(__EMSCRIPTEN__)
void WebTick(void* arg)
{
    assert(arg != nullptr);
    Tick(*static_cast<Session*>(arg));
}
#endif

#if defined(__ANDROID__)
// Sticky immersive mode: the status and navigation bars go away and the
// window covers the whole display, so the touch buttons are not under the
// system's bar. Called before InitWindow() so the surface is created at the
// final size; raylib does not follow a resize afterwards. Java's
// View.setSystemUiVisibility() is reached through JNI, since the app has no
// Java of its own.
void HideSystemBars()
{
    constexpr jint kLayoutStable         = 0x00000100;
    constexpr jint kLayoutHideNavigation = 0x00000200;
    constexpr jint kLayoutFullscreen     = 0x00000400;
    constexpr jint kHideNavigation       = 0x00000002;
    constexpr jint kFullscreen           = 0x00000004;
    constexpr jint kImmersiveSticky      = 0x00001000;

    android_app* app = GetAndroidApp();
    assert(app != nullptr && app->activity != nullptr);
    JavaVM* vm  = app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || env == nullptr) {
        TraceLog(LOG_WARNING, "ANDROID: could not attach to the JVM; system bars stay");
        return;
    }
    jobject   activity   = app->activity->clazz;
    jclass    actClass   = env->GetObjectClass(activity);
    jmethodID getWindow  = env->GetMethodID(actClass, "getWindow", "()Landroid/view/Window;");
    jobject   window     = env->CallObjectMethod(activity, getWindow);
    jclass    winClass   = env->GetObjectClass(window);
    jmethodID getDecor   = env->GetMethodID(winClass, "getDecorView", "()Landroid/view/View;");
    jobject   decor      = env->CallObjectMethod(window, getDecor);
    jclass    viewClass  = env->GetObjectClass(decor);
    jmethodID setUiFlags = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
    assert(getWindow != nullptr && getDecor != nullptr && setUiFlags != nullptr);
    env->CallVoidMethod(decor, setUiFlags, kLayoutStable | kLayoutHideNavigation | kLayoutFullscreen | kHideNavigation | kFullscreen | kImmersiveSticky);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        TraceLog(LOG_WARNING, "ANDROID: setSystemUiVisibility threw; system bars stay");
    }
    env->DeleteLocalRef(viewClass);
    env->DeleteLocalRef(decor);
    env->DeleteLocalRef(winClass);
    env->DeleteLocalRef(window);
    env->DeleteLocalRef(actClass);
    vm->DetachCurrentThread();
}

// A system dialog or a call can take the window away while InitWindow() is
// still running: raylib's second GL setup pass then runs with no context and
// leaves its default texture, shader and batch zeroed. Wait for the surface
// to come back (the INIT_WINDOW that follows rebinds it) and redo that pass.
// Returns false if the activity was destroyed while waiting.
bool RecoverWindow()
{
    constexpr int   kMaxPolls = 60 * 60;   // a minute at the poll rate below, then give up
    constexpr float kPollSec  = 1.0f / 60.0f;
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) { return true; }
    TraceLog(LOG_WARNING, "ANDROID: window lost during start-up, waiting for it to return");
    for (int i = 0; i < kMaxPolls && eglGetCurrentContext() == EGL_NO_CONTEXT; ++i) {
        if (WindowShouldClose()) { return false; }   // pumps the activity's events
        WaitTime(kPollSec);
    }
    if (eglGetCurrentContext() == EGL_NO_CONTEXT) { return false; }
    rlglInit(GetRenderWidth(), GetRenderHeight());   // the default font survived: it is loaded once, by the first pass
    assert(GetFontDefault().texture.id != 0);
    return true;
}
#endif

} // namespace

int main(int argc, char* argv[])
{
    assert(argc >= 1 && argv != nullptr);
#if defined(__ANDROID__)
    (void)argc; (void)argv;
    touch::SetEnabled(true);
    HideSystemBars();
    InitWindow(0, 0, cfg::kTitle);   // 0 x 0: the whole display, so the canvas letterboxes itself
#else
    // --touch: try the phone layout on the desktop, with the mouse as a finger.
    touch::SetEnabled(argc > 1 && std::strcmp(argv[1], "--touch") == 0);
#if defined(__EMSCRIPTEN__)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);   // the canvas follows the browser window and fullscreen
#endif
    InitWindow(cfg::kScreenW, cfg::kScreenH, TextFormat("%s v%s", cfg::kTitle, cfg::kVersion));
#endif
    SetExitKey(KEY_NULL);   // Esc pauses; Q on the title screen quits
    assert(IsWindowReady());
#if defined(__ANDROID__)
    if (!RecoverWindow()) { CloseWindow(); return 0; }
#endif

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
