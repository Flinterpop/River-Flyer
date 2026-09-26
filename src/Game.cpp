#include "Game.h"

#include <cassert>
#include <cmath>

#include "Config.h"
#include "Touch.h"
#include "Screen.h"
#include "Haptics.h"

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

// In the browser the page cannot close itself, and an iOS app is not meant
// to, so Q on the title does nothing there.
#if defined(__EMSCRIPTEN__) || defined(RF_IOS)
constexpr bool kCanQuit = false;
#else
constexpr bool kCanQuit = true;
#endif

// On-screen keyboard for pilot names: four rows of ten keys, then a row of
// three wide keys (SPACE, DEL, OK). The cursor column always stays in
// [0, kKeyCols); on the wide row it maps onto whichever wide key covers it.
constexpr int  kKeyCols = 10;
constexpr int  kKeyRows = 5;
constexpr int  kKeyCell = 40;                                        // px per key, including the gap
constexpr char kKeyChars[kKeyRows - 1][kKeyCols + 1] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-'!?"};
enum class WideKey { Space, Del, Ok };
constexpr int  kWideFirstCol[3] = {0, 4, 7};                         // where each wide key starts
constexpr int  kWideCols[3]     = {4, 3, 3};                         // and how many columns it spans

WideKey WideKeyAt(int col)
{
    assert(col >= 0 && col < kKeyCols);
    if (col < kWideFirstCol[1]) { return WideKey::Space; }
    if (col < kWideFirstCol[2]) { return WideKey::Del; }
    return WideKey::Ok;
}

// Panel geometry, shared by the drawing code and the touch hit-tests so a tap
// lands exactly on what was drawn.
constexpr int kTitlePanelW = 560, kTitlePanelH = 468;
constexpr int kTitlePx     = (cfg::kScreenW - kTitlePanelW) / 2;
int       TitlePy()        { return (screen::H() - kTitlePanelH) / 2 + 40; }
int       TitleRowY0()     { return TitlePy() + 30; }                 // first row; rows are kTitleRowH apart
constexpr int kTitleRowH   = 48;
constexpr int kTitleLabelX = cfg::kScreenW / 2 - 200;
constexpr int kTitleArrowW = 245;                                    // label + "<" zone, then value, then ">" zone
constexpr int kTitleValueW = 200;
Rectangle PlayBtn()        { return Rectangle {static_cast<float>(kTitlePx + 30), static_cast<float>(TitlePy() + 318), static_cast<float>(kTitlePanelW - 60), 56.0f}; }

constexpr int kNamePanelW  = kKeyCols * kKeyCell + 60, kNamePanelH = 390;
constexpr int kNamePx      = (cfg::kScreenW - kNamePanelW) / 2;
int       NamePy()         { return (screen::H() - kNamePanelH) / 2; }
constexpr int kNameKeysX   = kNamePx + 30;
int       NameKeysY()      { return NamePy() + 116; }

constexpr int kPausePanelW = 420;
constexpr int kPausePx     = (cfg::kScreenW - kPausePanelW) / 2;
int       PausePanelH()    { return touch::Enabled() ? 240 : 140; }   // taller to hold the buttons
int       PausePy()        { return (screen::H() - PausePanelH()) / 2; }
Rectangle PauseFlyBtn()    { return Rectangle {static_cast<float>(kPausePx + 20),  static_cast<float>(PausePy() + 80),  180.0f, 56.0f}; }
Rectangle PauseTitleBtn()  { return Rectangle {static_cast<float>(kPausePx + 220), static_cast<float>(PausePy() + 80),  180.0f, 56.0f}; }
Rectangle PauseMusicBtn()  { return Rectangle {static_cast<float>(kPausePx + 20),  static_cast<float>(PausePy() + 160), 380.0f, 50.0f}; }

constexpr float kOverTapDelay = 1.0f;                                // seconds before a tap leaves the game-over panel

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
    missiles_.Reset();
    terrain_.Reset(TuningFor(Diff()));
    boss_.Reset();
    lastStage_ = terrain_.StageIndex();
    bosses_ = 0;
    kills_ = 0; boatKills_ = 0; gunKills_ = 0; samKills_ = 0; missileKills_ = 0; stars_ = 0; bridges_ = 0; otters_ = 0;
    finalScore_ = 0; newRow_ = -1; shake_ = 0.0f;

    for (int i = 0; i < cfg::kMaxPilots; ++i) {
        Pilot& p = pilots_[static_cast<size_t>(i)];
        p.out = (i >= PilotCount());
        if (p.out) { continue; }
        p.map = (i == 0) ? input::PilotOne(!twoPlayer_) : input::PilotTwo();
        SetName(p.name, profiles_.At(profileIdx_[static_cast<size_t>(i)]));
        p.plane.Reset(twoPlayer_ ? ((i == 0) ? 50.0f : -50.0f) : 0.0f);   // pilot one on the right, matching their half of the touch controls
        p.assist = ((assistMask_ >> i) & 1) != 0;
        p.lives  = Diff().lives + (p.assist ? cfg::kAssistLives : 0);
        p.fuel   = cfg::kFuelMax;
        p.health = cfg::kHealthMax;
        p.grace = 0.0f; p.fireCooldown = 0.0f; p.foamTimer = 0.0f; p.refuelPump = -1;
        p.shield = 0.0f; p.spread = 0.0f; p.chaff = cfg::kChaffPerPlane; p.jamming = false; p.rwrTimer = 0.0f;
        p.scrapeTimer = 0.0f; p.smokeTimer = 0.0f; p.hurtFlash = 0.0f;
        assert(!terrain_.HitsBank(p.plane.Bounds()));   // must spawn in open water
    }
    touch::SetPlayers(PilotCount());   // two pilots split the screen
    state_ = State::Playing;
}

