#include "Terrain.h"

#include <cassert>
#include <cmath>

namespace {

float Clamp(float v, float lo, float hi)
{
    assert(lo <= hi);
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

float RandomDrift(float magnitude)
{
    assert(magnitude >= 0.0f);
    const int steps = static_cast<int>(magnitude);
    return static_cast<float>(GetRandomValue(-steps, steps));
}

// Cheap integer hash -> [0, 1). Deterministic so trees stay put as strips scroll.
float Hash01(uint32_t seed, uint32_t salt)
{
    uint32_t h = seed ^ (salt * 0x9E3779B9u);
    h ^= h >> 16; h *= 0x85EBCA6Bu;
    h ^= h >> 13; h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return static_cast<float>(h >> 8) / static_cast<float>(1u << 24);
}

const Color kSand      {214, 196, 140, 255};
const Color kSandWet   {160, 140, 90, 255};
const Color kTreeDark  {22, 84, 30, 255};
const Color kTreeLight {48, 132, 52, 255};

} // namespace

// ---- generation -----------------------------------------------------------

void Terrain::Reset()
{
    scrollOffset_ = 0.0f;
    distance_     = 0.0f;
    lastStep_     = 0.0f;

    for (Obstacle& o : obstacles_) {
        o.active = false;
        o.kind   = Kind::Rock;
        o.dir    = 1.0f;
        o.rect   = Rectangle {0.0f, 0.0f, 0.0f, 0.0f};
    }

    // Seed from the bottom strip upwards so the player starts in a wide,
    // straight channel and the twists appear at the top.
    Strip seed {static_cast<float>(cfg::kScreenW) * 0.5f, cfg::kRiverMaxW, nextSeed_};
    for (int i = cfg::kStripCount - 1; i >= 0; --i) {
        strips_[static_cast<size_t>(i)] = seed;
        seed = MakeNextStrip(seed);
    }
    assert(strips_.front().width >= cfg::kRiverMinW);
    assert(strips_.back().width  <= cfg::kRiverMaxW);
}

float Terrain::ScrollSpeed() const
{
    assert(cfg::kRampDistance > 0.0f);
    const float mult = Clamp(1.0f + distance_ / cfg::kRampDistance, 1.0f, cfg::kRampMaxMult);
    assert(mult >= 1.0f && mult <= cfg::kRampMaxMult);
    return cfg::kScrollSpeed * mult;
}

Terrain::Strip Terrain::MakeNextStrip(const Strip& above) const
{
    Strip next;
    next.width   = Clamp(above.width + RandomDrift(cfg::kRiverDriftW),
                         cfg::kRiverMinW, cfg::kRiverMaxW);
    const float half = next.width * 0.5f;
    next.centreX = Clamp(above.centreX + RandomDrift(cfg::kRiverDriftX),
                         cfg::kBankMargin + half,
                         static_cast<float>(cfg::kScreenW) - cfg::kBankMargin - half);
    next.seed    = above.seed * 1664525u + 1013904223u;
    assert(next.centreX - half >= 0.0f);
    assert(next.centreX + half <= static_cast<float>(cfg::kScreenW));
    return next;
}

void Terrain::ShiftStripsDown()
{
    // Drop the bottom strip, move everything down one slot, generate a new top.
    for (size_t i = strips_.size() - 1; i > 0; --i) {
        strips_[i] = strips_[i - 1];
    }
    strips_[0] = MakeNextStrip(strips_[1]);
    TrySpawnObstacle(strips_[0]);
}

void Terrain::TrySpawnObstacle(const Strip& strip)
{
    assert(cfg::kRockChance >= 0 && cfg::kRockChance + cfg::kFuelChance + cfg::kBoatChance <= 100);
    const int roll = GetRandomValue(1, 100);
    if (roll <= cfg::kRockChance) {
        PlaceObstacle(strip, Kind::Rock);
    } else if (roll <= cfg::kRockChance + cfg::kFuelChance) {
        PlaceObstacle(strip, Kind::Fuel);
    } else if (roll <= cfg::kRockChance + cfg::kFuelChance + cfg::kBoatChance) {
        PlaceObstacle(strip, Kind::Boat);
    }
}

void Terrain::PlaceObstacle(const Strip& strip, Kind kind)
{
    const float w    = (kind == Kind::Boat) ? cfg::kBoatW : cfg::kObstacleW;
    const float h    = (kind == Kind::Boat) ? cfg::kBoatH : cfg::kObstacleH;
    const float minX = strip.centreX - strip.width * 0.5f + cfg::kObstacleInset;
    const float maxX = strip.centreX + strip.width * 0.5f - w - cfg::kObstacleInset;
    assert(minX < maxX);

    // Find a free slot; the pool is fixed so give up if it is full.
    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect = Rectangle {
            static_cast<float>(GetRandomValue(static_cast<int>(minX), static_cast<int>(maxX))),
            -h,                          // just above the window, scrolls into view
            w, h};
        o.kind   = kind;
        o.dir    = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        o.active = true;
        return;
    }
}

