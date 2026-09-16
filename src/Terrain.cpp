#include "Terrain.h"

#include <cassert>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265f;

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

bool Roll(int percent)
{
    assert(percent >= 0 && percent <= 100);
    return GetRandomValue(1, 100) <= percent;
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

// Outer-bank width an island of 'islandW' needs so both channels stay legal.
float WidthNeededFor(float islandW)
{
    assert(islandW >= 0.0f);
    return islandW + 2.0f * cfg::kChannelMinW;
}

Color LerpColor(Color a, Color b, float t)
{
    assert(t >= 0.0f && t <= 1.0f);
    return Color {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t), 255 };
}

const Color kSandWet   {160, 140, 90, 255};
const Color kTreeDark  {22, 84, 30, 255};
const Color kShallow   {30, 90, 60, 255};

} // namespace

// ============================================================================
// Generation
// ============================================================================

void Terrain::Reset(const Tuning& tuning)
{
    assert(tuning.obstacleScale > 0.0f && tuning.scrollSpeed > 0.0f && tuning.rampDistance > 0.0f);
    tuning_       = tuning;
    scrollOffset_ = 0.0f;
    distance_     = 0.0f;
    lastStep_     = 0.0f;
    fishTimer_    = cfg::kFishInterval;
    phase_        = Phase::Single;
    phaseLeft_    = 0;
    islandGap_    = cfg::kStripCount;   // the first screen is always a single channel
    islandTarget_ = 0.0f;
    gunSpacing_   = 0;
    bridgeGap_    = cfg::kStripCount;

    for (Obstacle& o : obstacles_) {
        o.active = false;
        o.kind   = Kind::Rock;
        o.dir    = 1.0f;
        o.timer  = 0.0f;
        o.aim    = 0.0f;
        o.flash  = 0.0f;
        o.hp     = 1;
        o.rect   = Rectangle {0.0f, 0.0f, 0.0f, 0.0f};
    }
    for (Beast& b : critters_) { b.active = false; b.age = 0.0f; b.vx = 0.0f; b.kind = Critter::Duck; b.pos = Vector2 {0.0f, 0.0f}; }

    // Seed from the bottom strip upwards so the player starts in a wide,
    // straight channel and the twists appear at the top.
    const float mid = static_cast<float>(cfg::kScreenW) * 0.5f;
    Strip seed {mid, cfg::kRiverMaxW * 0.7f, mid, 0.0f, nextSeed_};
    for (int i = cfg::kStripCount - 1; i >= 0; --i) {
        strips_[static_cast<size_t>(i)] = seed;
        seed = MakeNextStrip(seed);
    }
    assert(strips_.front().width >= cfg::kRiverMinW);
    assert(strips_.back().islandWidth == 0.0f);
}

float Terrain::ScrollSpeed() const
{
    assert(tuning_.rampDistance > 0.0f);
    const float mult = Clamp(1.0f + distance_ / tuning_.rampDistance, 1.0f, cfg::kRampMaxMult);
    assert(mult >= 1.0f && mult <= cfg::kRampMaxMult);
    return tuning_.scrollSpeed * mult;
}

int Terrain::StageIndex() const
{
    const int idx = static_cast<int>(distance_ / cfg::kStageLength) % cfg::kStageCount;
    assert(idx >= 0 && idx < cfg::kStageCount);
    return idx;
}

float Terrain::StageProgress() const
{
    const float p = std::fmod(distance_, cfg::kStageLength);
    assert(p >= 0.0f && p < cfg::kStageLength);
    return p;
}

void Terrain::AdvancePhase(Strip& next)
{
    // Called once per new strip, after the outer banks are set. Decides the
    // island width for this strip and moves the phase machine along.
    switch (phase_) {
        case Phase::Single:
            next.islandWidth = 0.0f;
            if (islandGap_ > 0) { --islandGap_; break; }
            if (!Roll(cfg::kIslandChance)) { break; }
            islandTarget_ = static_cast<float>(GetRandomValue(static_cast<int>(cfg::kIslandMinW), static_cast<int>(cfg::kIslandMaxW)));
            if (next.width < WidthNeededFor(islandTarget_)) { break; }   // river too narrow here; try again later
            phase_     = Phase::Splitting;
            phaseLeft_ = cfg::kSplitStrips;
            break;
        case Phase::Splitting:
            --phaseLeft_;
            next.islandWidth = islandTarget_ * static_cast<float>(cfg::kSplitStrips - phaseLeft_) / static_cast<float>(cfg::kSplitStrips);
            if (phaseLeft_ == 0) {
                phase_     = Phase::Island;
                phaseLeft_ = GetRandomValue(cfg::kIslandMinStrips, cfg::kIslandMaxStrips);
            }
            break;
        case Phase::Island:
            --phaseLeft_;
            next.islandWidth = islandTarget_;
            if (phaseLeft_ == 0) {
                phase_     = Phase::Merging;
                phaseLeft_ = cfg::kSplitStrips;
            }
            break;
        case Phase::Merging:
            --phaseLeft_;
            next.islandWidth = islandTarget_ * static_cast<float>(phaseLeft_) / static_cast<float>(cfg::kSplitStrips);
            if (phaseLeft_ == 0) {
                phase_     = Phase::Single;
                islandGap_ = cfg::kIslandGapStrips;
            }
            break;
    }
    assert(next.islandWidth >= 0.0f && next.islandWidth <= cfg::kIslandMaxW);
}

