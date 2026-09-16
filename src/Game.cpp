#include "Game.h"

#include <cassert>
#include <cmath>

#include "Config.h"

namespace {

constexpr float kPi = 3.14159265f;

Vector2 RectCentre(const Rectangle& r)
{
    assert(r.width > 0.0f && r.height > 0.0f);
    return Vector2 {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

Terrain::Tuning TuningFor(const cfg::Difficulty& d)
{
    return Terrain::Tuning {d.obstacleScale, d.guns, d.shellSpeed, d.scrollSpeed, d.rampDistance};
}

// Bounded copy into a NUL-terminated name buffer.
void SetName(std::array<char, cfg::kNameMax + 1>& dst, const char* src)
{
    assert(src != nullptr);
    int n = 0;
    for (; src[n] != '\0' && n < cfg::kNameMax; ++n) { dst[static_cast<size_t>(n)] = src[n]; }
    dst[static_cast<size_t>(n)] = '\0';
}

} // namespace

// ============================================================================
// Construction and flow
// ============================================================================

Game::Game()
{
    scores_.Load();
    profiles_.Load();
    for (Pilot& p : pilots_) { p.out = true; }
    profileIdx_[1] = (profiles_.Count() > 1) ? 1 : 0;
    terrain_.Reset(TuningFor(cfg::kDifficulties[cfg::kDefaultDifficulty]));   // scrolling title backdrop
    state_ = State::Title;
    assert(profiles_.Count() > 0);
}

void Game::StartGame()
{
    bullets_.Reset();
    effects_.Reset();
    shells_.Reset();
    terrain_.Reset(TuningFor(Diff()));
    kills_ = 0; boatKills_ = 0; gunKills_ = 0; stars_ = 0; bridges_ = 0; otters_ = 0;
    finalScore_ = 0; newRow_ = -1; shake_ = 0.0f;

    for (int i = 0; i < cfg::kMaxPilots; ++i) {
        Pilot& p = pilots_[static_cast<size_t>(i)];
        p.out = (i >= PilotCount());
        if (p.out) { continue; }
        p.map = (i == 0) ? input::PilotOne(!twoPlayer_) : input::PilotTwo();
        SetName(p.name, profiles_.At(profileIdx_[static_cast<size_t>(i)]));
        p.plane.Reset(twoPlayer_ ? ((i == 0) ? -50.0f : 50.0f) : 0.0f);
        p.lives = Diff().lives;
        p.fuel  = cfg::kFuelMax;
        p.grace = 0.0f; p.fireCooldown = 0.0f; p.foamTimer = 0.0f; p.refuelPump = -1;
        p.shield = 0.0f; p.spread = 0.0f;
        assert(!terrain_.HitsBank(p.plane.Bounds()));   // must spawn in open water
    }
    state_ = State::Playing;
}

void Game::Update(float dt)
{
    assert(dt >= 0.0f);
    switch (state_) {
        case State::Title:     UpdateTitle(dt);     break;
        case State::EnterName: UpdateEnterName(dt); break;
        case State::Playing:   UpdatePlaying(dt);   break;
        case State::Paused:    UpdatePaused();      break;
        case State::GameOver:  UpdateGameOver(dt);  break;
    }
}

// ---- title --------------------------------------------------------------

void Game::UpdateTitle(float dt)
{
    assert(state_ == State::Title);
    terrain_.Update(dt);
    effects_.Drift(terrain_.LastStep());
    effects_.Update(dt);
    UpdateCritters(dt);
    demoX_ += cfg::kDemoSpeed * dt;
    if (demoX_ > static_cast<float>(cfg::kScreenW) + 100.0f) { demoX_ = -100.0f; }

    // Row navigation (pilot two only exists in a two-player game).
    if (input::MenuDown()) {
        row_ = static_cast<Row>((static_cast<int>(row_) + 1) % static_cast<int>(Row::Count));
        if (row_ == Row::PilotTwo && !twoPlayer_) { row_ = Row::Difficulty; }
    }
    if (input::MenuUp()) {
        row_ = static_cast<Row>((static_cast<int>(row_) + static_cast<int>(Row::Count) - 1) % static_cast<int>(Row::Count));
        if (row_ == Row::PilotTwo && !twoPlayer_) { row_ = Row::PilotOne; }
    }
    const int step = input::MenuRight() ? 1 : (input::MenuLeft() ? -1 : 0);
    if (step != 0) {
        const int n = profiles_.Count();
        switch (row_) {
            case Row::Players:    twoPlayer_ = !twoPlayer_; break;
            case Row::PilotOne:   profileIdx_[0] = (profileIdx_[0] + step + n) % n; break;
            case Row::PilotTwo:   profileIdx_[1] = (profileIdx_[1] + step + n) % n; break;
            case Row::Difficulty: difficulty_ = (difficulty_ + step + cfg::kDifficultyCount) % cfg::kDifficultyCount; break;
            case Row::Count:      break;
        }
    }
    if (IsKeyPressed(KEY_Q)) { quit_ = true; return; }
    if (IsKeyPressed(KEY_SPACE)) { StartGame(); return; }
    if (input::MenuConfirm()) {
        if (row_ == Row::PilotOne)      { BeginNameEntry(0); }
        else if (row_ == Row::PilotTwo) { BeginNameEntry(1); }
        else                            { StartGame(); }
    }
    assert(difficulty_ >= 0 && difficulty_ < cfg::kDifficultyCount);
}

void Game::BeginNameEntry(int pilotIndex)
{
    assert(pilotIndex >= 0 && pilotIndex < cfg::kMaxPilots);
    nameTarget_ = pilotIndex;
    SetName(name_, profiles_.At(profileIdx_[static_cast<size_t>(pilotIndex)]));
    nameLen_ = 0;
    while (name_[static_cast<size_t>(nameLen_)] != '\0') { ++nameLen_; }
    state_ = State::EnterName;
    assert(nameLen_ >= 0 && nameLen_ <= cfg::kNameMax);
}

void Game::UpdateEnterName(float dt)
{
    assert(state_ == State::EnterName);
    terrain_.Update(dt);
    effects_.Update(dt);

    // Typed characters: printable ASCII only, bounded per frame and in length.
    for (int i = 0; i < cfg::kNameInputRepeatChars; ++i) {
        const int c = GetCharPressed();
        if (c == 0) { break; }
        if (c >= 32 && c <= 126 && nameLen_ < cfg::kNameMax) {
            name_[static_cast<size_t>(nameLen_)] = static_cast<char>(c);
            ++nameLen_;
            name_[static_cast<size_t>(nameLen_)] = '\0';
        }
    }
    // Backspaces: drain the key queue so several in one frame all count,
    // plus key-repeat while it is held.
    int erase = IsKeyPressedRepeat(KEY_BACKSPACE) ? 1 : 0;
    for (int i = 0; i < cfg::kNameInputRepeatChars; ++i) {
        const int k = GetKeyPressed();
        if (k == 0) { break; }
        if (k == KEY_BACKSPACE) { ++erase; }
    }
    for (; erase > 0 && nameLen_ > 0; --erase) {
        --nameLen_;
        name_[static_cast<size_t>(nameLen_)] = static_cast<char>(0);
    }
    if (IsKeyPressed(KEY_ESCAPE)) { state_ = State::Title; return; }
    if (IsKeyPressed(KEY_ENTER) && nameLen_ > 0) { CommitName(); }
    assert(nameLen_ >= 0 && nameLen_ <= cfg::kNameMax);
}

void Game::CommitName()
{
    assert(state_ == State::EnterName && nameLen_ > 0);
    while (nameLen_ > 0 && name_[static_cast<size_t>(nameLen_ - 1)] == ' ') {   // no blank names
        --nameLen_;
        name_[static_cast<size_t>(nameLen_)] = '\0';
    }
    if (nameLen_ == 0) { return; }

    // Remember the other pilot's choice by name; Add() reorders the list.
    const int other = 1 - nameTarget_;
    std::array<char, cfg::kNameMax + 1> otherName {};
    SetName(otherName, profiles_.At(profileIdx_[static_cast<size_t>(other)]));

    profileIdx_[static_cast<size_t>(nameTarget_)] = profiles_.Add(name_.data());
    const int found = profiles_.Find(otherName.data());
    profileIdx_[static_cast<size_t>(other)] = (found >= 0) ? found : 0;
    state_ = State::Title;
    assert(profileIdx_[0] < profiles_.Count() && profileIdx_[1] < profiles_.Count());
}

// ---- playing ------------------------------------------------------------

void Game::UpdatePlaying(float dt)
{
    assert(state_ == State::Playing);
    if (input::PausePressed()) { state_ = State::Paused; return; }

    // Any pilot with the chute out slows the river for everyone.
    bool braking = false;
    for (const Pilot& p : pilots_) { if (p.Flying() && p.plane.Braking()) { braking = true; } }
    terrain_.Update(braking ? dt * cfg::kBrakeScrollMult : dt);
    if (braking) { audio_.Sustain(Audio::Sfx::Brake); }

    bullets_.Update(dt);
    effects_.Drift(terrain_.LastStep());
    effects_.Update(dt);
    shells_.Update(dt);
    UpdateGuns(dt);
    UpdateCritters(dt);
    ResolveBulletHits();
    for (Pilot& p : pilots_) { UpdatePilot(p, dt); }
    if (shake_ > 0.0f) { shake_ -= dt; }

    bool anyone = false;
    for (const Pilot& p : pilots_) { if (p.Active()) { anyone = true; } }
    if (!anyone) { FinishGame(); }
}

void Game::UpdatePilot(Pilot& p, float dt)
{
    assert(dt >= 0.0f);
    if (p.out) { return; }
    if (p.plane.Crashing()) {
        p.plane.UpdateCrash(dt);
        if (p.plane.CrashFinished()) { FinishCrash(p); }
        return;
    }
    p.plane.Update(dt, p.map);
    if (p.shield > 0.0f) { p.shield -= dt; }
    if (p.spread > 0.0f) { p.spread -= dt; }
    UpdateWake(p, dt);
    UpdateFiring(p, dt);
    if (p.grace > 0.0f) {
        p.grace -= dt;
    } else {
        CheckPilotCrash(p);
    }
    if (p.Flying() && UpdateFuel(p, dt)) {
        audio_.Play(Audio::Sfx::Whine);
        BeginCrash(p, Player::CrashStyle::Spiral);   // out of fuel: glide down in a spiral
    }
}

void Game::UpdateWake(Pilot& p, float dt)
{
    assert(dt >= 0.0f && p.Flying());
    p.foamTimer -= dt;
    if (p.foamTimer > 0.0f) { return; }
    p.foamTimer = cfg::kFoamSpawnGap;
    const Vector2 c     = p.plane.Centre();
    const float   tailY = c.y + cfg::kPlayerH * 0.45f;
    effects_.SpawnFoam(Vector2 {c.x - 5.0f, tailY}, -cfg::kFoamSpread);
    effects_.SpawnFoam(Vector2 {c.x + 5.0f, tailY},  cfg::kFoamSpread);
}

void Game::UpdateFiring(Pilot& p, float dt)
{
    assert(dt >= 0.0f && p.Flying());
    p.fireCooldown -= dt;
    if (p.fireCooldown > 0.0f) { return; }
    if (input::FireHeld(p.map)) {
        bullets_.Fire(p.plane.Muzzle());
        if (p.spread > 0.0f) {
            bullets_.Fire(p.plane.Muzzle(), -cfg::kSpreadVx);
            bullets_.Fire(p.plane.Muzzle(),  cfg::kSpreadVx);
        }
        audio_.Play(Audio::Sfx::Shoot);
        p.fireCooldown = cfg::kFireCooldown;
    }
}

bool Game::UpdateFuel(Pilot& p, float dt)
{
    assert(dt >= 0.0f && p.Flying());
    p.fuel -= Diff().fuelBurn * dt;

    // Flying over a depot refuels without destroying it.
    p.refuelPump = -1;
    const int hit = terrain_.FindObstacle(p.plane.Bounds());
    if (hit >= 0 && terrain_.ObstacleAt(hit).kind == Terrain::Kind::Fuel) {
        if (p.fuel < cfg::kFuelMax) {
            audio_.Sustain(Audio::Sfx::Slurp);
            p.refuelPump = hit;
        }
        p.fuel += cfg::kFuelRefillPerSec * dt;
    }
    if (p.fuel > cfg::kFuelMax) { p.fuel = cfg::kFuelMax; }
    if (p.fuel < 0.0f)          { p.fuel = 0.0f; }
    assert(p.fuel >= 0.0f && p.fuel <= cfg::kFuelMax);
    return p.fuel <= 0.0f;
}

void Game::UpdateGuns(float dt)
{
    std::array<Vector2, cfg::kMaxPilots> targets {};
    int  n = 0;
    bool mayFire = false;
    for (const Pilot& p : pilots_) {
        if (!p.Flying()) { continue; }
        targets[static_cast<size_t>(n)] = p.plane.Centre();
        ++n;
        if (p.grace <= 0.0f) { mayFire = true; }
    }
    if (terrain_.UpdateGuns(dt, targets, n, mayFire, shells_) > 0) { audio_.Play(Audio::Sfx::Thud); }
}

void Game::UpdateCritters(float dt)
{
    std::array<Vector2, cfg::kMaxPilots> planes {};
    int n = 0;
    for (const Pilot& p : pilots_) {
        if (p.Flying()) { planes[static_cast<size_t>(n)] = p.plane.Centre(); ++n; }
    }
    const int spotted = terrain_.UpdateCritters(dt, planes, n);
    if (spotted > 0 && state_ == State::Playing) {
        otters_ += spotted;
        audio_.Play(Audio::Sfx::Ding);
    }
}

void Game::ResolveBulletHits()
{
    // Bounded: kMaxBullets x kMaxObstacles checks at most.
    for (int b = 0; b < Bullets::Capacity(); ++b) {
        if (!bullets_.Active(b)) { continue; }
        const int hit = terrain_.FindObstacle(bullets_.Bounds(b));
        if (hit < 0) { continue; }
        const Terrain::Obstacle& o = terrain_.ObstacleAt(hit);
        if (Terrain::IsPickup(o.kind)) { continue; }   // bullets pass through pickups

        const Rectangle rect = o.rect;
        const Terrain::Kind kind = o.kind;
        const Vector2 at = (kind == Terrain::Kind::Bridge) ? Vector2 {bullets_.Bounds(b).x, rect.y + rect.height * 0.5f} : RectCentre(rect);
        bullets_.Kill(b);
        if (!terrain_.DamageObstacle(hit)) {
            effects_.Spawn(at, Effects::Style::Rock);   // chipped, still standing
            audio_.Play(Audio::Sfx::Pop);
            continue;
        }
        effects_.Spawn(RectCentre(rect), (kind == Terrain::Kind::Rock) ? Effects::Style::Rock : Effects::Style::Fuel);
        if (kind == Terrain::Kind::Bridge) { effects_.Spawn(Vector2 {rect.x + rect.width * 0.25f, at.y}, Effects::Style::Plane);
                                             effects_.Spawn(Vector2 {rect.x + rect.width * 0.75f, at.y}, Effects::Style::Plane); }
        audio_.Play(Audio::Sfx::Pop);
        if (kind == Terrain::Kind::Boat)        { ++boatKills_; }
        else if (kind == Terrain::Kind::Gun)    { ++gunKills_; }
        else if (kind == Terrain::Kind::Bridge) { ++bridges_; }
        else                                    { ++kills_; }
    }
    assert(kills_ >= 0);
}

void Game::Collect(Pilot& p, int obstacle)
{
    assert(p.Flying() && obstacle >= 0);
    const Terrain::Obstacle& o = terrain_.ObstacleAt(obstacle);
    assert(Terrain::IsPickup(o.kind));
    switch (o.kind) {
        case Terrain::Kind::Star:   ++stars_; break;
        case Terrain::Kind::Shield: p.shield = cfg::kShieldSeconds; break;
        case Terrain::Kind::Spread: p.spread = cfg::kSpreadSeconds; break;
        case Terrain::Kind::Life:   if (p.lives < cfg::kMaxLives) { ++p.lives; } break;
        default: break;
    }
    effects_.Spawn(RectCentre(o.rect), Effects::Style::Plane);
    audio_.Play(Audio::Sfx::Ding);
    terrain_.RemoveObstacle(obstacle);
}

void Game::CheckPilotCrash(Pilot& p)
{
    assert(p.Flying() && p.grace <= 0.0f);
    const Rectangle box = p.plane.Bounds();
    const bool shielded = p.shield > 0.0f;

    const int shell = shells_.Find(box);
    if (shell >= 0) {
        shells_.Kill(shell);
        effects_.Spawn(p.plane.Centre(), Effects::Style::Rock);
        if (shielded) { audio_.Play(Audio::Sfx::Pop); }
        else {
            audio_.Play(Audio::Sfx::Crunch);
            BeginCrash(p, Player::CrashStyle::Roll);
            return;
        }
    }
    const int hit = terrain_.FindObstacle(box);
    if (hit >= 0 && Terrain::IsPickup(terrain_.ObstacleAt(hit).kind)) { Collect(p, hit); return; }
    const bool solid = (hit >= 0) && Terrain::IsSolid(terrain_.ObstacleAt(hit).kind);
    if (solid) {
        effects_.Spawn(RectCentre(terrain_.ObstacleAt(hit).rect), Effects::Style::Rock);
        terrain_.RemoveObstacle(hit);
        if (shielded) { audio_.Play(Audio::Sfx::Pop); return; }   // the shield takes it
        audio_.Play(Audio::Sfx::Crunch);
        BeginCrash(p, Player::CrashStyle::Roll);
    } else if (terrain_.HitsBank(box)) {
        audio_.Play(Audio::Sfx::Whine);
        BeginCrash(p, Player::CrashStyle::Spiral);
    }
}

void Game::BeginCrash(Pilot& p, Player::CrashStyle style)
{
    assert(p.Flying());
    p.plane.BeginCrash(style);
    p.refuelPump = -1;
    shake_ = cfg::kShakeSeconds;
    assert(p.plane.Crashing());
}

void Game::FinishCrash(Pilot& p)
{
    assert(p.plane.Crashing() && p.plane.CrashFinished() && p.lives > 0);
    const bool roll = (p.plane.Style() == Player::CrashStyle::Roll);
    effects_.Spawn(p.plane.Centre(), roll ? Effects::Style::Splash : Effects::Style::Plane);
    audio_.Play(roll ? Audio::Sfx::Splash : Audio::Sfx::Crunch);

    --p.lives;
    if (p.lives == 0) {
        p.out = true;
        p.plane.Reset();   // clears the crash so Flying()/Crashing() are false
        return;
    }
    // Fresh plane at the start position, full tank, untouchable for a moment.
    const int idx = static_cast<int>(&p - pilots_.data());
    p.plane.Reset(twoPlayer_ ? ((idx == 0) ? -50.0f : 50.0f) : 0.0f);
    p.fuel  = cfg::kFuelMax;
    p.grace = cfg::kGraceSeconds;
    assert(p.Flying());
}

void Game::FinishGame()
{
    assert(state_ == State::Playing);
    finalScore_ = Score();
    // Table entry is under the pilot's name, or both names joined for co-op.
    std::array<char, cfg::kNameMax + 1> who {};
    if (twoPlayer_) {
        int n = 0;
        for (int i = 0; i < 2; ++i) {
            const char* s = pilots_[static_cast<size_t>(i)].name.data();
            for (int k = 0; s[k] != '\0' && n < cfg::kNameMax; ++k) { who[static_cast<size_t>(n)] = s[k]; ++n; }
            if (i == 0 && n < cfg::kNameMax) { who[static_cast<size_t>(n)] = '+'; ++n; }
        }
        who[static_cast<size_t>(n)] = '\0';
    } else {
        SetName(who, pilots_[0].name.data());
    }
    if (scores_.Qualifies(finalScore_)) {
        newRow_ = scores_.Add(who.data(), finalScore_);
        audio_.Play(Audio::Sfx::Fanfare);
    }
    state_ = State::GameOver;
}

void Game::UpdatePaused()
{
    assert(state_ == State::Paused);
    if (input::PausePressed() || input::MenuConfirm()) { state_ = State::Playing; return; }
    if (IsKeyPressed(KEY_Q) || input::MenuBack()) {
        for (Pilot& p : pilots_) { p.out = true; }
        state_ = State::Title;
    }
}

void Game::UpdateGameOver(float dt)
{
    assert(state_ == State::GameOver);
    effects_.Update(dt);   // let the final explosion finish
    if (input::MenuConfirm() || IsKeyPressed(KEY_R)) { state_ = State::Title; }
}

// ============================================================================
// Queries
// ============================================================================

bool Game::PilotVisible(const Pilot& p) const
{
    if (p.out) { return false; }
    if (p.grace <= 0.0f) { return true; }
    const float phase = std::fmod(p.grace * cfg::kBlinkHz, 1.0f);   // blink during grace
    return phase < 0.5f;
}

int Game::Score() const
{
    const int score = static_cast<int>(terrain_.Distance() * cfg::kPointsPerPx) + kills_ * cfg::kPointsPerKill
                    + boatKills_ * cfg::kPointsPerBoat + gunKills_ * cfg::kPointsPerGun
                    + stars_ * cfg::kPointsPerStar + bridges_ * cfg::kPointsPerBridge;
    assert(score >= 0);
    return score;
}

Vector2 Game::ShakeOffset() const
{
    if (shake_ <= 0.0f) { return Vector2 {0.0f, 0.0f}; }
    const float k = cfg::kShakeAmplitude * (shake_ / cfg::kShakeSeconds);
    return Vector2 {static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * k,
                    static_cast<float>(GetRandomValue(-100, 100)) * 0.01f * k};
}

// ============================================================================
// Drawing
// ============================================================================

void Game::Draw() const
{
    Camera2D cam {};
    cam.offset = ShakeOffset();
    cam.zoom   = 1.0f;
    BeginMode2D(cam);
    DrawWorld();
    EndMode2D();
    DrawDayNight();

    switch (state_) {
        case State::Title:     DrawTitle();                 break;
        case State::EnterName: DrawTitle(); DrawEnterName(); break;
        case State::Playing:   DrawHud(); if (IsKeyDown(KEY_TAB)) { DrawPeekTable(); } break;
        case State::Paused:    DrawHud(); DrawPaused();     break;
        case State::GameOver:  DrawHud(); DrawGameOver();   break;
    }
}

void Game::DrawWorld() const
{
    terrain_.Draw(sprites_);
    effects_.DrawFoam();
    for (const Pilot& p : pilots_) {
        if (p.Flying() && p.refuelPump >= 0) { DrawRefuelling(p); }
    }
    shells_.Draw();
    bullets_.Draw(sprites_.Bullet());
    for (const Pilot& p : pilots_) { p.plane.Draw(sprites_.Player(), PilotVisible(p)); if (p.Flying()) { DrawPowerUps(p); } }
    effects_.Draw();
}

void Game::DrawPowerUps(const Pilot& p) const
{
    assert(p.Flying());
    const Vector2 c = p.plane.Centre();
    if (p.shield > 0.0f) {
        const float pulse = 0.85f + 0.15f * std::sin(static_cast<float>(GetTime()) * 8.0f);
        const float fade  = (p.shield < 1.5f) ? p.shield / 1.5f : 1.0f;   // flickers out
        DrawCircleV(c, 34.0f * pulse, Fade(Color {80, 160, 255, 255}, 0.25f * fade));
        DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 34.0f * pulse, Fade(Color {160, 220, 255, 255}, 0.8f * fade));
    }
    if (p.spread > 0.0f) {
        DrawText(TextFormat("x3 %d", static_cast<int>(p.spread) + 1), static_cast<int>(c.x) + 22, static_cast<int>(c.y) - 24, 14, GOLD);
    }
}

void Game::DrawDayNight() const
{
    // Night falls and lifts once per kDayLength of river; dusk and dawn glow orange.
    const float phase = std::fmod(terrain_.Distance() / cfg::kDayLength, 1.0f);
    const float night = 0.5f * (1.0f - std::cos(2.0f * kPi * phase));
    const float dusk  = 4.0f * night * (1.0f - night);
    DrawRectangle(0, 0, cfg::kScreenW, cfg::kScreenH, Fade(Color {255, 120, 40, 255}, 0.10f * dusk * dusk));
    DrawRectangle(0, 0, cfg::kScreenW, cfg::kScreenH, Fade(Color {10, 20, 70, 255}, cfg::kNightAlpha * night));
}

void Game::DrawHudText(const char* text, int x, int y, int size)
{
    assert(text != nullptr && size > 0);
    DrawText(text, x + 2, y + 2, size, Fade(BLACK, 0.6f));   // shadow keeps it legible on grass
    DrawText(text, x, y, size, RAYWHITE);
}

void Game::DrawHud() const
{
    const char* score = TextFormat("SCORE %06d", Score());
    DrawHudText(score, (cfg::kScreenW - MeasureText(score, 28)) / 2, 8, 28);
    const char* best = TextFormat("BEST %06d", scores_.Best());
    DrawHudText(best, (cfg::kScreenW - MeasureText(best, 16)) / 2, 40, 16);
    const char* extras = TextFormat("Stars %d   Otters %d", stars_, otters_);
    DrawHudText(extras, (cfg::kScreenW - MeasureText(extras, 16)) / 2, 60, 16);
    DrawStageBanner();

    const Pilot& p1 = pilots_[0];
    DrawHudText(p1.name.data(), 10, 8, 20);
    DrawFuelBar(p1, 10, 36);
    if (twoPlayer_) {
        const Pilot& p2 = pilots_[1];
        DrawLives(p1, 10, 60, false);
        const int nameW = MeasureText(p2.name.data(), 20);
        DrawHudText(p2.name.data(), cfg::kScreenW - 10 - nameW, 8, 20);
        DrawFuelBar(p2, cfg::kScreenW - 10 - 50 - cfg::kFuelBarW, 36);
        DrawLives(p2, cfg::kScreenW - 10, 60, true);
    } else {
        DrawLives(p1, cfg::kScreenW - 10, 8, true);
    }
}

void Game::DrawFuelBar(const Pilot& p, int x, int y) const
{
    assert(p.fuel >= 0.0f && p.fuel <= cfg::kFuelMax);
    const int   fill = static_cast<int>(static_cast<float>(cfg::kFuelBarW) * (p.fuel / cfg::kFuelMax));
    const Color c    = (p.fuel < cfg::kFuelMax * 0.25f) ? RED : GREEN;
    DrawHudText("FUEL", x, y - 2, 16);
    DrawRectangle(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, Fade(BLACK, 0.3f));
    DrawRectangle(x + 50, y, fill, cfg::kFuelBarH, c);
    if (p.refuelPump >= 0) {
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 12.0f);
        DrawRectangle(x + 50, y, fill, cfg::kFuelBarH, Fade(RAYWHITE, 0.35f * pulse));
    }
    DrawRectangleLines(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, RAYWHITE);
}

