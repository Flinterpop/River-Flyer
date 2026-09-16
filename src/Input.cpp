#include "Input.h"

#include <cassert>

namespace {

constexpr int   kPads     = 2;      // gamepads we look at for menus
constexpr float kDeadZone = 0.25f;

bool PadDown(int pad, int button)
{
    return pad >= 0 && IsGamepadAvailable(pad) && IsGamepadButtonDown(pad, button);
}

bool AnyPadPressed(int button)
{
    for (int p = 0; p < kPads; ++p) {
        if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, button)) { return true; }
    }
    return false;
}

float PadAxis(int pad, int axis)
{
    if (pad < 0 || !IsGamepadAvailable(pad)) { return 0.0f; }
    const float v = GetGamepadAxisMovement(pad, axis);
    return (v > kDeadZone || v < -kDeadZone) ? v : 0.0f;
}

float Clamp1(float v)
{
    if (v < -1.0f) { return -1.0f; }
    if (v > 1.0f)  { return 1.0f; }
    return v;
}

} // namespace

namespace input {

InputMap PilotOne(bool solo)
{
    return InputMap {KEY_W, KEY_S, KEY_A, KEY_D, KEY_SPACE, 0, solo};
}

InputMap PilotTwo()
{
    return InputMap {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_RIGHT_CONTROL, 1, false};
}

Vector2 ReadMove(const InputMap& m)
{
    Vector2 dir {0.0f, 0.0f};
    if (IsKeyDown(m.left))  { dir.x -= 1.0f; }
    if (IsKeyDown(m.right)) { dir.x += 1.0f; }
    if (IsKeyDown(m.up))    { dir.y -= 1.0f; }
    if (IsKeyDown(m.down))  { dir.y += 1.0f; }
    if (m.alsoArrows) {
        if (IsKeyDown(KEY_LEFT))  { dir.x -= 1.0f; }
        if (IsKeyDown(KEY_RIGHT)) { dir.x += 1.0f; }
        if (IsKeyDown(KEY_UP))    { dir.y -= 1.0f; }
        if (IsKeyDown(KEY_DOWN))  { dir.y += 1.0f; }
    }
    // Gamepad: left stick or d-pad.
    dir.x += PadAxis(m.pad, GAMEPAD_AXIS_LEFT_X);
    dir.y += PadAxis(m.pad, GAMEPAD_AXIS_LEFT_Y);
    if (PadDown(m.pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT))  { dir.x -= 1.0f; }
    if (PadDown(m.pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) { dir.x += 1.0f; }
    if (PadDown(m.pad, GAMEPAD_BUTTON_LEFT_FACE_UP))    { dir.y -= 1.0f; }
    if (PadDown(m.pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN))  { dir.y += 1.0f; }
    dir.x = Clamp1(dir.x);
    dir.y = Clamp1(dir.y);
    assert(dir.x >= -1.0f && dir.x <= 1.0f && dir.y >= -1.0f && dir.y <= 1.0f);
    return dir;
}

bool FireHeld(const InputMap& m)
{
    if (IsKeyDown(m.fire)) { return true; }
    if (m.fire == KEY_RIGHT_CONTROL && IsKeyDown(KEY_RIGHT_SHIFT)) { return true; }   // either works for pilot 2
    return PadDown(m.pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || PadDown(m.pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
}

bool MenuUp()      { return IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W) || AnyPadPressed(GAMEPAD_BUTTON_LEFT_FACE_UP); }
bool MenuDown()    { return IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S) || AnyPadPressed(GAMEPAD_BUTTON_LEFT_FACE_DOWN); }
bool MenuLeft()    { return IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A) || AnyPadPressed(GAMEPAD_BUTTON_LEFT_FACE_LEFT); }
bool MenuRight()   { return IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || AnyPadPressed(GAMEPAD_BUTTON_LEFT_FACE_RIGHT); }
bool MenuConfirm() { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || AnyPadPressed(GAMEPAD_BUTTON_RIGHT_FACE_DOWN); }
bool MenuBack()    { return IsKeyPressed(KEY_ESCAPE) || AnyPadPressed(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT); }
bool PausePressed(){ return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || AnyPadPressed(GAMEPAD_BUTTON_MIDDLE_RIGHT); }

} // namespace input