Terrain::Strip Terrain::MakeNextStrip(const Strip& above)
{
    Strip next;
    next.seed  = above.seed * 1664525u + 1013904223u;
    next.width = Clamp(above.width + RandomDrift(cfg::kRiverDriftW), cfg::kRiverMinW, cfg::kRiverMaxW);

    // While an island is present or being built the outer banks must stay wide.
    const bool building = (phase_ != Phase::Single);
    if (building && next.width < WidthNeededFor(islandTarget_)) { next.width = WidthNeededFor(islandTarget_); }

    const float half = next.width * 0.5f;
    next.centreX = Clamp(above.centreX + RandomDrift(cfg::kRiverDriftX),
                         cfg::kBankMargin + half,
                         static_cast<float>(cfg::kScreenW) - cfg::kBankMargin - half);

    AdvancePhase(next);

    // Island centre drifts gently and is kept where both channels stay legal.
    const float left  = next.centreX - half;
    const float right = next.centreX + half;
    const float ihalf = next.islandWidth * 0.5f;
    float ic = above.islandCentre + RandomDrift(8.0f);
    if (above.islandWidth == 0.0f && next.islandWidth == 0.0f) { ic = next.centreX; }   // follow the river when idle
    next.islandCentre = Clamp(ic, left + cfg::kChannelMinW + ihalf, right - cfg::kChannelMinW - ihalf);

    assert(left >= 0.0f && right <= static_cast<float>(cfg::kScreenW));
    assert(next.islandWidth == 0.0f || (next.islandCentre - ihalf - left >= cfg::kChannelMinW - 0.01f));
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
    TrySpawnPickup(strips_[0]);
    TrySpawnBridge(strips_[0]);
    TryPlaceGun(strips_[0]);
    TryPlaceSam(strips_[0]);
    TrySpawnCritter(strips_[0]);
}

void Terrain::TrySpawnObstacle(const Strip& strip)
{
    // Rocks and boats scale with difficulty; fuel does not.
    const int rock = static_cast<int>(static_cast<float>(cfg::kRockChance) * tuning_.obstacleScale);
    const int boat = static_cast<int>(static_cast<float>(cfg::kBoatChance) * tuning_.obstacleScale);
    assert(rock >= 0 && rock + cfg::kFuelChance + boat <= 100);
    const int roll = GetRandomValue(1, 100);
    if (roll <= rock) {
        PlaceObstacle(strip, Kind::Rock);
    } else if (roll <= rock + cfg::kFuelChance) {
        PlaceObstacle(strip, Kind::Fuel);
    } else if (roll <= rock + cfg::kFuelChance + boat) {
        PlaceObstacle(strip, Kind::Boat);
    }
}

void Terrain::TrySpawnPickup(const Strip& strip)
{
    const int roll = GetRandomValue(1, 100);
    int acc = cfg::kStarChance;
    if (roll <= acc) { PlaceObstacle(strip, Kind::Star);   return; }
    acc += cfg::kShieldChance;
    if (roll <= acc) { PlaceObstacle(strip, Kind::Shield); return; }
    acc += cfg::kSpreadChance;
    if (roll <= acc) { PlaceObstacle(strip, Kind::Spread); return; }
    acc += cfg::kLifeChance;
    if (roll <= acc) { PlaceObstacle(strip, Kind::Life);   return; }
    assert(acc <= 100);
}

void Terrain::TrySpawnBridge(const Strip& strip)
{
    if (bridgeGap_ > 0) { --bridgeGap_; return; }
    if (strip.islandWidth > 0.0f || !Roll(cfg::kBridgeChance)) { return; }
    bridgeGap_ = cfg::kBridgeGap;

    const float left  = strip.centreX - strip.width * 0.5f;
    const float right = strip.centreX + strip.width * 0.5f;
    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect   = Rectangle {left + 2.0f, -cfg::kBridgeH, right - left - 4.0f, cfg::kBridgeH};
        o.kind   = Kind::Bridge;
        o.dir    = 1.0f;
        o.hp     = cfg::kBridgeHp;
        o.active = true;
        return;
    }
}

void Terrain::PlaceObstacle(const Strip& strip, Kind kind)
{
    float w = cfg::kObstacleW, h = cfg::kObstacleH;
    if (kind == Kind::Boat) { w = cfg::kBoatW; h = cfg::kBoatH; }
    if (IsPickup(kind))     { w = cfg::kPickupSize; h = cfg::kPickupSize; }

    // Pick a channel at random, then a spot inside it clear of the banks.
    std::array<Span, 2> spans {};
    const int  n    = SpansOf(strip.centreX, strip.width, strip.islandCentre, strip.islandWidth, spans);
    const Span span = spans[static_cast<size_t>(GetRandomValue(0, n - 1))];
    const float minX = span.left + cfg::kObstacleInset;
    const float maxX = span.right - w - cfg::kObstacleInset;
    if (maxX <= minX) { return; }   // channel too narrow for this one

    // Find a free slot; the pool is fixed so give up if it is full.
    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect = Rectangle {
            static_cast<float>(GetRandomValue(static_cast<int>(minX), static_cast<int>(maxX))),
            -h,                          // just above the window, scrolls into view
            w, h};
        o.kind   = kind;
        o.dir    = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        o.hp     = 1;
        o.active = true;
        return;
    }
}

void Terrain::TryPlaceGun(const Strip& strip)
{
    // Guns live on islands only, once the island is wide enough to hold one,
    // and never closer together than kGunSpacingStrips.
    if (gunSpacing_ > 0) { --gunSpacing_; }
    if (!tuning_.guns || strip.islandWidth < cfg::kGunMinIsland || gunSpacing_ > 0) { return; }
    if (!Roll(cfg::kGunChance)) { return; }
    gunSpacing_ = cfg::kGunSpacingStrips;
    const float reach = strip.islandWidth * 0.5f - cfg::kGunSize * 0.5f - 6.0f;
    assert(reach >= 0.0f);
    const float x = strip.islandCentre + RandomDrift(reach) - cfg::kGunSize * 0.5f;

    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect   = Rectangle {x, -cfg::kGunSize, cfg::kGunSize, cfg::kGunSize};
        o.kind   = Kind::Gun;
        o.dir    = 1.0f;
        o.timer  = cfg::kGunReload;   // a fresh gun waits a full reload before its first shot
        o.aim    = 180.0f;            // barrel down-river until it acquires the plane
        o.flash  = 0.0f;
        o.hp     = 1;
        o.active = true;
        return;
    }
}

