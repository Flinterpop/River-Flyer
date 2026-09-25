#include "Screen.h"

#include <cassert>

namespace screen {
namespace {
int g_height = cfg::kScreenH;
}

void Fit(int displayW, int displayH)
{
    assert(displayW > 0 && displayH > 0);
    // Match the display's shape at our fixed width, but never shorter than the
    // design height (the HUD and panels are laid out for it) and never taller
    // than the strip pool can cover.
    const long long wanted = static_cast<long long>(cfg::kScreenW) * displayH / displayW;
    int h = static_cast<int>(wanted);
    if (h < cfg::kScreenH)    { h = cfg::kScreenH; }
    if (h > cfg::kScreenHMax) { h = cfg::kScreenHMax; }
    // Any height works: StripCount()'s +2 covers the remainder, so the canvas
    // can match the display exactly instead of snapping to whole strips.
    g_height = h;
    assert(g_height >= cfg::kScreenH && g_height <= cfg::kScreenHMax);
    assert(StripCount() <= cfg::kStripCountMax);
}

int H() { return g_height; }

int StripCount() { return g_height / cfg::kStripH + 2; }

} // namespace screen
