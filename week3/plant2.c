#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#define LEAF_SEG    36
#define LEAF_VERTS  (LEAF_SEG * 2)
#define MAX_SEEDS   16384

typedef struct {
    Vector2 v[LEAF_VERTS];
    int     n;
    float   L;
    float   W;
} Leaf;

typedef struct {
    Vector2 pos;
    Color   color;
    bool    interior;
} Seed;

// Deterministic hash → [0,1). Used for jittering grid seeds in local-leaf
// coordinates so that a leaf and its mirror have pixel-identical seed sets.
static float hash01(int x, int y, int s) {
    unsigned u = (unsigned)(x * 374761393 + y * 668265263 + s * 2147483647);
    u = (u ^ (u >> 13)) * 1274126177u;
    return ((u ^ (u >> 16)) & 0xFFFFFF) / (float)0x1000000;
}

static Seed seeds[MAX_SEEDS];
static int  seedCount = 0;

static void addSeed(Vector2 p, Color c, bool interior) {
    if (seedCount >= MAX_SEEDS) return;
    seeds[seedCount++] = (Seed){p, c, interior};
}

// Lens-shape leaf (same construction as plant.c): two circular arcs meeting
// at (0,0) and (L,0). Node and tip are interchangeable.
static Leaf makeLeaf(float L, float W) {
    Leaf leaf;
    float d  = (L*L - W*W) / (4.0f * W);
    float R  = d + W * 0.5f;
    float cx = L * 0.5f;

    int idx = 0;
    float a0 = atan2f(d, -L * 0.5f);
    float a1 = atan2f(d,  L * 0.5f);
    for (int i = 0; i < LEAF_SEG; i++) {
        float t  = (float)i / (LEAF_SEG - 1);
        float th = a0 + (a1 - a0) * t;
        leaf.v[idx++] = (Vector2){ cx + R*cosf(th), -d + R*sinf(th) };
    }
    float b0 = atan2f(-d,  L * 0.5f);
    float b1 = atan2f(-d, -L * 0.5f);
    for (int i = 0; i < LEAF_SEG; i++) {
        float t  = (float)i / (LEAF_SEG - 1);
        float th = b0 + (b1 - b0) * t;
        leaf.v[idx++] = (Vector2){ cx + R*cosf(th),  d + R*sinf(th) };
    }
    leaf.n = idx;
    leaf.L = L;
    leaf.W = W;
    return leaf;
}

// Local-leaf-coords → world-coords, with optional reflection across the
// vertical stem axis x = stemX. Used for both boundary and interior seeds so
// reflected leaflets are pixel-identical to their mirror twins.
static Vector2 transformPt(Vector2 local, Vector2 origin, float angle,
                           float stemX, bool reflectAcrossStem) {
    float c = cosf(angle), s = sinf(angle);
    float x = local.x * c - local.y * s + origin.x;
    float y = local.x * s + local.y * c + origin.y;
    if (reflectAcrossStem) x = 2.0f * stemX - x;
    return (Vector2){x, y};
}

