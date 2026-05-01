#include "raylib.h"
#include <math.h>

//full definition of Epitrochoid 
void DrawEpitrochoid(Vector2 center, float R, float r, float d, float rotation, Color outlineColor, Color fillColor, bool drawOutline, bool fill)
{
    #define RESOLUTION 180
    Vector2 points[RESOLUTION + 1];

    for (int i = 0; i <= RESOLUTION; i++)
    {
        float t = ((float)i / RESOLUTION) * 2.0f * PI;

        float base_x = (R + r) * cos(t) - d * cos(((R + r) / r) * t);
        float base_y = (R + r) * sin(t) - d * sin(((R + r) / r) * t);

        float rotated_x = base_x * cos(rotation) - base_y * sin(rotation);
        float rotated_y = base_x * sin(rotation) + base_y * cos(rotation);

        points[i] = (Vector2){ center.x + rotated_x, center.y + rotated_y };
    }

    if (fill)
    {
        for (int i = 0; i < RESOLUTION; i++)
        {
            // Passing the vectors in CCW (face front to provent back-face culling)
            DrawTriangle(center, points[i + 1], points[i], fillColor);
        }
    }

    if (drawOutline){
        DrawLineStrip(points, RESOLUTION + 1, outlineColor);
    }
}


// Draws radial triangles to simulate botanical stamens/filaments
void DrawStamens(Vector2 center, int count, float innerRadius, float outerRadius, float baseWidth, Color color)
{
    for (int i = 0; i < count; i++)
    {
        //dist over 360 degree
        float angle = ((float)i / count) * 2.0f * PI;

        Vector2 tip = {
            center.x + cosf(angle) * outerRadius,
            center.y + sinf(angle) * outerRadius
        };

        //perpendiculer for base ofset
        float perpAngle = angle + (PI / 2.0f);

        Vector2 baseLeft = {
            center.x + cosf(angle) * innerRadius + cosf(perpAngle) * baseWidth,
            center.y + sinf(angle) * innerRadius + sinf(perpAngle) * baseWidth
        };

        Vector2 baseRight = {
            center.x + cosf(angle) * innerRadius - cosf(perpAngle) * baseWidth,
            center.y + sinf(angle) * innerRadius - sinf(perpAngle) * baseWidth
        };

        DrawTriangle(tip, baseLeft, baseRight, color);
        DrawTriangle(tip, baseRight, baseLeft, color);
        DrawCircleV(tip, baseWidth * 0.8f, color);
    }
}



int main(void)
{
    const bool OUTLINE = true;
    const bool FILL = false; 

    const int screenWidth = 800;
    const int screenHeight = 800;
    Vector2 center = { screenWidth / 2.0f, screenHeight / 2.0f };


    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Helleborus Botanical Render");
    SetTargetFPS(60);

    //picked using color selector of a real helleborus
    Color outerPetal = (Color){ 235, 242, 226, 255 };
    Color innerPetal = (Color){ 186, 212, 162, 255 };
    Color stamen     = (Color){ 221, 222, 215, 255 };

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        //outer
        float R1 = 130.0f;
        float r1 = R1 / 5.0f;
        float d1 = 64.0f;
        float rot1 = 1.0f;

        DrawEpitrochoid(center, R1, r1, d1, rot1, outerPetal, outerPetal, OUTLINE, FILL);

        //middel
        float R2 = 60.0f;
        float r2 = R2 / 5.0f;
        float d2 = 40.0f;
        float rot2 = rot1 + 10.0f + (PI / (R2 / r2));

        DrawEpitrochoid(center, R2, r2, d2, rot2, innerPetal, innerPetal, OUTLINE, FILL);

        //Stamens
        float stamenInnerRadius = 25.0f;
        float stamenOuterRadius = 85.0f;
        int stamenCount = 20;
        float stamenThickness = 2.5f;

        DrawStamens(center, stamenCount, stamenInnerRadius, stamenOuterRadius, stamenThickness, stamen);

        //heart
        float R3 = 30.0f;
        float r3 = R3 / 10.0f;
        float d3 = r3 / 2.0f;
        float rot3 = 1.0f;

        DrawEpitrochoid(center, R3, r3, d3, rot3, stamen, stamen, OUTLINE, FILL);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