void Game::DrawLives(const Pilot& p, int x, int y, bool rightToLeft) const
{
    assert(p.lives >= 0 && p.lives <= 9);
    const int spacing = 36;
    for (int i = 0; i < p.lives; ++i) {
        const float px = rightToLeft ? static_cast<float>(x - spacing * (i + 1)) : static_cast<float>(x + spacing * i);
        Sprites::DrawInto(sprites_.Player(), px, static_cast<float>(y), cfg::kPlayerW, cfg::kPlayerH, WHITE);
    }
}

void Game::DrawStageBanner() const
{
    const float prog = terrain_.StageProgress();
    if (prog > cfg::kStageBanner || terrain_.Distance() < 1.0f) { return; }
    const float fade = (prog < 60.0f) ? prog / 60.0f : ((cfg::kStageBanner - prog) < 120.0f ? (cfg::kStageBanner - prog) / 120.0f : 1.0f);
    const int   lap  = static_cast<int>(terrain_.Distance() / cfg::kStageLength);
    const char* text = TextFormat("STAGE %d: %s", lap + 1, cfg::kStages[terrain_.StageIndex()].name);
    const int   w    = MeasureText(text, 40);
    DrawText(text, (cfg::kScreenW - w) / 2 + 3, 133, 40, Fade(BLACK, 0.6f * fade));
    DrawText(text, (cfg::kScreenW - w) / 2, 130, 40, Fade(GOLD, fade));
}

