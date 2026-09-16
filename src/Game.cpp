#include "Game.h"

#include <cassert>
#include <cmath>

#include "Config.h"

namespace {

Vector2 RectCentre(const Rectangle& r)
{
    assert(r.width > 0.0f && r.height > 0.0f);
    return Vector2 {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

} // namespace

Game::Game()
{
    scores_.Load();
    Restart();
}

void Game::Restart()
{
    player_.Reset();
    bullets_.Reset();
    terrain_.Reset();
    effects_.Reset();
    lives_        = cfg::kLives;
    kills_        = 0;
    fuel_         = cfg::kFuelMax;
    fireCooldown_ = 0.0f;
    grace_        = 0.0f;
    finalScore_   = 0;
    newRow_       = -1;
    state_        = State::Playing;
    assert(!terrain_.HitsBank(player_.Bounds()));   // must spawn in open water
    assert(lives_ > 0 && fuel_ > 0.0f);
}

void Game::Update(float dt)
{
    assert(dt >= 0.0f);
    switch (state_) {
        case State::Playing:  UpdatePlaying(dt);  break;
        case State::Crashing:  UpdateCrashing(dt);  break;
        case State::EnterName: UpdateEnterName(dt); break;
        case State::GameOver:  UpdateGameOver(dt);  break;
    }
}

void Game::UpdatePlaying(float dt)
{
    assert(state_ == State::Playing);
    player_.Update(dt);
    // The drag chute holds the plane back: the river passes more slowly.
    const float riverDt = player_.Braking() ? dt * cfg::kBrakeScrollMult : dt;
    terrain_.Update(riverDt);
    if (player_.Braking()) { audio_.Sustain(Audio::Sfx::Brake); }
    bullets_.Update(dt);
    effects_.Update(dt);
    UpdateFiring(dt);
    ResolveBulletHits();

    if (grace_ > 0.0f) {
        grace_ -= dt;
    } else {
        CheckPlayerCrash();
    }
    if (state_ == State::Playing && UpdateFuel(dt)) {
        audio_.Play(Audio::Sfx::Whine);
        BeginCrash(Player::CrashStyle::Spiral);   // out of fuel: glide down in a spiral
    }
    assert(lives_ > 0);
}

void Game::UpdateCrashing(float dt)
{
    assert(state_ == State::Crashing);
    // The river keeps flowing and shots keep travelling; only the plane is scripted.
    terrain_.Update(dt);
    bullets_.Update(dt);
    effects_.Update(dt);
    ResolveBulletHits();
    player_.UpdateCrash(dt);

    if (player_.CrashFinished()) {
        const bool roll = (player_.Style() == Player::CrashStyle::Roll);
        effects_.Spawn(player_.Centre(), roll ? Effects::Style::Splash : Effects::Style::Plane);
        audio_.Play(roll ? Audio::Sfx::Splash : Audio::Sfx::Crunch);
        LoseLife();
    }
    assert(state_ != State::Crashing || player_.Crashing());
}

void Game::UpdateGameOver(float dt)
{
    assert(state_ == State::GameOver);
    effects_.Update(dt);   // let the final explosion finish
    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        Restart();
    }
}

void Game::FinishGame()
{
    assert(state_ == State::Crashing && lives_ == 0);
    finalScore_ = Score();
    if (scores_.Qualifies(finalScore_)) {
        // Pre-fill with the last name used so a returning player just hits Enter.
        nameLen_ = 0;
        for (const char* p = scores_.LastName(); *p != '\0' && nameLen_ < cfg::kNameMax; ++p) {
            name_[static_cast<size_t>(nameLen_)] = *p;
            ++nameLen_;
        }
        name_[static_cast<size_t>(nameLen_)] = '\0';
        audio_.Play(Audio::Sfx::Fanfare);
        state_ = State::EnterName;
    } else {
        state_ = State::GameOver;
    }
    assert(nameLen_ >= 0 && nameLen_ <= cfg::kNameMax);
}

void Game::UpdateEnterName(float dt)
{
    assert(state_ == State::EnterName);
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
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && nameLen_ > 0) {
        --nameLen_;
        name_[static_cast<size_t>(nameLen_)] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER) && nameLen_ > 0) {
        CommitName();
    }
    assert(nameLen_ >= 0 && nameLen_ <= cfg::kNameMax);
}

void Game::CommitName()
{
    assert(state_ == State::EnterName && nameLen_ > 0);
    // Trim trailing spaces so " " alone cannot be a name.
    while (nameLen_ > 0 && name_[static_cast<size_t>(nameLen_ - 1)] == ' ') {
        --nameLen_;
        name_[static_cast<size_t>(nameLen_)] = '\0';
    }
    if (nameLen_ == 0) { return; }
    newRow_ = scores_.Add(name_.data(), finalScore_);
    state_  = State::GameOver;
    assert(newRow_ >= 0 && newRow_ < scores_.Count());
}