void Terrain::TryPlaceSam(const Strip& strip)
{
    // Shares the island spacing with guns so a site is never on top of one.
    if (!tuning_.guns || strip.islandWidth < cfg::kSamMinIsland || gunSpacing_ > 0) { return; }
    if (!Roll(cfg::kSamChance)) { return; }
    gunSpacing_ = cfg::kGunSpacingStrips;
    const float reach = strip.islandWidth * 0.5f - cfg::kSamSize * 0.5f - 6.0f;
    assert(reach >= 0.0f);
    const float x = strip.islandCentre + RandomDrift(reach) - cfg::kSamSize * 0.5f;

    for (Obstacle& o : obstacles_) {
        if (o.active) { continue; }
        o.rect   = Rectangle {x, -cfg::kSamSize, cfg::kSamSize, cfg::kSamSize};
        o.kind   = Kind::Sam;
        o.dir    = 1.0f;
        o.timer  = cfg::kSamReload * 0.5f;   // half a reload before the first shot
        o.aim    = 0.0f;
        o.flash  = 0.0f;
        o.hp     = 1;
        o.active = true;
        return;
    }
}

int Terrain::UpdateSams(float dt, const std::array<Vector2, cfg::kMaxPilots>& targets, const std::array<int, cfg::kMaxPilots>& targetPilot,
                        int targetCount, bool mayFire, Missiles& missiles)
{
    assert(dt >= 0.0f && targetCount >= 0 && targetCount <= cfg::kMaxPilots);
    int launched = 0;
    for (Obstacle& o : obstacles_) {
        if (!o.active || o.kind != Kind::Sam) { continue; }
        if (o.flash > 0.0f) { o.flash -= dt; }
        o.aim += 180.0f * dt;                          // the dish just keeps spinning
        if (o.aim >= 360.0f) { o.aim -= 360.0f; }
        if (targetCount == 0 || o.rect.y < 20.0f) { continue; }

        const Vector2 c {o.rect.x + o.rect.width * 0.5f, o.rect.y + o.rect.height * 0.5f};
        int   best = -1;
        float bestD = cfg::kSamRange;
        for (int i = 0; i < targetCount; ++i) {
            const float ex = targets[static_cast<size_t>(i)].x - c.x;
            const float ey = targets[static_cast<size_t>(i)].y - c.y;
            const float d  = std::sqrt(ex * ex + ey * ey);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best < 0) { continue; }
        o.timer -= dt;
        if (o.timer > 0.0f || !mayFire) { continue; }
        missiles.Launch(Vector2 {c.x, o.rect.y}, targets[static_cast<size_t>(best)], targetPilot[static_cast<size_t>(best)]);
        o.timer = cfg::kSamReload;
        o.flash = 0.3f;
        ++launched;
    }
    assert(launched >= 0 && launched <= cfg::kMaxObstacles);
    return launched;
}

// ---- critters ---------------------------------------------------------------

void Terrain::SpawnCritter(Critter kind, Vector2 pos, float vx)
{
    for (Beast& b : critters_) {
        if (b.active) { continue; }
        b.kind = kind; b.pos = pos; b.vx = vx; b.age = 0.0f; b.active = true;
        return;
    }
}

void Terrain::TrySpawnCritter(const Strip& strip)
{
    std::array<Span, 2> spans {};
    const int  n    = SpansOf(strip.centreX, strip.width, strip.islandCentre, strip.islandWidth, spans);
    const Span span = spans[static_cast<size_t>(GetRandomValue(0, n - 1))];
    const float inWater = static_cast<float>(GetRandomValue(static_cast<int>(span.left + 30.0f), static_cast<int>(span.right - 30.0f)));

    if (Roll(cfg::kDuckChance)) {
        SpawnCritter(Critter::Duck, Vector2 {inWater, -10.0f}, (GetRandomValue(0, 1) == 0) ? -cfg::kDuckSpeed : cfg::kDuckSpeed);
    } else if (Roll(cfg::kDeerChance)) {
        // On a bank a little back from the shore, left or right.
        const bool  leftBank = GetRandomValue(0, 1) == 0;
        const float x = leftBank ? spans[0].left - 30.0f : spans[static_cast<size_t>(n - 1)].right + 30.0f;
        if (x > 20.0f && x < static_cast<float>(cfg::kScreenW) - 20.0f) { SpawnCritter(Critter::Deer, Vector2 {x, -10.0f}, 0.0f); }
    } else if (Roll(cfg::kOtterChance)) {
        SpawnCritter(Critter::Otter, Vector2 {inWater, -10.0f}, (GetRandomValue(0, 1) == 0) ? -cfg::kOtterSpeed : cfg::kOtterSpeed);
    }
}

void Terrain::TryFishJump()
{
    // Somewhere in the visible water, at a random row.
    const float y = static_cast<float>(GetRandomValue(120, cfg::kScreenH - 200));
    std::array<Span, 2> spans {};
    const int  n    = SpansAt(y, spans);
    const Span span = spans[static_cast<size_t>(GetRandomValue(0, n - 1))];
    if (span.right - span.left < 80.0f) { return; }
    const float x = static_cast<float>(GetRandomValue(static_cast<int>(span.left + 30.0f), static_cast<int>(span.right - 30.0f)));
    SpawnCritter(Critter::Fish, Vector2 {x, y}, 0.0f);
}

