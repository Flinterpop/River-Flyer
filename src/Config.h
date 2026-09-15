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
constexpr float kScrollSpeed  = 120.0f;                              // px per second, downward
constexpr float kRiverMinW    = 180.0f;
constexpr float kRiverMaxW    = 420.0f;
constexpr float kRiverDriftX  = 24.0f;                               // max centre change per strip
constexpr float kRiverDriftW  = 20.0f;                               // max width change per strip
constexpr float kBankMargin   = 40.0f;                               // keep the river this far from the window edge

// Obstacles live inside the river.
constexpr int   kMaxObstacles     = 32;
constexpr int   kObstacleChance   = 25;                              // percent chance per new strip
constexpr float kObstacleW        = 28.0f;
constexpr float kObstacleH        = 28.0f;

// Player craft.
constexpr float kPlayerW      = 32.0f;
constexpr float kPlayerH      = 40.0f;
constexpr float kPlayerSpeedX = 300.0f;                              // px per second
constexpr float kPlayerSpeedY = 220.0f;
constexpr float kPlayerStartY = kScreenH - 120.0f;

} // namespace cfg