void Game::Update(float dt)
{
    assert(dt >= 0.0f);
    if (IsKeyPressed(KEY_M)) { audio_.ToggleMusic(); }
    audio_.Sustain(Audio::Sfx::Music);
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
            case Row::Assist:     CycleAssist(step); break;
            case Row::Count:      break;
        }
    }
    if (kCanQuit && (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_BACK))) { quit_ = true; return; }
    if (IsKeyPressed(KEY_SPACE)) { StartGame(); return; }
    if (input::MenuConfirm()) {
        if (row_ == Row::PilotOne)      { BeginNameEntry(0); }
        else if (row_ == Row::PilotTwo) { BeginNameEntry(1); }
        else                            { StartGame(); }
    }
    if (touch::Tapped()) { TouchTitle(touch::TapPos()); }
    assert(difficulty_ >= 0 && difficulty_ < cfg::kDifficultyCount);
}

// A tap on a title row acts on it at once: the left part steps back, the
// right part steps forward, and the name itself opens the rename panel.
void Game::TouchTitle(Vector2 p)
{
    assert(state_ == State::Title);
    if (CheckCollisionPointRec(p, PlayBtn())) { StartGame(); return; }

    for (int r = 0; r < static_cast<int>(Row::Count); ++r) {
        const Row row = static_cast<Row>(r);
        if (row == Row::PilotTwo && !twoPlayer_) { continue; }
        const Rectangle band {static_cast<float>(kTitlePx), static_cast<float>(TitleRowY0() + r * kTitleRowH), static_cast<float>(kTitlePanelW), static_cast<float>(kTitleRowH)};
        if (!CheckCollisionPointRec(p, band)) { continue; }
        row_ = row;
        const bool  onName = (row == Row::PilotOne || row == Row::PilotTwo);
        const float x      = p.x - static_cast<float>(kTitleLabelX);
        const int   step   = (x < static_cast<float>(kTitleArrowW)) ? -1 : 1;
        if (onName && x >= static_cast<float>(kTitleArrowW) && x < static_cast<float>(kTitleArrowW + kTitleValueW)) {
            BeginNameEntry(row == Row::PilotOne ? 0 : 1);
            return;
        }
        const int n = profiles_.Count();
        switch (row) {
            case Row::Players:    twoPlayer_ = !twoPlayer_; break;
            case Row::PilotOne:   profileIdx_[0] = (profileIdx_[0] + step + n) % n; break;
            case Row::PilotTwo:   profileIdx_[1] = (profileIdx_[1] + step + n) % n; break;
            case Row::Difficulty: difficulty_ = (difficulty_ + step + cfg::kDifficultyCount) % cfg::kDifficultyCount; break;
            case Row::Assist:     CycleAssist(step); break;
            case Row::Count:      break;
        }
        return;
    }
    assert(profileIdx_[0] < profiles_.Count() && profileIdx_[1] < profiles_.Count());
}

void Game::BeginNameEntry(int pilotIndex)
{
    assert(pilotIndex >= 0 && pilotIndex < cfg::kMaxPilots);
    nameTarget_ = pilotIndex;
    SetName(name_, profiles_.At(profileIdx_[static_cast<size_t>(pilotIndex)]));
    nameLen_ = 0;
    while (name_[static_cast<size_t>(nameLen_)] != '\0') { ++nameLen_; }
    keyCol_ = 0;
    keyRow_ = 0;
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
        if (c >= 32 && c <= 126) { TypeChar(static_cast<char>(c)); }
    }
    // Backspaces: drain the key queue so several in one frame all count,
    // plus key-repeat while it is held.
    int erase = IsKeyPressedRepeat(KEY_BACKSPACE) ? 1 : 0;
    for (int i = 0; i < cfg::kNameInputRepeatChars; ++i) {
        const int k = GetKeyPressed();
        if (k == 0) { break; }
        if (k == KEY_BACKSPACE) { ++erase; }
    }
    for (; erase > 0; --erase) { EraseChar(); }

    // On-screen keyboard: arrows / d-pad / stick move, A types, B erases, Y is space.
    const input::Nav nav = input::NavPressed();
    if (nav.dx != 0 || nav.dy != 0) { MoveKeyCursor(nav.dx, nav.dy); }
    if (input::PadErasePressed()) { EraseChar(); }
    if (input::PadSpacePressed()) { TypeChar(' '); }
    if (input::PadTypePressed()) {
        PressKeyCursor();
        if (state_ != State::EnterName) { return; }   // OK key: name committed
    }

    if (IsKeyPressed(KEY_ESCAPE) || input::PadCancelPressed()) { state_ = State::Title; return; }
    if ((IsKeyPressed(KEY_ENTER) || input::PadDonePressed()) && nameLen_ > 0) { CommitName(); }
    if (state_ == State::EnterName && touch::Tapped()) { TouchEnterName(touch::TapPos()); }
    assert(nameLen_ >= 0 && nameLen_ <= cfg::kNameMax);
}