int Terrain::UpdateCritters(float dt, const std::array<Vector2, cfg::kMaxPilots>& planes, int planeCount)
{
    assert(dt >= 0.0f && planeCount >= 0 && planeCount <= cfg::kMaxPilots);
    fishTimer_ -= dt;
    if (fishTimer_ <= 0.0f) {
        fishTimer_ = cfg::kFishInterval;
        if (GetRandomValue(0, 1) == 0) { TryFishJump(); }
    }

    int spotted = 0;
    for (Beast& b : critters_) {
        if (!b.active) { continue; }
        b.age += dt;
        if (b.kind == Critter::Fish) {
            if (b.age >= cfg::kFishJumpSecs) { b.active = false; }
            continue;
        }
        // Everything else rides the river, and swimmers move sideways too.
        b.pos.y += lastStep_;
        if (b.kind == Critter::Duck || b.kind == Critter::Otter) {
            b.pos.x += b.vx * dt;
            std::array<Span, 2> spans {};
            const int n = SpansAt(b.pos.y, spans);
            bool inside = false;
            for (int i = 0; i < n; ++i) {
                const Span& s = spans[static_cast<size_t>(i)];
                if (b.pos.x > s.left + 14.0f && b.pos.x < s.right - 14.0f) { inside = true; }
            }
            if (!inside) { b.vx = -b.vx; b.pos.x += b.vx * dt * 2.0f; }   // turn at the shore
        }
        if (b.kind == Critter::Otter) {
            for (int i = 0; i < planeCount; ++i) {
                const float dx = planes[static_cast<size_t>(i)].x - b.pos.x;
                const float dy = planes[static_cast<size_t>(i)].y - b.pos.y;
                if (dx * dx + dy * dy < cfg::kOtterSpotRange * cfg::kOtterSpotRange) {
                    ++spotted;
                    b.active = false;   // dives
                    break;
                }
            }
        }
        if (b.pos.y > static_cast<float>(cfg::kScreenH) + 20.0f) { b.active = false; }
    }
    assert(spotted >= 0 && spotted <= cfg::kMaxCritters);
    return spotted;
}

// ---- boats and guns -----------------------------------------------------------

void Terrain::MoveBoat(Obstacle& o, float dt)
{
    assert(o.active && o.kind == Kind::Boat && dt >= 0.0f);
    o.rect.x += o.dir * cfg::kBoatSpeed * dt;

    // Stay in whichever channel holds the boat's centre (nearest if an island
    // has just grown under it); turn around a little short of each shore.
    std::array<Span, 2> spans {};
    const int   n  = SpansAt(o.rect.y + o.rect.height * 0.5f, spans);
    const float cx = o.rect.x + o.rect.width * 0.5f;
    int best = 0;
    for (int i = 1; i < n; ++i) {
        const float d0 = std::fabs(cx - (spans[static_cast<size_t>(best)].left + spans[static_cast<size_t>(best)].right) * 0.5f);
        const float d1 = std::fabs(cx - (spans[static_cast<size_t>(i)].left + spans[static_cast<size_t>(i)].right) * 0.5f);
        if (d1 < d0) { best = i; }
    }
    const Span& s = spans[static_cast<size_t>(best)];
    const float margin = 6.0f;
    if (o.rect.x < s.left + margin)                  { o.rect.x = s.left + margin;  o.dir = 1.0f; }
    if (o.rect.x + o.rect.width > s.right - margin)  { o.rect.x = s.right - margin - o.rect.width; o.dir = -1.0f; }
    assert(o.dir == 1.0f || o.dir == -1.0f);
}

int Terrain::UpdateGuns(float dt, const std::array<Vector2, cfg::kMaxPilots>& targets, int targetCount, bool mayFire, Shells& shells)
{
    assert(dt >= 0.0f && targetCount >= 0 && targetCount <= cfg::kMaxPilots);
    int fired = 0;
    if (targetCount == 0) { return 0; }
    for (Obstacle& o : obstacles_) {
        if (!o.active || o.kind != Kind::Gun) { continue; }
        if (o.flash > 0.0f) { o.flash -= dt; }

        const Vector2 c {o.rect.x + o.rect.width * 0.5f, o.rect.y + o.rect.height * 0.5f};
        // Nearest target wins.
        Vector2 target = targets[0];
        float   best   = 1.0e9f;
        for (int i = 0; i < targetCount; ++i) {
            const float ex = targets[static_cast<size_t>(i)].x - c.x;
            const float ey = targets[static_cast<size_t>(i)].y - c.y;
            const float d2 = ex * ex + ey * ey;
            if (d2 < best) { best = d2; target = targets[static_cast<size_t>(i)]; }
        }
        const float dx   = target.x - c.x;
        const float dy   = target.y - c.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const bool onScreen = c.y > 30.0f && c.y < static_cast<float>(cfg::kScreenH) - 40.0f;
        if (!onScreen || dist > cfg::kGunRange || dist < 1.0f) { continue; }

        // Track the plane; the sprite's barrel points up, so 0 deg = up, clockwise positive.
        o.aim = std::atan2(dx, -dy) * 180.0f / kPi;
        o.timer -= dt;
        if (o.timer > 0.0f || !mayFire) { continue; }

        const Vector2 vel {dx / dist * tuning_.shellSpeed, dy / dist * tuning_.shellSpeed};
        const Vector2 muzzle {c.x + dx / dist * o.rect.width * 0.5f, c.y + dy / dist * o.rect.height * 0.5f};
        shells.Fire(muzzle, vel);
        o.timer = cfg::kGunReload;
        o.flash = cfg::kGunFlashSeconds;
        ++fired;
    }
    assert(fired >= 0 && fired <= cfg::kMaxObstacles);
    return fired;
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

// ============================================================================
// Geometry
// ============================================================================

float Terrain::StripTopY(int index) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    // Strip 0 sits partly above the window; offset moves everything downward.
    return static_cast<float>((index - 1) * cfg::kStripH) + scrollOffset_;
}

