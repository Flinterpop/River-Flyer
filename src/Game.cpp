#include "Game.h"

#include <cassert>
#include <cmath>

#include "Config.h"

Game::Game()
{
    Restart();
}

void Game::Restart()
{
    player_.Reset();
    bullets_.Reset();
    terrain_.Reset();
    lives_        = cfg::kLives;
    kills_        = 0;
    fuel_         = cfg::kFuelMax;
    fireCooldown_ = 0.0f;
    grace_        = 0.0f;
    state_        = State::Playing;
    assert(!terrain_.HitsBank(player_.Bounds()));   // must spawn in open water
    assert(lives_ > 0 && fuel_ > 0.0f);
}

void Game::Update(float dt)
{
    assert(dt >= 0.0f);
    switch (state_) {
        case State::Playing:  UpdatePlaying(dt); break;
        case State::GameOver: UpdateGameOver();  break;
    }
}

void Game::UpdatePlaying(float dt)
{
    assert(state_ == State::Playing);
    terrain_.Update(dt);
    player_.Update(dt);
    bullets_.Update(dt);
    UpdateFiring(dt);
    UpdateFuel(dt);
    ResolveBulletHits();

    if (grace_ > 0.0f) {
        grace_ -= dt;
    } else {
        ResolvePlayerHits();
    }
    assert(lives_ >= 0);
}

void Game::UpdateGameOver()
{
    assert(state_ == State::GameOver);
    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        Restart();
    }
}

void Game::UpdateFiring(float dt)
{
    assert(dt >= 0.0f);
    fireCooldown_ -= dt;
    if (fireCooldown_ > 0.0f) { return; }
    if (IsKeyDown(KEY_SPACE)) {
        bullets_.Fire(player_.Muzzle());
        fireCooldown_ = cfg::kFireCooldown;
    }
    assert(fireCooldown_ <= cfg::kFireCooldown);
}

void Game::UpdateFuel(float dt)
{
    assert(dt >= 0.0f);
    fuel_ -= cfg::kFuelBurnPerSec * dt;

    // Flying over a depot refuels without destroying it.
    const int hit = terrain_.FindObstacle(player_.Bounds());
    if (hit >= 0 && terrain_.ObstacleAt(hit).kind == Terrain::Kind::Fuel) {
        fuel_ += cfg::kFuelRefillPerSec * dt;
    }
    if (fuel_ > cfg::kFuelMax) { fuel_ = cfg::kFuelMax; }

    if (fuel_ <= 0.0f) {
        LoseLife();
        fuel_ = cfg::kFuelMax;   // the new craft arrives with a full tank
    }
    assert(fuel_ > 0.0f && fuel_ <= cfg::kFuelMax);
}

void Game::ResolveBulletHits()
{
    // Bounded: kMaxBullets x kMaxObstacles checks at most.
    for (int b = 0; b < Bullets::Capacity(); ++b) {
        if (!bullets_.Active(b)) { continue; }
        const int hit = terrain_.FindObstacle(bullets_.Bounds(b));
        if (hit < 0) { continue; }
        terrain_.RemoveObstacle(hit);
        bullets_.Kill(b);
        ++kills_;
    }
    assert(kills_ >= 0);
}

void Game::ResolvePlayerHits()
{
    assert(grace_ <= 0.0f);
    const Rectangle box = player_.Bounds();
    const int       hit = terrain_.FindObstacle(box);
    const bool rock = (hit >= 0) && (terrain_.ObstacleAt(hit).kind == Terrain::Kind::Rock);
    if (rock || terrain_.HitsBank(box)) {
        if (rock) { terrain_.RemoveObstacle(hit); }
        LoseLife();
    }
}

void Game::LoseLife()
{
    assert(lives_ > 0);
    --lives_;
    if (lives_ == 0) {
        state_ = State::GameOver;
        return;
    }
    // Respawn in place but untouchable for a moment; the river keeps flowing
    // so the player does not get stuck against a bank forever.
    player_.Reset();
    grace_ = cfg::kGraceSeconds;
    assert(grace_ > 0.0f);
}

bool Game::PlayerVisible() const
{
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
    DrawHud();
}

void Game::DrawHud() const
{
    DrawText(TextFormat("SCORE %06d", Score()), 10, 10, 24, DARKBLUE);
    DrawFuelBar();
    DrawLives();
    if (state_ == State::GameOver) { DrawGameOver(); }
}

void Game::DrawFuelBar() const
{
    assert(fuel_ >= 0.0f && fuel_ <= cfg::kFuelMax);
    const int x    = 10;
    const int y    = 42;
    const int fill = static_cast<int>(static_cast<float>(cfg::kFuelBarW) * (fuel_ / cfg::kFuelMax));
    const Color c  = (fuel_ < cfg::kFuelMax * 0.25f) ? RED : GREEN;

    DrawText("FUEL", x, y - 2, 16, DARKBLUE);
    DrawRectangle(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, Fade(BLACK, 0.3f));
    DrawRectangle(x + 50, y, fill, cfg::kFuelBarH, c);
    DrawRectangleLines(x + 50, y, cfg::kFuelBarW, cfg::kFuelBarH, DARKBLUE);
}

void Game::DrawLives() const
{
    assert(lives_ >= 0 && lives_ <= cfg::kLives);
    const int spacing = 36;
    for (int i = 0; i < lives_; ++i) {
        DrawTexture(sprites_.Player(), cfg::kScreenW - spacing * (i + 1), 10, WHITE);
    }
}

void Game::DrawGameOver() const
{
    assert(state_ == State::GameOver);
    const char* msg1 = "Oh no! Out of planes!";
    const char* msg2 = "Press SPACE to fly again";
    const int   w1   = MeasureText(msg1, 32);
    const int   w2   = MeasureText(msg2, 24);
    assert(w1 > 0 && w1 < cfg::kScreenW && w2 > 0 && w2 < cfg::kScreenW);

    DrawRectangle(0, cfg::kScreenH / 2 - 50, cfg::kScreenW, 100, Fade(BLACK, 0.6f));
    DrawText(msg1, (cfg::kScreenW - w1) / 2, cfg::kScreenH / 2 - 40, 32, GOLD);
    DrawText(msg2, (cfg::kScreenW - w2) / 2, cfg::kScreenH / 2 + 8,  24, RAYWHITE);
}
