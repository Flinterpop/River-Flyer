// Compile-time tunables for the scroller. Everything here is a fixed bound so
// that no container in the game needs to grow at run time.
#pragma once

#include "raylib.h"

namespace cfg {

// Version: keep in lockstep with vcpkg.json and the README badge.
constexpr const char* kVersion = "0.7.1";
constexpr const char* kTitle   = "River Flyer";

constexpr int   kScreenW      = 960;
constexpr int   kScreenH      = 1000;                                // design height; the canvas grows to the display (see Screen.h)
constexpr int   kScreenHMax   = 1500;                                // tallest canvas the fixed-size pools are built for
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
constexpr int   kStripCountMax = kScreenHMax / kStripH + 2;          // +2 so the top/bottom edges are always covered
constexpr float kScrollSpeed  = 100.0f;                              // base px per second, downward
constexpr float kRiverMinW    = 200.0f;
constexpr float kRiverMaxW    = 640.0f;
constexpr float kRiverDriftX  = 28.0f;                               // max centre change per strip
constexpr float kRiverDriftW  = 24.0f;                               // max width change per strip
constexpr float kBankMargin   = 40.0f;                               // keep the river this far from the window edge

// Islands: the river splits into two channels around an island and rejoins.
constexpr float kChannelMinW      = 130.0f;                          // each channel stays at least this wide
constexpr float kIslandMinW       = 80.0f;
constexpr float kIslandMaxW       = 180.0f;
constexpr int   kIslandChance     = 8;                               // percent per strip, once the river is wide enough
constexpr int   kSplitStrips      = 6;                               // strips over which the island widens / narrows
constexpr int   kIslandMinStrips  = 8;                               // strips at full width
constexpr int   kIslandMaxStrips  = 18;
constexpr int   kIslandGapStrips  = 12;                              // single-channel strips between islands

// Island guns: turrets that track the plane and lob slow shells.
constexpr int   kGunChance        = 30;                              // percent per island strip wide enough
constexpr int   kGunSpacingStrips = 4;                               // strips between guns on one island
constexpr float kGunMinIsland     = 100.0f;                          // island width needed to hold a gun
constexpr float kGunSize          = 26.0f;
constexpr float kGunReload        = 2.8f;                            // seconds between shots
constexpr float kGunRange         = 520.0f;                          // only fires when the plane is this close
constexpr float kGunFlashSeconds  = 0.12f;
constexpr int   kPointsPerGun     = 300;
constexpr int   kMaxShells        = 16;
constexpr float kShellSpeed       = 130.0f;                          // px per second, slow enough to dodge
constexpr float kShellSeconds     = 5.0f;
constexpr float kShellR           = 4.0f;

// SAM sites: island launchers that fire homing missiles from long range.
constexpr int   kSamChance        = 20;                              // percent per island strip wide enough
constexpr float kSamMinIsland     = 120.0f;
constexpr float kSamSize          = 34.0f;
constexpr float kSamReload        = 6.0f;
constexpr float kSamRange         = 800.0f;                          // longer reach than the guns
constexpr int   kPointsPerSam     = 400;
constexpr int   kPointsPerMissile = 100;
constexpr int   kMaxMissiles      = 4;
constexpr float kMissileSpeed     = 220.0f;                          // px per second
constexpr float kMissileTurnDeg   = 140.0f;                          // degrees per second of steering
constexpr float kMissileSeconds   = 6.0f;
constexpr float kMissileSmokeGap  = 0.05f;
constexpr int   kChaffPerPlane    = 3;
constexpr float kChaffSeconds     = 2.0f;                            // how long the cloud sparkles
constexpr float kJamFuelMult      = 3.0f;                            // fuel burn while jamming
constexpr float kRwrFarGap        = 0.6f;                            // seconds between beeps at max range
constexpr float kRwrNearGap       = 0.1f;

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
constexpr float kPlayerStartUp = 140.0f;                             // start this far above the bottom edge
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

// Health: a plane takes damage instead of dying outright. Reaching zero is
// what loses a life. Scraping a bank or an island grinds the health down
// while contact lasts; solid hits take a chunk at once.
constexpr float kHealthMax          = 100.0f;
constexpr float kBankDamagePerSec   = 32.0f;                         // shore / island scrape
constexpr float kRockDamage         = 45.0f;                         // rock, boat, bridge
constexpr float kShellDamage        = 30.0f;                         // island gun shell
constexpr float kMissileDamage      = 55.0f;
constexpr float kHealthPack         = 45.0f;                         // restored by a health pickup
constexpr int   kHealthChance       = 4;                             // percent per new strip
constexpr float kSmokeHealth        = 55.0f;                         // trail smoke below this
constexpr float kSevereHealth       = 25.0f;                         // thicker, faster trail below this
constexpr float kSmokeGapHealthy    = 0.20f;                         // seconds between puffs at kSmokeHealth
constexpr float kSmokeGapSevere     = 0.07f;                         // ... and at zero
constexpr float kScrapeSparkGap     = 0.05f;                         // seconds between spark bursts while scraping
constexpr float kScrapeHapticGap    = 0.25f;                         // seconds of no contact before the next scrape buzz
constexpr int   kSparksPerBurst     = 7;
constexpr float kSparkSeconds       = 0.45f;
constexpr float kSparkSpeed         = 200.0f;                        // px per second
constexpr float kSparkR             = 3.2f;
constexpr float kHurtFlashSeconds   = 0.25f;                         // HUD bar flash after a hit
constexpr float kBounceX            = 44.0f;                         // sideways shove off a bank, px
constexpr float kBounceY            = 10.0f;                         // and a nudge downstream
constexpr int   kHealthBarW         = 200;
constexpr int   kHealthBarH         = 10;

// Assist: a per-pilot handicap so a younger player can fly alongside an
// older one without changing the world for both (spawn rates, scroll speed
// and guns are shared by definition).
constexpr int   kAssistLives      = 2;                               // extra planes
constexpr float kAssistDamage     = 0.5f;                            // hull damage taken
constexpr float kAssistFuelBurn   = 0.6f;                            // fuel burn rate

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
constexpr int   kMaxFoam        = 96;                                // wake, smoke and sparks share this pool
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

// Pilot profiles shown on the title screen.
constexpr int         kMaxProfiles  = 8;
constexpr const char* kProfilesFile = "profiles.txt";
constexpr int         kMaxPilots    = 2;

// Difficulty presets chosen on the title screen.
struct Difficulty {
    const char* name;
    int         lives;
    float       obstacleScale;   // multiplies rock / boat spawn chances
    bool        guns;            // island guns present
    float       shellSpeed;      // px per second
    float       fuelBurn;        // fuel per second
    float       scrollSpeed;     // base px per second
    float       rampDistance;    // px of travel per +1.0 speed multiplier
};
constexpr int        kDifficultyCount = 3;
constexpr Difficulty kDifficulties[kDifficultyCount] = {
    { "EASY",   5, 0.6f, false,  90.0f, 3.0f,  85.0f, 12000.0f },
    { "NORMAL", 3, 1.0f, true,  130.0f, 4.0f, 100.0f,  8000.0f },
    { "HARD",   3, 1.4f, true,  170.0f, 5.0f, 120.0f,  5000.0f },
};
constexpr int kDefaultDifficulty = 1;

// Screen shake on a crash, and the day/night cycle.
constexpr float kShakeSeconds   = 0.45f;
constexpr float kShakeAmplitude = 7.0f;                              // px
constexpr float kDayLength      = 14000.0f;                          // px of river per full day
constexpr float kNightAlpha     = 0.38f;

// Title screen demo plane.
constexpr float kDemoSpeed      = 160.0f;                            // px per second across the title

// Stages: the scenery shifts every kStageLength px; tints multiply the
// base textures so no extra art is needed. Blends over kStageBlend px.
struct Stage {
    const char* name;
    Color land;
    Color water;
    Color tree;
    float treeScaleX;      // cactus: narrow and tall
    float treeScaleY;
    float treeChance;      // multiplies kTreeChance
    Color sand;
    Color rock;
};
constexpr int   kStageCount  = 4;
constexpr float kStageLength = 6000.0f;
constexpr float kStageBlend  = 600.0f;
constexpr float kStageBanner = 500.0f;                               // px over which the stage name shows
constexpr Stage kStages[kStageCount] = {
    { "FOREST",   {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, 1.0f, 1.0f, 1.0f, {214, 196, 140, 255}, {255, 255, 255, 255} },
    { "FARMLAND", {235, 240, 150, 255}, {235, 245, 255, 255}, {215, 255, 170, 255}, 1.1f, 0.9f, 0.45f, {225, 205, 150, 255}, {255, 255, 255, 255} },
    { "CANYON",   {240, 195, 130, 255}, {170, 235, 225, 255}, {130, 205, 95, 255},  0.5f, 1.7f, 0.55f, {235, 190, 120, 255}, {225, 160, 120, 255} },
    { "SNOW",     {240, 245, 255, 255}, {165, 190, 255, 255}, {175, 205, 215, 255}, 1.0f, 1.2f, 0.7f,  {225, 230, 240, 255}, {235, 245, 255, 255} },
};

// Critters: harmless wildlife.
constexpr int   kMaxCritters    = 14;
constexpr int   kDuckChance     = 6;                                 // percent per new strip
constexpr int   kDeerChance     = 5;
constexpr int   kOtterChance    = 2;
constexpr float kFishInterval   = 3.0f;                              // seconds between jump chances
constexpr float kFishJumpSecs   = 0.8f;
constexpr float kDuckSpeed      = 22.0f;                             // px per second
constexpr float kOtterSpeed     = 40.0f;
constexpr float kOtterSpotRange = 80.0f;                             // fly this close to spot one

// Collectables and power-ups (spawned like obstacles, picked up on contact).
constexpr int   kStarChance     = 6;                                 // percent per new strip
constexpr int   kShieldChance   = 2;
constexpr int   kSpreadChance   = 2;
constexpr int   kLifeChance     = 1;
constexpr float kPickupSize     = 26.0f;
constexpr int   kPointsPerStar  = 50;
constexpr float kShieldSeconds  = 7.0f;
constexpr float kSpreadSeconds  = 10.0f;
constexpr float kSpreadVx       = 110.0f;                            // sideways speed of the outer bullets
constexpr int   kMaxLives       = 9;

// Bridges: span a single channel; shoot them open (kBridgeHp hits) or crash.
constexpr int   kBridgeChance   = 4;                                 // percent per single-channel strip
constexpr int   kBridgeGap      = 10;                                // strips between bridges
constexpr float kBridgeH        = 22.0f;
constexpr int   kBridgeHp       = 3;
constexpr int   kPointsPerBridge = 500;

// Stage boss: a gunship that arrives at the end of each stage, rakes the
// river with shells for a while, and leaves (or goes down) either way.
constexpr float kBossW            = 150.0f;
constexpr float kBossH            = 90.0f;
constexpr int   kBossHp           = 14;                              // bullet hits at stage 0
constexpr int   kBossHpPerStage   = 6;
constexpr float kBossSpeed        = 105.0f;                          // px per second across the river
constexpr float kBossEntrySpeed   = 90.0f;
constexpr float kBossStationY     = 70.0f;                           // where it settles, px from the top
constexpr float kBossMargin       = 10.0f;
constexpr float kBossReload       = 2.2f;                            // seconds between salvos
constexpr float kBossShellSpeed   = 180.0f;
constexpr float kBossSpread       = 0.22f;                           // radians between the three shells
constexpr float kBossSeconds      = 26.0f;                           // patience before it climbs away
constexpr float kBossFlashSeconds = 0.12f;
constexpr int   kPointsPerBoss    = 2000;
constexpr float kBossRamDamage    = 60.0f;                           // flying into it

// Music: an 8-bar chiptune loop synthesised at start-up.
constexpr float kMusicBpm     = 140.0f;
constexpr int   kMusicEighths = 64;                                  // 8 bars of 8
constexpr float kMusicVolume  = 0.35f;

// HUD.
constexpr int   kFuelBarW     = 200;
constexpr int   kFuelBarH     = 14;

} // namespace cfg
