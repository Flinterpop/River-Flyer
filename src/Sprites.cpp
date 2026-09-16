#include "Sprites.h"

#include <cassert>
#include <cmath>

#include "Config.h"

namespace {

constexpr int S = cfg::kSpriteScale;   // generation scale: sprites are drawn S times larger than shown

// Uploads an image to the GPU with mipmaps and trilinear filtering, and releases the CPU copy.
Texture2D Upload(Image& img)
{
    assert(img.data != nullptr);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    assert(tex.id != 0);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    return tex;
}

Color Lerp(Color a, Color b, float t)
{
    assert(t >= 0.0f && t <= 1.0f);
    return Color {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        255 };
}

Color Scale(Color c, float k)
{
    assert(k >= 0.0f && k <= 2.0f);
    auto clamp = [](float v) { return static_cast<unsigned char>((v > 255.0f) ? 255.0f : v); };
    return Color { clamp(c.r * k), clamp(c.g * k), clamp(c.b * k), c.a };
}

// Seamlessly tileable noise: blend four Perlin samples offset by a tile so the
// left edge continues into the right and the top into the bottom. Two octaves
// (coarse + fine) so the pattern has both large swells and small detail.
Image TileableNoise(int n, int offX, int offY, float coarse, float fine)
{
    assert(n > 0 && coarse > 0.0f && fine > coarse);
    Image out = GenImageColor(n, n, BLANK);
    const float scales[2]  = { coarse, fine };
    const float weights[2] = { 0.7f, 0.3f };
    float* acc = static_cast<float*>(MemAlloc(static_cast<unsigned int>(n * n) * sizeof(float)));
    assert(acc != nullptr);
    for (int i = 0; i < n * n; ++i) { acc[i] = 0.0f; }

    for (int oct = 0; oct < 2; ++oct) {
        Image a = GenImagePerlinNoise(n, n, offX,     offY,     scales[oct]);
        Image b = GenImagePerlinNoise(n, n, offX - n, offY,     scales[oct]);
        Image c = GenImagePerlinNoise(n, n, offX,     offY - n, scales[oct]);
        Image d = GenImagePerlinNoise(n, n, offX - n, offY - n, scales[oct]);
        Color* pa = LoadImageColors(a); Color* pb = LoadImageColors(b);
        Color* pc = LoadImageColors(c); Color* pd = LoadImageColors(d);
        assert(pa != nullptr && pb != nullptr && pc != nullptr && pd != nullptr);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const float fx = static_cast<float>(x) / static_cast<float>(n);
                const float fy = static_cast<float>(y) / static_cast<float>(n);
                const int   i  = y * n + x;
                const float v  = pa[i].r * (1.0f - fx) * (1.0f - fy) + pb[i].r * fx * (1.0f - fy)
                               + pc[i].r * (1.0f - fx) * fy         + pd[i].r * fx * fy;
                acc[i] += weights[oct] * v / 255.0f;
            }
        }
        UnloadImageColors(pa); UnloadImageColors(pb); UnloadImageColors(pc); UnloadImageColors(pd);
        UnloadImage(a); UnloadImage(b); UnloadImage(c); UnloadImage(d);
    }
    // Blending narrows the range; stretch back to 0..1 so highlights survive.
    float lo = 1.0f, hi = 0.0f;
    for (int i = 0; i < n * n; ++i) { if (acc[i] < lo) { lo = acc[i]; } if (acc[i] > hi) { hi = acc[i]; } }
    assert(hi > lo);
    for (int i = 0; i < n * n; ++i) {
        const unsigned char g = static_cast<unsigned char>((acc[i] - lo) / (hi - lo) * 255.0f);
        ImageDrawPixel(&out, i % n, i / n, Color {g, g, g, 255});
    }
    MemFree(acc);
    return out;
}

// Recolours a grayscale noise image between two colours, with the brightest
// 'sparkleAbove' fraction pushed towards white.
Image Colorize(Image& gray, Color dark, Color light, float sparkleAbove)
{
    assert(gray.width > 0 && gray.height > 0);
    assert(sparkleAbove > 0.0f && sparkleAbove <= 1.0f);
    Color* px = LoadImageColors(gray);
    assert(px != nullptr);
    Image out = GenImageColor(gray.width, gray.height, BLANK);
    const int n = gray.width * gray.height;
    for (int i = 0; i < n; ++i) {
        const float v = static_cast<float>(px[i].r) / 255.0f;
        Color c = Lerp(dark, light, v);
        if (v > sparkleAbove) { c = Lerp(c, RAYWHITE, (v - sparkleAbove) / (1.0f - sparkleAbove)); }
        ImageDrawPixel(&out, i % gray.width, i / gray.width, c);
    }
    UnloadImageColors(px);
    UnloadImage(gray);
    return out;
}

} // namespace

Sprites::Sprites()
{
    assert(IsWindowReady());
    player_ = GenPlayer();
    rock_   = GenRock();
    fuel_   = GenFuel();
    bullet_ = GenBullet();
    water_  = GenWater();
    grass_  = GenGrass();
    assert(player_.id != 0 && rock_.id != 0 && fuel_.id != 0 && bullet_.id != 0);
    assert(water_.id != 0 && grass_.id != 0);
}

