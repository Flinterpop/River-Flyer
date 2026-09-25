#pragma once

#include "Config.h"

// The canvas is always cfg::kScreenW wide, but its height follows the display
// so a tall phone or an iPad fills the glass instead of sitting in a letterbox.
// Chosen once at start-up (Fit), then constant for the run: every fixed-size
// container is sized for cfg::kScreenHMax, so the value only has to be a
// bound, not a compile-time constant.
namespace screen {

// Picks the canvas height for a display of 'displayW' x 'displayH' pixels:
// the display's aspect at kScreenW wide, clamped to [kScreenH, kScreenHMax].
void Fit(int displayW, int displayH);

// Canvas height in canvas pixels. cfg::kScreenH until Fit() says otherwise.
int H();

// Strips needed to cover H(), always <= cfg::kStripCountMax.
int StripCount();

// Bottom edge as a float, the form most callers want.
inline float Bottom() { return static_cast<float>(H()); }

} // namespace screen
