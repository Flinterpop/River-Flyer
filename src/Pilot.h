#pragma once

#include <array>

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
