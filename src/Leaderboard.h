#pragma once

// Game Center: sign the player in once, post scores, and open the standard
// leaderboard sheet. Every call is a no-op on the platforms that have no
// Game Center, and on iOS when the player declined or is not signed in, so
// the game never has to check first.
namespace leaderboard {

// Kicks off authentication. Safe to call once at start-up; the system shows
// its own banner and the player can dismiss it for good.
void Init();

// True once a player is signed in (so a menu entry can be shown).
bool Available();

// Posts a finished run. Ignored when not signed in.
void Post(int score);

// Shows Apple's leaderboard UI over the game.
void Show();

} // namespace leaderboard
