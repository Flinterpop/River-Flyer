// Compile-time tunables for the scroller. Everything here is a fixed bound so
// that no container in the game needs to grow at run time.
#pragma once

namespace cfg {

// Version: keep in lockstep with vcpkg.json and the README badge.
constexpr const char* kVersion = "0.2.0";
constexpr const char* kTitle   = "River Flyer";

constexpr int   kScreenW      = 640;
constexpr int   kScreenH      = 800;
constexpr int   kTargetFps    = 60;

// Rendering: sprites are generated at kSpriteScale x and filtered down;
// water/grass are Perlin tiles of kTileSize px, mirror-tiled and scrolled.
constexpr int   kSpriteScale  = 2;
constexpr int   kTileSize     = 256;
constexpr int   kSliceH       = 4;                                   // px per water slice when drawing the river
constexpr int   kTreesPerSide = 2;                                   // tree slots per strip per bank
constexpr float kTreeChance   = 0.45f;                               // fraction of slots that grow a tree
constexpr float kTreeMinR     = 5.0f;
constexpr float kTreeMaxR     = 12.0f;

// Terrain: the river is stored as a stack of horizontal strips.
constexpr int   kStripH       = 32;                                  // px per strip
constexpr int   kStripCount   = kScreenH / kStripH + 2;              // +2 so the top/bottom edges are always covered
constexpr float kScrollSpeed  = 100.0f;                              // base px per second, downward
constexpr float kRiverMinW    = 180.0f;
constexpr float kRiverMaxW    = 420.0f;
constexpr float kRiverDriftX  = 24.0f;                               // max centre change per strip
constexpr float kRiverDriftW  = 20.0f;                               // max width change per strip
constexpr float kBankMargin   = 40.0f;                               // keep the river this far from the window edge

// Difficulty ramp: scroll speed multiplier grows linearly with distance.
constexpr float kRampDistance = 8000.0f;                             // px of travel per +1.0 multiplier
constexpr float kRampMaxMult  = 2.0f;

// Obstacles live inside the river. Rocks kill; fuel depots refuel.
constexpr int   kMaxObstacles     = 32;
constexpr int   kRockChance       = 20;                              // percent chance per new strip
constexpr int   kFuelChance       = 12;                              // percent chance per new strip (rolled if no rock)
constexpr float kObstacleW        = 28.0f;
constexpr float kObstacleH        = 28.0f;
constexpr int   kBoatChance       = 7;                               // percent per new strip (rolled after rock/fuel)
constexpr float kBoatW            = 44.0f;
constexpr float kBoatH            = 20.0f;
constexpr float kBoatSpeed        = 70.0f;                           // px per second across the river
constexpr int   kPointsPerBoat    = 200;
constexpr float kObstacleInset    = 30.0f;                           // keep this far from the banks (covers inter-strip drift)
constexpr int   kFuelLabelSize    = 12;                              // "FUEL" caption above each depot

// Player craft.
constexpr float kPlayerW      = 32.0f;
constexpr float kPlayerH      = 40.0f;
constexpr float kPlayerSpeedX = 300.0f;                              // px per second
constexpr float kPlayerSpeedY = 220.0f;
constexpr float kPlayerStartY = kScreenH - 120.0f;
constexpr float kPlayerBottomMargin = 40.0f;                         // room below the tail for the flame / chute

// Bullets.
constexpr int   kMaxBullets   = 16;
constexpr float kBulletW      = 4.0f;
constexpr float kBulletH      = 12.0f;
constexpr float kBulletSpeed  = 600.0f;                              // px per second, upward
constexpr float kFireCooldown = 0.15f;                               // seconds between shots

// Fuel and score.
constexpr float kFuelMax          = 100.0f;
constexpr float kFuelBurnPerSec   = 4.0f;
constexpr float kFuelRefillPerSec = 45.0f;
constexpr int   kPointsPerKill    = 100;
constexpr float kPointsPerPx      = 0.1f;                            // distance contribution to score

// Lives: losing one resets the craft and grants a short blinking grace period.
constexpr int   kLives            = 3;
constexpr float kGraceSeconds     = 2.0f;
constexpr float kBlinkHz          = 8.0f;

// Afterburner flame drawn while climbing.
constexpr float kFlameLen     = 26.0f;                               // base length below the tail
constexpr float kFlameFlicker = 10.0f;                               // +/- length variation
constexpr float kFlameHz      = 30.0f;

// Drag chute while braking (down key).
constexpr float kChuteInflate    = 0.2f;                             // seconds to fully open
constexpr float kChuteRadius     = 15.0f;                            // canopy radius when open
constexpr float kChuteLineLen    = 22.0f;                            // shroud line length from the tail
constexpr float kBrakeScrollMult = 0.6f;                             // river speed while braking

// Spiral crash animation.
constexpr float kCrashSeconds = 1.2f;
constexpr float kCrashTurns   = 3.0f;                                // full rotations over the spiral
constexpr float kCrashRadius  = 30.0f;                               // peak spiral radius, px
constexpr float kCrashDrift   = 80.0f;                               // downstream drift over the spiral, px
constexpr float kCrashMinScale = 0.25f;                              // sprite scale at the end
constexpr int   kCrashPuffs    = 6;                                  // smoke puffs trailing the spiral
constexpr float kCrashPuffGap  = 0.04f;                              // progress between puffs
constexpr float kCrashPuffR    = 7.0f;

// Roll crash (rock hit): barrel-roll, smoke, then sink into the water.
constexpr float kRollSeconds   = 1.4f;
constexpr float kRollTurns     = 3.0f;                               // barrel rolls before hitting the water
constexpr float kRollDrift     = 110.0f;                             // downstream drift over the roll, px
constexpr float kRollWobbleDeg = 12.0f;                              // yaw wobble amplitude
constexpr float kRollSinkAt    = 0.75f;                              // progress at which it hits the water
constexpr int   kRollPuffs     = 8;
constexpr float kRollPuffGap   = 0.035f;
constexpr float kRollPuffRise  = 6.0f;                               // px each puff lags behind the tail per step

// Wake foam behind the plane.
constexpr int   kMaxFoam        = 64;
constexpr float kFoamSeconds    = 0.9f;
constexpr float kFoamSpawnGap   = 0.04f;                             // seconds between puff pairs
constexpr float kFoamSpread     = 28.0f;                             // sideways drift, px per second
constexpr float kFoamR          = 4.0f;

// Shore: shallow-water tint width and tree reflections.
constexpr float kShallowW       = 14.0f;
constexpr float kReflectReach   = 34.0f;                             // trees this close to the bank reflect
constexpr int   kTreeVariants   = 2;

// Explosion bursts: fixed pool, fixed particle count per burst.
constexpr int   kMaxBursts       = 8;
constexpr int   kBurstParticles  = 12;
constexpr float kBurstSeconds    = 0.6f;
constexpr float kBurstSpeed      = 140.0f;                           // particle speed, px per second
constexpr float kBurstParticleR  = 4.0f;
constexpr float kBurstFlashR     = 18.0f;

// Audio: the refuelling slurp is synthesised at start-up.
constexpr int   kAudioRate      = 22050;                             // samples per second
constexpr float kSlurpSeconds   = 0.35f;
constexpr float kSlurpHzStart   = 220.0f;
constexpr float kSlurpHzEnd     = 880.0f;
constexpr float kSlurpBubbleHz  = 24.0f;                             // amplitude wobble
constexpr float kSlurpGain      = 0.5f;

// High scores: fixed table persisted as a text file next to the exe.
constexpr int         kHighScoreCount = 10;
constexpr int         kNameMax        = 12;                          // characters, excluding the terminator
constexpr const char* kHighScoreFile  = "highscores.txt";
constexpr int         kNameInputRepeatChars = 8;                     // chars accepted per frame from the queue

// Refuelling animation: droplets along a hose from the pump to the plane.
constexpr int   kFuelDrops      = 6;
constexpr float kFuelDropHz     = 1.6f;                              // trips per second along the hose

// HUD.
constexpr int   kFuelBarW     = 200;
constexpr int   kFuelBarH     = 14;

} // namespace cfg
