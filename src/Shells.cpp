#include "Shells.h"
#include "Screen.h"

#include <cassert>

void Shells::Reset()
{
    for (Shell& s : pool_) {
        s.active = false;
        s.age    = 0.0f;
        s.pos    = Vector2 {0.0f, 0.0f};
        s.vel    = Vector2 {0.0f, 0.0f};
    }
}

void Shells::Fire(Vector2 from, Vector2 velocity)
{
    assert(velocity.x != 0.0f || velocity.y != 0.0f);
    for (Shell& s : pool_) {
        if (s.active) { continue; }
        s.pos = from; s.vel = velocity; s.age = 0.0f; s.active = true;
        return;
    }
}

void Shells::Update(float dt)
{
    assert(dt >= 0.0f);
    for (Shell& s : pool_) {
        if (!s.active) { continue; }
        s.pos.x += s.vel.x * dt;
        s.pos.y += s.vel.y * dt;
        s.age   += dt;
        const bool off = s.pos.x < -20.0f || s.pos.x > static_cast<float>(cfg::kScreenW) + 20.0f
                      || s.pos.y < -20.0f || s.pos.y > screen::Bottom() + 20.0f;
        if (off || s.age > cfg::kShellSeconds) { s.active = false; }
    }
}

void Shells::Draw() const
{
    for (const Shell& s : pool_) {
        if (!s.active) { continue; }
        DrawCircleV(s.pos, cfg::kShellR + 3.0f, Fade(ORANGE, 0.35f));   // glow
        DrawCircleV(s.pos, cfg::kShellR, MAROON);
        DrawCircleV(Vector2 {s.pos.x - 1.5f, s.pos.y - 1.5f}, cfg::kShellR * 0.4f, ORANGE);
    }
}

int Shells::Find(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    for (int i = 0; i < cfg::kMaxShells; ++i) {
        const Shell& s = pool_[static_cast<size_t>(i)];
        if (s.active && CheckCollisionCircleRec(s.pos, cfg::kShellR, r)) { return i; }
    }
    return -1;
}

void Shells::Kill(int i)
{
    assert(i >= 0 && i < cfg::kMaxShells);
    pool_[static_cast<size_t>(i)].active = false;
}
