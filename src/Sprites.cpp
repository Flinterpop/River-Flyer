#include "Sprites.h"

#include <cassert>

#include "Config.h"

namespace {

// Uploads an image to the GPU and releases the CPU copy.
Texture2D Upload(Image& img)
{
    assert(img.data != nullptr);
    const Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    assert(tex.id != 0);
    return tex;
}

} // namespace

Sprites::Sprites()
{
    assert(IsWindowReady());
    player_ = GenPlayer();
    rock_   = GenRock();
    fuel_   = GenFuel();
    bullet_ = GenBullet();
    assert(player_.id != 0 && rock_.id != 0 && fuel_.id != 0 && bullet_.id != 0);
}

Sprites::~Sprites()
{
    UnloadTexture(bullet_);
    UnloadTexture(fuel_);
    UnloadTexture(rock_);
    UnloadTexture(player_);
}

Texture2D Sprites::GenPlayer()
{
    const int w = static_cast<int>(cfg::kPlayerW);
    const int h = static_cast<int>(cfg::kPlayerH);
    Image img = GenImageColor(w, h, BLANK);
    // Fuselage, delta wings, tail.
    ImageDrawRectangle(&img, w / 2 - 3, 0,      6,  h,     YELLOW);
    ImageDrawRectangle(&img, 0,         h - 14, w,  10,    GOLD);
    ImageDrawRectangle(&img, w / 2 - 8, h - 6,  16, 6,     ORANGE);
    ImageDrawRectangle(&img, w / 2 - 2, 4,      4,  8,     SKYBLUE);   // canopy
    return Upload(img);
}

Texture2D Sprites::GenRock()
{
    const int w = static_cast<int>(cfg::kObstacleW);
    const int h = static_cast<int>(cfg::kObstacleH);
    Image img = GenImageColor(w, h, BLANK);
    ImageDrawCircle(&img, w / 2, h / 2, w / 2 - 1, GRAY);
    ImageDrawCircle(&img, w / 2 - 5, h / 2 - 5, 5, LIGHTGRAY);       // highlight
    ImageDrawCircle(&img, w / 2 + 6, h / 2 + 4, 4, DARKGRAY);        // shadow
    return Upload(img);
}

Texture2D Sprites::GenFuel()
{
    const int w = static_cast<int>(cfg::kObstacleW);
    const int h = static_cast<int>(cfg::kObstacleH);
    Image img = GenImageColor(w, h, RED);
    ImageDrawRectangle(&img, 3, 3, w - 6, h - 6, MAROON);
    ImageDrawRectangle(&img, 6, h / 2 - 3, w - 12, 6, RAYWHITE);     // "F" stroke
    ImageDrawRectangle(&img, 6, 6, 6, h - 12, RAYWHITE);
    return Upload(img);
}

Texture2D Sprites::GenBullet()
{
    const int w = static_cast<int>(cfg::kBulletW);
    const int h = static_cast<int>(cfg::kBulletH);
    Image img = GenImageColor(w, h, WHITE);
    ImageDrawRectangle(&img, 0, h / 2, w, h / 2, YELLOW);
    return Upload(img);
}
