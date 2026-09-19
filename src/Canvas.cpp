#include "Canvas.h"

#include <cassert>

#include "Config.h"

Canvas::Canvas()
{
    assert(IsWindowReady());
    target_ = LoadRenderTexture(cfg::kScreenW, cfg::kScreenH);
    assert(IsRenderTextureValid(target_));
    SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);   // smooth when scaled to a TV
}

Canvas::~Canvas()
{
    if (IsRenderTextureValid(target_)) { UnloadRenderTexture(target_); }
}

void Canvas::Begin()
{
    assert(IsRenderTextureValid(target_));
    BeginTextureMode(target_);
}

void Canvas::End()
{
    EndTextureMode();
}

Rectangle Canvas::Placement()
{
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());
    assert(sw > 0.0f && sh > 0.0f);

    // Largest scale that keeps the whole canvas visible, then centre it.
    const float sx    = sw / static_cast<float>(cfg::kScreenW);
    const float sy    = sh / static_cast<float>(cfg::kScreenH);
    const float scale = (sx < sy) ? sx : sy;
    const float w     = static_cast<float>(cfg::kScreenW) * scale;
    const float h     = static_cast<float>(cfg::kScreenH) * scale;
    assert(w <= sw + 0.5f && h <= sh + 0.5f);
    return Rectangle {(sw - w) * 0.5f, (sh - h) * 0.5f, w, h};
}

void Canvas::Present() const
{
    assert(IsRenderTextureValid(target_));
    // Render textures are stored upside down: a negative source height flips them back.
    const Rectangle src {0.0f, 0.0f, static_cast<float>(cfg::kScreenW), -static_cast<float>(cfg::kScreenH)};
    const Rectangle dst = Placement();
    assert(dst.width > 0.0f && dst.height > 0.0f);
    DrawTexturePro(target_.texture, src, dst, Vector2 {0.0f, 0.0f}, 0.0f, WHITE);
}
