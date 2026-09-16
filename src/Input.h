#pragma once

#include "raylib.h"

// Keyboard + gamepad bindings for one pilot, and shared menu navigation.
struct InputMap {
    int  up, down, left, right, fire;   // raylib KEY_* codes
    int  chaff, jam;
    int  pad;                            // gamepad index, or -1
    bool alsoArrows;                     // accept the arrow keys as well (single-player convenience)
};

namespace input {

InputMap PilotOne(bool solo);   // WASD + Space (+ arrows when solo), gamepad 0
InputMap PilotTwo();            // arrows + Right Ctrl / Right Shift, gamepad 1

Vector2 ReadMove(const InputMap& m);       // each axis in [-1, 1]
bool    FireHeld(const InputMap& m);
bool    ChaffPressed(const InputMap& m);
bool    JamHeld(const InputMap& m);

// Menu navigation from any keyboard or gamepad (edge-triggered).
bool MenuUp();
bool MenuDown();
bool MenuLeft();
bool MenuRight();
bool MenuConfirm();   // Enter / Space / pad A
bool MenuBack();      // Escape / pad B
bool PausePressed();  // Escape / pad Start

} // namespace input
