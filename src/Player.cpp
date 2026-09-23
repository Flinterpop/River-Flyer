#include "Player.h"

#include <cassert>
#include <cmath>

#include "Config.h"
#include "Sprites.h"

namespace {

constexpr float kTwoPi = 6.28318530718f;
constexpr float kPi    = 3.14159265359f;

float Clamp(float v, float lo, float hi)
{
    assert(lo <= hi);
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

} // namespace

void Player::Reset(float xOffset)
{
    assert(xOffset > -200.0f && xOffset < 200.0f);
    pos_.x     = (static_cast<float>(cfg::kScreenW) - cfg::kPlayerW) * 0.5f + xOffset;
    pos_.y     = cfg::kPlayerStartY;
    thrusting_ = false;
    braking_   = false;
    chute_     = 0.0f;
    crashT_    = -1.0f;
    assert(pos_.x >= 0.0f && pos_.y >= 0.0f);
    assert(!Crashing());
}

void Player::Update(float dt, const InputMap& map)
{
    assert(dt >= 0.0f);
    assert(!Crashing());
    const Vector2 dir = input::ReadMove(map);
    thrusting_ = (dir.y < 0.0f);
    braking_   = (dir.y > 0.0f);

    // Chute inflates while braking and collapses quickly when released.
    chute_ += braking_ ? (dt / cfg::kChuteInflate) : (-dt / (cfg::kChuteInflate * 0.5f));
    chute_  = Clamp(chute_, 0.0f, 1.0f);

    pos_.x += dir.x * cfg::kPlayerSpeedX * dt;
    pos_.y += dir.y * cfg::kPlayerSpeedY * dt;

    pos_.x = Clamp(pos_.x, 0.0f, static_cast<float>(cfg::kScreenW) - cfg::kPlayerW);
    pos_.y = Clamp(pos_.y, 0.0f, static_cast<float>(cfg::kScreenH) - cfg::kPlayerH - cfg::kPlayerBottomMargin);

    assert(pos_.x + cfg::kPlayerW <= static_cast<float>(cfg::kScreenW));
    assert(pos_.y + cfg::kPlayerH <= static_cast<float>(cfg::kScreenH));
}

// ---- crash ----------------------------------------------------------------

void Player::BeginCrash(CrashStyle style)
{
    assert(!Crashing());
    crashOrigin_ = Centre();
    crashStyle_  = style;
    crashT_      = 0.0f;
    thrusting_   = false;
    braking_     = false;
    chute_       = 0.0f;
    assert(Crashing() && !CrashFinished());
}

void Player::UpdateCrash(float dt)
{
    assert(dt >= 0.0f);
    assert(Crashing());
    crashT_ += dt;
    if (crashT_ > CrashSeconds()) { crashT_ = CrashSeconds(); }
    assert(crashT_ <= CrashSeconds());
}

float Player::CrashSeconds() const
{
    return (crashStyle_ == CrashStyle::Roll) ? cfg::kRollSeconds : cfg::kCrashSeconds;
}

float Player::CrashProgress() const
{
    assert(Crashing());
    const float t = crashT_ / CrashSeconds();
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
    if (crashStyle_ == CrashStyle::Roll) {
        // Roll: carried straight downstream with a slight sideways wobble.
        const float sway = 10.0f * std::sin(kTwoPi * 1.5f * t);
        return Vector2 {crashOrigin_.x + sway, crashOrigin_.y + cfg::kRollDrift * t};
    }
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
    if (!Crashing()) {
        DrawFlying(tex);
    } else if (crashStyle_ == CrashStyle::Roll) {
        DrawRoll(tex);
    } else {
        DrawSpiral(tex);
    }
}

void Player::DrawFlying(const Texture2D& tex) const
{
    assert(!Crashing());
    // Drop shadow on the water gives the plane some altitude.
    DrawEllipse(static_cast<int>(pos_.x + cfg::kPlayerW * 0.5f) + 10, static_cast<int>(pos_.y + cfg::kPlayerH * 0.5f) + 16,
                cfg::kPlayerW * 0.45f, cfg::kPlayerH * 0.3f, Fade(BLACK, 0.25f));
    if (thrusting_)    { DrawFlame(); }   // behind the sprite
    if (chute_ > 0.0f) { DrawChute(); }
    Sprites::DrawInto(tex, pos_.x, pos_.y, cfg::kPlayerW, cfg::kPlayerH, WHITE);
}

void Player::DrawChute() const
{
    assert(chute_ > 0.0f && chute_ <= 1.0f);
    // Canopy trails behind (below) the tail on two shroud lines and sways.
    const float sway  = 3.0f * std::sin(static_cast<float>(GetTime()) * 7.0f);
    const float r     = cfg::kChuteRadius * chute_;
    const Vector2 tail   {pos_.x + cfg::kPlayerW * 0.5f, pos_.y + cfg::kPlayerH - 2.0f};
    const Vector2 canopy {tail.x + sway, tail.y + cfg::kChuteLineLen * chute_};
    assert(r > 0.0f);

    DrawLineV(tail, Vector2 {canopy.x - r, canopy.y}, DARKGRAY);
    DrawLineV(tail, Vector2 {canopy.x + r, canopy.y}, DARKGRAY);
    // Lower half-disc = the dome bulging away from the plane, with white stripes.
    DrawCircleSector(canopy, r, 0.0f, 180.0f, 16, RED);
    DrawCircleSector(canopy, r, 30.0f, 60.0f, 4, RAYWHITE);
    DrawCircleSector(canopy, r, 120.0f, 150.0f, 4, RAYWHITE);
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

void Player::DrawSpiralSmoke() const
{
    assert(Crashing() && crashStyle_ == CrashStyle::Spiral);
    const float t = CrashProgress();
    // Puffs sit where the plane was a few steps ago; older ones are bigger and fainter.
    for (int k = 1; k <= cfg::kCrashPuffs; ++k) {
        const float back = t - cfg::kCrashPuffGap * static_cast<float>(k);
        if (back < 0.0f) { break; }
        const float age = static_cast<float>(k) / static_cast<float>(cfg::kCrashPuffs);   // 0 .. 1
        DrawCircleV(CentreAt(back), cfg::kCrashPuffR * (0.6f + age), Fade(GRAY, 0.6f * (1.0f - age)));
    }
}

void Player::DrawSpiral(const Texture2D& tex) const
{
    assert(Crashing() && crashStyle_ == CrashStyle::Spiral);
    DrawSpiralSmoke();
    const float t     = CrashProgress();
    const float scale = 1.0f - (1.0f - cfg::kCrashMinScale) * t;
    const float rot   = 360.0f * cfg::kCrashTurns * t * 1.5f;   // spins faster than it orbits
    assert(scale >= cfg::kCrashMinScale && scale <= 1.0f);

    const Vector2 c = Centre();
    Sprites::DrawIntoRotated(tex, c.x, c.y, cfg::kPlayerW * scale, cfg::kPlayerH * scale, rot, Fade(WHITE, 1.0f - 0.5f * t));
}

void Player::DrawRollSmoke() const
{
    assert(Crashing() && crashStyle_ == CrashStyle::Roll);
    const float t = CrashProgress();
    // Dark smoke streams off the tail and is left behind (down-screen) as the
    // plane keeps going; older puffs sit further back and spread out.
    for (int k = 1; k <= cfg::kRollPuffs; ++k) {
        const float back = t - cfg::kRollPuffGap * static_cast<float>(k);
        if (back < 0.0f) { break; }
        const float age = static_cast<float>(k) / static_cast<float>(cfg::kRollPuffs);   // 0 .. 1
        Vector2 p = CentreAt(back);
        p.y += cfg::kPlayerH * 0.5f + cfg::kRollPuffRise * static_cast<float>(k);
        p.x += 4.0f * std::sin(static_cast<float>(k) * 1.7f);    // ragged edge
        DrawCircleV(p, cfg::kCrashPuffR * (0.5f + 1.2f * age), Fade(DARKGRAY, 0.7f * (1.0f - age)));
    }
}

void Player::DrawRoll(const Texture2D& tex) const
{
    assert(Crashing() && crashStyle_ == CrashStyle::Roll);
    const float t = CrashProgress();
    // Smoke only while airborne; it stops once the plane hits the water.
    if (t < cfg::kRollSinkAt) { DrawRollSmoke(); }

    // Barrel roll: sprite width follows cos(roll) so it appears to turn about
    // its own length; a small yaw wobble sells the loss of control.
    const float roll   = kTwoPi * cfg::kRollTurns * t;
    float       squash = std::fabs(std::cos(roll));
    if (squash < 0.15f) { squash = 0.15f; }
    const float yaw = cfg::kRollWobbleDeg * std::sin(kTwoPi * 2.0f * t);

    // After hitting the water: shrink and fade to nothing (sinking).
    float sink = 0.0f;
    if (t > cfg::kRollSinkAt) { sink = (t - cfg::kRollSinkAt) / (1.0f - cfg::kRollSinkAt); }
    assert(sink >= 0.0f && sink <= 1.0f);
    const float scale = 1.0f - 0.8f * sink;
    const float alpha = 1.0f - sink;

    const Vector2 c = Centre();
    Sprites::DrawIntoRotated(tex, c.x, c.y, cfg::kPlayerW * squash * scale, cfg::kPlayerH * scale, yaw, Fade(WHITE, alpha));
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

void Player::Bounce(float dirX)
{
    assert(!Crashing());
    pos_.x += dirX * cfg::kBounceX;
    pos_.y += cfg::kBounceY;                 // knocked back downstream a little
    const float maxX = static_cast<float>(cfg::kScreenW) - cfg::kPlayerW;
    const float maxY = static_cast<float>(cfg::kScreenH) - cfg::kPlayerH - cfg::kPlayerBottomMargin;
    if (pos_.x < 0.0f)  { pos_.x = 0.0f; }
    if (pos_.x > maxX)  { pos_.x = maxX; }
    if (pos_.y < 0.0f)  { pos_.y = 0.0f; }
    if (pos_.y > maxY)  { pos_.y = maxY; }
}

Vector2 Player::Muzzle() const
{
    assert(!Crashing());
    return Vector2 {pos_.x + cfg::kPlayerW * 0.5f, pos_.y};
}