void Game::DrawRefuelling(const Pilot& p) const
{
    assert(p.Flying() && p.refuelPump >= 0);
    const Vector2 from = RectCentre(terrain_.ObstacleAt(p.refuelPump).rect);
    const Vector2 to   = p.plane.Centre();
    DrawLineEx(from, to, 3.0f, Fade(BLACK, 0.5f));
    DrawLineEx(from, to, 1.5f, DARKGRAY);
    const float phase = static_cast<float>(std::fmod(GetTime() * cfg::kFuelDropHz, 1.0));
    for (int i = 0; i < cfg::kFuelDrops; ++i) {
        float u = phase + static_cast<float>(i) / static_cast<float>(cfg::kFuelDrops);
        if (u >= 1.0f) { u -= 1.0f; }
        const Vector2 d {from.x + (to.x - from.x) * u, from.y + (to.y - from.y) * u};
        DrawCircleV(d, 3.5f, ORANGE);
        DrawCircleV(Vector2 {d.x - 1.0f, d.y - 1.0f}, 1.2f, RAYWHITE);
    }
}

// ---- panels -------------------------------------------------------------

void Game::DrawTitleRow(Row row, int y, const char* label, const char* value) const
{
    assert(label != nullptr && value != nullptr);
    const bool  sel = (row_ == row);
    const Color c   = sel ? GOLD : RAYWHITE;
    const int   x   = cfg::kScreenW / 2 - 200;
    DrawText(label, x, y, 24, c);
    DrawText(TextFormat("%s %s %s", sel ? "<" : " ", value, sel ? ">" : " "), x + 220, y, 24, c);
}