int Terrain::SpansOf(float centre, float width, float islandCentre, float islandWidth, std::array<Span, 2>& out)
{
    assert(width > 0.0f && islandWidth >= 0.0f);
    const float left  = centre - width * 0.5f;
    const float right = centre + width * 0.5f;
    if (islandWidth < 1.0f) {
        out[0] = Span {left, right};
        return 1;
    }
    out[0] = Span {left, islandCentre - islandWidth * 0.5f};
    out[1] = Span {islandCentre + islandWidth * 0.5f, right};
    assert(out[0].left < out[0].right && out[1].left < out[1].right);
    return 2;
}

int Terrain::SpansAt(float y, std::array<Span, 2>& out) const
{
    // Strip whose top edge is at or above y, clamped so index+1 exists.
    int idx = static_cast<int>(std::floor((y - scrollOffset_) / static_cast<float>(cfg::kStripH))) + 1;
    if (idx < 0) { idx = 0; }
    if (idx > cfg::kStripCount - 2) { idx = cfg::kStripCount - 2; }
    const float t = Clamp((y - StripTopY(idx)) / static_cast<float>(cfg::kStripH), 0.0f, 1.0f);

    const Strip& a = strips_[static_cast<size_t>(idx)];
    const Strip& b = strips_[static_cast<size_t>(idx + 1)];
    const float centre = a.centreX      + (b.centreX      - a.centreX)      * t;
    const float width  = a.width        + (b.width        - a.width)        * t;
    const float ic     = a.islandCentre + (b.islandCentre - a.islandCentre) * t;
    const float iw     = a.islandWidth  + (b.islandWidth  - a.islandWidth)  * t;
    const int n = SpansOf(centre, width, ic, iw, out);
    assert(out[0].left >= 0.0f && out[static_cast<size_t>(n - 1)].right <= static_cast<float>(cfg::kScreenW));
    return n;
}

bool Terrain::HitsBank(const Rectangle& r) const
{
    assert(r.width > 0.0f && r.height > 0.0f);
    // Shorelines are piecewise linear, so sampling every few px down the box
    // is enough. The box must sit wholly inside one channel at every sample.
    const float step = 6.0f;
    for (float y = r.y; y <= r.y + r.height + step; y += step) {
        const float sy = (y > r.y + r.height) ? (r.y + r.height) : y;
        std::array<Span, 2> spans {};
        const int n = SpansAt(sy, spans);
        bool inside = false;
        for (int i = 0; i < n; ++i) {
            if (r.x >= spans[static_cast<size_t>(i)].left && r.x + r.width <= spans[static_cast<size_t>(i)].right) { inside = true; }
        }
        if (!inside) { return true; }
        if (sy >= r.y + r.height) { break; }
    }
    return false;
}

// ============================================================================
// Stage palette
// ============================================================================

float Terrain::StageBlend() const
{
    const float t = StageProgress() / cfg::kStageBlend;
    return (t > 1.0f) ? 1.0f : t;
}

Color Terrain::StageColor(Color cfg::Stage::* member) const
{
    const int cur  = StageIndex();
    const int prev = (cur + cfg::kStageCount - 1) % cfg::kStageCount;
    if (distance_ < cfg::kStageLength) { return cfg::kStages[cur].*member; }   // no previous stage on the first lap
    return LerpColor(cfg::kStages[prev].*member, cfg::kStages[cur].*member, StageBlend());
}

float Terrain::StageValue(float cfg::Stage::* member) const
{
    const int cur  = StageIndex();
    const int prev = (cur + cfg::kStageCount - 1) % cfg::kStageCount;
    if (distance_ < cfg::kStageLength) { return cfg::kStages[cur].*member; }
    const float a = cfg::kStages[prev].*member, b = cfg::kStages[cur].*member;
    return a + (b - a) * StageBlend();
}

// ============================================================================
// Drawing
// ============================================================================

void Terrain::Draw(const Sprites& sprites) const
{
    // Land everywhere first (textured, scrolling), then the river cut into it.
    const float scroll = -distance_;
    const Rectangle grassSrc {0.0f, scroll, static_cast<float>(cfg::kScreenW), static_cast<float>(cfg::kScreenH)};
    DrawTexturePro(sprites.Grass(), grassSrc,
                   Rectangle {0.0f, 0.0f, static_cast<float>(cfg::kScreenW), static_cast<float>(cfg::kScreenH)},
                   Vector2 {0.0f, 0.0f}, 0.0f, StageColor(&cfg::Stage::land));
    DrawWater(sprites);
    DrawShallows();
    DrawShore();
    for (int i = 0; i < cfg::kStripCount; ++i) { DrawTrees(i, sprites); }
    DrawCritters();
    DrawObstacles(sprites);
}

void Terrain::DrawWater(const Sprites& sprites) const
{
    // Thin horizontal slices per channel between the interpolated shorelines;
    // the water tile scrolls with the river so it looks like it is flowing.
    const float scroll = -distance_;
    const float h      = static_cast<float>(cfg::kSliceH);
    const Color tint   = StageColor(&cfg::Stage::water);
    for (int y = -cfg::kSliceH; y < cfg::kScreenH; y += cfg::kSliceH) {
        const float fy = static_cast<float>(y);
        std::array<Span, 2> spans {};
        const int n = SpansAt(fy + h * 0.5f, spans);
        for (int i = 0; i < n; ++i) {
            const Span& s = spans[static_cast<size_t>(i)];
            const Rectangle src {s.left, fy + scroll, s.right - s.left, h};
            DrawTexturePro(sprites.Water(), src, Rectangle {s.left, fy, s.right - s.left, h}, Vector2 {0.0f, 0.0f}, 0.0f, tint);
        }
    }
}