// A tap on a key presses it; a tap outside the panel cancels.
void Game::TouchEnterName(Vector2 p)
{
    assert(state_ == State::EnterName);
    const Rectangle panel {static_cast<float>(kNamePx), static_cast<float>(NamePy()), static_cast<float>(kNamePanelW), static_cast<float>(kNamePanelH)};
    if (!CheckCollisionPointRec(p, panel)) { state_ = State::Title; return; }

    const int c = static_cast<int>(std::floor((p.x - static_cast<float>(kNameKeysX)) / static_cast<float>(kKeyCell)));
    const int r = static_cast<int>(std::floor((p.y - static_cast<float>(NameKeysY())) / static_cast<float>(kKeyCell)));
    if (c < 0 || c >= kKeyCols || r < 0 || r >= kKeyRows) { return; }   // the name box or a hint line
    keyCol_ = c;
    keyRow_ = r;
    PressKeyCursor();
    assert(keyRow_ >= 0 && keyRow_ < kKeyRows && keyCol_ >= 0 && keyCol_ < kKeyCols);
}

void Game::TypeChar(char c)
{
    assert(c >= 32 && c <= 126);
    if (nameLen_ >= cfg::kNameMax) { return; }
    name_[static_cast<size_t>(nameLen_)] = c;
    ++nameLen_;
    name_[static_cast<size_t>(nameLen_)] = '\0';
    assert(nameLen_ > 0 && nameLen_ <= cfg::kNameMax);
}

void Game::EraseChar()
{
    if (nameLen_ <= 0) { return; }
    --nameLen_;
    name_[static_cast<size_t>(nameLen_)] = '\0';
    assert(nameLen_ >= 0 && nameLen_ < cfg::kNameMax);
}

void Game::MoveKeyCursor(int dx, int dy)
{
    assert(dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1);
    keyRow_ = (keyRow_ + dy + kKeyRows) % kKeyRows;
    if (keyRow_ < kKeyRows - 1) {
        keyCol_ = (keyCol_ + dx + kKeyCols) % kKeyCols;
    } else if (dx != 0) {
        // Wide row: step between the three wide keys, landing on each one's first column.
        const int wide = (static_cast<int>(WideKeyAt(keyCol_)) + dx + 3) % 3;
        keyCol_ = kWideFirstCol[wide];
    }
    assert(keyRow_ >= 0 && keyRow_ < kKeyRows && keyCol_ >= 0 && keyCol_ < kKeyCols);
}

