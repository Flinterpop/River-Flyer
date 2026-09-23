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
    void SpawnDamageSmoke(Vector2 pos, float severity);  // dark trail from a damaged hull (0..1)
    void SpawnSparks(Vector2 pos, float awayX); // damage sparks flying off the hull
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

    enum class Puff { Wake, Smoke, Spark, Damage };

    struct Foam {
        Vector2 pos;
        Vector2 vel;
        float   age;
        float   life;    // seconds this puff lives for
        float   shade;   // damage smoke: 0 grey, 1 near-black
        Puff    kind;
        bool    active;
    };

    static Color Tint(Style style);
    static void  DrawBurst(const Burst& b);

    std::array<Burst, cfg::kMaxBursts> pool_ {};
    std::array<Foam, cfg::kMaxFoam>    foam_ {};
};
