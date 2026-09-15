#include "Terrain.h"

#include <cassert>

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

} // namespace

void Terrain::Reset()
{
    scrollOffset_ = 0.0f;
    distance_     = 0.0f;

    for (Obstacle& o : obstacles_) {
        o.active = false;
        o.kind   = Kind::Rock;
        o.rect   = Rectangle {0.0f, 0.0f, 0.0f, 0.0f};
    }

    // Seed from the bottom strip upwards so the player starts in a wide,
    // straight channel and the twists appear at the top.
    Strip seed {static_cast<float>(cfg::kScreenW) * 0.5f, cfg::kRiverMaxW};
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
    assert(cfg::kRockChance >= 0 && cfg::kRockChance + cfg::kFuelChance <= 100);
    const int roll = GetRandomValue(1, 100);
    if (roll <= cfg::kRockChance) {
        PlaceObstacle(strip, Kind::Rock);
    } else if (roll <= cfg::kRockChance + cfg::kFuelChance) {
        PlaceObstacle(strip, Kind::Fuel);
    }
}

void Terrain::PlaceObstacle(const Strip& strip, Kind kind)
{
    const float minX = strip.centreX - strip.width * 0.5f + cfg::kObstacleInset;
    const float maxX = strip.centreX + strip.width * 0.5f - cfg::kObstacleW - cfg::kObstacleInset;
    assert(minX < maxX);

    // Find a free slot; the pool is fixed so give up if it is full.
    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect = Rectangle {
            static_cast<float>(GetRandomValue(static_cast<int>(minX), static_cast<int>(maxX))),
            -cfg::kObstacleH,            // just above the window, scrolls into view
            cfg::kObstacleW, cfg::kObstacleH};
        o.kind   = kind;
        o.active = true;
        return;
    }
}

void Terrain::Update(float dt)
{
    assert(dt >= 0.0f);
    const float step = ScrollSpeed() * dt;
    distance_     += step;
    scrollOffset_ += step;

    // A single frame never scrolls more than a few strips; bound the loop anyway.
    for (int guard = 0; guard < 8 && scrollOffset_ >= static_cast<float>(cfg::kStripH); ++guard) {
        scrollOffset_ -= static_cast<float>(cfg::kStripH);
        ShiftStripsDown();
    }
    assert(scrollOffset_ >= 0.0f && scrollOffset_ < static_cast<float>(cfg::kStripH));

    for (Obstacle& o : obstacles_) {
        if (!o.active) { continue; }
        o.rect.y += step;
        if (o.rect.y > static_cast<float>(cfg::kScreenH)) { o.active = false; }
    }
}

Rectangle Terrain::StripRect(int index) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    // Strip 0 sits partly above the window; offset moves everything downward.
    const float y = static_cast<float>((index - 1) * cfg::kStripH) + scrollOffset_;
    return Rectangle {0.0f, y, static_cast<float>(cfg::kScreenW), static_cast<float>(cfg::kStripH)};
}

void Terrain::Draw(const Sprites& sprites) const
{
    for (int i = 0; i < cfg::kStripCount; ++i) {
        const Strip&    s = strips_[static_cast<size_t>(i)];
        const Rectangle r = StripRect(i);
        const float     left  = s.centreX - s.width * 0.5f;
        const float     right = s.centreX + s.width * 0.5f;

        DrawRectangleRec(Rectangle {0.0f, r.y, left, r.height}, LIME);
        DrawRectangleRec(Rectangle {left, r.y, right - left, r.height}, SKYBLUE);
        DrawRectangleRec(Rectangle {right, r.y, r.width - right, r.height}, LIME);
    }

    for (const Obstacle& o : obstacles_) {
        if (!o.active) { continue; }
        const Texture2D& tex = (o.kind == Kind::Rock) ? sprites.Rock() : sprites.Fuel();
        DrawTexture(tex, static_cast<int>(o.rect.x), static_cast<int>(o.rect.y), WHITE);
        if (o.kind == Kind::Fuel) { DrawFuelLabel(o.rect); }
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

bool Terrain::HitsBankStrip(const Rectangle& r, int index) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    const Strip&    s  = strips_[static_cast<size_t>(index)];
    const Rectangle sr = StripRect(index);
    if (!CheckCollisionRecs(r, sr)) { return false; }

    const float left  = s.centreX - s.width * 0.5f;
    const float right = s.centreX + s.width * 0.5f;
    return (r.x < left) || (r.x + r.width > right);
}

bool Terrain::HitsBank(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    for (int i = 0; i < cfg::kStripCount; ++i) {
        if (HitsBankStrip(r, i)) { return true; }
    }
    return false;
}

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