void Game::PressKeyCursor()
{
    assert(state_ == State::EnterName);
    if (keyRow_ < kKeyRows - 1) {
        TypeChar(kKeyChars[keyRow_][keyCol_]);
        return;
    }
    switch (WideKeyAt(keyCol_)) {
        case WideKey::Space: TypeChar(' '); break;
        case WideKey::Del:   EraseChar();   break;
        case WideKey::Ok:    if (nameLen_ > 0) { CommitName(); } break;
    }
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
    UpdateSams(dt);
    UpdateMissiles(dt);
    UpdateCritters(dt);
    UpdateBoss(dt);
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
    if (p.hurtFlash > 0.0f) { p.hurtFlash -= dt; }
    UpdateDamageSmoke(p, dt);
    UpdateCountermeasures(p, dt);
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
    p.fuel -= Diff().fuelBurn * (p.jamming ? cfg::kJamFuelMult : 1.0f) * (p.assist ? cfg::kAssistFuelBurn : 1.0f) * dt;

    // Flying over a depot refuels without destroying it.
    p.refuelPump = -1;
    const int hit = terrain_.FindObstacle(p.plane.Bounds());
    if (hit >= 0 && terrain_.ObstacleAt(hit).kind == Terrain::Kind::Fuel) {
        if (p.fuel < cfg::kFuelMax) {
            audio_.Sustain(Audio::Sfx::Slurp);
            p.refuelPump = hit;
        }
        p.chaff = cfg::kChaffPerPlane;   // the pump restocks the chaff too
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

void Game::UpdateSams(float dt)
{
    std::array<Vector2, cfg::kMaxPilots> targets {};
    std::array<int, cfg::kMaxPilots>     who {};
    int  n = 0;
    bool mayFire = false;
    for (int i = 0; i < cfg::kMaxPilots; ++i) {
        const Pilot& p = pilots_[static_cast<size_t>(i)];
        if (!p.Flying()) { continue; }
        targets[static_cast<size_t>(n)] = p.plane.Centre();
        who[static_cast<size_t>(n)]     = i;
        ++n;
        if (p.grace <= 0.0f) { mayFire = true; }
    }
    if (terrain_.UpdateSams(dt, targets, who, n, mayFire, missiles_) > 0) {
        audio_.Play(Audio::Sfx::Launch);
        shake_ = cfg::kShakeSeconds * 0.4f;
    }
}

void Game::UpdateMissiles(float dt)
{
    std::array<Missiles::Target, cfg::kMaxPilots> t {};
    for (int i = 0; i < cfg::kMaxPilots; ++i) {
        const Pilot& p = pilots_[static_cast<size_t>(i)];
        t[static_cast<size_t>(i)] = Missiles::Target {p.plane.Centre(), p.jamming, p.Flying()};
    }
    missiles_.Update(dt, t, terrain_.LastStep(), effects_);
}

void Game::UpdateCountermeasures(Pilot& p, float dt)
{
    assert(dt >= 0.0f && p.Flying());
    const int idx = static_cast<int>(&p - pilots_.data());
    p.jamming = input::JamHeld(p.map);

    if (input::ChaffPressed(p.map) && p.chaff > 0) {
        --p.chaff;
        const Vector2 c = p.plane.Centre();
        const Vector2 cloud {c.x, c.y + cfg::kPlayerH * 0.6f};
        missiles_.Decoy(idx, cloud);
        effects_.Spawn(cloud, Effects::Style::Chaff);
        audio_.Play(Audio::Sfx::Chaff);
    }

    // RWR: beep faster as the nearest missile chasing this pilot closes in.
    const float d = missiles_.NearestTo(idx, p.plane.Centre());
    if (d < 0.0f) { p.rwrTimer = 0.0f; return; }
    p.rwrTimer -= dt;
    if (p.rwrTimer <= 0.0f) {
        const float k = (d > cfg::kSamRange) ? 1.0f : d / cfg::kSamRange;
        p.rwrTimer = cfg::kRwrNearGap + (cfg::kRwrFarGap - cfg::kRwrNearGap) * k;
        audio_.Play(Audio::Sfx::Rwr);
    }
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

// The gunship arrives as each stage turns over, and is scored when it drops.
void Game::UpdateBoss(float dt)
{
    const int stage = terrain_.StageIndex();
    if (stage != lastStage_) {
        lastStage_ = stage;
        if (!boss_.Active()) { boss_.Spawn(stage); }
    }
    if (!boss_.Active()) { return; }

    std::array<Vector2, cfg::kMaxPilots> targets {};
    int n = 0;
    for (const Pilot& p : pilots_) { if (p.Flying()) { targets[static_cast<size_t>(n++)] = p.plane.Centre(); } }
    if (boss_.Update(dt, targets.data(), n, shells_)) { return; }   // it gave up and left

    // Flying into it hurts, and the shield does not save you from a gunship.
    if (boss_.Fighting()) {
        for (Pilot& p : pilots_) {
            if (!p.Flying() || p.grace > 0.0f) { continue; }
            if (!CheckCollisionRecs(p.plane.Bounds(), boss_.Bounds())) { continue; }
            effects_.Spawn(p.plane.Centre(), Effects::Style::Rock);
            audio_.Play(Audio::Sfx::Crunch);
            haptics::Play(haptics::Kind::Heavy);
            if (Damage(p, cfg::kBossRamDamage)) { BeginCrash(p, Player::CrashStyle::Roll); }
        }
    }
}

void Game::ResolveBulletHits()
{
    // Bounded: kMaxBullets x (kMaxObstacles + kMaxMissiles) checks at most.
    for (int b = 0; b < Bullets::Capacity(); ++b) {
        if (!bullets_.Active(b)) { continue; }
        if (boss_.Fighting() && CheckCollisionRecs(bullets_.Bounds(b), boss_.Bounds())) {
            bullets_.Kill(b);
            if (boss_.Hit()) {
                // Down it goes: a cluster of bursts, points, and a parting gift.
                const Vector2 c = boss_.Centre();
                effects_.Spawn(c, Effects::Style::Plane);
                effects_.Spawn(Vector2 {c.x - cfg::kBossW * 0.3f, c.y}, Effects::Style::Rock);
                effects_.Spawn(Vector2 {c.x + cfg::kBossW * 0.3f, c.y}, Effects::Style::Fuel);
                audio_.Play(Audio::Sfx::Crunch);
                haptics::Play(haptics::Kind::Heavy);
                shake_ = cfg::kShakeSeconds;
                ++bosses_;
                boss_.Clear();
            } else {
                audio_.Play(Audio::Sfx::Pop);
            }
            continue;
        }
        const int m = missiles_.Find(bullets_.Bounds(b));
        if (m >= 0) {
            effects_.Spawn(missiles_.Position(m), Effects::Style::Fuel);
            audio_.Play(Audio::Sfx::Pop);
            missiles_.Kill(m);
            bullets_.Kill(b);
            ++missileKills_;
            continue;
        }
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
        else if (kind == Terrain::Kind::Sam)    { ++samKills_; }
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
        case Terrain::Kind::Health:
            p.health += cfg::kHealthPack;
            if (p.health > cfg::kHealthMax) { p.health = cfg::kHealthMax; }
            break;
        default: break;
    }
    effects_.Spawn(RectCentre(o.rect), Effects::Style::Plane);
    audio_.Play(Audio::Sfx::Ding);
    haptics::Play(haptics::Kind::Light);
    terrain_.RemoveObstacle(obstacle);
}

// Takes 'amount' off the hull. Returns true when that was the last of it, so
// the caller can start the matching crash animation.
bool Game::Damage(Pilot& p, float amount)
{
    assert(p.Flying() && amount > 0.0f);
    if (p.shield > 0.0f) { return false; }          // the bubble eats it
    if (p.assist) { amount *= cfg::kAssistDamage; }
    p.health -= amount;
    p.hurtFlash = cfg::kHurtFlashSeconds;
    // A scrape arrives as a trickle every frame, so only a real bite buzzes.
    if (amount >= cfg::kShellDamage) { haptics::Play(haptics::Kind::Heavy); }
    if (p.health <= 0.0f) { p.health = 0.0f; return true; }
    return false;
}

// A damaged plane trails smoke; the worse the damage, the faster the puffs.
void Game::UpdateDamageSmoke(Pilot& p, float dt)
{
    assert(p.Flying());
    if (p.health >= cfg::kSmokeHealth) { p.smokeTimer = 0.0f; return; }
    p.smokeTimer -= dt;
    if (p.smokeTimer > 0.0f) { return; }
    const float hurt = 1.0f - (p.health / cfg::kSmokeHealth);        // 0 at the threshold, 1 at death
    p.smokeTimer = cfg::kSmokeGapHealthy + (cfg::kSmokeGapSevere - cfg::kSmokeGapHealthy) * hurt;
    const Vector2 c = p.plane.Centre();
    effects_.SpawnDamageSmoke(Vector2 {c.x + static_cast<float>(GetRandomValue(-4, 4)), c.y + cfg::kPlayerH * 0.45f}, hurt);
}

void Game::CheckPilotCrash(Pilot& p)
{
    assert(p.Flying() && p.grace <= 0.0f);
    const Rectangle box = p.plane.Bounds();
    const bool shielded = p.shield > 0.0f;

    const int missile = missiles_.Find(box);
    if (missile >= 0) {
        effects_.Spawn(missiles_.Position(missile), Effects::Style::Fuel);
        missiles_.Kill(missile);
        if (shielded) { audio_.Play(Audio::Sfx::Pop); }
        else {
            audio_.Play(Audio::Sfx::Crunch);
            effects_.SpawnSparks(p.plane.Centre(), 1.0f);
            effects_.SpawnSparks(p.plane.Centre(), -1.0f);
            if (Damage(p, cfg::kMissileDamage)) { BeginCrash(p, Player::CrashStyle::Roll); return; }
        }
    }
    const int shell = shells_.Find(box);
    if (shell >= 0) {
        shells_.Kill(shell);
        effects_.Spawn(p.plane.Centre(), Effects::Style::Rock);
        if (shielded) { audio_.Play(Audio::Sfx::Pop); }
        else {
            audio_.Play(Audio::Sfx::Crunch);
            effects_.SpawnSparks(p.plane.Centre(), 1.0f);
            if (Damage(p, cfg::kShellDamage)) { BeginCrash(p, Player::CrashStyle::Roll); return; }
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
        effects_.SpawnSparks(p.plane.Centre(), 1.0f);
        effects_.SpawnSparks(p.plane.Centre(), -1.0f);
        if (Damage(p, cfg::kRockDamage)) { BeginCrash(p, Player::CrashStyle::Roll); }
    } else if (terrain_.HitsBank(box)) {
        // Scraping the shore or an island: grind the hull down while contact
        // lasts, throw sparks off the side that touched, and bounce clear.
        // A shield spares the hull but still bounces — land is never flyable.
        const float away = terrain_.BankEscapeX(box);
        p.scrapeTimer -= GetFrameTime();
        if (p.scrapeTimer <= 0.0f) {
            if (p.scrapeTimer <= -cfg::kScrapeHapticGap) { haptics::Play(haptics::Kind::Medium); }
            p.scrapeTimer = cfg::kScrapeSparkGap;
            const Vector2 c = p.plane.Centre();
            effects_.SpawnSparks(Vector2 {c.x - away * cfg::kPlayerW * 0.5f, c.y}, -away);
        }
        audio_.Sustain(shielded ? Audio::Sfx::Pop : Audio::Sfx::Scrape);
        if (away != 0.0f) { p.plane.Bounce(away); }
        if (Damage(p, cfg::kBankDamagePerSec * GetFrameTime())) {
            audio_.Play(Audio::Sfx::Whine);
            BeginCrash(p, Player::CrashStyle::Spiral);
        }
    }
}

void Game::BeginCrash(Pilot& p, Player::CrashStyle style)
{
    assert(p.Flying());
    p.plane.BeginCrash(style);
    p.refuelPump = -1;
    shake_ = cfg::kShakeSeconds;
    haptics::Play(haptics::Kind::Heavy);
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
    p.plane.Reset(twoPlayer_ ? ((idx == 0) ? 50.0f : -50.0f) : 0.0f);
    p.fuel   = cfg::kFuelMax;
    p.health = cfg::kHealthMax;
    p.grace  = cfg::kGraceSeconds;
    p.chaff = cfg::kChaffPerPlane;
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
    overT_ = 0.0f;
    state_ = State::GameOver;
}

void Game::Pause()
{
    if (state_ == State::Playing) { state_ = State::Paused; }
    assert(state_ != State::Playing);
}

void Game::UpdatePaused()
{
    assert(state_ == State::Paused);
    if (input::PausePressed() || input::MenuConfirm()) { state_ = State::Playing; return; }
    if (IsKeyPressed(KEY_Q) || input::MenuBack()) {
        for (Pilot& p : pilots_) { p.out = true; }
        state_ = State::Title;
        return;
    }
    if (touch::Tapped()) { TouchPaused(touch::TapPos()); }
}

void Game::TouchPaused(Vector2 p)
{
    assert(state_ == State::Paused);
    if (CheckCollisionPointRec(p, PauseFlyBtn()))   { state_ = State::Playing; }
    if (CheckCollisionPointRec(p, PauseMusicBtn())) { audio_.ToggleMusic(); }
    if (CheckCollisionPointRec(p, PauseTitleBtn())) {
        for (Pilot& p2 : pilots_) { p2.out = true; }
        state_ = State::Title;
    }
    assert(state_ != State::GameOver);
}

void Game::UpdateGameOver(float dt)
{
    assert(state_ == State::GameOver);
    effects_.Update(dt);   // let the final explosion finish
    overT_ += dt;
    if (input::MenuConfirm() || IsKeyPressed(KEY_R)) { state_ = State::Title; }
    if (touch::Tapped() && overT_ >= kOverTapDelay) { state_ = State::Title; }   // not the fire finger still coming down
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
                    + stars_ * cfg::kPointsPerStar + bridges_ * cfg::kPointsPerBridge
                    + samKills_ * cfg::kPointsPerSam + missileKills_ * cfg::kPointsPerMissile
                    + bosses_ * cfg::kPointsPerBoss;
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
    missiles_.Draw();
    bullets_.Draw(sprites_.Bullet());
    for (const Pilot& p : pilots_) { p.plane.Draw(sprites_.Player(), PilotVisible(p)); if (p.Flying()) { DrawPowerUps(p); } }
    boss_.Draw(sprites_);
    effects_.Draw();
}

void Game::DrawPowerUps(const Pilot& p) const
{
    assert(p.Flying());
    const Vector2 c = p.plane.Centre();
    if (twoPlayer_) {
        // Whose plane is whose: "1" and "2" ride just above the canopy.
        const int   idx   = static_cast<int>(&p - pilots_.data());
        const char* label = (idx == 0) ? "1" : "2";
        const int   size  = 14;
        const int   x     = static_cast<int>(c.x) - MeasureText(label, size) / 2;
        const int   y     = static_cast<int>(c.y) - static_cast<int>(cfg::kPlayerH * 0.5f) - size - 2;
        DrawText(label, x + 1, y + 1, size, Fade(BLACK, 0.6f));
        DrawText(label, x, y, size, (idx == 0) ? RAYWHITE : Color {170, 215, 255, 255});
    }
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
    DrawRectangle(0, 0, cfg::kScreenW, screen::H(), Fade(Color {255, 120, 40, 255}, 0.10f * dusk * dusk));
    DrawRectangle(0, 0, cfg::kScreenW, screen::H(), Fade(Color {10, 20, 70, 255}, cfg::kNightAlpha * night));
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
    DrawBossBar();

    const Pilot& p1 = pilots_[0];
    DrawHudText(p1.name.data(), 10, 8, 20);
    if (p1.assist) { DrawHudText("ASSIST", 10 + MeasureText(p1.name.data(), 20) + 10, 11, 14); }
    DrawFuelBar(p1, 10, 36);
    DrawHealthBar(p1, 10, 56);
    DrawWarnings(p1, 10, 120, false);
    if (twoPlayer_) {
        const Pilot& p2 = pilots_[1];
        DrawLives(p1, 10, 76, false);
        DrawWarnings(p2, cfg::kScreenW - 10, 120, true);
        const int nameW = MeasureText(p2.name.data(), 20);
        DrawHudText(p2.name.data(), cfg::kScreenW - 10 - nameW, 8, 20);
        if (p2.assist) { DrawHudText("ASSIST", cfg::kScreenW - 10 - nameW - MeasureText("ASSIST", 14) - 10, 11, 14); }
        DrawFuelBar(p2, cfg::kScreenW - 10 - 50 - cfg::kFuelBarW, 36);
        DrawHealthBar(p2, cfg::kScreenW - 10 - 50 - cfg::kHealthBarW, 56);
        DrawLives(p2, cfg::kScreenW - 10, 76, true);
    } else {
        DrawLives(p1, cfg::kScreenW - 10, 8, true);
    }
}

// A red bar under the score while the gunship is up, so the fight has a clock.
void Game::DrawBossBar() const
{
    if (!boss_.Fighting()) { return; }
    const int w = 320, h = 10;
    const int x = (cfg::kScreenW - w) / 2, y = 84;
    const float frac = static_cast<float>(boss_.Hp()) / static_cast<float>(boss_.MaxHp());
    const char* label = "GUNSHIP";
    DrawHudText(label, (cfg::kScreenW - MeasureText(label, 14)) / 2, y - 18, 14);
    DrawRectangle(x, y, w, h, Fade(BLACK, 0.45f));
    DrawRectangle(x, y, static_cast<int>(static_cast<float>(w) * frac), h, Color {220, 60, 60, 255});
    DrawRectangleLines(x, y, w, h, RAYWHITE);
}

void Game::DrawHealthBar(const Pilot& p, int x, int y) const
{
    assert(p.health >= 0.0f && p.health <= cfg::kHealthMax);
    const float frac = p.health / cfg::kHealthMax;
    const int   fill = static_cast<int>(static_cast<float>(cfg::kHealthBarW) * frac);
    // Green while sound, amber once it is trailing smoke, red when critical.
    const Color c = (p.health <= cfg::kSevereHealth) ? Color {230, 60, 60, 255}
                  : (p.health <= cfg::kSmokeHealth)  ? Color {235, 170, 50, 255}
                                                     : Color {70, 205, 110, 255};
    DrawHudText("HULL", x, y - 1, 14);
    DrawRectangle(x + 50, y, cfg::kHealthBarW, cfg::kHealthBarH, Fade(BLACK, 0.3f));
    DrawRectangle(x + 50, y, fill, cfg::kHealthBarH, c);
    if (p.hurtFlash > 0.0f) {
        // Brief white wash over the remaining hull, so the flash can't be
        // mistaken for a full bar.
        const float a = p.hurtFlash / cfg::kHurtFlashSeconds;
        DrawRectangle(x + 50, y, fill, cfg::kHealthBarH, Fade(RAYWHITE, 0.5f * a));
    }
    if (p.health <= cfg::kSevereHealth) {
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 9.0f);
        DrawRectangleLines(x + 50, y, cfg::kHealthBarW, cfg::kHealthBarH, Fade(Color {255, 90, 90, 255}, 0.4f + 0.6f * pulse));
    } else {
        DrawRectangleLines(x + 50, y, cfg::kHealthBarW, cfg::kHealthBarH, RAYWHITE);
    }
}

void Game::DrawWarnings(const Pilot& p, int x, int y, bool rightAlign) const
{
    const int idx = static_cast<int>(&p - pilots_.data());
    const char* chaff = TextFormat("CHAFF x%d%s", p.chaff, p.jamming ? "  JAMMING" : "");
    const int   cw    = MeasureText(chaff, 16);
    DrawHudText(chaff, rightAlign ? x - cw : x, y, 16);
    if (p.Flying() && missiles_.NearestTo(idx, p.plane.Centre()) >= 0.0f && std::fmod(GetTime(), 0.3) < 0.18) {
        const char* warn = "MISSILE!  C: chaff  V: jam";
        const int   ww   = MeasureText(warn, 20);
        DrawText(warn, (rightAlign ? x - ww : x) + 2, y + 22, 20, Fade(BLACK, 0.6f));
        DrawText(warn, rightAlign ? x - ww : x, y + 20, 20, RED);
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

// OFF -> PILOT 1 -> PILOT 2 -> BOTH with two players; a plain toggle with one.
void Game::CycleAssist(int step)
{
    const int states = twoPlayer_ ? 4 : 2;
    assistMask_ = ((assistMask_ & (states - 1)) + step + states) % states;
}

const char* Game::AssistLabel() const
{
    if (!twoPlayer_) { return (assistMask_ & 1) ? "ON" : "OFF"; }
    switch (assistMask_ & 3) {
        case 1:  return "PILOT 1";
        case 2:  return "PILOT 2";
        case 3:  return "BOTH";
        default: return "OFF";
    }
}

void Game::DrawTitleRow(Row row, int y, const char* label, const char* value) const
{
    assert(label != nullptr && value != nullptr);
    const bool  sel = (row_ == row);
    const Color c   = sel ? GOLD : RAYWHITE;
    const int   x   = kTitleLabelX;
    DrawText(label, x, y, 24, c);
    DrawText(TextFormat("%s %s %s", sel ? "<" : " ", value, sel ? ">" : " "), x + 220, y, 24, c);
}

void Game::DrawButton(Rectangle r, const char* label, int size)
{
    assert(label != nullptr && size > 0);
    DrawRectangleRec(r, Fade(GOLD, 0.25f));
    DrawRectangleLinesEx(r, 2.0f, GOLD);
    DrawText(label, static_cast<int>(r.x + (r.width - static_cast<float>(MeasureText(label, size))) * 0.5f),
             static_cast<int>(r.y + (r.height - static_cast<float>(size)) * 0.5f), size, GOLD);
}

void Game::DrawTitle() const
{
    // Fly-by plane with its afterburner, then the panel.
    const float dy = 150.0f + 18.0f * std::sin(demoX_ * 0.02f);
    DrawEllipse(static_cast<int>(demoX_) + 10, static_cast<int>(dy) + 40, 14.0f, 9.0f, Fade(BLACK, 0.25f));
    Sprites::DrawIntoRotated(sprites_.Player(), demoX_, dy, cfg::kPlayerW * 1.5f, cfg::kPlayerH * 1.5f, 90.0f, WHITE);

    const int px = kTitlePx, py = TitlePy();
    DrawRectangle(px, py, kTitlePanelW, kTitlePanelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, kTitlePanelW, kTitlePanelH, GOLD);

    const char* title = "RIVER FLYER";
    DrawText(title, (cfg::kScreenW - MeasureText(title, 72)) / 2 + 3, py - 90 + 3, 72, Fade(BLACK, 0.6f));
    DrawText(title, (cfg::kScreenW - MeasureText(title, 72)) / 2, py - 90, 72, GOLD);

    int y = TitleRowY0();
    DrawTitleRow(Row::Players,    y, "PLAYERS",    twoPlayer_ ? "2" : "1");            y += kTitleRowH;
    DrawTitleRow(Row::PilotOne,   y, "PILOT 1",    profiles_.At(profileIdx_[0]));      y += kTitleRowH;
    if (twoPlayer_) { DrawTitleRow(Row::PilotTwo, y, "PILOT 2", profiles_.At(profileIdx_[1])); }
    y += kTitleRowH;
    DrawTitleRow(Row::Difficulty, y, "DIFFICULTY", Diff().name);                       y += kTitleRowH;
    DrawTitleRow(Row::Assist,     y, "ASSIST",     AssistLabel());                     y += 60;

    const int best1 = scores_.BestFor(profiles_.At(profileIdx_[0]));
    DrawText(TextFormat("%s's best: %06d", profiles_.At(profileIdx_[0]), best1), px + 30, y, 18, LIGHTGRAY); y += 40;

    if (touch::Enabled()) {
        DrawButton(PlayBtn(), "TAP TO FLY", 30);
        y = static_cast<int>(PlayBtn().y + PlayBtn().height) + 14;
        DrawText("Tap a row to change it, tap a name to rename it", px + 30, y, 16, LIGHTGRAY); y += 26;
#if defined(__ANDROID__)
        DrawText("Drag anywhere to steer, hold FIRE; Back pauses", px + 30, y, 16, LIGHTGRAY);
#else
        DrawText("Drag anywhere to steer, hold FIRE; top-right button pauses", px + 30, y, 16, LIGHTGRAY);
#endif
        return;
    }
    DrawText(TextFormat("SPACE / A: fly     Enter or A on a pilot: rename     %sM: music %s",
                        kCanQuit ? "Q: quit     " : "", audio_.MusicOn() ? "on" : "off"), px + 30, y, 16, LIGHTGRAY); y += 26;
    DrawText("P1: WASD + Space, C chaff, V jam    P2: arrows + RCtrl, / chaff, . jam", px + 30, y, 16, LIGHTGRAY); y += 26;
    int pads = 0;
    for (int i = 0; i < 2; ++i) { if (IsGamepadAvailable(i)) { ++pads; } }
    DrawText(TextFormat("Gamepads connected: %d", pads), px + 30, y, 16, pads > 0 ? LIME : GRAY);
}

void Game::DrawPaused() const
{
    assert(state_ == State::Paused);
    const int px = kPausePx, py = PausePy();
    DrawRectangle(px, py, kPausePanelW, PausePanelH(), Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, kPausePanelW, PausePanelH(), GOLD);
    const char* t = "PAUSED";
    DrawText(t, (cfg::kScreenW - MeasureText(t, 40)) / 2, py + 20, 40, GOLD);
    if (touch::Enabled()) {
        DrawButton(PauseFlyBtn(),   "FLY ON", 24);
        DrawButton(PauseTitleBtn(), "QUIT",   24);
        DrawButton(PauseMusicBtn(), TextFormat("MUSIC %s", audio_.MusicOn() ? "ON" : "OFF"), 20);
        return;
    }
    const char* h = "Esc: keep flying     Q: back to title";
    DrawText(h, (cfg::kScreenW - MeasureText(h, 18)) / 2, py + 85, 18, RAYWHITE);
}

void Game::DrawEnterName() const
{
    assert(state_ == State::EnterName);
    const int px = kNamePx, py = NamePy();
    DrawRectangle(px, py, kNamePanelW, kNamePanelH, Fade(BLACK, 0.85f));
    DrawRectangleLines(px, py, kNamePanelW, kNamePanelH, GOLD);
    const char* t = TextFormat("PILOT %d NAME", nameTarget_ + 1);
    DrawText(t, (cfg::kScreenW - MeasureText(t, 32)) / 2, py + 16, 32, GOLD);

    const int boxX = px + 30, boxY = py + 64;
    DrawRectangle(boxX, boxY, kNamePanelW - 60, 36, RAYWHITE);
    DrawText(name_.data(), boxX + 8, boxY + 6, 24, DARKBLUE);
    if (std::fmod(GetTime(), 1.0) < 0.5 && nameLen_ < cfg::kNameMax) {
        DrawRectangle(boxX + 8 + MeasureText(name_.data(), 24) + 2, boxY + 6, 3, 24, DARKBLUE);
    }

    DrawKeyboard(kNameKeysX, NameKeysY());
    if (touch::Enabled()) {
        DrawText("Tap the keys, then OK", px + 30, py + 328, 16, LIGHTGRAY);
        DrawText("Tap outside this panel to cancel", px + 30, py + 352, 16, LIGHTGRAY);
        return;
    }
    DrawText("Pad: A type   B erase   Y space   Start keep   Back cancel", px + 30, py + 328, 16, LIGHTGRAY);
    DrawText("Keys: type the name   Enter keep   Esc cancel", px + 30, py + 352, 16, LIGHTGRAY);
}

void Game::DrawKeyboard(int x, int y) const
{
    assert(state_ == State::EnterName);
    assert(keyRow_ >= 0 && keyRow_ < kKeyRows && keyCol_ >= 0 && keyCol_ < kKeyCols);
    const int   gap  = 4;
    const Color idle = Fade(RAYWHITE, 0.15f);

    for (int r = 0; r < kKeyRows - 1; ++r) {
        for (int c = 0; c < kKeyCols; ++c) {
            const bool hot = (r == keyRow_ && c == keyCol_);
            const int  kx  = x + c * kKeyCell, ky = y + r * kKeyCell;
            DrawRectangle(kx, ky, kKeyCell - gap, kKeyCell - gap, hot ? GOLD : idle);
            const char label[2] = {kKeyChars[r][c], '\0'};
            DrawText(label, kx + (kKeyCell - gap - MeasureText(label, 24)) / 2, ky + 6, 24, hot ? DARKBLUE : RAYWHITE);
        }
    }

    const char* wideLabels[3] = {"SPACE", "DEL", "OK"};
    const int   wy = y + (kKeyRows - 1) * kKeyCell;
    for (int w = 0; w < 3; ++w) {
        const bool hot = (keyRow_ == kKeyRows - 1 && static_cast<int>(WideKeyAt(keyCol_)) == w);
        const int  kx  = x + kWideFirstCol[w] * kKeyCell;
        const int  kw  = kWideCols[w] * kKeyCell - gap;
        DrawRectangle(kx, wy, kw, kKeyCell - gap, hot ? GOLD : idle);
        DrawText(wideLabels[w], kx + (kw - MeasureText(wideLabels[w], 20)) / 2, wy + 8, 20, hot ? DARKBLUE : RAYWHITE);
    }
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
    const int px = (cfg::kScreenW - panelW) / 2, py = (screen::H() - panelH) / 2;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.6f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawScoreTable(px + 30, py + 20);
}

void Game::DrawGameOver() const
{
    assert(state_ == State::GameOver);
    const int panelW = 400, panelH = 470;
    const int px = (cfg::kScreenW - panelW) / 2, py = (screen::H() - panelH) / 2;
    const char* msg1 = "Oh no! Out of planes!";
    const char* msg2 = touch::Enabled() ? "Tap: back to the title" : "SPACE: back to the title";
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawText(msg1, (cfg::kScreenW - MeasureText(msg1, 30)) / 2, py + 16, 30, GOLD);
    DrawText(TextFormat("Your score: %06d%s", finalScore_, (newRow_ >= 0) ? "  - new high score!" : ""), px + 30, py + 60, 18, RAYWHITE);
    DrawText(TextFormat("Stars %d  Bridges %d  SAMs %d  Missiles %d  Otters %d", stars_, bridges_, samKills_, missileKills_, otters_), px + 30, py + 80, 16, LIGHTGRAY);
    DrawScoreTable(px + 30, py + 100);
    DrawText(msg2, (cfg::kScreenW - MeasureText(msg2, 22)) / 2, py + panelH - 40, 22, RAYWHITE);
}
