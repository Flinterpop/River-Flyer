// Compile-time tunables for the scroller. Everything here is a fixed bound so
// that no container in the game needs to grow at run time.
#pragma once

namespace cfg {

constexpr int   kScreenW      = 640;
constexpr int   kScreenH      = 800;
constexpr int   kTargetFps    = 60;

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
constexpr float kObstacleInset    = 8.0f;                            // keep this far from the banks

// Player craft.
constexpr float kPlayerW      = 32.0f;
constexpr float kPlayerH      = 40.0f;
constexpr float kPlayerSpeedX = 300.0f;                              // px per second
constexpr float kPlayerSpeedY = 220.0f;
constexpr float kPlayerStartY = kScreenH - 120.0f;

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

// Spiral crash animation.
constexpr float kCrashSeconds = 1.2f;
constexpr float kCrashTurns   = 3.0f;                                // full rotations over the spiral
constexpr float kCrashRadius  = 30.0f;                               // peak spiral radius, px
constexpr float kCrashDrift   = 80.0f;                               // downstream drift over the spiral, px
constexpr float kCrashMinScale = 0.25f;                              // sprite scale at the end
constexpr int   kCrashPuffs    = 6;                                  // smoke puffs trailing the spiral
constexpr float kCrashPuffGap  = 0.04f;                              // progress between puffs
constexpr float kCrashPuffR    = 7.0f;

// Explosion bursts: fixed pool, fixed particle count per burst.
constexpr int   kMaxBursts       = 8;
constexpr int   kBurstParticles  = 12;
constexpr float kBurstSeconds    = 0.6f;
constexpr float kBurstSpeed      = 140.0f;                           // particle speed, px per second
constexpr float kBurstParticleR  = 4.0f;
constexpr float kBurstFlashR     = 18.0f;

// HUD.
constexpr int   kFuelBarW     = 200;
constexpr int   kFuelBarH     = 14;

} // namespace cfg
