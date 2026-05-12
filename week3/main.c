#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#define W 1000
#define H 500
#define MAX_SEEDS 8192

typedef struct {
    Vector2 pos;
    Color   color;
    bool    interior;
} Seed;

static Seed seeds[MAX_SEEDS];
static int  seedCount = 0;

static void addSeed(float x, float y, Color c, bool interior) {
    if (seedCount >= MAX_SEEDS) return;
    seeds[seedCount++] = (Seed){{x, y}, c, interior};
}

// Mirror a seed across the horizontal stem axis at y = axisY.
static void addSeedSym(float x, float y, float axisY, Color c) {
    addSeed(x, y,             c, true);
    addSeed(x, 2*axisY - y,   c, true);
}

static bool isPlant(Color c) {
    // Green-dominant pixel: leaves are green on a white/blue-grid background.
    return (c.g > c.r + 8) && (c.g > c.b + 8) && (c.g > 50);
}

int main(void) {
    InitWindow(W, H, "Voronoi Plant");

    // --- Load image and build silhouette mask --------------------------------
    Image img = LoadImage("pant.png");
    if (img.data == NULL) {
        TraceLog(LOG_ERROR, "Failed to load pant.png");
        CloseWindow();
        return 1;
    }
    ImageResize(&img, W, H);
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *src = LoadImageColors(img);

    bool *mask = malloc((size_t)W * H);
    for (int i = 0; i < W*H; i++) mask[i] = isPlant(src[i]);

    // --- Sample boundary seeds along silhouette edge -------------------------
    // Collect edge pixels first, then take every Kth in scan order.
    int   *edgeIdx = malloc(sizeof(int) * W * H);
    int    edgeN  = 0;
    for (int y = 1; y < H-1; y++) {
        for (int x = 1; x < W-1; x++) {
            int i = y*W + x;
            if (!mask[i]) continue;
            if (mask[i-1] && mask[i+1] && mask[i-W] && mask[i+W]) continue;
            edgeIdx[edgeN++] = i;
        }
    }
    int stride = (edgeN > 600) ? edgeN / 600 : 1;  // ~600 boundary seeds
    for (int k = 0; k < edgeN; k += stride) {
        int i = edgeIdx[k];
        addSeed((float)(i % W), (float)(i / W), BLANK, false);
    }
    free(edgeIdx);

    // --- Interior seeds: stem, nodes, leaf tips, mirrored across stem axis ---
    // The stem runs roughly horizontally across the image. Adjust axisY if needed.
    float axisY = H * 0.52f;

    // Stem points: a row of seeds along the central axis.
    int stemN = 24;
    float stemX0 = W * 0.27f, stemX1 = W * 0.92f;
    Color stemCol = (Color){120, 90, 40, 255};
    for (int i = 0; i < stemN; i++) {
        float t = (float)i / (stemN - 1);
        float x = stemX0 + t * (stemX1 - stemX0);
        addSeed(x, axisY, stemCol, true);
    }

    // Leaf nodes: 5 leaflet pairs along the stem. Place a node seed and tip seed
    // for each side; mirroring is automatic via addSeedSym.
    int leafPairs = 5;
    Color nodeCol = (Color){ 90, 160,  70, 255};
    Color veinCol = (Color){110, 185,  90, 255};
    Color tipCol  = (Color){ 60, 130,  55, 255};
    for (int i = 0; i < leafPairs; i++) {
        float t  = (i + 0.5f) / leafPairs;
        float cx = W*0.30f + t * W*0.58f;

        // node — where leaflet meets stem
        addSeedSym(cx, axisY - H*0.04f, axisY, nodeCol);

        // mid-vein samples (creates several cells inside each leaflet)
        addSeedSym(cx - W*0.015f, axisY - H*0.12f, axisY, veinCol);
        addSeedSym(cx + W*0.015f, axisY - H*0.12f, axisY, veinCol);
        addSeedSym(cx,            axisY - H*0.20f, axisY, veinCol);

        // tip
        addSeedSym(cx, axisY - H*0.30f, axisY, tipCol);
    }

    // --- Render Voronoi: per-pixel nearest seed ------------------------------
    Color *buf = malloc(sizeof(Color) * W * H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float best = 1e30f;
            int   bi   = -1;
            for (int s = 0; s < seedCount; s++) {
                float dx = seeds[s].pos.x - x;
                float dy = seeds[s].pos.y - y;
                float d  = dx*dx + dy*dy;
                if (d < best) { best = d; bi = s; }
            }
            buf[y*W + x] = (bi >= 0 && seeds[bi].interior)
                             ? seeds[bi].color
                             : RAYWHITE;
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

    UnloadImageColors(src);
    UnloadImage(img);
    free(mask);

    bool showSeeds = true;

    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) showSeeds = !showSeeds;

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTexture(tex, 0, 0, WHITE);
        if (showSeeds) {
            for (int s = 0; s < seedCount; s++) {
                if (seeds[s].interior)
                    DrawCircleV(seeds[s].pos, 2.0f, BLACK);
            }
        }
        DrawText("SPACE: toggle seeds", 10, H-24, 14, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(tex);
    free(buf);
    CloseWindow();
    return 0;
}
