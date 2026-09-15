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
    state_        = State::Playing;
    assert(!terrain_.HitsBank(player_.Bounds()));   // must spawn in open water
    assert(lives_ > 0 && fuel_ > 0.0f);
}

void Game::Update(float dt)
{
    assert(dt >= 0.0f);
    switch (state_) {
        case State::Playing:  UpdatePlaying(dt);  break;
        case State::Crashing: UpdateCrashing(dt); break;
        case State::GameOver: UpdateGameOver(dt); break;
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
        state_ = State::GameOver;
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
    if (state_ == State::GameOver) { return false; }   // the plane is gone
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
