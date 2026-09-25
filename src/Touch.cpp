#include "Touch.h"

#include <cassert>
#include <cmath>

#include "Canvas.h"
#include "Config.h"

namespace {

// Layout is in units of u = the shorter window side / kUnitDiv, so the buttons
// are the same size under a thumb on a phone and on a tablet.
constexpr int   kMaxPoints   = 8;       // raylib tracks at most this many fingers
constexpr float kUnitDiv     = 12.0f;
constexpr float kStickRadius = 0.9f;    // u: finger travel for full deflection
constexpr float kStickDead   = 0.1f;    // u: ignore jitter this close to the origin
constexpr float kFireR       = 1.25f;
constexpr float kSmallR      = 0.8f;
constexpr float kPauseR      = 0.55f;
constexpr float kAlpha       = 0.35f;
constexpr float kHeldAlpha   = 0.7f;

struct Point  { int id; Vector2 pos; };
struct Frame  { Point pts[kMaxPoints]; int n; };
struct Circle { Vector2 c; float r; };

constexpr int kMaxPilots = 2;

bool    enabled  = false;
bool    mouseToo = false;   // desktop only: the mouse is finger 0
bool    controls = false;
int     players  = 1;
Frame   cur {}, prev {};

// One set of controls per pilot: pilot 0 on the right, pilot 1 on the left.
struct PilotTouch {
    bool    stickOn {false};
    int     stickId {-1};
    Vector2 stickOrigin {0.0f, 0.0f};
    Vector2 stickPos    {0.0f, 0.0f};
    Vector2 move        {0.0f, 0.0f};
    bool    fire {false}, jam {false};
    bool    chaff {false}, chaffWas {false};
};
PilotTouch pilots[kMaxPilots] {};

bool pause = false, pauseWas = false;

bool    tapped = false;
Vector2 tapPos {0.0f, 0.0f};

float Unit()
{
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    assert(w > 0.0f && h > 0.0f);
    return ((w < h) ? w : h) / kUnitDiv;
}

// Pilot 0's buttons hug the bottom-right corner; pilot 1's are mirrored into
// the bottom-left so each player's thumbs stay in their own half.
float SideX(int pilot, float fromEdge)
{
    const float w = static_cast<float>(GetScreenWidth());
    return (pilot == 0) ? (w - fromEdge) : fromEdge;
}

Circle FireBtn(int pilot)  { const float u = Unit(); return Circle {{SideX(pilot, 1.7f * u), static_cast<float>(GetScreenHeight()) - 1.7f * u}, kFireR * u}; }
Circle JamBtn(int pilot)   { const float u = Unit(); return Circle {{SideX(pilot, 1.7f * u), static_cast<float>(GetScreenHeight()) - 4.3f * u}, kSmallR * u}; }
Circle ChaffBtn(int pilot) { const float u = Unit(); return Circle {{SideX(pilot, 4.3f * u), static_cast<float>(GetScreenHeight()) - 1.5f * u}, kSmallR * u}; }
Circle PauseBtn()          { const float u = Unit(); return Circle {{static_cast<float>(GetScreenWidth()) - 0.9f * u, 0.9f * u}, kPauseR * u}; }

// Which pilot a finger at 'x' steers: the right half is pilot one, the left
// half pilot two. With one player the whole window is theirs.
int PilotAt(float x)
{
    if (players < 2) { return 0; }
    return (x >= static_cast<float>(GetScreenWidth()) * 0.5f) ? 0 : 1;
}

bool Inside(Vector2 p, const Circle& c)
{
    const float dx = p.x - c.c.x, dy = p.y - c.c.y;
    return dx * dx + dy * dy <= c.r * c.r;
}

bool OnAnyButton(Vector2 p)
{
    if (!controls) { return false; }
    if (Inside(p, PauseBtn())) { return true; }
    for (int i = 0; i < players; ++i) {
        if (Inside(p, FireBtn(i)) || Inside(p, JamBtn(i)) || Inside(p, ChaffBtn(i))) { return true; }
    }
    return false;
}

bool AnyInside(const Frame& f, const Circle& c)
{
    assert(f.n >= 0 && f.n <= kMaxPoints);
    for (int i = 0; i < f.n; ++i) { if (Inside(f.pts[i].pos, c)) { return true; } }
    return false;
}

bool Find(const Frame& f, int id, Vector2& pos)
{
    assert(f.n >= 0 && f.n <= kMaxPoints);
    for (int i = 0; i < f.n; ++i) {
        if (f.pts[i].id == id) { pos = f.pts[i].pos; return true; }
    }
    return false;
}

void ReadPoints(Frame& f)
{
    f.n = 0;
    const int n = GetTouchPointCount();
    for (int i = 0; i < n && i < kMaxPoints; ++i) {
        f.pts[f.n] = Point {GetTouchPointId(i), GetTouchPosition(i)};
        ++f.n;
    }
    if (f.n == 0 && mouseToo && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        f.pts[0] = Point {0, GetMousePosition()};
        f.n = 1;
    }
    assert(f.n >= 0 && f.n <= kMaxPoints);
}

Vector2 ToCanvas(Vector2 window)
{
    const Rectangle place = Canvas::Placement();
    assert(place.width > 0.0f && place.height > 0.0f);
    const float scale = place.width / static_cast<float>(cfg::kScreenW);
    return Vector2 {(window.x - place.x) / scale, (window.y - place.y) / scale};
}

float Deflect(float d, float radius)
{
    assert(radius > 0.0f);
    if (std::fabs(d) < kStickDead * Unit()) { return 0.0f; }
    const float v = d / radius;
    return (v < -1.0f) ? -1.0f : ((v > 1.0f) ? 1.0f : v);
}

void UpdateStick(PilotTouch& p)
{
    if (p.stickOn && !Find(cur, p.stickId, p.stickPos)) { p.stickOn = false; }
    if (!p.stickOn) { p.move = Vector2 {0.0f, 0.0f}; return; }

    // The base trails the finger: past full deflection it is dragged along,
    // so reversing direction responds at once instead of after a long swipe back.
    const float r   = kStickRadius * Unit();
    Vector2     d   = {p.stickPos.x - p.stickOrigin.x, p.stickPos.y - p.stickOrigin.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > r) {
        const float k = 1.0f - r / len;
        p.stickOrigin.x += d.x * k;
        p.stickOrigin.y += d.y * k;
        d.x *= r / len;
        d.y *= r / len;
    }
    p.move = Vector2 {Deflect(d.x, r), Deflect(d.y, r)};
    assert(p.move.x >= -1.0f && p.move.x <= 1.0f && p.move.y >= -1.0f && p.move.y <= 1.0f);
}

void DrawButton(const Circle& c, const char* label, bool held, Color tint)
{
    assert(label != nullptr);
    DrawCircleV(c.c, c.r, Fade(tint, held ? kHeldAlpha : kAlpha));
    DrawCircleLinesV(c.c, c.r, Fade(RAYWHITE, 0.8f));
    const int size = static_cast<int>(c.r * 0.45f);
    DrawText(label, static_cast<int>(c.c.x) - MeasureText(label, size) / 2, static_cast<int>(c.c.y) - size / 2, size, RAYWHITE);
}

} // namespace