void Terrain::MoveBoat(Obstacle& o, float dt)
{
    assert(o.active && o.kind == Kind::Boat && dt >= 0.0f);
    o.rect.x += o.dir * cfg::kBoatSpeed * dt;
    // Turn around a little short of each bank; clamp so it never sits on land.
    float left = 0.0f, right = 0.0f;
    BankAt(o.rect.y + o.rect.height * 0.5f, left, right);
    const float margin = 6.0f;
    if (o.rect.x < left + margin)                { o.rect.x = left + margin;  o.dir = 1.0f; }
    if (o.rect.x + o.rect.width > right - margin) { o.rect.x = right - margin - o.rect.width; o.dir = -1.0f; }
    assert(o.dir == 1.0f || o.dir == -1.0f);
}

void Terrain::Update(float dt)
{
    assert(dt >= 0.0f);
    const float step = ScrollSpeed() * dt;
    distance_     += step;
    scrollOffset_ += step;
    lastStep_      = step;

    // A single frame never scrolls more than a few strips; bound the loop anyway.
    for (int guard = 0; guard < 8 && scrollOffset_ >= static_cast<float>(cfg::kStripH); ++guard) {
        scrollOffset_ -= static_cast<float>(cfg::kStripH);
        ShiftStripsDown();
    }
    assert(scrollOffset_ >= 0.0f && scrollOffset_ < static_cast<float>(cfg::kStripH));

    for (Obstacle& o : obstacles_) {
        if (!o.active) { continue; }
        o.rect.y += step;
        if (o.rect.y > static_cast<float>(cfg::kScreenH)) { o.active = false; continue; }
        if (o.kind == Kind::Boat && o.rect.y > 0.0f) { MoveBoat(o, dt); }
    }
}

// ---- geometry -------------------------------------------------------------

float Terrain::StripTopY(int index) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    // Strip 0 sits partly above the window; offset moves everything downward.
    return static_cast<float>((index - 1) * cfg::kStripH) + scrollOffset_;
}

void Terrain::BankAt(float y, float& left, float& right) const
{
    // Strip whose top edge is at or above y, clamped so index+1 exists.
    int idx = static_cast<int>(std::floor((y - scrollOffset_) / static_cast<float>(cfg::kStripH))) + 1;
    if (idx < 0) { idx = 0; }
    if (idx > cfg::kStripCount - 2) { idx = cfg::kStripCount - 2; }
    const float t = Clamp((y - StripTopY(idx)) / static_cast<float>(cfg::kStripH), 0.0f, 1.0f);

    const Strip& a = strips_[static_cast<size_t>(idx)];
    const Strip& b = strips_[static_cast<size_t>(idx + 1)];
    const float centre = a.centreX + (b.centreX - a.centreX) * t;
    const float half   = (a.width + (b.width - a.width) * t) * 0.5f;
    left  = centre - half;
    right = centre + half;
    assert(left >= 0.0f && right <= static_cast<float>(cfg::kScreenW) && left < right);
}

bool Terrain::HitsBank(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    // Banks are piecewise linear, so sampling every few px down the box is enough.
    const float step = 6.0f;
    for (float y = r.y; y <= r.y + r.height + step; y += step) {
        const float sy = (y > r.y + r.height) ? (r.y + r.height) : y;
        float left = 0.0f, right = 0.0f;
        BankAt(sy, left, right);
        if (r.x < left || r.x + r.width > right) { return true; }
        if (sy >= r.y + r.height) { break; }
    }
    return false;
}

// ---- drawing --------------------------------------------------------------

