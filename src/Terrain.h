#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"
#include "Sprites.h"

// A river that flows down the screen. Stored as a fixed stack of horizontal
// strips (index 0 = top of screen) plus a fixed pool of obstacles. Nothing is
// allocated after construction.
class Terrain {
public:
    enum class Kind { Rock, Fuel };

    struct Strip {
        float centreX;   // river centre line in screen space
        float width;     // river width
    };

    struct Obstacle {
        Rectangle rect;
        Kind      kind;
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

    // Total distance scrolled since Reset, in pixels.
    float Distance() const { return distance_; }

    // Current scroll speed after the difficulty ramp, px per second.
    float ScrollSpeed() const;

private:
    Strip     MakeNextStrip(const Strip& above) const;
    void      ShiftStripsDown();
    void      TrySpawnObstacle(const Strip& strip);
    void      PlaceObstacle(const Strip& strip, Kind kind);
    Rectangle StripRect(int index) const;
    bool      HitsBankStrip(const Rectangle& r, int index) const;

    std::array<Strip, cfg::kStripCount>          strips_ {};
    std::array<Obstacle, cfg::kMaxObstacles>     obstacles_ {};
    float scrollOffset_ {0.0f};   // 0 .. kStripH; sub-strip scroll position
    float distance_     {0.0f};
};
