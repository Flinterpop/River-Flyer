#pragma once

#include <array>
#include <cstdint>

#include "raylib.h"

#include "Config.h"
#include "Sprites.h"

// A river that flows down the screen. Stored as a fixed stack of horizontal
// strips (index 0 = top of screen) plus a fixed pool of obstacles. Nothing is
// allocated after construction.
//
// Each strip holds the river centre and width at its TOP edge; between strips
// the banks are interpolated linearly, so both rendering and collision see a
// smooth shoreline rather than 32 px steps.
class Terrain {
public:
    enum class Kind { Rock, Fuel, Boat };

    struct Strip {
        float    centreX;   // river centre line at the top edge, screen space
        float    width;     // river width at the top edge
        uint32_t seed;      // deterministic tree placement for this strip
    };

    struct Obstacle {
        Rectangle rect;
        Kind      kind;
        float     dir;      // boats: -1 or +1, the way it is crossing
        bool      active;
    };

    void Reset();
    void Update(float dt);
    void Draw(const Sprites& sprites) const;

    // True if 'r' touches a river bank.
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
    Strip MakeNextStrip(const Strip& above) const;
    void  ShiftStripsDown();
    void  TrySpawnObstacle(const Strip& strip);
    void  PlaceObstacle(const Strip& strip, Kind kind);
    void  MoveBoat(Obstacle& o, float dt);

    float StripTopY(int index) const;                 // screen y of a strip's top edge
    void  BankAt(float y, float& left, float& right) const;   // interpolated bank x at screen y

    void DrawWater(const Sprites& sprites) const;
    void DrawShore() const;
    void DrawShallows() const;
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
};