void Terrain::Draw(const Sprites& sprites) const
{
    // Land everywhere first (textured, scrolling), then the river cut into it.
    const float scroll = -distance_;
    const Rectangle grassSrc {0.0f, scroll, static_cast<float>(cfg::kScreenW), static_cast<float>(cfg::kScreenH)};
    DrawTexturePro(sprites.Grass(), grassSrc,
                   Rectangle {0.0f, 0.0f, static_cast<float>(cfg::kScreenW), static_cast<float>(cfg::kScreenH)},
                   Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
    DrawWater(sprites);
    DrawShallows();
    DrawShore();
    for (int i = 0; i < cfg::kStripCount; ++i) { DrawTrees(i, sprites); }
    DrawObstacles(sprites);
}

void Terrain::DrawWater(const Sprites& sprites) const
{
    // Thin horizontal slices between the interpolated banks; the water tile
    // scrolls with the river so it looks like it is flowing.
    const float scroll = -distance_;
    const float h      = static_cast<float>(cfg::kSliceH);
    for (int y = -cfg::kSliceH; y < cfg::kScreenH; y += cfg::kSliceH) {
        const float fy = static_cast<float>(y);
        float left = 0.0f, right = 0.0f;
        BankAt(fy + h * 0.5f, left, right);
        const Rectangle src {left, fy + scroll, right - left, h};
        DrawTexturePro(sprites.Water(), src, Rectangle {left, fy, right - left, h}, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
    }
}

void Terrain::DrawShallows() const
{
    // A greenish, slightly darker band of shallow water hugging each bank.
    const float h = static_cast<float>(cfg::kSliceH);
    for (int y = -cfg::kSliceH; y < cfg::kScreenH; y += cfg::kSliceH) {
        const float fy = static_cast<float>(y);
        float left = 0.0f, right = 0.0f;
        BankAt(fy + h * 0.5f, left, right);
        const float w = cfg::kShallowW;
        DrawRectangleGradientH(static_cast<int>(left), static_cast<int>(fy), static_cast<int>(w), cfg::kSliceH,
                               Fade(Color {30, 90, 60, 255}, 0.35f), Fade(Color {30, 90, 60, 255}, 0.0f));
        DrawRectangleGradientH(static_cast<int>(right - w), static_cast<int>(fy), static_cast<int>(w), cfg::kSliceH,
                               Fade(Color {30, 90, 60, 255}, 0.0f), Fade(Color {30, 90, 60, 255}, 0.35f));
    }
}

void Terrain::DrawTreeReflection(float tx, float ty, float r, int side) const
{
    assert(side == 0 || side == 1);
    float left = 0.0f, right = 0.0f;
    BankAt(ty, left, right);
    const float bank = (side == 0) ? left : right;
    const float gap  = (side == 0) ? (bank - tx) : (tx - bank);   // tree centre to shoreline
    if (gap > cfg::kReflectReach) { return; }
    // Mirror across the shoreline; the water side gets a stretched, faint smudge.
    const float rx = (side == 0) ? (bank + gap) : (bank - gap);
    const float fade = 0.28f * (1.0f - gap / cfg::kReflectReach);
    DrawEllipse(static_cast<int>(rx), static_cast<int>(ty + 2.0f), r * 0.9f, r * 1.5f, Fade(kTreeDark, fade));
}

void Terrain::DrawShore() const
{
    // Sand line along each bank, drawn segment by segment between strip tops,
    // with a thin wet edge on the water side.
    for (int i = 0; i < cfg::kStripCount - 1; ++i) {
        const float y0 = StripTopY(i);
        const float y1 = StripTopY(i + 1);
        const Strip& a = strips_[static_cast<size_t>(i)];
        const Strip& b = strips_[static_cast<size_t>(i + 1)];
        const Vector2 l0 {a.centreX - a.width * 0.5f, y0};
        const Vector2 l1 {b.centreX - b.width * 0.5f, y1};
        const Vector2 r0 {a.centreX + a.width * 0.5f, y0};
        const Vector2 r1 {b.centreX + b.width * 0.5f, y1};
        DrawLineEx(Vector2 {l0.x - 2.0f, l0.y}, Vector2 {l1.x - 2.0f, l1.y}, 4.0f, kSand);
        DrawLineEx(Vector2 {r0.x + 2.0f, r0.y}, Vector2 {r1.x + 2.0f, r1.y}, 4.0f, kSand);
        DrawLineEx(l0, l1, 1.5f, kSandWet);
        DrawLineEx(r0, r1, 1.5f, kSandWet);
    }
}

void Terrain::DrawTrees(int index, const Sprites& sprites) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    const Strip& s  = strips_[static_cast<size_t>(index)];
    const float  y0 = StripTopY(index);

    for (int side = 0; side < 2; ++side) {
        for (int k = 0; k < cfg::kTreesPerSide; ++k) {
            const uint32_t salt = static_cast<uint32_t>(side * 16 + k * 4);
            if (Hash01(s.seed, salt) > cfg::kTreeChance) { continue; }   // empty slot
            const float ty = y0 + Hash01(s.seed, salt + 1) * static_cast<float>(cfg::kStripH);
            const float r  = cfg::kTreeMinR + Hash01(s.seed, salt + 2) * (cfg::kTreeMaxR - cfg::kTreeMinR);

            float left = 0.0f, right = 0.0f;
            BankAt(ty, left, right);
            const float lo = (side == 0) ? r + 4.0f : right + r + 8.0f;
            const float hi = (side == 0) ? left - r - 8.0f : static_cast<float>(cfg::kScreenW) - r - 4.0f;
            if (hi - lo < r) { continue; }   // bank too narrow here

            const float tx = lo + Hash01(s.seed, salt + 3) * (hi - lo);
            // Sprite variant and a slight tint per tree so a bank is not a field of clones.
            const int   variant = (Hash01(s.seed, salt + 4) < 0.5f) ? 0 : 1;
            const float shade   = 0.85f + 0.3f * Hash01(s.seed, salt + 5);
            const Color tint    { static_cast<unsigned char>(255.0f * ((shade > 1.0f) ? 1.0f : shade)),
                                  static_cast<unsigned char>(255.0f * ((shade > 1.0f) ? 1.0f : shade)),
                                  static_cast<unsigned char>(255.0f * ((shade > 1.0f) ? 1.0f : shade) * 0.95f), 255 };
            DrawTreeReflection(tx, ty, r, side);
            DrawEllipse(static_cast<int>(tx) + 4, static_cast<int>(ty) + 5, r, r * 0.8f, Fade(BLACK, 0.28f));   // shadow
            Sprites::DrawInto(sprites.Tree(variant), tx - r, ty - r, r * 2.0f, r * 2.0f, tint);
        }
    }
}

void Terrain::DrawObstacles(const Sprites& sprites) const
{
    for (const Obstacle& o : obstacles_) {
        if (!o.active) { continue; }
        const float cx = o.rect.x + o.rect.width * 0.5f;
        const float cy = o.rect.y + o.rect.height * 0.5f;
        if (o.kind == Kind::Rock) {
            // Ripple ring and a shadow in the water, then the lit rock.
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), o.rect.width * 0.72f, Fade(RAYWHITE, 0.35f));
            DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 4, o.rect.width * 0.5f, o.rect.height * 0.42f, Fade(BLACK, 0.3f));
            Sprites::DrawInto(sprites.Rock(), o.rect.x, o.rect.y, o.rect.width, o.rect.height, WHITE);
        } else if (o.kind == Kind::Boat) {
            // Wake: two foam lines trailing from the stern, then shadow, then the boat
            // (sprite faces +x; flip horizontally when heading left).
            const float stern = (o.dir > 0.0f) ? o.rect.x : o.rect.x + o.rect.width;
            const float back  = -o.dir * 26.0f;
            DrawLineEx(Vector2 {stern, cy - 4.0f}, Vector2 {stern + back, cy - 10.0f}, 2.0f, Fade(RAYWHITE, 0.5f));
            DrawLineEx(Vector2 {stern, cy + 4.0f}, Vector2 {stern + back, cy + 10.0f}, 2.0f, Fade(RAYWHITE, 0.5f));
            DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 5, o.rect.width * 0.5f, o.rect.height * 0.5f, Fade(BLACK, 0.3f));
            const Rectangle src {0.0f, 0.0f, static_cast<float>(sprites.Boat().width) * o.dir, static_cast<float>(sprites.Boat().height)};
            DrawTexturePro(sprites.Boat(), src, o.rect, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
        } else {
            Sprites::DrawInto(sprites.Fuel(), o.rect.x, o.rect.y, o.rect.width, o.rect.height, WHITE);
            DrawFuelLabel(o.rect);
        }
    }
}

