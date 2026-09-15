#include "Audio.h"

#include <cassert>
#include <cmath>
#include <cstdint>

#include "Config.h"

namespace {

constexpr float kTwoPi = 6.28318530718f;

// Scratch buffer for synthesis; the longest effect bounds it.
constexpr int kMaxFrames = static_cast<int>(cfg::kAudioRate * 1.5f);
std::array<float, kMaxFrames> g_scratch {};

int FramesFor(float seconds)
{
    assert(seconds > 0.0f && seconds <= 1.5f);
    const int frames = static_cast<int>(seconds * static_cast<float>(cfg::kAudioRate));
    assert(frames > 0 && frames <= kMaxFrames);
    return frames;
}

float TimeOf(int frame)
{
    return static_cast<float>(frame) / static_cast<float>(cfg::kAudioRate);
}

// Deterministic white noise in [-1, 1] (LCG), so the effects are identical every run.
float Noise()
{
    static uint32_t state = 0x12345678u;
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / static_cast<float>(1u << 24) * 2.0f - 1.0f;
}

// Uploads the first 'frames' of the scratch buffer as a 16-bit mono Sound.
Sound Commit(int frames)
{
    assert(frames > 0 && frames <= kMaxFrames);
    Wave wave {};
    wave.frameCount = static_cast<unsigned int>(frames);
    wave.sampleRate = static_cast<unsigned int>(cfg::kAudioRate);
    wave.sampleSize = 16;
    wave.channels   = 1;
    wave.data       = MemAlloc(static_cast<unsigned int>(frames) * sizeof(int16_t));   // freed by UnloadWave
    assert(wave.data != nullptr);

    int16_t* out = static_cast<int16_t*>(wave.data);
    for (int i = 0; i < frames; ++i) {
        float s = g_scratch[static_cast<size_t>(i)];
        if (s > 1.0f)  { s = 1.0f; }
        if (s < -1.0f) { s = -1.0f; }
        out[i] = static_cast<int16_t>(s * 32767.0f);
    }
    const Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    assert(sound.frameCount > 0);
    return sound;
}

} // namespace

Audio::Audio()
{
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        TraceLog(LOG_WARNING, "AUDIO: no device, sound effects disabled");
        return;
    }
    bank_[static_cast<size_t>(Sfx::Slurp)]  = GenSlurp();
    bank_[static_cast<size_t>(Sfx::Shoot)]  = GenShoot();
    bank_[static_cast<size_t>(Sfx::Pop)]    = GenPop();
    bank_[static_cast<size_t>(Sfx::Crunch)] = GenCrunch();
    bank_[static_cast<size_t>(Sfx::Whine)]  = GenWhine();
    bank_[static_cast<size_t>(Sfx::Splash)] = GenSplash();
    bank_[static_cast<size_t>(Sfx::Brake)]  = GenBrake();
    bank_[static_cast<size_t>(Sfx::Fanfare)] = GenFanfare();
    assert(bank_.front().frameCount > 0 && bank_.back().frameCount > 0);
}

Audio::~Audio()
{
    if (!ready_) { return; }
    for (Sound& s : bank_) { UnloadSound(s); }
    CloseAudioDevice();
}

void Audio::Play(Sfx sfx)
{
    assert(sfx != Sfx::Count);
    if (!ready_) { return; }
    PlaySound(bank_[static_cast<size_t>(sfx)]);
}

void Audio::Sustain(Sfx sfx)
{
    assert(sfx != Sfx::Count);
    if (!ready_) { return; }
    const Sound& s = bank_[static_cast<size_t>(sfx)];
    if (!IsSoundPlaying(s)) { PlaySound(s); }
}

// ---- generators -----------------------------------------------------------

// Slurp: sine sweeping upward, wobbled by a slow amplitude modulation so it
// bubbles, under a linear fade-out.
Sound Audio::GenSlurp()
{
    const int frames = FramesFor(cfg::kSlurpSeconds);
    float phase = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float t    = TimeOf(i);
        const float u    = t / cfg::kSlurpSeconds;
        const float freq = cfg::kSlurpHzStart + (cfg::kSlurpHzEnd - cfg::kSlurpHzStart) * u * u;
        const float wob  = 0.55f + 0.45f * std::sin(kTwoPi * cfg::kSlurpBubbleHz * t);
        phase += kTwoPi * freq / static_cast<float>(cfg::kAudioRate);
        g_scratch[static_cast<size_t>(i)] = std::sin(phase) * (1.0f - u) * wob * cfg::kSlurpGain;
    }
    return Commit(frames);
}

