#pragma once

#include "raylib.h"

#include "Config.h"
#include "Shells.h"
#include "Sprites.h"

// The gunship that shows up at the end of every stage. One at a time: it
// slides in from the top, weaves across the river raking the water with
// shells, and takes bullets until it goes down. The river keeps scrolling
// underneath — it is a fight to survive, not a wall to break through.
class Boss {
public:
    void Reset();                                   // no boss on screen
    void Spawn(int stage);                          // tougher each stage
    bool Active() const { return state_ != State::Gone; }
    bool Fighting() const { return state_ == State::Fighting; }

    // Returns true when the boss left of its own accord (its patience ran
    // out), so the caller can stop drawing a health bar without scoring it.
    bool Update(float dt, const Vector2* targets, int targetCount, Shells& shells);
    void Draw(const Sprites& sprites) const;

    Rectangle Bounds() const;
    int  Hp() const { return hp_; }
    int  MaxHp() const { return maxHp_; }
    Vector2 Centre() const;

    // One bullet hit; true when that was the last of its armour.
    bool Hit();

    // Called once the caller has scored the kill.
    void Clear() { state_ = State::Gone; }

private:
    enum class State { Gone, Entering, Fighting, Dying };

    State   state_ {State::Gone};
    Vector2 pos_   {0.0f, 0.0f};    // top-left
    float   vx_    {0.0f};
    int     hp_    {0};
    int     maxHp_ {0};
    float   fireTimer_ {0.0f};
    float   flash_     {0.0f};      // white flash after a hit
    float   life_      {0.0f};      // seconds on station before it gives up
};