namespace touch {

void SetEnabled(bool on, bool mouseIsFinger) { enabled = on; mouseToo = on && mouseIsFinger; }
bool Enabled()           { return enabled; }
void ShowControls(bool on) { controls = on; }

void SetPlayers(int count)
{
    assert(count == 1 || count == 2);
    if (count == players) { return; }
    players = count;
    for (PilotTouch& p : pilots) { p = PilotTouch {}; }   // drop sticks: the halves just moved
}

int Players() { return players; }

void Update()
{
    tapped = false;
    if (!enabled) { return; }
    prev = cur;
    ReadPoints(cur);

    // New fingers: away from the buttons they become the stick while flying,
    // or a menu tap otherwise.
    for (int i = 0; i < cur.n; ++i) {
        const Point& p = cur.pts[i];
        Vector2 unused {};
        if (Find(prev, p.id, unused) || OnAnyButton(p.pos)) { continue; }
        if (controls) {
            PilotTouch& pt = pilots[PilotAt(p.pos.x)];
            if (!pt.stickOn) { pt.stickOn = true; pt.stickId = p.id; pt.stickOrigin = p.pos; pt.stickPos = p.pos; }
        } else if (!tapped) {
            tapped = true;
            tapPos = ToCanvas(p.pos);
        }
    }
    for (int i = 0; i < kMaxPilots; ++i) { UpdateStick(pilots[i]); }

    for (int i = 0; i < kMaxPilots; ++i) {
        PilotTouch& pt = pilots[i];
        const bool live = controls && i < players;
        pt.fire = live && AnyInside(cur, FireBtn(i));
        pt.jam  = live && AnyInside(cur, JamBtn(i));
        const bool chaffNow = live && AnyInside(cur, ChaffBtn(i));
        pt.chaff    = chaffNow && !pt.chaffWas;
        pt.chaffWas = chaffNow;
    }
    const bool pauseNow = controls && AnyInside(cur, PauseBtn());
    pause    = pauseNow && !pauseWas;
    pauseWas = pauseNow;
    assert(!(pilots[0].fire && !controls));
}

namespace {
bool Valid(int pilot) { return pilot >= 0 && pilot < kMaxPilots && pilot < players; }
}

Vector2 Move(int pilot)         { return Valid(pilot) ? pilots[pilot].move : Vector2 {0.0f, 0.0f}; }
bool    FireHeld(int pilot)     { return Valid(pilot) && pilots[pilot].fire; }
bool    ChaffPressed(int pilot) { return Valid(pilot) && pilots[pilot].chaff; }
bool    JamHeld(int pilot)      { return Valid(pilot) && pilots[pilot].jam; }
bool    PausePressed()          { return pause; }

bool    Tapped()      { return tapped; }
Vector2 TapPos()      { return tapPos; }

void Draw()
{
    if (!enabled || !controls) { return; }
    const float u = Unit();

    // Two players: a faint line shows where one half ends and the other begins.
    if (players > 1) {
        const float x = static_cast<float>(GetScreenWidth()) * 0.5f;
        DrawLineEx(Vector2 {x, 0.0f}, Vector2 {x, static_cast<float>(GetScreenHeight())}, 1.0f, Fade(RAYWHITE, 0.15f));
    }

    for (int i = 0; i < players; ++i) {
        const PilotTouch& pt = pilots[i];
        // Pilot two's controls are tinted so the halves are easy to tell apart.
        const Color tint = (i == 0) ? RAYWHITE : Color {200, 230, 255, 255};
        if (pt.stickOn) {
            DrawCircleV(pt.stickOrigin, kStickRadius * u, Fade(tint, 0.12f));
            DrawCircleLinesV(pt.stickOrigin, kStickRadius * u, Fade(tint, 0.5f));
            DrawCircleV(Vector2 {pt.stickOrigin.x + pt.move.x * kStickRadius * u, pt.stickOrigin.y + pt.move.y * kStickRadius * u}, 0.35f * u, Fade(tint, 0.6f));
        }
        DrawButton(FireBtn(i),  "FIRE",  pt.fire,     RED);
        DrawButton(JamBtn(i),   "JAM",   pt.jam,      SKYBLUE);
        DrawButton(ChaffBtn(i), "CHAFF", pt.chaffWas, GOLD);
    }

    const Circle pb = PauseBtn();
    DrawCircleV(pb.c, pb.r, Fade(BLACK, pauseWas ? kHeldAlpha : kAlpha));
    DrawCircleLinesV(pb.c, pb.r, Fade(RAYWHITE, 0.8f));
    const float bw = pb.r * 0.22f, bh = pb.r * 0.9f;
    DrawRectangleV(Vector2 {pb.c.x - bw * 1.6f, pb.c.y - bh * 0.5f}, Vector2 {bw, bh}, RAYWHITE);
    DrawRectangleV(Vector2 {pb.c.x + bw * 0.6f, pb.c.y - bh * 0.5f}, Vector2 {bw, bh}, RAYWHITE);
}

} // namespace touch
