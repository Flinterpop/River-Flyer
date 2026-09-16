#include "Bullets.h"

#include <cassert>

#include "Sprites.h"

void Bullets::Reset()
{
    for (Bullet& b : pool_) {
        b.active = false;
        b.vx     = 0.0f;
        b.rect   = Rectangle {0.0f, 0.0f, cfg::kBulletW, cfg::kBulletH};
    }
}

void Bullets::Fire(Vector2 muzzle, float vx)
{
    assert(muzzle.x >= 0.0f && muzzle.x <= static_cast<float>(cfg::kScreenW));
    for (Bullet& b : pool_) {
        if (b.active) { continue; }
        b.rect.x = muzzle.x - cfg::kBulletW * 0.5f;
        b.rect.y = muzzle.y - cfg::kBulletH;
        b.vx     = vx;
        b.active = true;
        return;
    }
}

void Bullets::Update(float dt)
{
    assert(dt >= 0.0f);
    for (Bullet& b : pool_) {
        if (!b.active) { continue; }
        b.rect.y -= cfg::kBulletSpeed * dt;
        b.rect.x += b.vx * dt;
        if (b.rect.y + b.rect.height < 0.0f || b.rect.x < -20.0f || b.rect.x > static_cast<float>(cfg::kScreenW) + 20.0f) { b.active = false; }
    }
}

void Bullets::Draw(const Texture2D& tex) const
{
    assert(tex.id != 0);
    for (const Bullet& b : pool_) {
        if (b.active) { Sprites::DrawInto(tex, b.rect.x, b.rect.y, b.rect.width, b.rect.height, WHITE); }
    }
}

bool Bullets::Active(int i) const
{
    assert(i >= 0 && i < Capacity());
    return pool_[static_cast<size_t>(i)].active;
}

Rectangle Bullets::Bounds(int i) const
{
    assert(i >= 0 && i < Capacity());
    assert(pool_[static_cast<size_t>(i)].active);
    return pool_[static_cast<size_t>(i)].rect;
}

void Bullets::Kill(int i)
{
    assert(i >= 0 && i < Capacity());
    pool_[static_cast<size_t>(i)].active = false;
}
