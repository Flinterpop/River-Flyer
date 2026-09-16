#pragma once

#include <array>
#include <cstdint>

#include "raylib.h"

#include "Config.h"
#include "Shells.h"
#include "Sprites.h"

// A river that flows down the screen. Stored as a fixed stack of horizontal
// strips (index 0 = top of screen) plus a fixed pool of obstacles. Nothing is
// allocated after construction.
//
// Each strip holds the river's outer banks (centre and width) and an optional
// island (centre and width, 0 = none) at its TOP edge; between strips both are
// interpolated linearly, so rendering and collision see smooth shorelines.
// With an island the water is two channels (spans); without, one.
class Terrain {
public:
    enum class Kind { Rock, Fuel, Boat, Gun };

    struct Strip {
        float    centreX;        // river centre line at the top edge, screen space
        float    width;          // outer-bank width at the top edge
        float    islandCentre;   // island centre line (meaningful even when width is 0)
        float    islandWidth;    // 0 = single channel
        uint32_t seed;           // deterministic tree placement for this strip
    };

    struct Span {
        float left;
        float right;
    };

    struct Obstacle {
        Rectangle rect;
        Kind      kind;
        float     dir;      // boats: -1 or +1, the way it is crossing
        float     timer;    // guns: seconds until the next shot
        float     aim;      // guns: barrel angle, degrees clockwise from up
        float     flash;    // guns: muzzle flash time left
        bool      active;
    };

    void Reset();
    void Update(float dt);
    void Draw(const Sprites& sprites) const;

    // Guns track and fire at 'target' (screen space) when in range; returns
    // how many shots were fired this frame so the caller can play a sound.
    int UpdateGuns(float dt, Vector2 target, bool mayFire, Shells& shells);

    // True if 'r' touches a river bank (outer bank or island).
    bool HitsBank(const Rectangle& r) const;

    // Index of the first active obstacle overlapping 'r', or -1.
    int FindObstacle(const Rectangle& r) const;

    static constexpr int ObstacleCapacity() { return cfg::kMaxObstacles; }
    const Obstacle& ObstacleAt(int i) const;
    void            RemoveObstacle(int i);

    // Pixels scrolled by the most recent Update().
    float LastStep() const { return lastStep_; }

    // Total distance scrolled since Reset, in pixels.
    float Distance() const { return distance_; }

    // Current scroll speed after the difficulty ramp, px per second.
    float ScrollSpeed() const;

private:
    enum class Phase { Single, Splitting, Island, Merging };

    Strip MakeNextStrip(const Strip& above);          // advances the island phase machine
    void  AdvancePhase(Strip& next);
    void  ShiftStripsDown();
    void  TrySpawnObstacle(const Strip& strip);
    void  PlaceObstacle(const Strip& strip, Kind kind);
    void  TryPlaceGun(const Strip& strip);
    void  MoveBoat(Obstacle& o, float dt);

    float StripTopY(int index) const;                              // screen y of a strip's top edge
    int   SpansAt(float y, std::array<Span, 2>& out) const;         // water spans at screen y: 1 or 2
    static int SpansOf(float centre, float width, float islandCentre, float islandWidth, std::array<Span, 2>& out);

    void DrawWater(const Sprites& sprites) const;
    void DrawShallows() const;
    void DrawShore() const;
    static void DrawShoreSegment(Vector2 a, Vector2 b, float landSide);
    void DrawTrees(int index, const Sprites& sprites) const;
    void DrawTreeReflection(float tx, float ty, float r, int side) const;
    void DrawObstacles(const Sprites& sprites) const;
    static void DrawFuelLabel(const Rectangle& depot);

    std::array<Strip, cfg::kStripCount>          strips_ {};
    std::array<Obstacle, cfg::kMaxObstacles>     obstacles_ {};
    float    scrollOffset_ {0.0f};   // 0 .. kStripH; sub-strip scroll position
    float    distance_     {0.0f};
    float    lastStep_     {0.0f};
    uint32_t nextSeed_     {0x9E3779B9u};

    Phase phase_        {Phase::Single};
    int   phaseLeft_    {0};         // strips remaining in the current phase
    int   islandGap_    {0};         // single strips still required before the next island
    float islandTarget_ {0.0f};      // full width of the island being built
    int   gunSpacing_   {0};         // strips to wait before another gun may be placed
};
