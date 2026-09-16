#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// Fixed pool of upward-travelling shots.
class Bullets {
public:
    void Reset();
    void Fire(Vector2 muzzle, float vx = 0.0f);   // silently drops the shot if the pool is full
    void Update(float dt);
    void Draw(const Texture2D& tex) const;

    // Pool access for collision checks in Game.
    static constexpr int Capacity() { return cfg::kMaxBullets; }
    bool      Active(int i) const;
    Rectangle Bounds(int i) const;
    void      Kill(int i);

private:
    struct Bullet {
        Rectangle rect;
        float     vx;
        bool      active;
    };
    std::array<Bullet, cfg::kMaxBullets> pool_ {};
};
