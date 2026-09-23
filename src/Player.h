#pragma once

#include "raylib.h"

#include "Config.h"
#include "Input.h"

// The player's craft. Moves freely in both axes, clamped to the window.
// Two modes: flying (input-driven) and crashing (a scripted spiral that
// ignores input and collisions until it finishes).
class Player {
public:
    void Reset(float xOffset = 0.0f);            // start position, shifted sideways for a second pilot
    void Update(float dt, const InputMap& map);  // flying only
    void Draw(const Texture2D& tex, bool visible) const;

    // Axis-aligned hit box in world/screen space (flying only).
    Rectangle Bounds() const;

    // Where bullets leave the craft (nose, top centre).
    Vector2 Muzzle() const;

    // Push the craft sideways and downstream after a bank scrape, so it
    // bounces clear instead of grinding along the shore.
    void Bounce(float dirX);

    // Down key held this frame (drag chute out).
    bool Braking() const { return braking_; }

    // Centre of the sprite as currently drawn (valid in both modes).
    Vector2 Centre() const;

    // Two scripted crashes: Spiral (bank hit: tumbles in a shrinking spiral)
    // and Roll (rock hit: barrel-rolls, spews smoke, then sinks). Call
    // BeginCrash(), then UpdateCrash() each frame until CrashFinished().
    enum class CrashStyle { Spiral, Roll };
    void BeginCrash(CrashStyle style);
    void UpdateCrash(float dt);
    bool Crashing() const { return crashT_ >= 0.0f; }
    bool CrashFinished() const { return crashT_ >= CrashSeconds(); }
    CrashStyle Style() const { return crashStyle_; }

private:
    void DrawFlying(const Texture2D& tex) const;
    void DrawFlame() const;
    void DrawChute() const;
    float CrashSeconds() const;                  // duration of the current crash style
    float CrashProgress() const;                 // 0 .. 1
    Vector2 CentreAt(float t) const;             // crash-path position at progress t
    void DrawSpiral(const Texture2D& tex) const;
    void DrawSpiralSmoke() const;
    void DrawRoll(const Texture2D& tex) const;
    void DrawRollSmoke() const;

    Vector2 pos_       {0.0f, 0.0f};             // top-left corner while flying
    bool    thrusting_ {false};                  // up key held this frame
    bool    braking_   {false};                  // down key held this frame
    float   chute_     {0.0f};                   // chute inflation 0 .. 1
    float   crashT_    {-1.0f};                  // seconds into the crash, or -1 when flying
    CrashStyle crashStyle_ {CrashStyle::Spiral};
    Vector2 crashOrigin_ {0.0f, 0.0f};           // sprite centre where the crash began
};
