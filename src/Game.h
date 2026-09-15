#pragma once

#include "Player.h"
#include "Terrain.h"

// Top-level game state machine. Owns the player and the terrain.
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
    void DrawHud() const;

    State   state_ {State::Playing};
    Player  player_;
    Terrain terrain_;
};