void Terrain::DrawShallows() const
{
    // A greenish, slightly darker band of shallow water hugging every shoreline.
    const float h = static_cast<float>(cfg::kSliceH);
    const float w = cfg::kShallowW;
    for (int y = -cfg::kSliceH; y < cfg::kScreenH; y += cfg::kSliceH) {
        const float fy = static_cast<float>(y);
        std::array<Span, 2> spans {};
        const int n = SpansAt(fy + h * 0.5f, spans);
        for (int i = 0; i < n; ++i) {
            const Span& s = spans[static_cast<size_t>(i)];
            const float band = ((s.right - s.left) < 2.0f * w) ? (s.right - s.left) * 0.5f : w;
            DrawRectangleGradientH(static_cast<int>(s.left), static_cast<int>(fy), static_cast<int>(band), cfg::kSliceH,
                                   Fade(kShallow, 0.35f), Fade(kShallow, 0.0f));
            DrawRectangleGradientH(static_cast<int>(s.right - band), static_cast<int>(fy), static_cast<int>(band), cfg::kSliceH,
                                   Fade(kShallow, 0.0f), Fade(kShallow, 0.35f));
        }
    }
}

void Terrain::DrawShoreSegment(Vector2 a, Vector2 b, float landSide, Color sand) const
{
    // Sand on the land side of the line, a thin wet edge on the water side.
    assert(landSide == -1.0f || landSide == 1.0f);
    DrawLineEx(Vector2 {a.x + 2.0f * landSide, a.y}, Vector2 {b.x + 2.0f * landSide, b.y}, 4.0f, sand);
    DrawLineEx(a, b, 1.5f, kSandWet);
}

void Terrain::DrawShore() const
{
    const Color sand = StageColor(&cfg::Stage::sand);
    for (int i = 0; i < cfg::kStripCount - 1; ++i) {
        const float y0 = StripTopY(i);
        const float y1 = StripTopY(i + 1);
        const Strip& a = strips_[static_cast<size_t>(i)];
        const Strip& b = strips_[static_cast<size_t>(i + 1)];
        DrawShoreSegment(Vector2 {a.centreX - a.width * 0.5f, y0}, Vector2 {b.centreX - b.width * 0.5f, y1}, -1.0f, sand);
        DrawShoreSegment(Vector2 {a.centreX + a.width * 0.5f, y0}, Vector2 {b.centreX + b.width * 0.5f, y1},  1.0f, sand);
        if (a.islandWidth > 0.0f || b.islandWidth > 0.0f) {
            // Island shores: land is inward, so the sand sits towards the island centre.
            DrawShoreSegment(Vector2 {a.islandCentre - a.islandWidth * 0.5f, y0}, Vector2 {b.islandCentre - b.islandWidth * 0.5f, y1},  1.0f, sand);
            DrawShoreSegment(Vector2 {a.islandCentre + a.islandWidth * 0.5f, y0}, Vector2 {b.islandCentre + b.islandWidth * 0.5f, y1}, -1.0f, sand);
        }
    }
}

void Terrain::DrawTreeReflection(float tx, float ty, float r, int side) const
{
    assert(side == 0 || side == 1);
    std::array<Span, 2> spans {};
    const int n = SpansAt(ty, spans);
    const float bank = (side == 0) ? spans[0].left : spans[static_cast<size_t>(n - 1)].right;
    const float gap  = (side == 0) ? (bank - tx) : (tx - bank);   // tree centre to shoreline
    if (gap > cfg::kReflectReach) { return; }
    // Mirror across the shoreline; the water side gets a stretched, faint smudge.
    const float rx = (side == 0) ? (bank + gap) : (bank - gap);
    const float fade = 0.28f * (1.0f - gap / cfg::kReflectReach);
    DrawEllipse(static_cast<int>(rx), static_cast<int>(ty + 2.0f), r * 0.9f, r * 1.5f, Fade(kTreeDark, fade));
}

void Terrain::DrawTrees(int index, const Sprites& sprites) const
{
    assert(index >= 0 && index < cfg::kStripCount);
    const Strip& s  = strips_[static_cast<size_t>(index)];
    const float  y0 = StripTopY(index);
    const Color  stageTint = StageColor(&cfg::Stage::tree);
    const float  sx = StageValue(&cfg::Stage::treeScaleX);
    const float  sy = StageValue(&cfg::Stage::treeScaleY);
    const float  chance = cfg::kTreeChance * StageValue(&cfg::Stage::treeChance);

    // Sides: 0 = left bank, 1 = right bank, 2 = island.
    for (int side = 0; side < 3; ++side) {
        for (int k = 0; k < cfg::kTreesPerSide; ++k) {
            const uint32_t salt = static_cast<uint32_t>(side * 16 + k * 4);
            if (Hash01(s.seed, salt) > chance) { continue; }   // empty slot
            const float ty = y0 + Hash01(s.seed, salt + 1) * static_cast<float>(cfg::kStripH);
            const float r  = cfg::kTreeMinR + Hash01(s.seed, salt + 2) * (cfg::kTreeMaxR - cfg::kTreeMinR);

            std::array<Span, 2> spans {};
            const int n = SpansAt(ty, spans);
            float lo = 0.0f, hi = 0.0f;
            if (side == 0)      { lo = r + 4.0f;                         hi = spans[0].left - r - 8.0f; }
            else if (side == 1) { lo = spans[static_cast<size_t>(n - 1)].right + r + 8.0f; hi = static_cast<float>(cfg::kScreenW) - r - 4.0f; }
            else if (n == 2)    { lo = spans[0].right + r + 6.0f;        hi = spans[1].left - r - 6.0f; }
            else                { continue; }                            // no island at this row
            if (hi - lo < r) { continue; }   // land too narrow here

            const float tx = lo + Hash01(s.seed, salt + 3) * (hi - lo);
            // Sprite variant and a slight tint per tree so a bank is not a field of clones.
            const int   variant = (Hash01(s.seed, salt + 4) < 0.5f) ? 0 : 1;
            const float shade   = 0.85f + 0.3f * Hash01(s.seed, salt + 5);
            const float k1      = (shade > 1.0f) ? 1.0f : shade;
            const Color tint { static_cast<unsigned char>(stageTint.r * k1), static_cast<unsigned char>(stageTint.g * k1),
                               static_cast<unsigned char>(stageTint.b * k1 * 0.95f), 255 };
            if (side < 2) { DrawTreeReflection(tx, ty, r, side); }
            DrawEllipse(static_cast<int>(tx) + 4, static_cast<int>(ty) + 5, r * sx, r * 0.8f, Fade(BLACK, 0.28f));   // shadow
            Sprites::DrawInto(sprites.Tree(variant), tx - r * sx, ty - r * sy, r * 2.0f * sx, r * 2.0f * sy, tint);
        }
    }
}