// Deterministic per-cell color in *local* coords — leaf and its mirror agree.
// Dark green with subtle variation toward yellow-green; some cells lean
// darker/khaki for the mottled, lit-from-behind look in the reference.
static Color leafColorAt(Vector2 local, float L, int gx, int gy) {
    float t  = local.x / L;                        // 0 at node, 1 at tip
    float k  = sinf(t * PI);                       // bright in middle
    float j  = hash01(gx, gy, 7) - 0.5f;           // ±0.5 per-cell jitter
    int r = (int)( 28 + 50 * t  + 60 * j);
    int g = (int)( 78 + 70 * k  + 40 * j);
    int b = (int)( 22 + 18 * (1.0f - t) + 25 * j);
    if (r < 6)   r = 6;   if (r > 200) r = 200;
    if (g < 30)  g = 30;  if (g > 220) g = 220;
    if (b < 6)   b = 6;   if (b > 200) b = 200;
    return (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
}

// Emit boundary + interior seeds for one placement of the leaf. Reflection is
// applied uniformly so the right and left leaflets are seed-identical mirrors.
// Interior seeds are a jittered grid clipped to the lens envelope — gives the
// dense, irregular Voronoi pattern of a real leaf's areolae (vein islands).
static void emitLeafSeeds(const Leaf *leaf, Vector2 origin, float angle,
                          float stemX, bool reflectAcrossStem) {
    // Boundary — every vertex of the lens polygon (no color, acts as a wall).
    for (int i = 0; i < leaf->n; i++) {
        Vector2 w = transformPt(leaf->v[i], origin, angle, stemX, reflectAcrossStem);
        addSeed(w, BLANK, false);
    }

    // Interior — jittered grid in local coords. NX×NY tuned for ~150 cells.
    const int NX = 10;
    const int NY = 4;
    for (int i = 1; i < NX; i++) {
        float t = (float)i / NX;
        float xLocal = t * leaf->L;
        float halfW  = 0.48f * leaf->W * sinf(t * PI);
        if (halfW < 0.5f) continue;
        for (int j = -NY; j <= NY; j++) {
            float yLocal = (j / (float)NY) * halfW;
            // Skip cells that fall outside the lens (cheap envelope check).
            if (fabsf(yLocal) > halfW) continue;
            // Deterministic jitter from (i,j) — same on the mirror side.
            float jx = (hash01(i, j, 1) - 0.5f) * (leaf->L / NX) * 0.7f;
            float jy = (hash01(i, j, 2) - 0.5f) * (halfW / NY)  * 0.7f;
            Vector2 local = { xLocal + jx, yLocal + jy };
            addSeed(transformPt(local, origin, angle, stemX, reflectAcrossStem),
                    leafColorAt(local, leaf->L, i, j), true);
        }
    }
    // Midrib: a denser row along y=0 so the main vein reads as a single line.
    for (int i = 1; i < NX; i++) {
        float t = (float)i / NX;
        Vector2 local = { t * leaf->L, 0 };
        addSeed(transformPt(local, origin, angle, stemX, reflectAcrossStem),
                leafColorAt(local, leaf->L, i, 0), true);
    }
}

int main(void) {
    const int W = 600, H = 1000;
    InitWindow(W, H, "Voronoi plant of reflections");

    Leaf leaf = makeLeaf(170.0f, 58.0f);

    // Plant layout — same as plant.c.
    const float stemX  = W * 0.5f;
    const float baseY  = H * 0.95f;
    const float tipY   = H * 0.18f;
    const int   pairs  = 5;
    const float tiltUp = 15.0f * PI / 180.0f;
    const float rightAngle = -tiltUp;

    // --- Leaflet seeds ---------------------------------------------------
    for (int i = 0; i < pairs; i++) {
        float t  = 0.10f + (i / (float)(pairs - 1)) * 0.75f;
        float ny = baseY + t * (tipY - baseY);
        Vector2 node = {stemX, ny};
        emitLeafSeeds(&leaf, node, rightAngle, stemX, false);
        emitLeafSeeds(&leaf, node, rightAngle, stemX, true);
    }
    // Extra near-top pair, steeper (closer to vertical) so they sit just
    // below the terminal leaflet — only reflection, no asymmetry.
    {
        float ny    = baseY + 0.92f * (tipY - baseY);
        float steep = 55.0f * PI / 180.0f;     // angle above horizontal
        Vector2 node = {stemX, ny};
        emitLeafSeeds(&leaf, node, -steep, stemX, false);
        emitLeafSeeds(&leaf, node, -steep, stemX, true);
    }

    // Terminal leaflet — its own mirror.
    emitLeafSeeds(&leaf, (Vector2){stemX, tipY}, -PI/2, stemX, false);

    // --- Per-pixel nearest-seed Voronoi with vein-glow shading ----------
    // For each pixel we keep the two smallest squared distances (d1sq, d2sq).
    // edge dist e = (sqrt(d2sq) - sqrt(d1sq)) * 0.5 — perpendicular distance
    // to the nearest Voronoi edge. Bright vein at e≈0, fading into the cell
    // color as e grows. Within a cell, distance to seed gives a subtle dark
    // centre, like in a real leaf areole.
    Color *buf = malloc(sizeof(Color) * W * H);

    const Color veinCol = (Color){240, 230, 170, 255};      // pale yellow-cream
    const float veinFalloff = 1.6f;                          // pixels
    const float darkCentre  = 0.45f;                         // 0..1 darken

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float d1sq = 1e30f, d2sq = 1e30f;
            int   i1 = -1;
            for (int s = 0; s < seedCount; s++) {
                float dx = seeds[s].pos.x - x;
                float dy = seeds[s].pos.y - y;
                float d  = dx*dx + dy*dy;
                if (d < d1sq)      { d2sq = d1sq; d1sq = d; i1 = s; }
                else if (d < d2sq) { d2sq = d; }
            }
            int i = y * W + x;
            if (i1 < 0 || !seeds[i1].interior) { buf[i] = RAYWHITE; continue; }

            float d1 = sqrtf(d1sq);
            float d2 = sqrtf(d2sq);
            float e  = (d2 - d1) * 0.5f;
            float cellR = (d1 + d2) * 0.5f;           // local cell radius

            // Vein glow: 1 at edge, → 0 in cell interior.
            float vein = expf(-e / veinFalloff);

            // Dark-centre falloff: bright near edge, dim near seed.
            // r in [0,1] where 0 = seed, 1 = cell edge.
            float r = d1 / (cellR + 1e-3f);
            if (r > 1.0f) r = 1.0f;
            float shade = 1.0f - darkCentre * (1.0f - r);

            Color base = seeds[i1].color;
            float br = base.r * shade;
            float bg = base.g * shade;
            float bb = base.b * shade;

            float rr = br + (veinCol.r - br) * vein;
            float gg = bg + (veinCol.g - bg) * vein;
            float bbf= bb + (veinCol.b - bb) * vein;

            if (rr < 0) rr = 0; if (rr > 255) rr = 255;
            if (gg < 0) gg = 0; if (gg > 255) gg = 255;
            if (bbf< 0) bbf= 0; if (bbf> 255) bbf= 255;
            buf[i] = (Color){(unsigned char)rr, (unsigned char)gg,
                             (unsigned char)bbf, 255};
        }
    }

    Image voro = {
        .data    = buf,
        .width   = W,
        .height  = H,
        .mipmaps = 1,
        .format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    Texture2D tex = LoadTextureFromImage(voro);

    bool showSeeds = false;
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) showSeeds = !showSeeds;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTexture(tex, 0, 0, WHITE);

        // Dotted stem axis where the brown stem used to be.
        {
            Color dotCol = (Color){90, 90, 90, 255};
            float dotR = 2.0f;
            float spacing = 12.0f;
            for (float y = tipY; y <= baseY; y += spacing) {
                DrawCircle((int)stemX, (int)y, dotR, dotCol);
            }
        }
        if (showSeeds) {
            for (int s = 0; s < seedCount; s++) {
                Color c = seeds[s].interior ? BLACK : (Color){200,80,80,255};
                DrawCircleV(seeds[s].pos, 1.5f, c);
            }
        }
        DrawText("SPACE: toggle seeds", 10, H - 24, 14, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(tex);
    free(buf);
    CloseWindow();
    return 0;
}