void Terrain::DrawFuelLabel(const Rectangle& depot)
{
    // Caption centred above the pump, with a dark shadow so it reads on the water.
    const char* text = "FUEL";
    const int   tw   = MeasureText(text, cfg::kFuelLabelSize);
    assert(tw > 0 && tw < cfg::kScreenW);
    const int x = static_cast<int>(depot.x + depot.width * 0.5f) - tw / 2;
    const int y = static_cast<int>(depot.y) - cfg::kFuelLabelSize - 2;
    DrawText(text, x + 1, y + 1, cfg::kFuelLabelSize, DARKBLUE);
    DrawText(text, x,     y,     cfg::kFuelLabelSize, RAYWHITE);
}

// ---- obstacles ------------------------------------------------------------

int Terrain::FindObstacle(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    for (int i = 0; i < cfg::kMaxObstacles; ++i) {
        const Obstacle& o = obstacles_[static_cast<size_t>(i)];
        if (o.active && CheckCollisionRecs(r, o.rect)) { return i; }
    }
    return -1;
}

const Terrain::Obstacle& Terrain::ObstacleAt(int i) const
{
    assert(i >= 0 && i < cfg::kMaxObstacles);
    assert(obstacles_[static_cast<size_t>(i)].active);
    return obstacles_[static_cast<size_t>(i)];
}

void Terrain::RemoveObstacle(int i)
{
    assert(i >= 0 && i < cfg::kMaxObstacles);
    obstacles_[static_cast<size_t>(i)].active = false;
}
