#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// A river that flows down the screen. Stored as a fixed stack of horizontal
// strips (index 0 = top of screen) plus a fixed pool of obstacles. Nothing is
// allocated after construction.
class Terrain {
public:
    struct Strip {
        float centreX;   // river centre line in screen space
        float width;     // river width
    };

    struct Obstacle {
        Rectangle rect;
        bool      active;
    };

    void Reset();
    void Update(float dt);
    void Draw() const;

    // True if 'r' touches a bank or an obstacle.
    bool Collides(const Rectangle& r) const;

    // Total distance scrolled since Reset, in pixels. Used for scoring.
    float Distance() const { return distance_; }

private:
    Strip     MakeNextStrip(const Strip& above) const;
    void      ShiftStripsDown();
    void      TrySpawnObstacle(const Strip& strip);
    Rectangle StripRect(int index) const;
    bool      HitsBank(const Rectangle& r, int index) const;

    std::array<Strip, cfg::kStripCount>          strips_ {};
    std::array<Obstacle, cfg::kMaxObstacles>     obstacles_ {};
    float scrollOffset_ {0.0f};   // 0 .. kStripH; sub-strip scroll position
    float distance_     {0.0f};
};
