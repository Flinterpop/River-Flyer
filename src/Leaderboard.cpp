#include "Leaderboard.h"

// Everywhere but iOS there is no Game Center; Leaderboard.mm replaces this
// file on that platform.
#if !defined(RF_IOS)

namespace leaderboard {
void Init() {}
bool Available() { return false; }
void Post(int) {}
void Show() {}
}   // namespace leaderboard

#endif
