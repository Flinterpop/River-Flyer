#include "Missiles.h"

#include <cassert>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265f;

float Length(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

Vector2 Unit(Vector2 v)
{
    const float len = Length(v);
    assert(len > 0.0f);
    return Vector2 {v.x / len, v.y / len};
}

// Rotates 'dir' towards 'want' by at most 'maxDeg' degrees.
Vector2 TurnTowards(Vector2 dir, Vector2 want, float maxDeg)
{
    assert(maxDeg >= 0.0f);
    const float have  = std::atan2(dir.y, dir.x);
    const float goal  = std::atan2(want.y, want.x);
    float delta = goal - have;
    while (delta > kPi)  { delta -= 2.0f * kPi; }
    while (delta < -kPi) { delta += 2.0f * kPi; }
    const float lim = maxDeg * kPi / 180.0f;
    if (delta > lim)  { delta = lim; }
    if (delta < -lim) { delta = -lim; }
    const float a = have + delta;
    return Vector2 {std::cos(a), std::sin(a)};
}

} // namespace

void Missiles::Reset()
{
    for (Missile& m : pool_) {
        m.active = false; m.onChaff = false; m.pilot = 0; m.age = 0.0f; m.smoke = 0.0f;
        m.pos = Vector2 {0.0f, 0.0f}; m.dir = Vector2 {0.0f, -1.0f}; m.chaff = Vector2 {0.0f, 0.0f};
    }
}

void Missiles::Launch(Vector2 from, Vector2 towards, int pilot)
{
    assert(pilot >= 0 && pilot < cfg::kMaxPilots);
    for (Missile& m : pool_) {
        if (m.active) { continue; }
        m.pos = from; m.dir = Unit(Vector2 {towards.x - from.x, towards.y - from.y});
        m.pilot = pilot; m.age = 0.0f; m.smoke = 0.0f; m.onChaff = false; m.active = true;
        return;
    }
}

void Missiles::Update(float dt, const std::array<Target, cfg::kMaxPilots>& targets, float riverStep, Effects& effects)
{
    assert(dt >= 0.0f && riverStep >= 0.0f);
    for (Missile& m : pool_) {
        if (!m.active) { continue; }
        m.age += dt;
        if (m.onChaff) { m.chaff.y += riverStep; }   // chaff drifts with the water

        // Steer: towards the chaff, or the pilot unless jammed / gone.
        const Target& t = targets[static_cast<size_t>(m.pilot)];
        Vector2 goal = m.pos;
        bool    steer = false;
        if (m.onChaff)                    { goal = m.chaff;  steer = true; }
        else if (t.alive && !t.jamming)   { goal = t.pos;    steer = true; }
        if (steer) {
            const Vector2 want {goal.x - m.pos.x, goal.y - m.pos.y};
            if (Length(want) > 1.0f) { m.dir = TurnTowards(m.dir, Unit(want), cfg::kMissileTurnDeg * dt); }
        }
        m.pos.x += m.dir.x * cfg::kMissileSpeed * dt;
        m.pos.y += m.dir.y * cfg::kMissileSpeed * dt;

        // Exhaust smoke.
        m.smoke -= dt;
        if (m.smoke <= 0.0f) {
            m.smoke = cfg::kMissileSmokeGap;
            effects.SpawnSmoke(Vector2 {m.pos.x - m.dir.x * 10.0f, m.pos.y - m.dir.y * 10.0f});
        }

        // Reached the chaff: burst harmlessly. Timed out or off-screen: gone.
        const bool onTarget = m.onChaff && Length(Vector2 {m.chaff.x - m.pos.x, m.chaff.y - m.pos.y}) < 14.0f;
        const bool off = m.pos.x < -40.0f || m.pos.x > static_cast<float>(cfg::kScreenW) + 40.0f
                      || m.pos.y < -40.0f || m.pos.y > static_cast<float>(cfg::kScreenH) + 40.0f;
        if (onTarget || m.age > cfg::kMissileSeconds || off) {
            effects.Spawn(m.pos, Effects::Style::Fuel);
            m.active = false;
        }
    }
}

