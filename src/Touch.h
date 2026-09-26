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
//
// With two pilots (SetPlayers(2)) the window splits down the middle: pilot one
// owns the right half and the bottom-right buttons, pilot two the left half
// and a mirrored set, so two people can share a tablet. Each half has its own
// floating stick, and a finger belongs to whichever half it landed in.
namespace touch {

void SetEnabled(bool on, bool mouseIsFinger);
bool Enabled();

void Update();                  // once per frame, before Game::Update
void ShowControls(bool on);     // true while the game is being played
void SetPlayers(int count);     // 1 or 2; splits the screen when 2
int  Players();

// Flying. 'pilot' is 0 (right half) or 1 (left half); anything else reads as
// no input, so a gamepad-only pilot can pass its index straight through.
Vector2 Move(int pilot);        // each axis in [-1, 1]
bool    FireHeld(int pilot);
bool    ChaffPressed(int pilot);   // edge-triggered
bool    JamHeld(int pilot);
bool    PausePressed();            // edge-triggered, shared

// Menus: a finger landed this frame away from any button.
bool    Tapped();
Vector2 TapPos();               // in canvas coordinates; may lie outside the canvas

// Window-space overlay: call after Canvas::Present(), inside BeginDrawing().
void Draw();

} // namespace touch