Sprites::~Sprites()
{
    UnloadTexture(grass_);
    UnloadTexture(water_);
    UnloadTexture(bullet_);
    UnloadTexture(fuel_);
    UnloadTexture(rock_);
    UnloadTexture(player_);
}

void Sprites::DrawInto(const Texture2D& tex, float x, float y, float w, float h, Color tint)
{
    assert(tex.id != 0 && w > 0.0f && h > 0.0f);
    const Rectangle src {0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    DrawTexturePro(tex, src, Rectangle {x, y, w, h}, Vector2 {0.0f, 0.0f}, 0.0f, tint);
}

void Sprites::DrawIntoRotated(const Texture2D& tex, float cx, float cy, float w, float h, float deg, Color tint)
{
    assert(tex.id != 0 && w > 0.0f && h > 0.0f);
    const Rectangle src {0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height)};
    DrawTexturePro(tex, src, Rectangle {cx, cy, w, h}, Vector2 {w * 0.5f, h * 0.5f}, deg, tint);
}

// ---- plane ------------------------------------------------------------------

Texture2D Sprites::GenPlayer()
{
    const int w = static_cast<int>(cfg::kPlayerW) * S;   // 64
    const int h = static_cast<int>(cfg::kPlayerH) * S;   // 80
    Image img = GenImageColor(w, h, BLANK);
    const float cx = static_cast<float>(w) * 0.5f;

    const Color body   {235, 190, 40, 255};
    const Color bodyLo {185, 140, 20, 255};    // shaded (right) side, light from top-left
    const Color wing   {225, 175, 30, 255};
    const Color wingLo {170, 125, 15, 255};
    const Color edge   {110, 80, 10, 255};

    // Main wings: swept back, shaded from root (light) to tip (dark).
    ImageDrawTriangleEx(&img, Vector2 {cx - 6.0f, 30.0f}, Vector2 {2.0f, 62.0f}, Vector2 {cx - 4.0f, 60.0f}, wing, wingLo, wing);
    ImageDrawTriangleEx(&img, Vector2 {cx + 6.0f, 30.0f}, Vector2 {cx + 4.0f, 60.0f}, Vector2 {62.0f, 62.0f}, wingLo, wingLo, Scale(wingLo, 0.8f));
    ImageDrawLineEx(&img, Vector2 {2.0f, 62.0f}, Vector2 {cx - 4.0f, 60.0f}, 1, edge);        // trailing edges
    ImageDrawLineEx(&img, Vector2 {62.0f, 62.0f}, Vector2 {cx + 4.0f, 60.0f}, 1, edge);

    // Tail planes and fin.
    ImageDrawTriangle(&img, Vector2 {cx - 4.0f, 62.0f}, Vector2 {cx - 16.0f, 74.0f}, Vector2 {cx - 3.0f, 74.0f}, wing);
    ImageDrawTriangle(&img, Vector2 {cx + 4.0f, 62.0f}, Vector2 {cx + 3.0f, 74.0f}, Vector2 {cx + 16.0f, 74.0f}, wingLo);
    ImageDrawRectangle(&img, static_cast<int>(cx) - 2, 58, 4, 18, edge);                  // fin (seen edge-on)

    // Fuselage: rounded nose, shaded right half, dark centre seam.
    ImageDrawRectangle(&img, static_cast<int>(cx) - 7, 10, 7, 64, body);
    ImageDrawRectangle(&img, static_cast<int>(cx),     10, 7, 64, bodyLo);
    ImageDrawCircle(&img, static_cast<int>(cx), 10, 7, body);
    ImageDrawRectangle(&img, static_cast<int>(cx) - 1, 12, 1, 60, Scale(body, 0.85f));
    ImageDrawRectangle(&img, static_cast<int>(cx) - 7, 72, 14, 2, edge);                 // tail cap

    // Engine cowl and prop hub at the nose.
    ImageDrawCircle(&img, static_cast<int>(cx), 6, 5, DARKGRAY);
    ImageDrawCircle(&img, static_cast<int>(cx), 6, 2, LIGHTGRAY);
    ImageDrawLineEx(&img, Vector2 {cx - 14.0f, 4.0f}, Vector2 {cx + 14.0f, 4.0f}, 2, Fade(BLACK, 0.55f));   // prop blur

    // Canopy: blue glass with a white glint.
    ImageDrawCircle(&img, static_cast<int>(cx), 26, 6, Color {60, 130, 210, 255});
    ImageDrawCircle(&img, static_cast<int>(cx), 26, 4, Color {120, 190, 250, 255});
    ImageDrawPixel(&img, static_cast<int>(cx) - 2, 23, RAYWHITE);
    ImageDrawPixel(&img, static_cast<int>(cx) - 1, 23, RAYWHITE);

    // Navigation lights: red port, green starboard.
    ImageDrawCircle(&img, 4, 61, 2, RED);
    ImageDrawCircle(&img, 60, 61, 2, LIME);

    return Upload(img);
}

// ---- rock -------------------------------------------------------------------

