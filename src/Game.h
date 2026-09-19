#pragma once

#include <array>

#include "Audio.h"
#include "Bullets.h"
#include "Effects.h"
#include "HighScores.h"
#include "Missiles.h"
#include "Pilot.h"
#include "Profiles.h"
#include "Shells.h"
#include "Sprites.h"
#include "Terrain.h"

// Top-level game state machine. Owns the pilots, bullets, terrain, effects,
// sprites and audio. Must be constructed after InitWindow() and destroyed
// before CloseWindow().
class Game {
public:
    enum class State { Title, EnterName, Playing, Paused, GameOver };

    Game();
    void Update(float dt);
    void Draw() const;
    bool WantsQuit() const { return quit_; }
    bool Playing() const   { return state_ == State::Playing; }
    void Pause();          // when the host was away (backgrounded, stalled): no effect outside play

private:
    // Title-screen rows.
    enum class Row { Players, PilotOne, PilotTwo, Difficulty, Count };

    // ---- flow ----
    void StartGame();
    void UpdateTitle(float dt);
    void UpdateEnterName(float dt);
    void UpdatePlaying(float dt);
    void UpdatePaused();
    void UpdateGameOver(float dt);
    void FinishGame();
    void BeginNameEntry(int pilotIndex);
    void CommitName();
    void TypeChar(char c);
    void EraseChar();
    void MoveKeyCursor(int dx, int dy);   // on-screen keyboard
    void PressKeyCursor();
    void TouchTitle(Vector2 p);           // a tap on a panel, in canvas coordinates
    void TouchEnterName(Vector2 p);
    void TouchPaused(Vector2 p);

    // ---- per-pilot play ----
    void UpdatePilot(Pilot& p, float dt);
    void UpdateFiring(Pilot& p, float dt);
    void UpdateWake(Pilot& p, float dt);
    bool UpdateFuel(Pilot& p, float dt);          // true when the tank has just run dry
    void CheckPilotCrash(Pilot& p);
    void BeginCrash(Pilot& p, Player::CrashStyle style);
    void FinishCrash(Pilot& p);
    void ResolveBulletHits();
    void Collect(Pilot& p, int obstacle);
    void UpdateCritters(float dt);
    void UpdateGuns(float dt);
    void UpdateSams(float dt);
    void UpdateMissiles(float dt);
    void UpdateCountermeasures(Pilot& p, float dt);
    bool PilotVisible(const Pilot& p) const;
    int  Score() const;
    int  PilotCount() const { return twoPlayer_ ? 2 : 1; }
    const cfg::Difficulty& Diff() const { return cfg::kDifficulties[difficulty_]; }

    // ---- drawing ----
    void DrawWorld() const;
    void DrawDayNight() const;
    void DrawHud() const;
    static void DrawHudText(const char* text, int x, int y, int size);
    void DrawFuelBar(const Pilot& p, int x, int y) const;
    void DrawWarnings(const Pilot& p, int x, int y, bool rightAlign) const;
    void DrawLives(const Pilot& p, int x, int y, bool rightToLeft) const;
    void DrawTitle() const;
    void DrawTitleRow(Row row, int y, const char* label, const char* value) const;
    void DrawPaused() const;
    void DrawGameOver() const;
    void DrawEnterName() const;
    void DrawKeyboard(int x, int y) const;
    void DrawScoreTable(int x, int y) const;
    static void DrawButton(Rectangle r, const char* label, int size);   // tap target on a panel
    void DrawRefuelling(const Pilot& p) const;
    void DrawStageBanner() const;
    void DrawPowerUps(const Pilot& p) const;
    void DrawPeekTable() const;
    Vector2 ShakeOffset() const;

    Audio      audio_;
    Sprites    sprites_;
    State      state_ {State::Title};
    Bullets    bullets_;
    Terrain    terrain_;
    Effects    effects_;
    Shells     shells_;
    Missiles   missiles_;
    HighScores scores_;
    Profiles   profiles_;

    std::array<Pilot, cfg::kMaxPilots> pilots_ {};
    bool twoPlayer_  {false};
    int  difficulty_ {cfg::kDefaultDifficulty};
    std::array<int, cfg::kMaxPilots> profileIdx_ {0, 1};   // title-screen selection per pilot
    Row  row_        {Row::Players};
    float demoX_     {-100.0f};    // title-screen fly-by plane
    bool  quit_      {false};

    int   kills_      {0};
    int   boatKills_  {0};
    int   gunKills_   {0};
    int   samKills_   {0};
    int   missileKills_ {0};
    int   stars_      {0};
    int   bridges_    {0};
    int   otters_     {0};
    int   finalScore_ {0};         // frozen when the last plane is lost
    int   newRow_     {-1};        // row of the entry just added, or -1
    float shake_      {0.0f};      // seconds of screen shake left
    float overT_      {0.0f};      // seconds on the game-over panel: taps count only after a moment

    // Name being typed (title-screen rename); which pilot it is for.
    std::array<char, cfg::kNameMax + 1> name_ {};
    int nameLen_    {0};
    int nameTarget_ {0};
    int keyCol_     {0};           // on-screen keyboard cursor
    int keyRow_     {0};
};
