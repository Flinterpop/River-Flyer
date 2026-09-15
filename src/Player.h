#pragma once

#include "raylib.h"

// The player's craft. Moves freely in both axes, clamped to the window.
class Player {
public:
    void Reset();
    void Update(float dt);
    void Draw() const;

    // Axis-aligned hit box in world/screen space.
    Rectangle Bounds() const;

private:
    Vector2 pos_ {0.0f, 0.0f};   // top-left corner
};