void Missiles::Decoy(int pilot, Vector2 chaff)
{
    assert(pilot >= 0 && pilot < cfg::kMaxPilots);
    for (Missile& m : pool_) {
        if (m.active && m.pilot == pilot && !m.onChaff) { m.onChaff = true; m.chaff = chaff; }
    }
}

void Missiles::Draw() const
{
    for (const Missile& m : pool_) {
        if (!m.active) { continue; }
        const float a  = std::atan2(m.dir.y, m.dir.x);
        const float ca = std::cos(a), sa = std::sin(a);
        auto at = [&](float fwd, float side) { return Vector2 {m.pos.x + ca * fwd - sa * side, m.pos.y + sa * fwd + ca * side}; };

        // Exhaust flame, body, nose, fins, and a pair of eyes that make it silly.
        const float flick = 6.0f + 3.0f * std::sin(static_cast<float>(GetTime()) * 40.0f);
        DrawTriangle(at(-12.0f, -4.0f), at(-12.0f - flick, 0.0f), at(-12.0f, 4.0f), ORANGE);
        DrawTriangle(at(-12.0f, -2.0f), at(-12.0f - flick * 0.6f, 0.0f), at(-12.0f, 2.0f), YELLOW);
        DrawLineEx(at(-12.0f, 0.0f), at(10.0f, 0.0f), 8.0f, LIGHTGRAY);
        DrawTriangle(at(10.0f, -4.0f), at(18.0f, 0.0f), at(10.0f, 4.0f), RED);
        DrawTriangle(at(-12.0f, -4.0f), at(-16.0f, -9.0f), at(-6.0f, -4.0f), DARKGRAY);
        DrawTriangle(at(-12.0f, 4.0f), at(-6.0f, 4.0f), at(-16.0f, 9.0f), DARKGRAY);
        DrawCircleV(at(4.0f, -2.5f), 2.0f, RAYWHITE); DrawCircleV(at(4.6f, -2.5f), 1.0f, BLACK);
        DrawCircleV(at(4.0f,  2.5f), 2.0f, RAYWHITE); DrawCircleV(at(4.6f,  2.5f), 1.0f, BLACK);
    }
}

int Missiles::Find(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    for (int i = 0; i < cfg::kMaxMissiles; ++i) {
        const Missile& m = pool_[static_cast<size_t>(i)];
        if (!m.active) { continue; }
        const Vector2 nose {m.pos.x + m.dir.x * 14.0f, m.pos.y + m.dir.y * 14.0f};
        if (CheckCollisionCircleRec(nose, 6.0f, r) || CheckCollisionCircleRec(m.pos, 6.0f, r)) { return i; }
    }
    return -1;
}

void Missiles::Kill(int i)
{
    assert(i >= 0 && i < cfg::kMaxMissiles);
    pool_[static_cast<size_t>(i)].active = false;
}

Vector2 Missiles::Position(int i) const
{
    assert(i >= 0 && i < cfg::kMaxMissiles && pool_[static_cast<size_t>(i)].active);
    return pool_[static_cast<size_t>(i)].pos;
}

float Missiles::NearestTo(int pilot, Vector2 pos) const
{
    assert(pilot >= 0 && pilot < cfg::kMaxPilots);
    float best = -1.0f;
    for (const Missile& m : pool_) {
        if (!m.active || m.onChaff || m.pilot != pilot) { continue; }
        const float d = Length(Vector2 {m.pos.x - pos.x, m.pos.y - pos.y});
        if (best < 0.0f || d < best) { best = d; }
    }
    return best;
}

bool Missiles::AnyChasing() const
{
    for (const Missile& m : pool_) { if (m.active && !m.onChaff) { return true; } }
    return false;
}