void Game::DrawTitle() const
{
    // Fly-by plane with its afterburner, then the panel.
    const float dy = 150.0f + 18.0f * std::sin(demoX_ * 0.02f);
    DrawEllipse(static_cast<int>(demoX_) + 10, static_cast<int>(dy) + 40, 14.0f, 9.0f, Fade(BLACK, 0.25f));
    Sprites::DrawIntoRotated(sprites_.Player(), demoX_, dy, cfg::kPlayerW * 1.5f, cfg::kPlayerH * 1.5f, 90.0f, WHITE);

    const int panelW = 560, panelH = 420;
    const int px = (cfg::kScreenW - panelW) / 2, py = (cfg::kScreenH - panelH) / 2 + 40;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);

    const char* title = "RIVER FLYER";
    DrawText(title, (cfg::kScreenW - MeasureText(title, 72)) / 2 + 3, py - 90 + 3, 72, Fade(BLACK, 0.6f));
    DrawText(title, (cfg::kScreenW - MeasureText(title, 72)) / 2, py - 90, 72, GOLD);

    int y = py + 30;
    DrawTitleRow(Row::Players,    y, "PLAYERS",    twoPlayer_ ? "2" : "1");            y += 48;
    DrawTitleRow(Row::PilotOne,   y, "PILOT 1",    profiles_.At(profileIdx_[0]));      y += 48;
    if (twoPlayer_) { DrawTitleRow(Row::PilotTwo, y, "PILOT 2", profiles_.At(profileIdx_[1])); }
    y += 48;
    DrawTitleRow(Row::Difficulty, y, "DIFFICULTY", Diff().name);                       y += 60;

    const int best1 = scores_.BestFor(profiles_.At(profileIdx_[0]));
    DrawText(TextFormat("%s's best: %06d", profiles_.At(profileIdx_[0]), best1), px + 30, y, 18, LIGHTGRAY); y += 40;

    DrawText("SPACE / A: fly     Enter on a pilot: rename     Q: quit", px + 30, y, 16, LIGHTGRAY); y += 26;
    DrawText("P1: WASD + Space    P2: arrows + Right Ctrl    Esc: pause", px + 30, y, 16, LIGHTGRAY); y += 26;
    int pads = 0;
    for (int i = 0; i < 2; ++i) { if (IsGamepadAvailable(i)) { ++pads; } }
    DrawText(TextFormat("Gamepads connected: %d", pads), px + 30, y, 16, pads > 0 ? LIME : GRAY);
}

