#pragma once

#include <array>

#include "raylib.h"

// Sound effects, synthesised at start-up so the repo carries no audio assets.
// RAII around the raylib audio device: constructed once by Game, destroyed
// with it. If no audio device is available every Play call is a no-op.
class Audio {
public:
    enum class Sfx {
        Slurp,    // taking on fuel
        Shoot,    // one bullet
        Pop,      // rock or depot destroyed by a bullet
        Crunch,   // plane hits a rock
        Whine,    // plane spirals into the bank / out of fuel
        Splash,   // plane hits the water
        Brake,    // drag chute hiss
        Fanfare,  // new high score
        Thud,     // island gun firing
        Ding,     // pickup collected
        Count
    };

    Audio();
    ~Audio();
    Audio(const Audio&)            = delete;
    Audio& operator=(const Audio&) = delete;

    void Play(Sfx sfx);       // restart the effect from the beginning
    void Sustain(Sfx sfx);    // keep it going: retrigger only once it has finished

private:
    static constexpr int kCount = static_cast<int>(Sfx::Count);

    static Sound GenSlurp();
    static Sound GenShoot();
    static Sound GenPop();
    static Sound GenCrunch();
    static Sound GenWhine();
    static Sound GenSplash();
    static Sound GenBrake();
    static Sound GenFanfare();
    static Sound GenThud();
    static Sound GenDing();

    bool                       ready_ {false};
    std::array<Sound, kCount>  bank_ {};
};