// Shoot: short "pew" - a fast downward sweep with a square-ish edge.
Sound Audio::GenShoot()
{
    const float dur    = 0.09f;
    const int   frames = FramesFor(dur);
    float phase = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float u    = TimeOf(i) / dur;
        const float freq = 1400.0f * std::exp(-3.0f * u) + 200.0f;
        phase += kTwoPi * freq / static_cast<float>(cfg::kAudioRate);
        const float s    = std::sin(phase);
        const float edge = (s >= 0.0f) ? 1.0f : -1.0f;
        g_scratch[static_cast<size_t>(i)] = (0.6f * s + 0.4f * edge) * (1.0f - u) * 0.35f;
    }
    return Commit(frames);
}

// Pop: a burst of noise with a fast exponential decay.
Sound Audio::GenPop()
{
    const float dur    = 0.18f;
    const int   frames = FramesFor(dur);
    for (int i = 0; i < frames; ++i) {
        const float u = TimeOf(i) / dur;
        g_scratch[static_cast<size_t>(i)] = Noise() * std::exp(-6.0f * u) * 0.5f;
    }
    return Commit(frames);
}

// Crunch: low thump plus crackling noise, both decaying.
Sound Audio::GenCrunch()
{
    const float dur    = 0.5f;
    const int   frames = FramesFor(dur);
    float lp = 0.0f;   // one-pole low-pass on the noise so it sounds heavy
    for (int i = 0; i < frames; ++i) {
        const float t = TimeOf(i);
        const float u = t / dur;
        lp += 0.25f * (Noise() - lp);
        const float thump = std::sin(kTwoPi * 70.0f * t) * std::exp(-5.0f * u);
        const float crack = lp * std::exp(-4.0f * u);
        g_scratch[static_cast<size_t>(i)] = (0.6f * thump + 0.7f * crack) * 0.8f;
    }
    return Commit(frames);
}

// Whine: falling-plane siren - long downward sweep with vibrato, fading.
Sound Audio::GenWhine()
{
    const int frames = FramesFor(cfg::kCrashSeconds);
    float phase = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float t    = TimeOf(i);
        const float u    = t / cfg::kCrashSeconds;
        const float freq = 900.0f * (1.0f - u) * (1.0f - u) + 120.0f + 30.0f * std::sin(kTwoPi * 9.0f * t);
        phase += kTwoPi * freq / static_cast<float>(cfg::kAudioRate);
        g_scratch[static_cast<size_t>(i)] = std::sin(phase) * (1.0f - 0.7f * u) * 0.4f;
    }
    return Commit(frames);
}

// Splash: low-passed noise with a quick swell and a longer tail.
Sound Audio::GenSplash()
{
    const float dur    = 0.45f;
    const int   frames = FramesFor(dur);
    float lp = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float u = TimeOf(i) / dur;
        lp += 0.12f * (Noise() - lp);
        const float env = (u < 0.08f) ? (u / 0.08f) : std::exp(-4.0f * (u - 0.08f));
        g_scratch[static_cast<size_t>(i)] = lp * env * 2.2f;
    }
    return Commit(frames);
}

// Fanfare: three rising notes (C5 E5 G5) then a held C6, square-ish and bright.
Sound Audio::GenFanfare()
{
    const float dur    = 1.1f;
    const int   frames = FramesFor(dur);
    const float notes[4]  = { 523.25f, 659.25f, 783.99f, 1046.5f };
    const float starts[4] = { 0.0f, 0.18f, 0.36f, 0.54f };
    float phase = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float t = TimeOf(i);
        int n = 0;
        for (int k = 1; k < 4; ++k) { if (t >= starts[k]) { n = k; } }
        const float since = t - starts[n];
        const float len   = (n == 3) ? (dur - starts[3]) : (starts[n + 1] - starts[n]);
        const float env   = (since < 0.01f) ? (since / 0.01f) : (1.0f - since / len);
        phase += kTwoPi * notes[n] / static_cast<float>(cfg::kAudioRate);
        const float s = std::sin(phase);
        const float edge = (s >= 0.0f) ? 1.0f : -1.0f;
        g_scratch[static_cast<size_t>(i)] = (0.7f * s + 0.3f * edge) * env * 0.35f;
    }
    return Commit(frames);
}

// Brake: steady air-rush hiss, looped by Sustain() while the chute is out.
Sound Audio::GenBrake()
{
    const float dur    = 0.5f;
    const int   frames = FramesFor(dur);
    float lp = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float u = TimeOf(i) / dur;
        lp += 0.35f * (Noise() - lp);
        // Gentle fade at both ends so retriggering does not click.
        float env = 1.0f;
        if (u < 0.05f)      { env = u / 0.05f; }
        else if (u > 0.95f) { env = (1.0f - u) / 0.05f; }
        g_scratch[static_cast<size_t>(i)] = lp * env * 0.5f;
    }
    return Commit(frames);
}
