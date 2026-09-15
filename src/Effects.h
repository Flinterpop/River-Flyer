#pragma once

#include <array>

#include "raylib.h"

#include "Config.h"

// Short-lived visual effects: a fixed pool of particle bursts. Each burst is a
// central flash plus kBurstParticles debris dots flying outward and fading.
class Effects {
public:
    enum class Style { Rock, Fuel, Plane, Splash };

    void Reset();
    void Spawn(Vector2 centre, Style style);   // silently dropped if the pool is full
    void Update(float dt);
    void Draw() const;

private:
    struct Burst {
        Vector2 centre;
        float   age;
        Style   style;
        bool    active;
    };

    static Color Tint(Style style);
    static void  DrawBurst(const Burst& b);

    std::array<Burst, cfg::kMaxBursts> pool_ {};
};
