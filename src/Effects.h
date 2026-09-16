#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// Short-lived visual effects: a fixed pool of particle bursts. Each burst is a
// central flash plus kBurstParticles debris dots flying outward and fading.
class Effects {
public:
    enum class Style { Rock, Fuel, Plane, Splash, Chaff };

    void Reset();
    void Spawn(Vector2 centre, Style style);   // silently dropped if the pool is full
    void SpawnFoam(Vector2 pos, float vx);     // wake puff with sideways velocity
    void SpawnSmoke(Vector2 pos);              // grey exhaust puff, no drift
    void Update(float dt);
    void Drift(float dy);                      // carry foam downstream with the river
    void Draw() const;                         // bursts (on top of everything)
    void DrawFoam() const;                     // wake (under the plane)

private:
    struct Burst {
        Vector2 centre;
        float   age;
        Style   style;
        bool    active;
    };

    struct Foam {
        Vector2 pos;
        float   vx;
        float   age;
        bool    smoke;   // grey, larger, longer
        bool    active;
    };

    static Color Tint(Style style);
    static void  DrawBurst(const Burst& b);

    std::array<Burst, cfg::kMaxBursts> pool_ {};
    std::array<Foam, cfg::kMaxFoam>    foam_ {};
};
