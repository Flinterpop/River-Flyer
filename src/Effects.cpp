#include "Effects.h"

#include <cassert>
#include <cmath>

namespace {

constexpr float kTwoPi = 6.28318530718f;

} // namespace

void Effects::Reset()
{
    for (Burst& b : pool_) {
        b.active = false;
        b.age    = 0.0f;
        b.centre = Vector2 {0.0f, 0.0f};
        b.style  = Style::Rock;
    }
}

void Effects::Spawn(Vector2 centre, Style style)
{
    assert(centre.x >= -cfg::kObstacleW && centre.x <= static_cast<float>(cfg::kScreenW) + cfg::kObstacleW);
    for (Burst& b : pool_) {
        if (b.active) { continue; }
        b.centre = centre;
        b.age    = 0.0f;
        b.style  = style;
        b.active = true;
        return;
    }
}

void Effects::Update(float dt)
{
    assert(dt >= 0.0f);
    for (Burst& b : pool_) {
        if (!b.active) { continue; }
        b.age += dt;
        if (b.age >= cfg::kBurstSeconds) { b.active = false; }
    }
}

Color Effects::Tint(Style style)
{
    switch (style) {
        case Style::Rock:  return LIGHTGRAY;
        case Style::Fuel:  return ORANGE;
        case Style::Plane: return GOLD;
    }
    assert(false && "unhandled Style");
    return WHITE;
}

void Effects::DrawBurst(const Burst& b)
{
    assert(b.active);
    const float t = b.age / cfg::kBurstSeconds;          // 0 .. 1 over the burst's life
    assert(t >= 0.0f && t < 1.0f);
    const float fade = 1.0f - t;
    const Color tint = Tint(b.style);

    // Central flash: grows fast, gone by a third of the way through.
    if (t < 0.33f) {
        const float flashT = t / 0.33f;
        DrawCircleV(b.centre, cfg::kBurstFlashR * flashT, Fade(WHITE, 1.0f - flashT));
    }

    // Debris ring: evenly spaced, alternate particles slower for a ragged look.
    for (int i = 0; i < cfg::kBurstParticles; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(cfg::kBurstParticles);
        const float speed = cfg::kBurstSpeed * ((i % 2 == 0) ? 1.0f : 0.6f);
        const float r     = speed * b.age;
        const Vector2 p {b.centre.x + std::cos(angle) * r, b.centre.y + std::sin(angle) * r};
        DrawCircleV(p, cfg::kBurstParticleR * fade, Fade(tint, fade));
    }
}

void Effects::Draw() const
{
    for (const Burst& b : pool_) {
        if (b.active) { DrawBurst(b); }
    }
}
