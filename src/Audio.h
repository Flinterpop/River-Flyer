#pragma once

#include "raylib.h"

// Sound effects, synthesised at start-up so the repo carries no audio assets.
// RAII around the raylib audio device: constructed once by Game, destroyed
// with it. If no audio device is available every Play* call is a no-op.
class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&)            = delete;
    Audio& operator=(const Audio&) = delete;

    // Refuelling slurp; retriggers itself only once the previous one has finished.
    void PlaySlurp();

private:
    static Sound GenSlurp();

    bool  ready_ {false};
    Sound slurp_ {};
};
