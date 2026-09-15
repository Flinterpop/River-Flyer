#pragma once

#include "raylib.h"

#include "Config.h"

// The player's craft. Moves freely in both axes, clamped to the window.
// Two modes: flying (input-driven) and crashing (a scripted spiral that
// ignores input and collisions until it finishes).
class Player {
public:
    void Reset();
    void Update(float dt);                       // flying only
    void Draw(const Texture2D& tex, bool visible) const;

    // Axis-aligned hit box in world/screen space (flying only).
    Rectangle Bounds() const;

    // Where bullets leave the craft (nose, top centre).
    Vector2 Muzzle() const;

    // Centre of the sprite as currently drawn (valid in both modes).
    Vector2 Centre() const;

    // Spiral crash: call BeginCrash(), then UpdateCrash() each frame until
    // CrashFinished() reports true.
    void BeginCrash();
    void UpdateCrash(float dt);
    bool Crashing() const { return crashT_ >= 0.0f; }
    bool CrashFinished() const { return crashT_ >= cfg::kCrashSeconds; }

private:
    void DrawFlying(const Texture2D& tex) const;
    void DrawFlame() const;
    void DrawCrashing(const Texture2D& tex) const;
    float CrashProgress() const;                 // 0 .. 1
    Vector2 CentreAt(float t) const;             // spiral position at crash progress t
    void DrawSmokeTrail() const;

    Vector2 pos_       {0.0f, 0.0f};             // top-left corner while flying
    bool    thrusting_ {false};                  // up key held this frame
    float   crashT_    {-1.0f};                  // seconds into the crash, or -1 when flying
    Vector2 crashOrigin_ {0.0f, 0.0f};           // sprite centre where the crash began
};
