#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"
#include "Effects.h"

// Fixed pool of homing missiles fired by SAM sites. Each chases one pilot
// (or a chaff cloud once decoyed) with a limited turn rate, so a hard
// sidestep at close range makes it overshoot.
class Missiles {
public:
    struct Target {
        Vector2 pos;
        bool    jamming;   // pilot is jamming: the missile flies straight
        bool    alive;     // pilot is flying (otherwise the missile goes ballistic)
    };

    void Reset();
    void Launch(Vector2 from, Vector2 towards, int pilot);   // dropped silently if the pool is full
    void Update(float dt, const std::array<Target, cfg::kMaxPilots>& targets, float riverStep, Effects& effects);
    void Draw() const;

    // Decoy: every missile chasing 'pilot' switches to the chaff point.
    void Decoy(int pilot, Vector2 chaff);

    // Index of the first live missile whose nose touches 'r', or -1.
    int     Find(const Rectangle& r) const;
    void    Kill(int i);
    Vector2 Position(int i) const;

    // Distance from 'pos' to the nearest live missile chasing 'pilot', or -1 if none.
    float NearestTo(int pilot, Vector2 pos) const;

private:
    struct Missile {
        Vector2 pos;
        Vector2 dir;      // unit heading
        Vector2 chaff;    // decoy point when onChaff
        int     pilot;
        float   age;
        float   smoke;    // seconds until the next smoke puff
        bool    onChaff;
        bool    active;
    };
    std::array<Missile, cfg::kMaxMissiles> pool_ {};
};
