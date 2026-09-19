#pragma once

#include "raylib.h"

// Touch controls for phones and tablets. Off unless enabled: on in the Android
// build, on in the browser build when the device has a touchscreen, and
// switchable on the desktop with --touch so the layer can be tried with the
// mouse standing in for a finger. That stand-in is desktop-only: browsers
// synthesise a mouse click after every tap, which would count twice.
//
// Two modes, chosen by ShowControls(): while flying, the screen is a floating
// stick (drag anywhere to steer) plus FIRE / CHAFF / JAM / pause buttons drawn
// in window space, so on a tall phone they sit in the black bars around the
// canvas. In the menus the buttons are hidden and every new finger is a tap,
// reported in canvas coordinates for the panels to hit-test.
namespace touch {

void SetEnabled(bool on, bool mouseIsFinger);
bool Enabled();

void Update();                  // once per frame, before Game::Update
void ShowControls(bool on);     // true while the game is being played

// Flying (pilot one).
Vector2 Move();                 // each axis in [-1, 1]
bool    FireHeld();
bool    ChaffPressed();         // edge-triggered
bool    JamHeld();
bool    PausePressed();         // edge-triggered

// Menus: a finger landed this frame away from any button.
bool    Tapped();
Vector2 TapPos();               // in canvas coordinates; may lie outside the canvas
bool    TappedIn(Rectangle canvasRect);

// Window-space overlay: call after Canvas::Present(), inside BeginDrawing().
void Draw();

} // namespace touch
