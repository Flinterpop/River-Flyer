// Headless checks of the parts of the game that are pure logic: the river
// generator, the canvas height, the gunship boss, the hull / fuel / assist
// rules, the high-score table and the pilot list. No window is opened, so
// they run anywhere the desktop build runs, including a build server.
//
// The generator's own assertions are half the oracle here, so this target
// keeps NDEBUG undefined in every configuration (see CMakeLists.txt): a
// Release run still traps a bad clamp the way a Debug run would. CHECK is the
// other half and never compiles out.
//
// Records are written next to this executable, in its own folder, so a run
// never disturbs the game's highscores.txt or profiles.txt.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "raylib.h"

#include "Boss.h"
#include "Config.h"
#include "HighScores.h"
#include "Pilot.h"
#include "Profiles.h"
#include "Boss.h"
#include "Screen.h"
#include "Shells.h"
#include "Terrain.h"

namespace {

int checksRun = 0;
int checksFailed = 0;

void Check(bool ok, const char* what, const char* file, int line)
{
    ++checksRun;
    if (ok) { return; }
    ++checksFailed;
    std::printf("FAIL %s:%d  %s\n", file, line, what);
}

#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)

Terrain::Tuning TuningFor(const cfg::Difficulty& d)
{
    return Terrain::Tuning {d.obstacleScale, d.guns, d.shellSpeed, d.scrollSpeed, d.rampDistance};
}

// Where the plane starts a game: the generator must never open on a bank.
Rectangle SpawnBox()
{
    return Rectangle {(static_cast<float>(cfg::kScreenW) - cfg::kPlayerW) * 0.5f, screen::Bottom() - cfg::kPlayerStartUp,
                      cfg::kPlayerW, cfg::kPlayerH};
}

// Sweep the screen with probe rectangles, the way the game asks about the
// plane, and check whatever they find. ObstacleAt() is only defined for an
// index FindObstacle() returned, so this is the supported way to look.
// Returns how many probes hit something.
int CheckObstacles(const Terrain& t)
{
    constexpr float kProbe = 40.0f;   // the plane is 32 x 40, so nothing solid slips between probes
    int hits = 0;
    for (float y = 0.0f; y < screen::Bottom(); y += kProbe) {
        for (float x = 0.0f; x < static_cast<float>(cfg::kScreenW); x += kProbe) {
            const int idx = t.FindObstacle(Rectangle {x, y, kProbe, kProbe});
            if (idx < 0) { continue; }
            ++hits;
            const Terrain::Obstacle& o = t.ObstacleAt(idx);
            CHECK(o.active);
            CHECK(std::isfinite(o.rect.x) && std::isfinite(o.rect.y));
            CHECK(o.rect.width > 0.0f && o.rect.height > 0.0f);
            CHECK(o.rect.x > -200.0f && o.rect.x < static_cast<float>(cfg::kScreenW) + 200.0f);
        }
    }
    CHECK(hits >= 0);
    return hits;
}

// Scroll a fresh river for a long run on one difficulty and seed. The clamp
// assertions inside Terrain fire on a bad strip; these checks cover the rest.
void RunRiver(int difficulty, unsigned int seed)
{
    constexpr int   kSteps = 1500;            // ~25 s of play: enough for several islands and a stage change
    constexpr float kDt    = 1.0f / 60.0f;

    SetRandomSeed(seed);
    Terrain terrain;
    const cfg::Difficulty& d = cfg::kDifficulties[difficulty];
    terrain.Reset(TuningFor(d));

    CHECK(!terrain.HitsBank(SpawnBox()));     // a game must be able to start here
    CHECK(terrain.Distance() == 0.0f);

    float previous = 0.0f;
    for (int i = 0; i < kSteps; ++i) {
        terrain.Update(kDt);
        CHECK(terrain.LastStep() >= 0.0f);
        CHECK(terrain.Distance() >= previous);
        previous = terrain.Distance();
    }

    // The difficulty ramp is bounded at both ends.
    const float speed = terrain.ScrollSpeed();
    CHECK(speed >= d.scrollSpeed - 0.01f);
    CHECK(speed <= d.scrollSpeed * cfg::kRampMaxMult + 0.01f);

    CheckObstacles(terrain);
    CHECK(terrain.StageIndex() >= 0 && terrain.StageIndex() < cfg::kStageCount);
    CHECK(terrain.StageProgress() >= 0.0f && terrain.StageProgress() < cfg::kStageLength);
}

// Every difficulty, several seeds: islands only appear on some of them, so one
// river is not a test. A seed that crosses the island clamp bounds (the v0.6.0
// crash on ARM) trips an assertion inside Terrain rather than a CHECK here.
// Run at the desktop's design height and at the tallest canvas a phone or
// tablet gets, so the strip pool is exercised at its bound.
void TestTerrain()
{
    constexpr int kSeeds = 8;
    constexpr int kHeights[] = {cfg::kScreenH, cfg::kScreenHMax};
    for (const int h : kHeights) {
        screen::Fit(cfg::kScreenW, h);
        CHECK(screen::H() == h);
        CHECK(screen::StripCount() <= cfg::kStripCountMax);
        for (int d = 0; d < cfg::kDifficultyCount; ++d) {
            for (int s = 0; s < kSeeds; ++s) {
                RunRiver(d, static_cast<unsigned int>(1000 + d * kSeeds + s));
            }
        }
    }
    screen::Fit(cfg::kScreenW, cfg::kScreenH);
}

// The canvas height follows the display's shape and stays inside its bounds.
void TestScreenFit()
{
    screen::Fit(1920, 1080);                          // landscape monitor: never shorter than the design
    CHECK(screen::H() == cfg::kScreenH);
    screen::Fit(2048, 2732);                          // iPad Pro 13" (portrait)
    CHECK(screen::H() == 1280);
    screen::Fit(1080, 2400);                          // 20:9 Android phone: clamped
    CHECK(screen::H() == cfg::kScreenHMax);
    screen::Fit(cfg::kScreenW, cfg::kScreenH);
    CHECK(screen::H() == cfg::kScreenH);
}

// Live shells in the pool, counted by killing each one Find() reports.
int DrainShells(Shells& shells)
{
    const Rectangle everywhere {-1000.0f, -1000.0f, 4000.0f, 4000.0f};
    int n = 0;
    for (int i = 0; i < cfg::kMaxShells; ++i) {
        const int idx = shells.Find(everywhere);
        if (idx < 0) { break; }
        shells.Kill(idx);
        ++n;
    }
    return n;
}

// Steps the boss until it is on station. Returns false if it never arrives.
bool BringOnStation(Boss& boss, Shells& shells)
{
    constexpr int   kMaxSteps = 60 * 10;
    constexpr float kDt       = 1.0f / 60.0f;
    for (int i = 0; i < kMaxSteps && !boss.Fighting(); ++i) {
        boss.Update(kDt, nullptr, 0, shells);
    }
    return boss.Fighting();
}

// What TestBoss() below does not cover: the boss holds fire with nobody to
// aim at, a salvo is exactly three shells, and a killed boss stays Active
// (drawn as Dying, ignoring Update) until the caller clears it.
void TestBossSalvo()
{
    constexpr float kDt = 1.0f / 60.0f;
    Shells shells;
    shells.Reset();
    Boss boss;
    boss.Reset();
    boss.Spawn(0);
    CHECK(BringOnStation(boss, shells));

    const int reload = static_cast<int>(cfg::kBossReload / kDt) + 2;
    for (int i = 0; i < reload; ++i) { boss.Update(kDt, nullptr, 0, shells); }
    CHECK(DrainShells(shells) == 0);                 // reloaded, but nobody to shoot

    const Vector2 plane {static_cast<float>(cfg::kScreenW) * 0.5f, 900.0f};
    boss.Update(kDt, &plane, 1, shells);             // the held salvo goes at once
    CHECK(DrainShells(shells) == 3);
    for (int i = 0; i < reload; ++i) { boss.Update(kDt, &plane, 1, shells); }
    CHECK(DrainShells(shells) == 3);                 // and one more per reload

    const int hp = boss.MaxHp();
    for (int i = 1; i < hp; ++i) { CHECK(!boss.Hit()); }
    CHECK(boss.Hit());
    CHECK(boss.Hp() == 0 && boss.Active() && !boss.Fighting());
    CHECK(!boss.Hit());                              // no score for hitting a wreck
    for (int i = 0; i < reload; ++i) { CHECK(!boss.Update(kDt, &plane, 1, shells)); }
    CHECK(DrainShells(shells) == 0);                 // a dying boss does not fire
    boss.Clear();
    CHECK(!boss.Active());
}

// Hull damage, health packs, fuel burn and the assist handicap.
void TestHullAndAssist()
{
    Pilot p;
    p.health = cfg::kHealthMax;
    CHECK(HullDamage(p, 30.0f) == 30.0f);
    CHECK(p.health == cfg::kHealthMax - 30.0f);
    HealHull(p, cfg::kHealthPack);
    CHECK(p.health == cfg::kHealthMax);              // a pack never overfills
    CHECK(HullDamage(p, cfg::kHealthMax * 3.0f) > 0.0f);
    CHECK(p.health == 0.0f);                         // clamps rather than going negative

    p.health = cfg::kHealthMax;
    p.shield = 1.0f;
    CHECK(HullDamage(p, 50.0f) == 0.0f);             // the shield eats it
    CHECK(p.health == cfg::kHealthMax);
    p.shield = 0.0f;

    p.assist = true;
    CHECK(HullDamage(p, 40.0f) == 40.0f * cfg::kAssistDamage);
    CHECK(p.health == cfg::kHealthMax - 40.0f * cfg::kAssistDamage);

    p.assist = false; p.jamming = false;
    CHECK(FuelBurnMult(p) == 1.0f);
    p.jamming = true;
    CHECK(FuelBurnMult(p) == cfg::kJamFuelMult);
    p.assist = true;
    CHECK(FuelBurnMult(p) == cfg::kJamFuelMult * cfg::kAssistFuelBurn);
    p.jamming = false;
    CHECK(FuelBurnMult(p) == cfg::kAssistFuelBurn);
    CHECK(FuelBurnMult(p) < 1.0f);                   // assist really is slower

    for (int d = 0; d < cfg::kDifficultyCount; ++d) {
        const int base = cfg::kDifficulties[d].lives;
        p.assist = false;
        CHECK(StartingLives(p, base) == base);
        p.assist = true;
        CHECK(StartingLives(p, base) == base + cfg::kAssistLives);
    }
}

void TestHighScores()
{
    HighScores table;
    CHECK(table.Count() == 0);
    CHECK(table.Best() == 0);
    CHECK(table.Qualifies(1));                       // anything enters an empty table

    // More entries than the table holds, worst first, so every insert reorders it.
    constexpr int kAdded = cfg::kHighScoreCount + 2;
    for (int i = 0; i < kAdded; ++i) {
        const int row = table.Add(TextFormat("PILOT%d", i), 100 + i * 10);
        CHECK(row >= 0 && row < cfg::kHighScoreCount);
    }
    CHECK(table.Count() == cfg::kHighScoreCount);    // capped, not grown

    for (int i = 1; i < table.Count(); ++i) {
        CHECK(table.At(i - 1).score >= table.At(i).score);
    }
    const int best = 100 + (kAdded - 1) * 10;
    CHECK(table.Best() == best);
    CHECK(table.BestFor(TextFormat("PILOT%d", kAdded - 1)) == best);
    CHECK(table.BestFor("pilot0") == 0);             // pushed out by the higher scores
    CHECK(table.BestFor("NOBODY") == 0);

    const int lowest = table.At(table.Count() - 1).score;
    CHECK(!table.Qualifies(lowest));                 // a tie goes below, so it does not enter
    CHECK(table.Qualifies(lowest + 1));

    // Round trip through storage: Add() saves, so a fresh table loads the same rows.
    HighScores loaded;
    loaded.Load();
    CHECK(loaded.Count() == table.Count());
    for (int i = 0; i < loaded.Count() && i < table.Count(); ++i) {
        CHECK(loaded.At(i).score == table.At(i).score);
        CHECK(std::strcmp(loaded.At(i).name.data(), table.At(i).name.data()) == 0);
    }
}

void TestProfiles()
{
    Profiles list;
    const int start = list.Count();
    CHECK(start >= 0 && start <= cfg::kMaxProfiles);

    // Past capacity: the list caps and the oldest names fall off the end.
    constexpr int kAdded = cfg::kMaxProfiles + 2;
    for (int i = 0; i < kAdded; ++i) {
        const int at = list.Add(TextFormat("ACE%d", i));
        CHECK(at == 0);                              // a new name goes to the front
        CHECK(list.Count() <= cfg::kMaxProfiles);
    }
    CHECK(list.Count() == cfg::kMaxProfiles);

    const char* newest = TextFormat("ACE%d", kAdded - 1);
    CHECK(std::strcmp(list.At(0), newest) == 0);
    CHECK(list.Find(newest) == 0);
    CHECK(list.Find("ace0") == -1);                  // gone, and the search is case-insensitive
    CHECK(list.Find("NOBODY") == -1);

    // An existing name moves to the front instead of being added twice.
    const char* second = list.At(1);
    char kept[cfg::kNameMax + 1] = {0};
    for (int i = 0; i < cfg::kNameMax && second[i] != 0; ++i) { kept[i] = second[i]; }
    const int moved = list.Add(kept);
    CHECK(moved == 0);
    CHECK(list.Count() == cfg::kMaxProfiles);
    CHECK(std::strcmp(list.At(0), kept) == 0);

    list.Save();
    Profiles loaded;
    loaded.Load();
    CHECK(loaded.Count() == list.Count());
    for (int i = 0; i < loaded.Count() && i < list.Count(); ++i) {
        CHECK(std::strcmp(loaded.At(i), list.At(i)) == 0);
    }
}

} // namespace

