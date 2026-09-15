#pragma once

#include "raylib.h"

// GPU textures for every drawable thing. Generated procedurally at start-up so
// the repo carries no binary assets; swap the Gen* bodies for LoadTexture()
// calls when real art exists. RAII: constructed after InitWindow(), destroyed
// before CloseWindow().
class Sprites {
public:
    Sprites();
    ~Sprites();
    Sprites(const Sprites&)            = delete;
    Sprites& operator=(const Sprites&) = delete;

    const Texture2D& Player() const { return player_; }
    const Texture2D& Rock()   const { return rock_; }
    const Texture2D& Fuel()   const { return fuel_; }
    const Texture2D& Bullet() const { return bullet_; }

private:
    static Texture2D GenPlayer();
    static Texture2D GenRock();
    static Texture2D GenFuel();
    static Texture2D GenBullet();

    Texture2D player_ {};
    Texture2D rock_   {};
    Texture2D fuel_   {};
    Texture2D bullet_ {};
};