void Game::DrawPaused() const
{
    assert(state_ == State::Paused);
    const int panelW = 420, panelH = 140;
    const int px = (cfg::kScreenW - panelW) / 2, py = (cfg::kScreenH - panelH) / 2;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    const char* t = "PAUSED";
    DrawText(t, (cfg::kScreenW - MeasureText(t, 40)) / 2, py + 20, 40, GOLD);
    const char* h = "Esc: keep flying     Q: back to title";
    DrawText(h, (cfg::kScreenW - MeasureText(h, 18)) / 2, py + 85, 18, RAYWHITE);
}

void Game::DrawEnterName() const
{
    assert(state_ == State::EnterName);
    const int panelW = 440, panelH = 200;
    const int px = (cfg::kScreenW - panelW) / 2, py = (cfg::kScreenH - panelH) / 2;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    const char* t = TextFormat("PILOT %d NAME", nameTarget_ + 1);
    DrawText(t, (cfg::kScreenW - MeasureText(t, 32)) / 2, py + 16, 32, GOLD);

    const int boxX = px + 30, boxY = py + 70;
    DrawRectangle(boxX, boxY, panelW - 60, 36, RAYWHITE);
    DrawText(name_.data(), boxX + 8, boxY + 6, 24, DARKBLUE);
    if (std::fmod(GetTime(), 1.0) < 0.5 && nameLen_ < cfg::kNameMax) {
        DrawRectangle(boxX + 8 + MeasureText(name_.data(), 24) + 2, boxY + 6, 3, 24, DARKBLUE);
    }
    DrawText("Enter: keep     Esc: cancel", px + 30, py + 130, 18, LIGHTGRAY);
}

