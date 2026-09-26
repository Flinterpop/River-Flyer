#include "Boss.h"

#include <cassert>
#include <cmath>

#include "Screen.h"

void Boss::Reset()
{
    state_ = State::Gone;
    pos_   = Vector2 {0.0f, 0.0f};
    vx_    = 0.0f;
    hp_    = 0;
    maxHp_ = 0;
    fireTimer_ = 0.0f;
    flash_     = 0.0f;
    life_      = 0.0f;
}

void Boss::Spawn(int stage)
{
    assert(stage >= 0);
    state_ = State::Entering;
    pos_   = Vector2 {(static_cast<float>(cfg::kScreenW) - cfg::kBossW) * 0.5f, -cfg::kBossH};
    vx_    = cfg::kBossSpeed;
    maxHp_ = cfg::kBossHp + stage * cfg::kBossHpPerStage;
    hp_    = maxHp_;
    fireTimer_ = cfg::kBossReload;
    flash_     = 0.0f;
    life_      = cfg::kBossSeconds;
}

Rectangle Boss::Bounds() const
{
    return Rectangle {pos_.x, pos_.y, cfg::kBossW, cfg::kBossH};
}

Vector2 Boss::Centre() const
{
    return Vector2 {pos_.x + cfg::kBossW * 0.5f, pos_.y + cfg::kBossH * 0.5f};
}

bool Boss::Hit()
{
    if (state_ != State::Fighting) { return false; }   // no free hits on the way in
    flash_ = cfg::kBossFlashSeconds;
    --hp_;
    if (hp_ > 0) { return false; }
    hp_    = 0;
    state_ = State::Dying;
    return true;
}

bool Boss::Update(float dt, const Vector2* targets, int targetCount, Shells& shells)
{
    assert(dt >= 0.0f && targetCount >= 0);
    if (state_ == State::Gone || state_ == State::Dying) { return false; }
    if (flash_ > 0.0f) { flash_ -= dt; }

    if (state_ == State::Entering) {
        pos_.y += cfg::kBossEntrySpeed * dt;
        if (pos_.y >= cfg::kBossStationY) { pos_.y = cfg::kBossStationY; state_ = State::Fighting; }
        return false;
    }

    // Weave across the river, turning at the window edges.
    pos_.x += vx_ * dt;
    if (pos_.x < cfg::kBossMargin) { pos_.x = cfg::kBossMargin; vx_ = -vx_; }
    const float maxX = static_cast<float>(cfg::kScreenW) - cfg::kBossW - cfg::kBossMargin;
    if (pos_.x > maxX) { pos_.x = maxX; vx_ = -vx_; }

    // Salvo: a spread of shells aimed at the nearest plane.
    fireTimer_ -= dt;
    if (fireTimer_ <= 0.0f && targetCount > 0) {
        fireTimer_ = cfg::kBossReload;
        const Vector2 from = Vector2 {pos_.x + cfg::kBossW * 0.5f, pos_.y + cfg::kBossH};
        int nearest = 0;
        float best  = 1e9f;
        for (int i = 0; i < targetCount; ++i) {
            const float dx = targets[i].x - from.x, dy = targets[i].y - from.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; nearest = i; }
        }
        const float dx = targets[nearest].x - from.x, dy = targets[nearest].y - from.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 1.0f) {
            for (int k = -1; k <= 1; ++k) {
                const float a = std::atan2(dy, dx) + static_cast<float>(k) * cfg::kBossSpread;
                shells.Fire(from, Vector2 {std::cos(a) * cfg::kBossShellSpeed, std::sin(a) * cfg::kBossShellSpeed});
            }
        }
    }

    // It does not chase for ever: after a while it climbs away.
    life_ -= dt;
    if (life_ <= 0.0f) {
        state_ = State::Gone;
        return true;
    }
    return false;
}

void Boss::Draw(const Sprites& sprites) const
{
    if (state_ == State::Gone || state_ == State::Dying) { return; }
    const Rectangle r = Bounds();
    DrawEllipse(static_cast<int>(r.x + r.width * 0.5f) + 6, static_cast<int>(r.y + r.height + 10.0f),
                r.width * 0.45f, r.height * 0.2f, Fade(BLACK, 0.25f));
    const Color tint = (flash_ > 0.0f) ? Color {255, 190, 190, 255} : WHITE;
    Sprites::DrawInto(sprites.Boss(), r.x, r.y, r.width, r.height, tint);
}
