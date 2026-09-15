#include "Player.h"

#include <cassert>
#include <cmath>

#include "Config.h"

namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr float kPi    = 3.14159265359f;

// Reads WASD / arrow keys and returns a direction in [-1, 1] per axis.
Vector2 ReadInput()
{
    Vector2 dir {0.0f, 0.0f};
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) { dir.x -= 1.0f; }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { dir.x += 1.0f; }
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) { dir.y -= 1.0f; }
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) { dir.y += 1.0f; }
    assert(dir.x >= -1.0f && dir.x <= 1.0f);
    assert(dir.y >= -1.0f && dir.y <= 1.0f);
    return dir;
}

float Clamp(float v, float lo, float hi)
{
    assert(lo <= hi);
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

} // namespace

void Player::Reset()
{
    pos_.x     = (static_cast<float>(cfg::kScreenW) - cfg::kPlayerW) * 0.5f;
    pos_.y     = cfg::kPlayerStartY;
    thrusting_ = false;
    crashT_    = -1.0f;
    assert(pos_.x >= 0.0f && pos_.y >= 0.0f);
    assert(!Crashing());
}

void Player::Update(float dt)
{
    assert(dt >= 0.0f);
    assert(!Crashing());
    const Vector2 dir = ReadInput();
    thrusting_ = (dir.y < 0.0f);

    pos_.x += dir.x * cfg::kPlayerSpeedX * dt;
    pos_.y += dir.y * cfg::kPlayerSpeedY * dt;

    pos_.x = Clamp(pos_.x, 0.0f, static_cast<float>(cfg::kScreenW) - cfg::kPlayerW);
    pos_.y = Clamp(pos_.y, 0.0f, static_cast<float>(cfg::kScreenH) - cfg::kPlayerH);

    assert(pos_.x + cfg::kPlayerW <= static_cast<float>(cfg::kScreenW));
    assert(pos_.y + cfg::kPlayerH <= static_cast<float>(cfg::kScreenH));
}

// ---- crash ----------------------------------------------------------------

void Player::BeginCrash()
{
    assert(!Crashing());
    crashOrigin_ = Centre();
    crashT_      = 0.0f;
    thrusting_   = false;
    assert(Crashing() && !CrashFinished());
}

void Player::UpdateCrash(float dt)
{
    assert(dt >= 0.0f);
    assert(Crashing());
    crashT_ += dt;
    if (crashT_ > cfg::kCrashSeconds) { crashT_ = cfg::kCrashSeconds; }
    assert(crashT_ <= cfg::kCrashSeconds);
}

float Player::CrashProgress() const
{
    assert(Crashing());
    const float t = crashT_ / cfg::kCrashSeconds;
    assert(t >= 0.0f && t <= 1.0f);
    return t;
}

Vector2 Player::Centre() const
{
    if (!Crashing()) {
        return Vector2 {pos_.x + cfg::kPlayerW * 0.5f, pos_.y + cfg::kPlayerH * 0.5f};
    }
    return CentreAt(CrashProgress());
}

Vector2 Player::CentreAt(float t) const
{
    assert(Crashing());
    assert(t >= 0.0f && t <= 1.0f);
    // Spiral: radius swells then collapses (sin), angle winds kCrashTurns
    // times, and the whole thing drifts downstream with the river.
    const float angle  = kTwoPi * cfg::kCrashTurns * t;
    const float radius = cfg::kCrashRadius * std::sin(kPi * t);
    assert(radius >= -0.001f);
    return Vector2 {crashOrigin_.x + std::cos(angle) * radius,
                    crashOrigin_.y + std::sin(angle) * radius + cfg::kCrashDrift * t};
}

// ---- drawing --------------------------------------------------------------

void Player::Draw(const Texture2D& tex, bool visible) const
{
    assert(tex.id != 0);
    if (!visible) { return; }   // blink frame during the respawn grace period
    if (Crashing()) {
        DrawCrashing(tex);
    } else {
        DrawFlying(tex);
    }
}

void Player::DrawFlying(const Texture2D& tex) const
{
    assert(!Crashing());
    if (thrusting_) { DrawFlame(); }   // behind the sprite
    DrawTexture(tex, static_cast<int>(pos_.x), static_cast<int>(pos_.y), WHITE);
}

void Player::DrawFlame() const
{
    // Flicker the flame length with a fast sine so it looks alive.
    const float flicker = std::sin(static_cast<float>(GetTime()) * cfg::kFlameHz * kTwoPi);
    const float len     = cfg::kFlameLen + cfg::kFlameFlicker * flicker;
    assert(len > 0.0f);

    const float cx   = pos_.x + cfg::kPlayerW * 0.5f;
    const float tail = pos_.y + cfg::kPlayerH - 4.0f;
    const float halfW = 7.0f;

    // Outer orange plume, inner yellow core, both pointing down from the tail.
    DrawTriangle(Vector2 {cx - halfW, tail}, Vector2 {cx, tail + len}, Vector2 {cx + halfW, tail}, ORANGE);
    DrawTriangle(Vector2 {cx - halfW * 0.5f, tail}, Vector2 {cx, tail + len * 0.6f}, Vector2 {cx + halfW * 0.5f, tail}, YELLOW);
}

void Player::DrawSmokeTrail() const
{
    assert(Crashing());
    const float t = CrashProgress();
    // Puffs sit where the plane was a few steps ago; older ones are bigger and fainter.
    for (int k = 1; k <= cfg::kCrashPuffs; ++k) {
        const float back = t - cfg::kCrashPuffGap * static_cast<float>(k);
        if (back < 0.0f) { break; }
        const float age = static_cast<float>(k) / static_cast<float>(cfg::kCrashPuffs);   // 0 .. 1
        DrawCircleV(CentreAt(back), cfg::kCrashPuffR * (0.6f + age), Fade(GRAY, 0.6f * (1.0f - age)));
    }
}

void Player::DrawCrashing(const Texture2D& tex) const
{
    assert(Crashing());
    DrawSmokeTrail();
    const float t     = CrashProgress();
    const float scale = 1.0f - (1.0f - cfg::kCrashMinScale) * t;
    const float rot   = 360.0f * cfg::kCrashTurns * t * 1.5f;   // spins faster than it orbits
    assert(scale >= cfg::kCrashMinScale && scale <= 1.0f);

    const Vector2   c   = Centre();
    const float     w   = cfg::kPlayerW * scale;
    const float     h   = cfg::kPlayerH * scale;
    const Rectangle src {0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    const Rectangle dst {c.x, c.y, w, h};
    DrawTexturePro(tex, src, dst, Vector2 {w * 0.5f, h * 0.5f}, rot, Fade(WHITE, 1.0f - 0.5f * t));
}

// ---- geometry -------------------------------------------------------------

Rectangle Player::Bounds() const
{
    assert(!Crashing());
    // Slightly smaller than the sprite so near-misses feel fair.
    const float inset = 3.0f;
    assert(inset * 2.0f < cfg::kPlayerW && inset * 2.0f < cfg::kPlayerH);
    return Rectangle {pos_.x + inset, pos_.y + inset,
                      cfg::kPlayerW - 2.0f * inset, cfg::kPlayerH - 2.0f * inset};
}

Vector2 Player::Muzzle() const
{
    assert(!Crashing());
    return Vector2 {pos_.x + cfg::kPlayerW * 0.5f, pos_.y};
}
