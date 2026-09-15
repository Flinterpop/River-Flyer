#pragma once

#include "raylib.h"

// The player's craft. Moves freely in both axes, clamped to the window.
class Player {
public:
    void Reset();
    void Update(float dt);
    void Draw(const Texture2D& tex, bool visible) const;

    // Axis-aligned hit box in world/screen space.
    Rectangle Bounds() const;

    // Where bullets leave the craft (nose, top centre).
    Vector2 Muzzle() const;

private:
    Vector2 pos_ {0.0f, 0.0f};   // top-left corner
};