Texture2D Sprites::GenRock()
{
    const int   n = static_cast<int>(cfg::kObstacleW) * S;   // 56
    const float R = static_cast<float>(n) * 0.5f - 1.0f;
    const float c = static_cast<float>(n) * 0.5f;
    Image noise = GenImagePerlinNoise(n, n, 300, 700, 6.0f);
    Color* np   = LoadImageColors(noise);
    assert(np != nullptr);
    Image img = GenImageColor(n, n, BLANK);

    // Sphere shading: normal from height, light from the top-left, plus Perlin bumps.
    const float lx = -0.55f, ly = -0.6f, lz = 0.58f;
    const Color base {128, 122, 116, 255};
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f - c) / R;
            const float dy = (static_cast<float>(y) + 0.5f - c) / R;
            const float d2 = dx * dx + dy * dy;
            if (d2 > 1.0f) { continue; }
            const float nz   = std::sqrt(1.0f - d2);
            float diffuse    = dx * lx + dy * ly + nz * lz;
            if (diffuse < 0.0f) { diffuse = 0.0f; }
            const float bump = 0.75f + 0.5f * static_cast<float>(np[y * n + x].r) / 255.0f;
            const float k    = (0.3f + 0.7f * diffuse) * bump;
            Color col = Scale(base, (k > 1.6f) ? 1.6f : k);
            if (d2 > 0.86f) { col = Scale(col, 0.7f); }   // darker rim
            ImageDrawPixel(&img, x, y, col);
        }
    }
    UnloadImageColors(np);
    UnloadImage(noise);
    return Upload(img);
}

// ---- fuel pump on a dock ---------------------------------------------------

Texture2D Sprites::GenFuel()
{
    const int n = static_cast<int>(cfg::kObstacleW) * S;   // 56
    Image img = GenImageColor(n, n, BLANK);

    // Wooden dock: planks with gaps and a darker rim.
    const Color plank {150, 105, 60, 255};
    ImageDrawRectangle(&img, 0, 0, n, n, Color {95, 62, 30, 255});
    for (int p = 0; p < 6; ++p) {
        ImageDrawRectangle(&img, 2, 2 + p * 9, n - 4, 7, (p % 2 == 0) ? plank : Scale(plank, 0.9f));
    }

    // Pump: red body with a shaded side, white display, hose and nozzle.
    const Color red   {210, 40, 45, 255};
    const Color redLo {150, 25, 30, 255};
    ImageDrawRectangle(&img, 12, 8, 14, 40, red);
    ImageDrawRectangle(&img, 26, 8, 8, 40, redLo);
    ImageDrawRectangle(&img, 12, 8, 22, 4, Scale(redLo, 0.8f));                  // top cap
    ImageDrawRectangle(&img, 15, 15, 14, 12, RAYWHITE);                            // display
    ImageDrawRectangle(&img, 17, 18, 10, 2, DARKBLUE);
    ImageDrawRectangle(&img, 17, 22, 7, 2, DARKBLUE);
    ImageDrawRectangle(&img, 15, 32, 14, 8, redLo);                                // nozzle holster
    ImageDrawRectangle(&img, 10, 46, 26, 4, DARKGRAY);                             // base plinth
    ImageDrawLineEx(&img, Vector2 {36.0f, 14.0f}, Vector2 {46.0f, 24.0f}, 3, DARKGRAY);   // hose
    ImageDrawLineEx(&img, Vector2 {46.0f, 24.0f}, Vector2 {46.0f, 38.0f}, 3, DARKGRAY);
    ImageDrawRectangle(&img, 43, 36, 8, 6, BLACK);                                 // nozzle

    return Upload(img);
}

// ---- bullet ----------------------------------------------------------------

Texture2D Sprites::GenBullet()
{
    const int w = static_cast<int>(cfg::kBulletW) * S;   // 8
    const int h = static_cast<int>(cfg::kBulletH) * S;   // 24
    Image img = GenImageColor(w, h, BLANK);
    ImageDrawRectangle(&img, 0, 2, w, h - 2, Color {255, 150, 40, 200});   // orange glow
    ImageDrawRectangle(&img, 2, 0, w - 4, h - 4, YELLOW);                    // hot core
    ImageDrawRectangle(&img, 3, 0, w - 6, 6, RAYWHITE);                      // white tip
    return Upload(img);
}

// ---- terrain tiles ---------------------------------------------------------

Texture2D Sprites::GenWater()
{
    const int n = cfg::kTileSize;
    Image noise = TileableNoise(n, 0, 0, 2.5f, 7.0f);
    Image img   = Colorize(noise, Color {30, 112, 200, 255}, Color {98, 178, 240, 255}, 0.9f);
    Texture2D tex = Upload(img);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);   // the tile is seamless
    return tex;
}

Texture2D Sprites::GenGrass()
{
    const int n = cfg::kTileSize;
    Image noise = TileableNoise(n, 900, 400, 4.0f, 12.0f);
    Image img   = Colorize(noise, Color {36, 108, 40, 255}, Color {90, 166, 62, 255}, 0.995f);
    Texture2D tex = Upload(img);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    return tex;
}
