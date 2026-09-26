#include "Haptics.h"

#if defined(RF_IOS)

#import <UIKit/UIKit.h>

// UIFeedbackGenerator must be used from the main thread, which is where the
// game loop runs. The generators are created once and kept for the life of
// the app (no ARC in this target, so they are simply never released).
namespace {
UIImpactFeedbackGenerator* g_light  = nil;
UIImpactFeedbackGenerator* g_medium = nil;
UIImpactFeedbackGenerator* g_heavy  = nil;
bool g_ready = false;
}

namespace haptics {

void Init()
{
    if (g_ready) { return; }
    // iPads have no Taptic Engine: the generators exist but do nothing, which
    // is the behaviour we want anyway.
    g_light  = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
    g_medium = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
    g_heavy  = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
    [g_light prepare];
    [g_medium prepare];
    [g_heavy prepare];
    g_ready = true;
}

void Play(Kind kind)
{
    if (!g_ready) { return; }
    UIImpactFeedbackGenerator* gen = (kind == Kind::Light)  ? g_light
                                   : (kind == Kind::Medium) ? g_medium
                                                            : g_heavy;
    [gen impactOccurred];
    [gen prepare];   // keep the engine warm for the next hit
}

} // namespace haptics

#endif
