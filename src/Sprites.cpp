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
    // A little petrol pump: red body, white display window, grey hose and nozzle.
    const int w = static_cast<int>(cfg::kObstacleW);
    const int h = static_cast<int>(cfg::kObstacleH);
    Image img = GenImageColor(w, h, BLANK);
    ImageDrawRectangle(&img, 2,  h - 4, w - 8, 4,     DARKGRAY);        // base plinth
    ImageDrawRectangle(&img, 4,  2,     w - 12, h - 4, RED);            // body
    ImageDrawRectangle(&img, 4,  2,     w - 12, 3,     MAROON);         // top shadow
    ImageDrawRectangle(&img, 7,  6,     w - 18, 8,     RAYWHITE);       // display window
    ImageDrawRectangle(&img, 9,  9,     w - 22, 2,     DARKBLUE);       // digits
    ImageDrawRectangle(&img, 8,  17,    w - 20, 6,     MAROON);         // nozzle holster
    ImageDrawRectangle(&img, w - 7, 6,  2, 14,         DARKGRAY);       // hose
    ImageDrawRectangle(&img, w - 9, 18, 6, 4,          BLACK);          // nozzle
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
