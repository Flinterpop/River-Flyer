#pragma once

#include "Bullets.h"
#include "Effects.h"
#include "Player.h"
#include "Sprites.h"
#include "Terrain.h"

// Top-level game state machine. Owns the player, bullets, terrain, effects
// and sprites. Must be constructed after InitWindow() and destroyed before
// CloseWindow().
class Game {
public:
    enum class State { Playing, Crashing, GameOver };

    Game();
    void Update(float dt);
    void Draw() const;

private:
    void Restart();
    void UpdatePlaying(float dt);
    void UpdateCrashing(float dt);
    void UpdateGameOver(float dt);
    void UpdateFiring(float dt);
    bool UpdateFuel(float dt);            // true when the tank has just run dry
    void ResolveBulletHits();
    void CheckPlayerCrash();
    void BeginCrash();
    void LoseLife();
    bool PlayerVisible() const;
    int  Score() const;

    void DrawHud() const;
    void DrawFuelBar() const;
    void DrawLives() const;
    void DrawGameOver() const;

    Sprites sprites_;
    State   state_ {State::Playing};
    Player  player_;
    Bullets bullets_;
    Terrain terrain_;
    Effects effects_;

    int   lives_        {0};
    int   kills_        {0};
    float fuel_         {0.0f};
    float fireCooldown_ {0.0f};   // seconds until the next shot is allowed
    float grace_        {0.0f};   // seconds of invulnerability left after a respawn
};
