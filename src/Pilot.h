#pragma once

#include <array>
#include <cassert>

#include "Config.h"
#include "Input.h"
#include "Player.h"

// Everything that belongs to one player in a game: their plane, controls,
// planes left, fuel, timers and name. Plain data; Game drives it.
struct Pilot {
    Player   plane;
    InputMap map {};
    std::array<char, cfg::kNameMax + 1> name {};

    int   lives        {0};
    float health       {0.0f};    // hull integrity, 0 .. kHealthMax
    float fuel         {0.0f};
    float grace        {0.0f};    // seconds of invulnerability left after a respawn
    float fireCooldown {0.0f};
    float foamTimer    {0.0f};
    int   refuelPump   {-1};      // obstacle index being drawn from, or -1
    bool  out          {false};   // no planes left
    bool  assist       {false};   // easier ride for a younger pilot
    float shield       {0.0f};    // seconds of shield bubble left
    float spread       {0.0f};    // seconds of three-way fire left
    int   chaff        {0};       // chaff bundles left
    bool  jamming      {false};   // jam key held this frame
    float rwrTimer     {0.0f};    // seconds until the next warning beep
    float scrapeTimer  {0.0f};    // seconds until the next spark burst while scraping
    float smokeTimer   {0.0f};    // seconds until the next damage-smoke puff
    float hurtFlash    {0.0f};    // seconds of red HUD flash after taking a hit

    bool Active() const { return !out; }
    bool Flying() const { return !out && !plane.Crashing(); }
};

// The hull and fuel rules, kept free of Game's sound, haptics and effects so
// the headless tests can check them.

// Takes 'amount' off the hull after the shield and assist have had their say.
// Returns what the hull actually lost: 0 when the shield ate the hit.
inline float HullDamage(Pilot& p, float amount)
{
    assert(amount > 0.0f);
    if (p.shield > 0.0f) { return 0.0f; }
    const float taken = p.assist ? amount * cfg::kAssistDamage : amount;
    p.health -= taken;
    if (p.health < 0.0f) { p.health = 0.0f; }
    assert(p.health >= 0.0f && p.health <= cfg::kHealthMax);
    return taken;
}

// A health pack: restores 'amount', never past a full hull.
inline void HealHull(Pilot& p, float amount)
{
    assert(amount > 0.0f);
    p.health += amount;
    if (p.health > cfg::kHealthMax) { p.health = cfg::kHealthMax; }
    assert(p.health > 0.0f && p.health <= cfg::kHealthMax);
}

// How much faster (or slower) than the difficulty's rate this pilot burns fuel.
inline float FuelBurnMult(const Pilot& p)
{
    const float jam    = p.jamming ? cfg::kJamFuelMult : 1.0f;
    const float assist = p.assist ? cfg::kAssistFuelBurn : 1.0f;
    assert(jam > 0.0f && assist > 0.0f);
    return jam * assist;
}

// Planes a pilot starts a game with.
inline int StartingLives(const Pilot& p, int difficultyLives)
{
    assert(difficultyLives > 0);
    return difficultyLives + (p.assist ? cfg::kAssistLives : 0);
}
