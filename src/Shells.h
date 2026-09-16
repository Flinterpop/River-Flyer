#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// Fixed pool of enemy shells fired by island guns. Each flies in a straight
// line at a fixed speed and dies off-screen or after its lifetime.
class Shells {
public:
    void Reset();
    void Fire(Vector2 from, Vector2 velocity);   // silently dropped if the pool is full
    void Update(float dt);
    void Draw() const;

    // Index of the first live shell touching 'r', or -1.
    int Find(const Rectangle& r) const;
    void Kill(int i);

private:
    struct Shell {
        Vector2 pos;
        Vector2 vel;
        float   age;
        bool    active;
    };
    std::array<Shell, cfg::kMaxShells> pool_ {};
};
