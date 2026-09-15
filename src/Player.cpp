#include "Player.h"

#include <cassert>

#include "Config.h"

namespace {

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
    pos_.x = (static_cast<float>(cfg::kScreenW) - cfg::kPlayerW) * 0.5f;
    pos_.y = cfg::kPlayerStartY;
    assert(pos_.x >= 0.0f && pos_.y >= 0.0f);
}

void Player::Update(float dt)
{
    assert(dt >= 0.0f);
    const Vector2 dir = ReadInput();

    pos_.x += dir.x * cfg::kPlayerSpeedX * dt;
    pos_.y += dir.y * cfg::kPlayerSpeedY * dt;

    pos_.x = Clamp(pos_.x, 0.0f, static_cast<float>(cfg::kScreenW) - cfg::kPlayerW);
    pos_.y = Clamp(pos_.y, 0.0f, static_cast<float>(cfg::kScreenH) - cfg::kPlayerH);

    assert(pos_.x + cfg::kPlayerW <= static_cast<float>(cfg::kScreenW));
    assert(pos_.y + cfg::kPlayerH <= static_cast<float>(cfg::kScreenH));
}

void Player::Draw() const
{
    // Simple delta-wing silhouette: nose at top centre, wings at the base.
    const Vector2 nose  {pos_.x + cfg::kPlayerW * 0.5f, pos_.y};
    const Vector2 left  {pos_.x,                        pos_.y + cfg::kPlayerH};
    const Vector2 right {pos_.x + cfg::kPlayerW,        pos_.y + cfg::kPlayerH};
    DrawTriangle(nose, left, right, YELLOW);
    DrawTriangleLines(nose, left, right, ORANGE);
}

Rectangle Player::Bounds() const
{
    return Rectangle {pos_.x, pos_.y, cfg::kPlayerW, cfg::kPlayerH};
}