void Game::DrawScoreTable(int x, int y) const
{
    assert(x >= 0 && y >= 0);
    const int rowH = 26;
    DrawText("TOP PILOTS", x, y, 20, RAYWHITE);
    y += rowH + 4;
    for (int i = 0; i < cfg::kHighScoreCount; ++i) {
        const int   ry = y + i * rowH;
        const Color c  = (i == newRow_) ? GOLD : ((i < scores_.Count()) ? RAYWHITE : GRAY);
        DrawText(TextFormat("%2d.", i + 1), x, ry, 20, c);
        if (i < scores_.Count()) {
            const HighScores::Entry& e = scores_.At(i);
            DrawText(e.name.data(), x + 44, ry, 20, c);
            DrawText(TextFormat("%06d", e.score), x + 260, ry, 20, c);
        } else {
            DrawText("- - -", x + 44, ry, 20, c);
        }
    }
}

void Game::DrawPeekTable() const
{
    const int panelW = 400, panelH = 340;
    const int px = (cfg::kScreenW - panelW) / 2, py = (cfg::kScreenH - panelH) / 2;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.6f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawScoreTable(px + 30, py + 20);
}

void Game::DrawGameOver() const
{
    assert(state_ == State::GameOver);
    const int panelW = 400, panelH = 470;
    const int px = (cfg::kScreenW - panelW) / 2, py = (cfg::kScreenH - panelH) / 2;
    const char* msg1 = "Oh no! Out of planes!";
    const char* msg2 = "SPACE: back to the title";
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawText(msg1, (cfg::kScreenW - MeasureText(msg1, 30)) / 2, py + 16, 30, GOLD);
    DrawText(TextFormat("Your score: %06d%s", finalScore_, (newRow_ >= 0) ? "  - new high score!" : ""), px + 30, py + 60, 18, RAYWHITE);
    DrawText(TextFormat("Stars %d   Bridges %d   Otters spotted %d", stars_, bridges_, otters_), px + 30, py + 80, 16, LIGHTGRAY);
    DrawScoreTable(px + 30, py + 100);
    DrawText(msg2, (cfg::kScreenW - MeasureText(msg2, 22)) / 2, py + panelH - 40, 22, RAYWHITE);
}
