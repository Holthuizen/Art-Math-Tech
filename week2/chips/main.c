// Pringles Can - AMT 25-2B Week 2
// Click / SPACE to eject a chip (hyperbolic paraboloid saddle surface)

#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include "rlgl.h"

#define MAX_CHIPS   60
#define CHIP_RINGS  18
#define CHIP_SPOKES 24
#define CHIP_SCALE  0.55f
#define GRAVITY    -7.0f

#define FLOOR_Y     0.0f
#define CAN_RADIUS  0.72f
#define CAN_HEIGHT  3.8f
#define CAN_BOTTOM  FLOOR_Y
#define CAN_TOP    (FLOOR_Y + CAN_HEIGHT)

#define PILE_INNER  (CAN_RADIUS + 0.3f)
#define PILE_OUTER  (CAN_RADIUS + 2.2f)

typedef enum { FLYING, SETTLED } ChipState;

typedef struct {
    Vector3   pos, vel;
    Vector3   rotAxis;
    float     rotAngle, rotSpeed;
    Vector3   settledAxis;
    float     settledRot;
    ChipState state;
    Color     color;
    bool      active;
} Chip;

static Chip chips[MAX_CHIPS];
static int  chipCount = 0;

static Vector3 ChipVertex(float u, float v) {
    float x = u;
    float y = v * 0.7f;
    float z = (x*x - y*y) * 0.38f;
    return (Vector3){ x * CHIP_SCALE, z * CHIP_SCALE, y * CHIP_SCALE };
}

static void DrawChip(Vector3 pos, Vector3 axis, float angle, Color col) {
    Quaternion q   = QuaternionFromAxisAngle(axis, angle);
    Matrix     rot = QuaternionToMatrix(q);

    // family A — fix u, vary v
    for (int i = 0; i <= CHIP_RINGS; i++) {
        float u    = -1.0f + 2.0f * i / CHIP_RINGS;
        float fade = 1.0f - fabsf(u);
        Color c = col; c.a = (unsigned char)(60 + 180 * fade);
        for (int j = 0; j < CHIP_SPOKES; j++) {
            float v0 = -1.0f + 2.0f *  j      / CHIP_SPOKES;
            float v1 = -1.0f + 2.0f * (j + 1) / CHIP_SPOKES;
            Vector3 p0 = Vector3Add(Vector3Transform(ChipVertex(u, v0), rot), pos);
            Vector3 p1 = Vector3Add(Vector3Transform(ChipVertex(u, v1), rot), pos);
            rlSetLineWidth(5.0f); 
            DrawLine3D(p0, p1, c);
        }
    }
    // family B — fix v, vary u (slightly darker)
    for (int j = 0; j <= CHIP_SPOKES; j++) {
        float v    = -1.0f + 2.0f * j / CHIP_SPOKES;
        float fade = 1.0f - fabsf(v);
        Color c = col;
        c.r = (unsigned char)(c.r * 0.72f);
        c.g = (unsigned char)(c.g * 0.72f);
        c.a = (unsigned char)(60 + 180 * fade);
        for (int i = 0; i < CHIP_RINGS; i++) {
            float u0 = -1.0f + 2.0f *  i      / CHIP_RINGS;
            float u1 = -1.0f + 2.0f * (i + 1) / CHIP_RINGS;
            Vector3 p0 = Vector3Add(Vector3Transform(ChipVertex(u0, v), rot), pos);
            Vector3 p1 = Vector3Add(Vector3Transform(ChipVertex(u1, v), rot), pos);
            DrawLine3D(p0, p1, c);
        }
    }
}

static float RandF(float lo, float hi) {
    return lo + (hi - lo) * ((float)GetRandomValue(0, 10000) / 10000.0f);
}
static Vector3 RandUnitVec(void) {
    return Vector3Normalize((Vector3){ RandF(-1,1), RandF(-1,1), RandF(-1,1) });
}
static Color ChipColor(void) {
    return (Color){ GetRandomValue(210,240), GetRandomValue(165,200), GetRandomValue(75,120), 255 };
}

