#include "raylib.h"
#include <math.h>

#define LEAF_SEG 48
#define LEAF_VERTS (LEAF_SEG * 2)

typedef struct {
    Vector2 v[LEAF_VERTS];
    int     n;
    float   L;   // length along long axis (node to tip)
} Leaf;

// Lens-shape leaf: intersection of two circular arcs.
// Local coords: node at (0,0), tip at (L,0), long axis along +x.
// Both endpoints are circular meeting points, so node and tip are identical —
// the shape has bilateral symmetry along its long axis AND 180° rotation
// symmetry about (L/2, 0).
static Leaf makeLeaf(float L, float W) {
    Leaf leaf;
    float d = (L*L - W*W) / (4.0f * W);   // arc-center offset from long axis
    float R = d + W * 0.5f;                // arc radius
    float cx = L * 0.5f;

    int idx = 0;

    // Upper arc: center (cx, -d). Sweep from (0,0) over the top to (L,0).
    float a0 = atan2f(d, -L * 0.5f);
    float a1 = atan2f(d,  L * 0.5f);
    for (int i = 0; i < LEAF_SEG; i++) {
        float t  = (float)i / (LEAF_SEG - 1);
        float th = a0 + (a1 - a0) * t;
        leaf.v[idx++] = (Vector2){ cx + R*cosf(th), -d + R*sinf(th) };
    }
    // Lower arc: center (cx, +d). Sweep from (L,0) under the bottom to (0,0).
    float b0 = atan2f(-d,  L * 0.5f);
    float b1 = atan2f(-d, -L * 0.5f);
    for (int i = 0; i < LEAF_SEG; i++) {
        float t  = (float)i / (LEAF_SEG - 1);
        float th = b0 + (b1 - b0) * t;
        leaf.v[idx++] = (Vector2){ cx + R*cosf(th),  d + R*sinf(th) };
    }
    leaf.n = idx;
    leaf.L = L;
    return leaf;
}

// Place leaf into world coords with rotation, then optionally reflect across
// the vertical stem axis x = stemX. The reflection is the key operation used
// to build the plant from a single leaflet.
static void drawLeafPlaced(const Leaf *leaf, Vector2 origin, float angle,
                           float stemX, bool reflectAcrossStem,
                           Color fill, Color edge) {
    Vector2 tmp[LEAF_VERTS];
    float c = cosf(angle), s = sinf(angle);
    for (int i = 0; i < leaf->n; i++) {
        // Reversing source order on reflection keeps winding consistent so
        // DrawTriangleFan isn't back-face-culled.
        int src = reflectAcrossStem ? (leaf->n - 1 - i) : i;
        Vector2 p = leaf->v[src];
        float x = p.x * c - p.y * s + origin.x;
        float y = p.x * s + p.y * c + origin.y;
        if (reflectAcrossStem) x = 2.0f * stemX - x;
        tmp[i] = (Vector2){ x, y };
    }
    DrawTriangleFan(tmp, leaf->n, fill);
    for (int i = 0; i < leaf->n; i++) {
        DrawLineEx(tmp[i], tmp[(i + 1) % leaf->n], 1.5f, edge);
    }

    // Midvein: node (local 0,0) → tip (local L,0), transformed identically.
    float tx = leaf->L * c + origin.x;
    float ty = leaf->L * s + origin.y;
    if (reflectAcrossStem) tx = 2.0f * stemX - tx;
    DrawLineEx(origin, (Vector2){tx, ty}, 1.2f, edge);
}

int main(void) {
    const int W = 600, H = 1000;
    InitWindow(W, H, "Plant of reflections");

    Leaf leaf = makeLeaf(130.0f, 45.0f);

    // Plant layout: stem vertical, base at the bottom, tip at the top.
    const float stemX  = W * 0.5f;
    const float baseY  = H * 0.95f;
    const float tipY   = H * 0.18f;
    const int   pairs  = 5;

    // Leaflets angled up toward the tip by this much from horizontal.
    // Right-side leaflet has long-axis pointing right (+x) by default; tilting
    // up means rotating by -tiltUp (screen y grows downward, so up = -y).
    const float tiltUp     = 20.0f * PI / 180.0f;
    const float rightAngle = -tiltUp;

    Color leafFill = (Color){110, 175,  90, 255};
    Color leafEdge = (Color){ 40,  80,  40, 255};
    Color stemCol  = (Color){110,  80,  40, 255};

    bool showAxis = true;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) showAxis = !showAxis;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Stem
        DrawLineEx((Vector2){stemX, baseY},
                   (Vector2){stemX, tipY}, 4.0f, stemCol);

        // Pairs of leaflets along the stem. Right leaflet drawn once; left is
        // its reflection across the vertical stem axis.
        for (int i = 0; i < pairs; i++) {
            float t   = 0.10f + (i / (float)(pairs - 1)) * 0.75f;
            float ny  = baseY + t * (tipY - baseY);
            Vector2 node = {stemX, ny};

            drawLeafPlaced(&leaf, node, rightAngle, stemX, false, leafFill, leafEdge);
            drawLeafPlaced(&leaf, node, rightAngle, stemX, true,  leafFill, leafEdge);

            DrawCircleV(node, 3.0f, stemCol);
        }

        // Terminal leaflet at the tip, pointing straight up. It is its own
        // reflection across the stem axis, so we only draw it once.
        Vector2 tipNode = {stemX, tipY};
        drawLeafPlaced(&leaf, tipNode, -PI/2, stemX, false, leafFill, leafEdge);
        DrawCircleV(tipNode, 3.0f, stemCol);

        if (showAxis) {
            for (int y = 0; y < H; y += 12)
                DrawLine((int)stemX, y, (int)stemX, y + 6, LIGHTGRAY);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