void Game::UpdateFiring(float dt)
{
    assert(dt >= 0.0f);
    fireCooldown_ -= dt;
    if (fireCooldown_ > 0.0f) { return; }
    if (IsKeyDown(KEY_SPACE)) {
        bullets_.Fire(player_.Muzzle());
        audio_.Play(Audio::Sfx::Shoot);
        fireCooldown_ = cfg::kFireCooldown;
    }
    assert(fireCooldown_ <= cfg::kFireCooldown);
}

bool Game::UpdateFuel(float dt)
{
    assert(dt >= 0.0f);
    fuel_ -= cfg::kFuelBurnPerSec * dt;

    // Flying over a depot refuels without destroying it.
    const int hit = terrain_.FindObstacle(player_.Bounds());
    if (hit >= 0 && terrain_.ObstacleAt(hit).kind == Terrain::Kind::Fuel) {
        if (fuel_ < cfg::kFuelMax) { audio_.Sustain(Audio::Sfx::Slurp); }   // only while actually taking fuel on
        fuel_ += cfg::kFuelRefillPerSec * dt;
    }
    if (fuel_ > cfg::kFuelMax) { fuel_ = cfg::kFuelMax; }
    if (fuel_ < 0.0f)          { fuel_ = 0.0f; }

    assert(fuel_ >= 0.0f && fuel_ <= cfg::kFuelMax);
    return fuel_ <= 0.0f;
}

void Game::ResolveBulletHits()
{
    // Bounded: kMaxBullets x kMaxObstacles checks at most.
    for (int b = 0; b < Bullets::Capacity(); ++b) {
        if (!bullets_.Active(b)) { continue; }
        const int hit = terrain_.FindObstacle(bullets_.Bounds(b));
        if (hit < 0) { continue; }

        const Terrain::Obstacle& o = terrain_.ObstacleAt(hit);
        const Effects::Style style = (o.kind == Terrain::Kind::Rock) ? Effects::Style::Rock
                                                                     : Effects::Style::Fuel;
        effects_.Spawn(RectCentre(o.rect), style);
        audio_.Play(Audio::Sfx::Pop);
        terrain_.RemoveObstacle(hit);
        bullets_.Kill(b);
        ++kills_;
    }
    assert(kills_ >= 0);
}

void Game::CheckPlayerCrash()
{
    assert(grace_ <= 0.0f && state_ == State::Playing);
    const Rectangle box = player_.Bounds();
    const int       hit = terrain_.FindObstacle(box);
    const bool rock = (hit >= 0) && (terrain_.ObstacleAt(hit).kind == Terrain::Kind::Rock);

    if (rock) {
        effects_.Spawn(RectCentre(terrain_.ObstacleAt(hit).rect), Effects::Style::Rock);
        terrain_.RemoveObstacle(hit);
        audio_.Play(Audio::Sfx::Crunch);
        BeginCrash(Player::CrashStyle::Roll);
    } else if (terrain_.HitsBank(box)) {
        audio_.Play(Audio::Sfx::Whine);
        BeginCrash(Player::CrashStyle::Spiral);
    }
}

void Game::BeginCrash(Player::CrashStyle style)
{
    assert(state_ == State::Playing);
    player_.BeginCrash(style);
    state_ = State::Crashing;
    assert(player_.Crashing());
}

void Game::LoseLife()
{
    assert(lives_ > 0 && state_ == State::Crashing);
    --lives_;
    if (lives_ == 0) {
        FinishGame();
        return;
    }
    // Fresh plane at the start position, full tank, untouchable for a moment.
    player_.Reset();
    fuel_  = cfg::kFuelMax;
    grace_ = cfg::kGraceSeconds;
    state_ = State::Playing;
    assert(grace_ > 0.0f && !player_.Crashing());
}

bool Game::PlayerVisible() const
{
    if (state_ == State::GameOver || state_ == State::EnterName) { return false; }   // the plane is gone
    if (grace_ <= 0.0f) { return true; }
    // Blink at kBlinkHz while in the grace period.
    const float phase = std::fmod(grace_ * cfg::kBlinkHz, 1.0f);
    return phase < 0.5f;
}

int Game::Score() const
{
    const int score = static_cast<int>(terrain_.Distance() * cfg::kPointsPerPx) + kills_ * cfg::kPointsPerKill;
    assert(score >= 0);
    return score;
}

void Game::Draw() const
{
    terrain_.Draw(sprites_);
    bullets_.Draw(sprites_.Bullet());
    player_.Draw(sprites_.Player(), PlayerVisible());
    effects_.Draw();
    DrawHud();
}

void Game::DrawHud() const
{
    DrawHudText(TextFormat("SCORE %06d", Score()), 10, 10, 24);
    DrawFuelBar();
    DrawLives();
    {
        const char* best = TextFormat("BEST %06d", scores_.Best());
        DrawHudText(best, (cfg::kScreenW - MeasureText(best, 16)) / 2, 14, 16);
    }
    if (state_ == State::EnterName) { DrawEnterName(); }
    if (state_ == State::GameOver)  { DrawGameOver(); }
    // Hold Tab during play to peek at the table (the game keeps running).
    if ((state_ == State::Playing || state_ == State::Crashing) && IsKeyDown(KEY_TAB)) { DrawPeekTable(); }
}