void Terrain::DrawCritters() const
{
    for (const Beast& b : critters_) {
        if (!b.active) { continue; }
        const int x = static_cast<int>(b.pos.x), y = static_cast<int>(b.pos.y);
        switch (b.kind) {
            case Critter::Duck: {
                const float f = (b.vx > 0.0f) ? 1.0f : -1.0f;
                DrawLineEx(Vector2 {b.pos.x - f * 6.0f, b.pos.y + 2.0f}, Vector2 {b.pos.x - f * 22.0f, b.pos.y + 7.0f}, 1.5f, Fade(RAYWHITE, 0.5f));
                DrawLineEx(Vector2 {b.pos.x - f * 6.0f, b.pos.y - 2.0f}, Vector2 {b.pos.x - f * 22.0f, b.pos.y - 7.0f}, 1.5f, Fade(RAYWHITE, 0.5f));
                DrawEllipse(x, y, 9.0f, 6.0f, Color {235, 200, 60, 255});                       // body
                DrawCircleV(Vector2 {b.pos.x + f * 8.0f, b.pos.y - 3.0f}, 4.0f, Color {60, 120, 50, 255});   // head
                DrawTriangle(Vector2 {b.pos.x + f * 11.0f, b.pos.y - 4.0f}, Vector2 {b.pos.x + f * 16.0f, b.pos.y - 2.0f},
                             Vector2 {b.pos.x + f * 11.0f, b.pos.y - 1.0f}, ORANGE);            // beak
                break;
            }
            case Critter::Fish: {
                const float t = b.age / cfg::kFishJumpSecs;               // 0..1
                const float lift = 4.0f * t * (1.0f - t);                 // arc
                const float rot  = -60.0f + 120.0f * t;
                if (t < 0.15f || t > 0.85f) { DrawCircleV(b.pos, 10.0f * (0.5f + lift), Fade(RAYWHITE, 0.5f)); }   // splash in / out
                const Vector2 c {b.pos.x + 20.0f * (t - 0.5f), b.pos.y - 26.0f * lift};
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 9.0f, 4.0f, Color {190, 200, 210, 255});
                DrawTriangle(Vector2 {c.x - 8.0f, c.y}, Vector2 {c.x - 14.0f, c.y - 4.0f + rot * 0.02f}, Vector2 {c.x - 14.0f, c.y + 4.0f}, Color {170, 180, 190, 255});
                break;
            }
            case Critter::Deer: {
                DrawEllipse(x + 3, y + 8, 12.0f, 5.0f, Fade(BLACK, 0.25f));                    // shadow
                for (int leg = -1; leg <= 1; leg += 2) {
                    DrawLineEx(Vector2 {b.pos.x + leg * 7.0f, b.pos.y + 3.0f}, Vector2 {b.pos.x + leg * 8.0f, b.pos.y + 10.0f}, 2.0f, Color {90, 60, 30, 255});
                }
                DrawEllipse(x, y, 12.0f, 7.0f, Color {150, 105, 60, 255});                     // body
                DrawCircleV(Vector2 {b.pos.x + 12.0f, b.pos.y - 5.0f}, 4.5f, Color {140, 95, 50, 255});   // head
                DrawLineEx(Vector2 {b.pos.x + 13.0f, b.pos.y - 8.0f}, Vector2 {b.pos.x + 17.0f, b.pos.y - 15.0f}, 1.5f, Color {90, 60, 30, 255});   // antler
                DrawLineEx(Vector2 {b.pos.x + 11.0f, b.pos.y - 8.0f}, Vector2 {b.pos.x + 9.0f, b.pos.y - 15.0f}, 1.5f, Color {90, 60, 30, 255});
                break;
            }
            case Critter::Otter: {
                const float f = (b.vx > 0.0f) ? 1.0f : -1.0f;
                DrawLineEx(Vector2 {b.pos.x - f * 8.0f, b.pos.y}, Vector2 {b.pos.x - f * 26.0f, b.pos.y - 6.0f}, 1.5f, Fade(RAYWHITE, 0.45f));
                DrawLineEx(Vector2 {b.pos.x - f * 8.0f, b.pos.y}, Vector2 {b.pos.x - f * 26.0f, b.pos.y + 6.0f}, 1.5f, Fade(RAYWHITE, 0.45f));
                DrawEllipse(x, y, 11.0f, 4.5f, Color {80, 50, 30, 255});                       // sleek body
                DrawCircleV(Vector2 {b.pos.x + f * 10.0f, b.pos.y}, 3.5f, Color {110, 75, 45, 255});   // head
                DrawCircleV(Vector2 {b.pos.x + f * 12.0f, b.pos.y - 1.0f}, 1.0f, BLACK);        // eye
                break;
            }
        }
    }
}

