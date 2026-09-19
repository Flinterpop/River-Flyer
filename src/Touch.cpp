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

bool    enabled  = false;
bool    controls = false;
Frame   cur {}, prev {};

bool    stickOn = false;
int     stickId = -1;
Vector2 stickOrigin {0.0f, 0.0f};
Vector2 stickPos    {0.0f, 0.0f};
Vector2 move        {0.0f, 0.0f};

bool fire = false, jam = false;
bool chaff = false, chaffWas = false;
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

Circle FireBtn()  { const float u = Unit(); return Circle {{static_cast<float>(GetScreenWidth()) - 1.7f * u, static_cast<float>(GetScreenHeight()) - 1.7f * u}, kFireR * u}; }
Circle JamBtn()   { const float u = Unit(); return Circle {{static_cast<float>(GetScreenWidth()) - 1.7f * u, static_cast<float>(GetScreenHeight()) - 4.3f * u}, kSmallR * u}; }
Circle ChaffBtn() { const float u = Unit(); return Circle {{static_cast<float>(GetScreenWidth()) - 4.3f * u, static_cast<float>(GetScreenHeight()) - 1.5f * u}, kSmallR * u}; }
Circle PauseBtn() { const float u = Unit(); return Circle {{static_cast<float>(GetScreenWidth()) - 0.9f * u, 0.9f * u}, kPauseR * u}; }

bool Inside(Vector2 p, const Circle& c)
{
    const float dx = p.x - c.c.x, dy = p.y - c.c.y;
    return dx * dx + dy * dy <= c.r * c.r;
}

bool OnAnyButton(Vector2 p)
{
    return controls && (Inside(p, FireBtn()) || Inside(p, JamBtn()) || Inside(p, ChaffBtn()) || Inside(p, PauseBtn()));
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
    if (f.n == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {   // desktop: the mouse is finger 0
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

void UpdateStick()
{
    if (stickOn && !Find(cur, stickId, stickPos)) { stickOn = false; }
    if (!stickOn) { move = Vector2 {0.0f, 0.0f}; return; }

    // The base trails the finger: past full deflection it is dragged along,
    // so reversing direction responds at once instead of after a long swipe back.
    const float r   = kStickRadius * Unit();
    Vector2     d   = {stickPos.x - stickOrigin.x, stickPos.y - stickOrigin.y};
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len > r) {
        const float k = 1.0f - r / len;
        stickOrigin.x += d.x * k;
        stickOrigin.y += d.y * k;
        d.x *= r / len;
        d.y *= r / len;
    }
    move = Vector2 {Deflect(d.x, r), Deflect(d.y, r)};
    assert(move.x >= -1.0f && move.x <= 1.0f && move.y >= -1.0f && move.y <= 1.0f);
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

void SetEnabled(bool on) { enabled = on; }
bool Enabled()           { return enabled; }
void ShowControls(bool on) { controls = on; }

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
            if (!stickOn) { stickOn = true; stickId = p.id; stickOrigin = p.pos; stickPos = p.pos; }
        } else if (!tapped) {
            tapped = true;
            tapPos = ToCanvas(p.pos);
        }
    }
    UpdateStick();

    fire = controls && AnyInside(cur, FireBtn());
    jam  = controls && AnyInside(cur, JamBtn());
    const bool chaffNow = controls && AnyInside(cur, ChaffBtn());
    chaff    = chaffNow && !chaffWas;
    chaffWas = chaffNow;
    const bool pauseNow = controls && AnyInside(cur, PauseBtn());
    pause    = pauseNow && !pauseWas;
    pauseWas = pauseNow;
    assert(!(fire && !controls));
}

Vector2 Move()        { return move; }
bool    FireHeld()    { return fire; }
bool    ChaffPressed(){ return chaff; }
bool    JamHeld()     { return jam; }
bool    PausePressed(){ return pause; }

bool    Tapped()      { return tapped; }
Vector2 TapPos()      { return tapPos; }

bool TappedIn(Rectangle r)
{
    assert(r.width >= 0.0f && r.height >= 0.0f);
    return tapped && CheckCollisionPointRec(tapPos, r);
}

void Draw()
{
    if (!enabled || !controls) { return; }
    const float u = Unit();
    if (stickOn) {
        DrawCircleV(stickOrigin, kStickRadius * u, Fade(RAYWHITE, 0.12f));
        DrawCircleLinesV(stickOrigin, kStickRadius * u, Fade(RAYWHITE, 0.5f));
        DrawCircleV(Vector2 {stickOrigin.x + move.x * kStickRadius * u, stickOrigin.y + move.y * kStickRadius * u}, 0.35f * u, Fade(RAYWHITE, 0.6f));
    }
    DrawButton(FireBtn(),  "FIRE",  fire,     RED);
    DrawButton(JamBtn(),   "JAM",   jam,      SKYBLUE);
    DrawButton(ChaffBtn(), "CHAFF", chaffWas, GOLD);

    const Circle pb = PauseBtn();
    DrawCircleV(pb.c, pb.r, Fade(BLACK, pauseWas ? kHeldAlpha : kAlpha));
    DrawCircleLinesV(pb.c, pb.r, Fade(RAYWHITE, 0.8f));
    const float bw = pb.r * 0.22f, bh = pb.r * 0.9f;
    DrawRectangleV(Vector2 {pb.c.x - bw * 1.6f, pb.c.y - bh * 0.5f}, Vector2 {bw, bh}, RAYWHITE);
    DrawRectangleV(Vector2 {pb.c.x + bw * 0.6f, pb.c.y - bh * 0.5f}, Vector2 {bw, bh}, RAYWHITE);
}

} // namespace touch
