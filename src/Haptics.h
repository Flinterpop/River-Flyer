#pragma once

// Taptic feedback on iPhone and iPad. Every call is a no-op on the other
// platforms and when the device has no haptic engine, so callers never have
// to guard. Cheap enough to call from gameplay code; the system coalesces
// taps that arrive too close together.
namespace haptics {

enum class Kind {
    Light,    // a pickup
    Medium,   // a scrape or a shell
    Heavy,    // a rock, a missile, the hull giving out
};

void Init();               // once, after the window exists
void Play(Kind kind);

} // namespace haptics