void Game::DrawPeekTable() const
{
    assert(state_ == State::Playing || state_ == State::Crashing);
    const int panelW = 400;
    const int panelH = 340;
    const int px     = (cfg::kScreenW - panelW) / 2;
    const int py     = (cfg::kScreenH - panelH) / 2;
    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.6f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawScoreTable(px + 30, py + 20);
}

void Game::DrawHudText(const char* text, int x, int y, int size)
{
    assert(text != nullptr && size > 0);
    DrawText(text, x + 2, y + 2, size, Fade(BLACK, 0.6f));   // shadow keeps it legible on grass
    DrawText(text, x, y, size, RAYWHITE);
}

void Game::DrawFuelBar() const
{
    assert(fuel_ >= 0.0f && fuel_ <= cfg::kFuelMax);
    const int x    = 10;
    const int y    = 42;
    const int fill = static_cast<int>(static_cast<float>(cfg::kFuelBarW) * (fuel_ / cfg::kFuelMax));
    const Color c  = (fuel_ < cfg::kFuelMax * 0.25f) ? RED : GREEN;

    DrawHudText("FUEL", x, y - 2, 16);
    DrawRectangle(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, Fade(BLACK, 0.3f));
    DrawRectangle(x + 50, y, fill, cfg::kFuelBarH, c);
    DrawRectangleLines(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, RAYWHITE);
}

void Game::DrawLives() const
{
    assert(lives_ >= 0 && lives_ <= cfg::kLives);
    const int spacing = 36;
    for (int i = 0; i < lives_; ++i) {
        Sprites::DrawInto(sprites_.Player(), static_cast<float>(cfg::kScreenW - spacing * (i + 1)), 10.0f, cfg::kPlayerW, cfg::kPlayerH, WHITE);
    }
}

void Game::DrawScoreTable(int x, int y) const
{
    assert(x >= 0 && y >= 0);
    const int rowH = 26;
    DrawText("TOP PILOTS", x, y, 20, RAYWHITE);
    y += rowH + 4;
    for (int i = 0; i < cfg::kHighScoreCount; ++i) {
        const int   ry   = y + i * rowH;
        const Color c    = (i == newRow_) ? GOLD : ((i < scores_.Count()) ? RAYWHITE : GRAY);
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

void Game::DrawGameOver() const
{
    assert(state_ == State::GameOver);
    const int   panelW = 400;
    const int   panelH = 460;
    const int   px     = (cfg::kScreenW - panelW) / 2;
    const int   py     = (cfg::kScreenH - panelH) / 2;
    const char* msg1   = "Oh no! Out of planes!";
    const char* msg2   = "Press SPACE to fly again";
    const int   w1     = MeasureText(msg1, 30);
    const int   w2     = MeasureText(msg2, 22);
    assert(w1 > 0 && w1 < panelW && w2 > 0 && w2 < panelW);

    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawText(msg1, (cfg::kScreenW - w1) / 2, py + 16, 30, GOLD);
    DrawText(TextFormat("Your score: %06d", finalScore_), px + 30, py + 60, 20, RAYWHITE);
    DrawScoreTable(px + 30, py + 100);
    DrawText(msg2, (cfg::kScreenW - w2) / 2, py + panelH - 40, 22, RAYWHITE);
}

void Game::DrawEnterName() const
{
    assert(state_ == State::EnterName);
    const int   panelW = 440;
    const int   panelH = 200;
    const int   px     = (cfg::kScreenW - panelW) / 2;
    const int   py     = (cfg::kScreenH - panelH) / 2;
    const char* msg1   = "NEW HIGH SCORE!";
    const int   w1     = MeasureText(msg1, 32);
    assert(w1 > 0 && w1 < panelW);

    DrawRectangle(px, py, panelW, panelH, Fade(BLACK, 0.7f));
    DrawRectangleLines(px, py, panelW, panelH, GOLD);
    DrawText(msg1, (cfg::kScreenW - w1) / 2, py + 16, 32, GOLD);
    DrawText(TextFormat("%06d - type your name:", finalScore_), px + 30, py + 62, 20, RAYWHITE);

    // Text box with a blinking cursor.
    const int boxX = px + 30;
    const int boxY = py + 92;
    DrawRectangle(boxX, boxY, panelW - 60, 36, RAYWHITE);
    DrawText(name_.data(), boxX + 8, boxY + 6, 24, DARKBLUE);
    const bool blink = std::fmod(GetTime(), 1.0) < 0.5;
    if (blink && nameLen_ < cfg::kNameMax) {
        const int cx = boxX + 8 + MeasureText(name_.data(), 24) + 2;
        DrawRectangle(cx, boxY + 6, 3, 24, DARKBLUE);
    }
    DrawText("Enter when done", px + 30, py + 144, 18, LIGHTGRAY);
}