void Terrain::DrawBridge(const Obstacle& o) const
{
    assert(o.active && o.kind == Kind::Bridge);
    const int x = static_cast<int>(o.rect.x), y = static_cast<int>(o.rect.y);
    const int w = static_cast<int>(o.rect.width), h = static_cast<int>(o.rect.height);
    DrawRectangle(x, y + 4, w, h, Fade(BLACK, 0.3f));                                   // shadow on the water
    DrawRectangle(x, y, w, h, Color {120, 95, 70, 255});                                 // deck
    for (int px = x + 8; px < x + w - 8; px += 24) { DrawRectangle(px, y + 2, 2, h - 4, Color {90, 70, 50, 255}); }   // planks
    DrawRectangle(x, y, w, 3, Color {70, 55, 40, 255});                                  // railings
    DrawRectangle(x, y + h - 3, w, 3, Color {70, 55, 40, 255});
    // Damage: cracks appear as hits land.
    for (int c = 0; c < cfg::kBridgeHp - o.hp; ++c) {
        const float cx = o.rect.x + o.rect.width * (0.25f + 0.25f * static_cast<float>(c));
        DrawLineEx(Vector2 {cx - 8.0f, o.rect.y + 2.0f}, Vector2 {cx + 6.0f, o.rect.y + o.rect.height - 2.0f}, 2.0f, BLACK);
        DrawLineEx(Vector2 {cx + 6.0f, o.rect.y + o.rect.height - 2.0f}, Vector2 {cx + 12.0f, o.rect.y + 6.0f}, 2.0f, BLACK);
    }
}

void Terrain::DrawObstacles(const Sprites& sprites) const
{
    const Color rockTint = StageColor(&cfg::Stage::rock);
    for (const Obstacle& o : obstacles_) {
        if (!o.active) { continue; }
        const float cx = o.rect.x + o.rect.width * 0.5f;
        const float cy = o.rect.y + o.rect.height * 0.5f;
        switch (o.kind) {
            case Kind::Rock:
                DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), o.rect.width * 0.72f, Fade(RAYWHITE, 0.35f));
                DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 4, o.rect.width * 0.5f, o.rect.height * 0.42f, Fade(BLACK, 0.3f));
                Sprites::DrawInto(sprites.Rock(), o.rect.x, o.rect.y, o.rect.width, o.rect.height, rockTint);
                break;
            case Kind::Boat: {
                const float stern = (o.dir > 0.0f) ? o.rect.x : o.rect.x + o.rect.width;
                const float back  = -o.dir * 26.0f;
                DrawLineEx(Vector2 {stern, cy - 4.0f}, Vector2 {stern + back, cy - 10.0f}, 2.0f, Fade(RAYWHITE, 0.5f));
                DrawLineEx(Vector2 {stern, cy + 4.0f}, Vector2 {stern + back, cy + 10.0f}, 2.0f, Fade(RAYWHITE, 0.5f));
                DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 5, o.rect.width * 0.5f, o.rect.height * 0.5f, Fade(BLACK, 0.3f));
                const Rectangle src {0.0f, 0.0f, static_cast<float>(sprites.Boat().width) * o.dir, static_cast<float>(sprites.Boat().height)};
                DrawTexturePro(sprites.Boat(), src, o.rect, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
                break;
            }
            case Kind::Gun: {
                DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 4, o.rect.width * 0.5f, o.rect.height * 0.4f, Fade(BLACK, 0.3f));
                Sprites::DrawIntoRotated(sprites.Gun(), cx, cy, o.rect.width, o.rect.height, o.aim, WHITE);
                if (o.flash > 0.0f) {
                    const float a = o.aim * kPi / 180.0f;
                    const float r = o.rect.width * 0.55f;
                    DrawCircleV(Vector2 {cx + std::sin(a) * r, cy - std::cos(a) * r}, 6.0f, Fade(YELLOW, 0.9f));
                }
                break;
            }
            case Kind::Sam: {
                DrawEllipse(static_cast<int>(cx) + 3, static_cast<int>(cy) + 5, o.rect.width * 0.55f, o.rect.height * 0.45f, Fade(BLACK, 0.3f));
                Sprites::DrawInto(sprites.Sam(), o.rect.x, o.rect.y, o.rect.width, o.rect.height, WHITE);
                // Spinning radar dish on a mast at the corner of the pad.
                const Vector2 mast {o.rect.x + 7.0f, o.rect.y + 9.0f};
                const float   a = o.aim * kPi / 180.0f;
                DrawCircleV(mast, 3.0f, DARKGRAY);
                DrawLineEx(mast, Vector2 {mast.x + std::cos(a) * 9.0f, mast.y + std::sin(a) * 9.0f}, 3.0f, LIGHTGRAY);
                DrawCircleV(Vector2 {mast.x + std::cos(a) * 9.0f, mast.y + std::sin(a) * 9.0f}, 2.5f, RAYWHITE);
                if (o.flash > 0.0f) { DrawCircleV(Vector2 {cx, o.rect.y}, 10.0f * o.flash / 0.3f + 4.0f, Fade(ORANGE, 0.8f)); }
                break;
            }
            case Kind::Bridge:
                DrawBridge(o);
                break;
            case Kind::Fuel:
                Sprites::DrawInto(sprites.Fuel(), o.rect.x, o.rect.y, o.rect.width, o.rect.height, WHITE);
                DrawFuelLabel(o.rect);
                break;
            case Kind::Star: case Kind::Shield: case Kind::Spread: case Kind::Life: {
                // Bob and glow so pickups read as "good".
                const float bob = 2.0f * std::sin(static_cast<float>(GetTime()) * 4.0f + cx * 0.05f);
                DrawCircleV(Vector2 {cx, cy + bob}, o.rect.width * 0.7f, Fade(RAYWHITE, 0.25f + 0.1f * std::sin(static_cast<float>(GetTime()) * 6.0f)));
                Sprites::DrawInto(sprites.Pickup(o.kind == Kind::Star ? 0 : o.kind == Kind::Shield ? 1 : o.kind == Kind::Spread ? 2 : 3),
                                  o.rect.x, o.rect.y + bob, o.rect.width, o.rect.height, WHITE);
                break;
            }
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

// ============================================================================
// Obstacle queries
// ============================================================================

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

bool Terrain::DamageObstacle(int i)
{
    assert(i >= 0 && i < cfg::kMaxObstacles);
    Obstacle& o = obstacles_[static_cast<size_t>(i)];
    assert(o.active && o.hp > 0);
    --o.hp;
    if (o.hp > 0) { return false; }
    o.active = false;
    return true;
}
