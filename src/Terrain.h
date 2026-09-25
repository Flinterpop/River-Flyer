#pragma once

#include <array>
#include <cstdint>

#include "raylib.h"

#include "Config.h"
#include "Missiles.h"
#include "Shells.h"
#include "Sprites.h"

// A river that flows down the screen. Stored as a fixed stack of horizontal
// strips (index 0 = top of screen) plus fixed pools of obstacles (including
// pickups) and critters. Nothing is allocated after construction.
//
// Each strip holds the river's outer banks (centre and width) and an optional
// island (centre and width, 0 = none) at its TOP edge; between strips both are
// interpolated linearly, so rendering and collision see smooth shorelines.
// With an island the water is two channels (spans); without, one.
class Terrain {
public:
    // Solid things, fuel, and pickups all live in one pool so collision is one query.
    enum class Kind { Rock, Fuel, Boat, Gun, Sam, Bridge, Star, Shield, Spread, Life, Health };
    static bool IsPickup(Kind k) { return k == Kind::Star || k == Kind::Shield || k == Kind::Spread || k == Kind::Life || k == Kind::Health; }
    static bool IsSolid(Kind k)  { return k == Kind::Rock || k == Kind::Boat || k == Kind::Gun || k == Kind::Sam || k == Kind::Bridge; }

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
        int       hp;       // bridges: hits left
        bool      active;
    };

    // Per-game tuning from the difficulty preset.
    struct Tuning {
        float obstacleScale;
        bool  guns;
        float shellSpeed;
        float scrollSpeed;
        float rampDistance;
    };

    void Reset(const Tuning& tuning);
    void Update(float dt);
    void Draw(const Sprites& sprites) const;

    // Guns track and fire at the nearest of 'targets' when in range; returns
    // how many shots were fired this frame so the caller can play a sound.
    int UpdateGuns(float dt, const std::array<Vector2, cfg::kMaxPilots>& targets, int targetCount, bool mayFire, Shells& shells);

    // SAM sites: launch at the nearest target in range; returns launches this frame.
    // 'targetPilot' maps each target slot to a pilot index for the missile to chase.
    int UpdateSams(float dt, const std::array<Vector2, cfg::kMaxPilots>& targets, const std::array<int, cfg::kMaxPilots>& targetPilot,
                   int targetCount, bool mayFire, Missiles& missiles);

    // Critters: returns how many otters were spotted this frame (a plane came close).
    int UpdateCritters(float dt, const std::array<Vector2, cfg::kMaxPilots>& planes, int planeCount);

    // True if 'r' touches a river bank (outer bank or island).
    bool HitsBank(const Rectangle& r) const;

    // Which way to shove a box that is touching a bank: -1 (left), +1 (right)
    // or 0 if it is clear. Away from whichever edge it has crossed furthest.
    float BankEscapeX(const Rectangle& r) const;

    // Index of the first active obstacle overlapping 'r', or -1.
    int FindObstacle(const Rectangle& r) const;

    // Only valid for an index FindObstacle() returned: the pool is not iterable.
    const Obstacle& ObstacleAt(int i) const;
    void            RemoveObstacle(int i);
    bool            DamageObstacle(int i);   // one hit; true when it is destroyed

    // Pixels scrolled by the most recent Update().
    float LastStep() const { return lastStep_; }

    // Total distance scrolled since Reset, in pixels.
    float Distance() const { return distance_; }

    // Current scroll speed after the difficulty ramp, px per second.
    float ScrollSpeed() const;

    // Scenery stage: index, and px travelled into it.
    int   StageIndex() const;
    float StageProgress() const;

private:
    enum class Phase { Single, Splitting, Island, Merging };

    enum class Critter { Duck, Fish, Deer, Otter };
    struct Beast {
        Vector2 pos;
        float   vx;
        float   age;
        Critter kind;
        bool    active;
    };

    Strip MakeNextStrip(const Strip& above);          // advances the island phase machine
    void  AdvancePhase(Strip& next);
    void  ShiftStripsDown();
    void  TrySpawnObstacle(const Strip& strip);
    void  TrySpawnPickup(const Strip& strip);
    void  TrySpawnBridge(const Strip& strip);
    void  TrySpawnCritter(const Strip& strip);
    void  PlaceObstacle(const Strip& strip, Kind kind);
    void  TryPlaceGun(const Strip& strip);
    void  TryPlaceSam(const Strip& strip);
    void  MoveBoat(Obstacle& o, float dt);
    void  SpawnCritter(Critter kind, Vector2 pos, float vx);
    void  TryFishJump();

    float StripTopY(int index) const;                              // screen y of a strip's top edge
    int   SpansAt(float y, std::array<Span, 2>& out) const;         // water spans at screen y: 1 or 2
    static int SpansOf(float centre, float width, float islandCentre, float islandWidth, std::array<Span, 2>& out);

    // Stage palette, blended across the boundary.
    float StageBlend() const;                                       // 0 = previous stage .. 1 = current
    Color StageColor(Color cfg::Stage::* member) const;
    float StageValue(float cfg::Stage::* member) const;

    void DrawWater(const Sprites& sprites) const;
    void DrawShallows() const;
    void DrawShore() const;
    void DrawShoreSegment(Vector2 a, Vector2 b, float landSide, Color sand) const;
    void DrawTrees(int index, const Sprites& sprites) const;
    void DrawTreeReflection(float tx, float ty, float r, int side) const;
    void DrawObstacles(const Sprites& sprites) const;
    void DrawBridge(const Obstacle& o) const;
    void DrawCritters() const;
    static void DrawFuelLabel(const Rectangle& depot);

    std::array<Strip, cfg::kStripCountMax>       strips_ {};
    std::array<Obstacle, cfg::kMaxObstacles>     obstacles_ {};
    std::array<Beast, cfg::kMaxCritters>         critters_ {};
    float    scrollOffset_ {0.0f};   // 0 .. kStripH; sub-strip scroll position
    float    distance_     {0.0f};
    float    lastStep_     {0.0f};
    float    fishTimer_    {0.0f};
    uint32_t nextSeed_     {0x9E3779B9u};

    Phase phase_        {Phase::Single};
    int   phaseLeft_    {0};         // strips remaining in the current phase
    int   islandGap_    {0};         // single strips still required before the next island
    float islandTarget_ {0.0f};      // full width of the island being built
    int   gunSpacing_   {0};         // strips to wait before another gun may be placed
    int   bridgeGap_    {0};         // strips to wait before another bridge
    Tuning tuning_      {1.0f, true, cfg::kShellSpeed, cfg::kScrollSpeed, cfg::kRampDistance};
};