// The stage boss: armour scales with the stage, it only takes hits once it
// has reached station, it stays on screen while it weaves, it fires, and it
// leaves of its own accord if nobody shoots it down.
void TestBoss()
{
    Shells shells;
    shells.Reset();
    Boss boss;
    boss.Reset();
    CHECK(!boss.Active());

    boss.Spawn(2);
    CHECK(boss.Active());
    CHECK(!boss.Fighting());                       // still on its way in
    CHECK(boss.Hp() == cfg::kBossHp + 2 * cfg::kBossHpPerStage);
    CHECK(!boss.Hit());                            // no free hits during the entry
    CHECK(boss.Hp() == boss.MaxHp());

    const Vector2 target {static_cast<float>(cfg::kScreenW) * 0.5f, screen::Bottom() - 200.0f};
    const float   dt = 1.0f / 60.0f;
    for (int i = 0; i < 600 && !boss.Fighting(); ++i) { boss.Update(dt, &target, 1, shells); }
    CHECK(boss.Fighting());
    CHECK(std::fabs(boss.Bounds().y - cfg::kBossStationY) < 0.01f);

    float minX = 1e9f, maxX = -1e9f;
    for (int i = 0; i < 600; ++i) {
        boss.Update(dt, &target, 1, shells);
        const Rectangle r = boss.Bounds();
        minX = (r.x < minX) ? r.x : minX;
        maxX = (r.x > maxX) ? r.x : maxX;
        CHECK(r.x >= cfg::kBossMargin - 0.01f);
        CHECK(r.x + cfg::kBossW <= static_cast<float>(cfg::kScreenW) - cfg::kBossMargin + 0.01f);
    }
    CHECK(maxX - minX > 100.0f);                   // it really does weave
    CHECK(shells.Find(Rectangle {0.0f, 0.0f, static_cast<float>(cfg::kScreenW), screen::Bottom()}) >= 0);

    int hits = 0;
    const int armour = boss.Hp();
    while (boss.Hp() > 0 && hits < armour + 2) { const bool dead = boss.Hit(); ++hits; if (dead) { break; } }
    CHECK(hits == armour);
    boss.Clear();
    CHECK(!boss.Active());

    boss.Spawn(0);
    bool left = false;
    for (int i = 0; i < 60 * 120 && !left; ++i) { left = boss.Update(dt, &target, 1, shells); }
    CHECK(left);
    CHECK(!boss.Active());
}

int main()
{
    SetTraceLogLevel(LOG_WARNING);   // no raylib banner, but keep storage warnings

    TestScreenFit();
    TestTerrain();
    TestBossSalvo();
    TestHullAndAssist();
    TestHighScores();
    TestProfiles();
    TestBoss();

    std::printf("%s: %d checks, %d failed\n", (checksFailed == 0) ? "PASS" : "FAIL", checksRun, checksFailed);
    return (checksFailed == 0) ? 0 : 1;
}
