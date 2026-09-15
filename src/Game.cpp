#include "Game.h"

#include <cassert>

#include "Config.h"

Game::Game()
{
    Restart();
}

void Game::Restart()
{
    player_.Reset();
    terrain_.Reset();
    state_ = State::Playing;
    assert(!terrain_.Collides(player_.Bounds()));   // must spawn in open water
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
    if (terrain_.Collides(player_.Bounds())) {
        state_ = State::GameOver;
    }
}

void Game::UpdateGameOver()
{
    assert(state_ == State::GameOver);
    if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER)) {
        Restart();
    }
}

void Game::Draw() const
{
    terrain_.Draw();
    player_.Draw();
    DrawHud();
}

void Game::DrawHud() const
{
    DrawText(TextFormat("DIST %06d", static_cast<int>(terrain_.Distance())), 10, 10, 20, RAYWHITE);
    DrawFPS(cfg::kScreenW - 90, 10);

    if (state_ == State::GameOver) {
        const char* msg  = "CRASHED - press R to restart";
        const int   size = 24;
        const int   w    = MeasureText(msg, size);
        assert(w > 0 && w < cfg::kScreenW);
        DrawRectangle(0, cfg::kScreenH / 2 - 30, cfg::kScreenW, 60, Fade(BLACK, 0.6f));
        DrawText(msg, (cfg::kScreenW - w) / 2, cfg::kScreenH / 2 - size / 2, size, RED);
    }
}