static void EjectChip(void) {
    if (chipCount >= MAX_CHIPS) return;
    Chip *c = &chips[chipCount++];
    c->pos       = (Vector3){ RandF(-0.1f, 0.1f), CAN_TOP + 0.05f, RandF(-0.1f, 0.1f) };
    c->vel       = (Vector3){ RandF(-3.0f, 3.0f), RandF(4.5f, 7.5f), RandF(-3.0f, 3.0f) };
    c->rotAxis   = RandUnitVec();
    c->rotAngle  = RandF(0, 6.28f);
    c->rotSpeed  = RandF(3.0f, 9.0f) * (GetRandomValue(0,1) ? 1.f : -1.f);
    c->state     = FLYING;
    c->color     = ChipColor();
    c->active    = true;
    c->settledAxis = Vector3Normalize((Vector3){ RandF(-1,1), 0.05f, RandF(-1,1) });
    c->settledRot  = RandF(-0.5f, 0.5f);
}

static void UpdateChips(float dt) {
    for (int i = 0; i < chipCount; i++) {
        Chip *c = &chips[i];
        if (!c->active || c->state == SETTLED) continue;
        c->vel.y    += GRAVITY * dt;
        c->pos       = Vector3Add(c->pos, Vector3Scale(c->vel, dt));
        c->rotAngle += c->rotSpeed * dt;

        if (c->pos.y < FLOOR_Y + CHIP_SCALE * 0.4f) {
            // count settled chips for stacking height
            int layer = 0;
            for (int j = 0; j < i; j++)
                if (chips[j].state == SETTLED) layer++;

                // place in ring OUTSIDE the can
                float angle  = RandF(0, 6.28f);
            float radius = RandF(PILE_INNER, PILE_OUTER);
            c->pos = (Vector3){
                cosf(angle) * radius,
                FLOOR_Y + CHIP_SCALE * 0.12f + layer * 0.032f,
                sinf(angle) * radius
            };
            c->rotAxis  = c->settledAxis;
            c->rotAngle = c->settledRot;
            c->vel      = Vector3Zero();
            c->state    = SETTLED;
        }
    }
}

static void DrawCan(void) {
    int   segs   = 48;
    Color body   = { 180, 30, 20, 255 };
    Color silver = { 210, 210, 218, 255 };
    Color stripe = { 220, 200,  30, 255 };

    Vector3 base = { 0.0f, CAN_BOTTOM, 0.0f };

    // solid body — no wires
    DrawCylinder(base, CAN_RADIUS, CAN_RADIUS, CAN_HEIGHT, segs, body);

    // top rim
    DrawCylinder((Vector3){0, CAN_TOP - 0.09f, 0},
                 CAN_RADIUS + 0.03f, CAN_RADIUS + 0.03f, 0.11f, segs, silver);
    // bottom cap
    DrawCylinder(base, CAN_RADIUS + 0.01f, CAN_RADIUS + 0.01f, 0.05f, segs, silver);

    // label stripes
    for (int k = 0; k < 3; k++) {
        float y = CAN_BOTTOM + CAN_HEIGHT * (0.25f + k * 0.22f);
        DrawCylinder((Vector3){0, y, 0},
                     CAN_RADIUS + 0.006f, CAN_RADIUS + 0.006f, 0.05f, segs, stripe);
    }
}

int main(void) {
    const int W = 1280, H = 800;
    InitWindow(W, H, "Pringles");
    SetTargetFPS(60);

    Camera3D cam = {
        .position   = { 8.0f, 7.0f, 10.0f },
        .target     = { 0.0f, -3.2f, 0.0f },
        .up         = { 0.0f, 50.0f, 0.0f },
        .fovy       = 50.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    float time = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        time    += dt;

        float orbitAngle = time * 0.18f;
        float orbitDist  = 9.0f;
        cam.position.x   = cosf(orbitAngle) * orbitDist;
        cam.position.z   = sinf(orbitAngle) * orbitDist;
        cam.position.y   = 2.8f + sinf(time * 0.12f) * 0.4f;
        cam.target       = (Vector3){ 0.0f, 1.0f, 0.0f };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) EjectChip();
        if (IsKeyPressed(KEY_SPACE))                 EjectChip();

        UpdateChips(dt);

        BeginDrawing();
        ClearBackground((Color){10, 8, 12, 255});
        BeginMode3D(cam);
        DrawGrid(28, 0.5f);
        DrawCan();
        for (int i = 0; i < chipCount; i++)
            if (chips[i].active)
                DrawChip(chips[i].pos, chips[i].rotAxis,
                         chips[i].rotAngle, chips[i].color);
                EndMode3D();
            EndDrawing();
    }

    CloseWindow();
    return 0;
}
