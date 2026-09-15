#pragma once

#include "Bullets.h"
#include "Player.h"
#include "Sprites.h"
#include "Terrain.h"

// Top-level game state machine. Owns the player, bullets, terrain and sprites.
// Must be constructed after InitWindow() and destroyed before CloseWindow().
class Game {
public:
    enum class State { Playing, GameOver };

    Game();
    void Update(float dt);
    void Draw() const;

private:
    void Restart();
    void UpdatePlaying(float dt);
    void UpdateGameOver();
    void UpdateFiring(float dt);
    void UpdateFuel(float dt);
    void ResolveBulletHits();
    void ResolvePlayerHits();
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

    int   lives_        {0};
    int   kills_        {0};
    float fuel_         {0.0f};
    float fireCooldown_ {0.0f};   // seconds until the next shot is allowed
    float grace_        {0.0f};   // seconds of invulnerability left after a respawn
};
