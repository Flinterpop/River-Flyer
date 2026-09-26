#include "Haptics.h"

// Non-Apple platforms: nothing to buzz. The iOS implementation lives in
// Haptics.mm, which is compiled instead of this file on that platform.
#if !defined(RF_IOS)

namespace haptics {
void Init() {}
void Play(Kind) {}
}   // namespace haptics

#endif
