#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// GPU textures for every drawable thing. Generated procedurally at start-up so
// the repo carries no binary assets; swap the Gen* bodies for LoadTexture()
// calls when real art exists. Sprites are generated at kSpriteScale times
// their on-screen size and drawn down with trilinear filtering so edges are
// smooth. RAII: constructed after InitWindow(), destroyed before CloseWindow().
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
    const Texture2D& Water()  const { return water_; }
    const Texture2D& Grass()  const { return grass_; }
    const Texture2D& Tree(int variant) const;
    const Texture2D& Boat()   const { return boat_; }
    const Texture2D& Gun()    const { return gun_; }
    const Texture2D& Sam()    const { return sam_; }
    const Texture2D& Boss()   const { return boss_; }
    const Texture2D& Pickup(int which) const;   // 0 star, 1 shield, 2 spread, 3 life, 4 health

    // Draws the whole texture scaled into the given on-screen box.
    static void DrawInto(const Texture2D& tex, float x, float y, float w, float h, Color tint);

    // Same, but rotated about the box centre (angle in degrees).
    static void DrawIntoRotated(const Texture2D& tex, float cx, float cy, float w, float h, float deg, Color tint);

private:
    static Texture2D GenPlayer();
    static Texture2D GenRock();
    static Texture2D GenFuel();
    static Texture2D GenBullet();
    static Texture2D GenWater();
    static Texture2D GenGrass();
    static Texture2D GenTree(int variant);
    static Texture2D GenBoat();
    static Texture2D GenGun();
    static Texture2D GenSam();
    static Texture2D GenBoss();
    static Texture2D GenPickup(int which);

    Texture2D player_ {};
    Texture2D rock_   {};
    Texture2D fuel_   {};
    Texture2D bullet_ {};
    Texture2D water_  {};
    Texture2D grass_  {};
    Texture2D boat_   {};
    Texture2D gun_    {};
    Texture2D sam_    {};
    Texture2D boss_   {};
    std::array<Texture2D, 5> pickups_ {};
    std::array<Texture2D, cfg::kTreeVariants> trees_ {};
};
