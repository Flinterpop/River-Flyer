#include "Audio.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "Config.h"

namespace {

constexpr float kTwoPi = 6.28318530718f;

} // namespace

Audio::Audio()
{
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        TraceLog(LOG_WARNING, "AUDIO: no device, sound effects disabled");
        return;
    }
    slurp_ = GenSlurp();
    assert(slurp_.frameCount > 0);
}

Audio::~Audio()
{
    if (ready_) {
        UnloadSound(slurp_);
        CloseAudioDevice();
    }
}

void Audio::PlaySlurp()
{
    if (!ready_) { return; }
    if (IsSoundPlaying(slurp_)) { return; }
    PlaySound(slurp_);
}

// A "slurp": a sine that sweeps upward in pitch, wobbled by a slow amplitude
// modulation so it bubbles, under a linear fade-out envelope.
Sound Audio::GenSlurp()
{
    assert(IsAudioDeviceReady());
    const int frames = static_cast<int>(cfg::kSlurpSeconds * static_cast<float>(cfg::kAudioRate));
    assert(frames > 0 && frames < cfg::kAudioRate * 5);

    Wave wave {};
    wave.frameCount = static_cast<unsigned int>(frames);
    wave.sampleRate = static_cast<unsigned int>(cfg::kAudioRate);
    wave.sampleSize = 16;
    wave.channels   = 1;
    wave.data       = MemAlloc(static_cast<unsigned int>(frames) * sizeof(int16_t));   // freed by UnloadWave
    assert(wave.data != nullptr);

    int16_t* out   = static_cast<int16_t*>(wave.data);
    float    phase = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float t    = static_cast<float>(i) / static_cast<float>(cfg::kAudioRate);
        const float u    = t / cfg::kSlurpSeconds;                      // 0 .. 1 through the sound
        const float freq = cfg::kSlurpHzStart + (cfg::kSlurpHzEnd - cfg::kSlurpHzStart) * u * u;
        const float env  = 1.0f - u;
        const float wob  = 0.55f + 0.45f * std::sin(kTwoPi * cfg::kSlurpBubbleHz * t);
        phase += kTwoPi * freq / static_cast<float>(cfg::kAudioRate);
        const float s = std::sin(phase) * env * wob * cfg::kSlurpGain;
        assert(s >= -1.0f && s <= 1.0f);
        out[i] = static_cast<int16_t>(s * 32767.0f);
    }

    const Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}
