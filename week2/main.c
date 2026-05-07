// Apples in a Bowl — A Mathematical Still Life
// AMT 25-2B Week 2
//
// Math from the lecture:
//   • Parametric surfaces r(u,v) for apples and bowl (1.3)
//   • Surface normals via cross product of partials  N = ∂_u r × ∂_v r (2.4)
//   • Lambert shading uses N pointwise — same machinery as the second
//     fundamental form, repurposed for tonal modelling
//
// Geometry strategy:
//   1. Each apple is approximated by a bounding sphere of radius APPLE_R.
//   2. Spheres are placed so their CENTERS are ≥ 2·APPLE_R apart in 3D,
//      meaning spheres may touch but never overlap.
//   3. Bottom layer: spheres rest on the bowl interior — center at
//      y = bowl_y(r) + APPLE_R (sphere tangent to bowl from above).
//   4. Top layer: each new sphere is dropped at (x,z); its rest height is
//      max over existing spheres of  y_i + sqrt((2R)^2 − d_xz^2)
//      — geometric tangency with whatever it lands on.
//   5. The actual apple mesh is then built inside the sphere.

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>

#define CLAMPF(x,lo,hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))

// ── helpers ───────────────────────────────────────────────────────────────

static Vector3 v3lerp(Vector3 a, Vector3 b, float t) {
    return (Vector3){a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t};
}
static Vector3 v3norm(Vector3 v) {
    float l = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (l < 1e-8f) return (Vector3){0,1,0};
    return (Vector3){v.x/l, v.y/l, v.z/l};
}
static Color v3col(Vector3 v) {
    return (Color){
        (unsigned char)CLAMPF(v.x*255.0f, 0.0f, 255.0f),
        (unsigned char)CLAMPF(v.y*255.0f, 0.0f, 255.0f),
        (unsigned char)CLAMPF(v.z*255.0f, 0.0f, 255.0f),
        255
    };
}
static float rf(void) { return (float)rand() / (float)RAND_MAX; }

// ── lighting ──────────────────────────────────────────────────────────────

static const Vector3 KEY_DIR  = { 0.45f, 0.78f,  0.40f };
static const Vector3 FILL_DIR = {-0.55f, 0.40f, -0.30f };
static const Vector3 KEY_TINT = { 1.05f, 1.00f, 0.92f };
static const Vector3 FILL_T   = { 0.85f, 0.92f, 1.05f };
static const float   AMB      = 0.34f;
static const float   FILL_S   = 0.42f;

static Color shade(Vector3 albedo, Vector3 N) {
    Vector3 Lk = v3norm(KEY_DIR), Lf = v3norm(FILL_DIR), n = v3norm(N);
    float dk = fmaxf(0.0f, n.x*Lk.x + n.y*Lk.y + n.z*Lk.z);
    float df = fmaxf(0.0f, n.x*Lf.x + n.y*Lf.y + n.z*Lf.z) * FILL_S;
    Vector3 lit = {
        albedo.x * (AMB + dk*KEY_TINT.x + df*FILL_T.x),
        albedo.y * (AMB + dk*KEY_TINT.y + df*FILL_T.y),
        albedo.z * (AMB + dk*KEY_TINT.z + df*FILL_T.z)
    };
    return v3col(lit);
}

// ── Apple variants ────────────────────────────────────────────────────────

typedef struct {
    float ar, ah;
    float rBias, hBias;
    float dimple, navel;
    Vector3 colA, colB;     // base, blush
    float   blushPow;
} AVar;

static AVar G_VAR[3] = {
    // golden delicious — yellow-green with rosy blush
    {0.78f, 0.86f, 0.20f, 0.07f, 0.55f, 0.20f,
     {0.92f, 0.84f, 0.30f}, {0.93f, 0.42f, 0.18f}, 1.4f},
    // riper, deeper crimson
    {0.76f, 0.84f, 0.22f, 0.06f, 0.52f, 0.22f,
     {0.94f, 0.78f, 0.26f}, {0.85f, 0.18f, 0.12f}, 1.6f},
    // greener apple, lighter blush
    {0.78f, 0.88f, 0.18f, 0.08f, 0.58f, 0.20f,
     {0.72f, 0.86f, 0.32f}, {0.90f, 0.55f, 0.22f}, 1.3f}
};

// ── Apple shape, normal, albedo ───────────────────────────────────────────

static Vector3 appleShape(float u, float v, int vi) {
    AVar V = G_VAR[vi];
    float su = sinf(u), cu = cosf(u);
    float r = su * (1.0f + V.rBias * cosf(u - 0.55f));
    float dt = PI - u;
    r *= (1.0f - V.dimple * expf(-12.0f * dt*dt));
    r *= (1.0f - V.navel  * expf(-18.0f * u*u));
    float h = -cu + V.hBias * sinf(2.0f*u);
    h -= 0.18f * expf(-12.0f * dt*dt);
    h += 0.07f * expf(-18.0f * u*u);
    return (Vector3){V.ar*r*cosf(v), V.ah*h, V.ar*r*sinf(v)};
}

static Vector3 appleNormal(float u, float v, int vi) {
    float h = 0.0025f;
    float uc = u;
    if (uc < 0.04f) uc = 0.04f;
    if (uc > PI - 0.04f) uc = PI - 0.04f;
    Vector3 ru = Vector3Subtract(appleShape(uc+h, v, vi), appleShape(uc-h, v, vi));
    Vector3 rv = Vector3Subtract(appleShape(uc, v+h, vi), appleShape(uc, v-h, vi));
    return v3norm(Vector3CrossProduct(ru, rv));
}

static Vector3 appleAlbedo(float u, float v, float blushAngle, int vi) {
    AVar V = G_VAR[vi];
    float dv = v - blushAngle;
    while (dv >  PI) dv -= 2.0f * PI;
    while (dv < -PI) dv += 2.0f * PI;
    float side = 0.5f + 0.5f * cosf(dv);
    side = powf(side, V.blushPow);
    side *= sinf(u);
    side *= 0.86f + 0.14f * sinf(31.0f*v + 1.5f*u);
    side  = CLAMPF(side, 0.0f, 1.0f);
    Vector3 col = v3lerp(V.colA, V.colB, side);
    float sp = sinf(57.0f*u + 1.1f) * cosf(73.0f*v + 0.4f);
    if (sp > 0.93f) {
        col.x = fminf(1.0f, col.x + 0.10f);
        col.y = fminf(1.0f, col.y + 0.10f);
        col.z = fminf(1.0f, col.z + 0.06f);
    }
    return col;
}

// ── Apple instance build ──────────────────────────────────────────────────

typedef struct {
    Vector3 pos;            // = sphere centre
    float yaw, tilt, tiltDir;
    Vector3 scale;
    float blushAngle;
    int variant;
} AppleInst;

#define NU 70
#define NV 88

static Mesh buildAppleInstance(AppleInst inst) {
    Matrix Ry = MatrixRotateY(inst.yaw);
    Vector3 ta = {cosf(inst.tiltDir), 0, sinf(inst.tiltDir)};
    Matrix Rt = MatrixRotate(ta, inst.tilt);
    Matrix R  = MatrixMultiply(Ry, Rt);
    Matrix S  = MatrixScale(inst.scale.x, inst.scale.y, inst.scale.z);
    Matrix Tr = MatrixTranslate(inst.pos.x, inst.pos.y, inst.pos.z);
    Matrix M  = MatrixMultiply(Tr, MatrixMultiply(R, S));

    int maxV = NU * NV * 6;
    Mesh m = {0};
    m.vertices = (float*)malloc(maxV * 3 * sizeof(float));
    m.colors   = (unsigned char*)malloc(maxV * 4 * sizeof(unsigned char));
    int n = 0;
    for (int iu = 0; iu < NU; iu++) {
        for (int iv = 0; iv < NV; iv++) {
            float us[4] = {PI*iu/NU, PI*(iu+1)/NU, PI*(iu+1)/NU, PI*iu/NU};
            float vs[4] = {2*PI*iv/NV, 2*PI*iv/NV, 2*PI*(iv+1)/NV, 2*PI*(iv+1)/NV};
            Vector3 wp[4]; Color wc[4];
            for (int i = 0; i < 4; i++) {
                Vector3 lp = appleShape(us[i], vs[i], inst.variant);
                Vector3 ln = appleNormal(us[i], vs[i], inst.variant);
                wp[i] = Vector3Transform(lp, M);
                Vector3 wn = Vector3Transform(ln, R);
                Vector3 al = appleAlbedo(us[i], vs[i], inst.blushAngle, inst.variant);
                wc[i] = shade(al, wn);
            }
            int tri[2][3] = {{0,1,2},{0,2,3}};
            for (int t = 0; t < 2; t++)
                for (int k = 0; k < 3; k++) {
                    int idx = tri[t][k];
                    m.vertices[n*3]   = wp[idx].x;
                    m.vertices[n*3+1] = wp[idx].y;
                    m.vertices[n*3+2] = wp[idx].z;
                    m.colors[n*4]   = wc[idx].r;
                    m.colors[n*4+1] = wc[idx].g;
                    m.colors[n*4+2] = wc[idx].b;
                    m.colors[n*4+3] = wc[idx].a;
                    n++;
                }
        }
    }
    m.vertexCount = n; m.triangleCount = n/3;
    UploadMesh(&m, false);
    return m;
}

// ── Stem ──────────────────────────────────────────────────────────────────

#define NS 12
static Mesh buildStemInstance(AppleInst inst) {
    AVar V = G_VAR[inst.variant];
    Matrix Ry = MatrixRotateY(inst.yaw);
    Vector3 ta = {cosf(inst.tiltDir), 0, sinf(inst.tiltDir)};
    Matrix Rt = MatrixRotate(ta, inst.tilt);
    Matrix R  = MatrixMultiply(Ry, Rt);
    Matrix S  = MatrixScale(inst.scale.x, inst.scale.y, inst.scale.z);
    Matrix Tr = MatrixTranslate(inst.pos.x, inst.pos.y, inst.pos.z);
    Matrix M  = MatrixMultiply(Tr, MatrixMultiply(R, S));
    Vector3 albedo = {0.30f, 0.18f, 0.08f};
    float r0 = 0.030f, r1 = 0.018f;
    float h0 = V.ah * 0.78f, h1 = h0 + 0.22f;
    float lx = 0.05f * cosf(inst.blushAngle * 1.7f);
    float lz = 0.05f * sinf(inst.blushAngle * 1.7f);

    int maxV = NS * 6;
    Mesh m = {0};
    m.vertices = (float*)malloc(maxV * 3 * sizeof(float));
    m.colors   = (unsigned char*)malloc(maxV * 4 * sizeof(unsigned char));
    int n = 0;
    for (int i = 0; i < NS; i++) {
        float a0 = 2*PI*i/NS, a1 = 2*PI*(i+1)/NS;
        Vector3 lp[4] = {
            {r0*cosf(a0),    h0, r0*sinf(a0)},
            {r0*cosf(a1),    h0, r0*sinf(a1)},
            {r1*cosf(a1)+lx, h1, r1*sinf(a1)+lz},
            {r1*cosf(a0)+lx, h1, r1*sinf(a0)+lz}
        };
        float am = (a0+a1)*0.5f;
        Vector3 ln = v3norm((Vector3){cosf(am), 0.4f, sinf(am)});
        Vector3 wn = Vector3Transform(ln, R);
        Color c = shade(albedo, wn);
        Vector3 wp[4];
        for (int j = 0; j < 4; j++) wp[j] = Vector3Transform(lp[j], M);
        int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int t = 0; t < 2; t++)
            for (int k = 0; k < 3; k++) {
                int idx = tri[t][k];
                m.vertices[n*3]   = wp[idx].x;
                m.vertices[n*3+1] = wp[idx].y;
                m.vertices[n*3+2] = wp[idx].z;
                m.colors[n*4]   = c.r;
                m.colors[n*4+1] = c.g;
                m.colors[n*4+2] = c.b;
                m.colors[n*4+3] = c.a;
                n++;
            }
    }
    m.vertexCount = n; m.triangleCount = n/3;
    UploadMesh(&m, false);
    return m;
}

// ── Bowl ──────────────────────────────────────────────────────────────────
// Sized so apples nestle into it with their tops above the rim.

#define APPLE_R       0.85f                        // bounding sphere radius
#define BOWL_RIM_R    2.85f                        // mouth radius
#define BOWL_WALL_R   3.18f                        // outer radius (wall thickness)
#define BOWL_RIM_Y    0.45f                        // rim height above ground
#define BOWL_DEP      1.55f                        // depth of bowl interior
#define BOWL_BOT_Y    (BOWL_RIM_Y - BOWL_DEP)      // = -1.10
#define BOWL_USABLE_R 1.85f                        // apple-centre placement disk

static float bowlYprof(float r) {
    float t = r / BOWL_RIM_R;
    if (t > 1.0f) t = 1.0f;
    float tt = t*t;
    return BOWL_BOT_Y + BOWL_DEP * tt * (2.0f - tt);
}
static float bowlYprime(float r) {
    float h = 0.005f;
    return (bowlYprof(r+h) - bowlYprof(r-h)) / (2*h);
}

static Vector3 bowlAlbedo(float r, float theta) {
    Vector3 white    = {0.96f, 0.95f, 0.92f};
    Vector3 cream    = {0.93f, 0.92f, 0.87f};
    Vector3 blueDeep = {0.18f, 0.30f, 0.55f};
    float t = r / BOWL_RIM_R;
    if (t > 0.97f) return blueDeep;
    if (t > 0.78f) {
        float petals = 0.5f + 0.5f * cosf(12.0f * theta);
        float band   = 0.5f + 0.5f * cosf((t - 0.78f) / 0.19f * 5.0f * PI);
        float p      = petals * band;
        if (p > 0.45f)
            return v3lerp(white, blueDeep, (p - 0.45f) / 0.55f * 0.85f);
        return white;
    }
    return cream;
}

#define NRB 50
#define NTB 100
static Mesh buildBowl(void) {
    int maxV = NRB*NTB*6 + NTB*12 + 64;
    Mesh m = {0};
    m.vertices = (float*)malloc(maxV * 3 * sizeof(float));
    m.colors   = (unsigned char*)malloc(maxV * 4 * sizeof(unsigned char));
    int n = 0;

    // inner concave surface
    for (int ir = 0; ir < NRB; ir++) {
        for (int it = 0; it < NTB; it++) {
            float r0 = BOWL_RIM_R*ir/NRB, r1 = BOWL_RIM_R*(ir+1)/NRB;
            if (r0 < 0.02f) r0 = 0.02f;
            float t0 = 2*PI*it/NTB, t1 = 2*PI*(it+1)/NTB;
            Vector3 lp[4] = {
                {r0*cosf(t0), bowlYprof(r0), r0*sinf(t0)},
                {r1*cosf(t0), bowlYprof(r1), r1*sinf(t0)},
                {r1*cosf(t1), bowlYprof(r1), r1*sinf(t1)},
                {r0*cosf(t1), bowlYprof(r0), r0*sinf(t1)}
            };
            float rs[4] = {r0,r1,r1,r0}, ts[4] = {t0,t0,t1,t1};
            Color cc[4];
            for (int i = 0; i < 4; i++) {
                float dy = bowlYprime(rs[i]);
                float l  = sqrtf(dy*dy + 1.0f);
                Vector3 nrm = {-dy/l*cosf(ts[i]), 1.0f/l, -dy/l*sinf(ts[i])};
                cc[i] = shade(bowlAlbedo(rs[i], ts[i]), nrm);
            }
            int tri[2][3] = {{0,1,2},{0,2,3}};
            for (int t = 0; t < 2; t++)
                for (int k = 0; k < 3; k++) {
                    int idx = tri[t][k];
                    m.vertices[n*3]   = lp[idx].x;
                    m.vertices[n*3+1] = lp[idx].y;
                    m.vertices[n*3+2] = lp[idx].z;
                    m.colors[n*4]   = cc[idx].r;
                    m.colors[n*4+1] = cc[idx].g;
                    m.colors[n*4+2] = cc[idx].b;
                    m.colors[n*4+3] = cc[idx].a;
                    n++;
                }
        }
    }
    // top lip (blue)
    for (int it = 0; it < NTB; it++) {
        float t0 = 2*PI*it/NTB, t1 = 2*PI*(it+1)/NTB;
        Vector3 lp[4] = {
            {BOWL_RIM_R *cosf(t0), BOWL_RIM_Y, BOWL_RIM_R *sinf(t0)},
            {BOWL_RIM_R *cosf(t1), BOWL_RIM_Y, BOWL_RIM_R *sinf(t1)},
            {BOWL_WALL_R*cosf(t1), BOWL_RIM_Y, BOWL_WALL_R*sinf(t1)},
            {BOWL_WALL_R*cosf(t0), BOWL_RIM_Y, BOWL_WALL_R*sinf(t0)}
        };
        Vector3 nrm = {0,1,0};
        Color c = shade((Vector3){0.18f,0.30f,0.55f}, nrm);
        int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int t = 0; t < 2; t++)
            for (int k = 0; k < 3; k++) {
                int idx = tri[t][k];
                m.vertices[n*3]   = lp[idx].x;
                m.vertices[n*3+1] = lp[idx].y;
                m.vertices[n*3+2] = lp[idx].z;
                m.colors[n*4]   = c.r;
                m.colors[n*4+1] = c.g;
                m.colors[n*4+2] = c.b;
                m.colors[n*4+3] = c.a;
                n++;
            }
    }
    // outer wall (white-cream)
    for (int it = 0; it < NTB; it++) {
        float t0 = 2*PI*it/NTB, t1 = 2*PI*(it+1)/NTB;
        float wb = BOWL_BOT_Y - 0.12f;
        Vector3 lp[4] = {
            {BOWL_WALL_R*cosf(t0), wb,         BOWL_WALL_R*sinf(t0)},
            {BOWL_WALL_R*cosf(t1), wb,         BOWL_WALL_R*sinf(t1)},
            {BOWL_WALL_R*cosf(t1), BOWL_RIM_Y, BOWL_WALL_R*sinf(t1)},
            {BOWL_WALL_R*cosf(t0), BOWL_RIM_Y, BOWL_WALL_R*sinf(t0)}
        };
        Vector3 nrms[4] = {
            v3norm((Vector3){cosf(t0),0,sinf(t0)}),
            v3norm((Vector3){cosf(t1),0,sinf(t1)}),
            v3norm((Vector3){cosf(t1),0,sinf(t1)}),
            v3norm((Vector3){cosf(t0),0,sinf(t0)})
        };
        Color cc[4];
        for (int i = 0; i < 4; i++)
            cc[i] = shade((Vector3){0.93f, 0.92f, 0.86f}, nrms[i]);
        int tri[2][3] = {{0,1,2},{0,2,3}};
        for (int t = 0; t < 2; t++)
            for (int k = 0; k < 3; k++) {
                int idx = tri[t][k];
                m.vertices[n*3]   = lp[idx].x;
                m.vertices[n*3+1] = lp[idx].y;
                m.vertices[n*3+2] = lp[idx].z;
                m.colors[n*4]   = cc[idx].r;
                m.colors[n*4+1] = cc[idx].g;
                m.colors[n*4+2] = cc[idx].b;
                m.colors[n*4+3] = cc[idx].a;
                n++;
            }
    }
    m.vertexCount = n; m.triangleCount = n/3;
    UploadMesh(&m, false);
    return m;
}

// ── Marble ────────────────────────────────────────────────────────────────

static Vector3 marbleColor(float x, float z) {
    Vector3 base  = {0.93f, 0.92f, 0.90f};
    Vector3 vein1 = {0.62f, 0.60f, 0.62f};
    Vector3 vein2 = {0.78f, 0.76f, 0.74f};
    float t1 = sinf(x*0.7f + sinf(z*1.3f)*0.5f + sinf(x*2.1f + z*0.4f)*0.3f);
    float t2 = sinf(z*0.5f + cosf(x*1.7f + z*0.9f)*0.4f);
    float v1 = fmaxf(0.0f, t1 - 0.62f) * 1.4f;
    float v2 = fmaxf(0.0f, t2 - 0.7f)  * 1.0f;
    Vector3 c = v3lerp(base, vein1, fminf(v1, 1.0f) * 0.65f);
    return v3lerp(c, vein2, fminf(v2, 1.0f) * 0.4f);
}

#define NMG 60
#define MARBLE_SIZE 9.0f
static Mesh buildMarble(void) {
    int maxV = NMG * NMG * 6;
    Mesh m = {0};
    m.vertices = (float*)malloc(maxV * 3 * sizeof(float));
    m.colors   = (unsigned char*)malloc(maxV * 4 * sizeof(unsigned char));
    int n = 0;
    float yPlane = BOWL_BOT_Y - 0.12f - 0.001f;
    Vector3 nrm = {0,1,0};
    for (int i = 0; i < NMG; i++) {
        for (int j = 0; j < NMG; j++) {
            float s = MARBLE_SIZE / NMG;
            float x0 = -MARBLE_SIZE*0.5f + i*s, x1 = x0 + s;
            float z0 = -MARBLE_SIZE*0.5f + j*s, z1 = z0 + s;
            Vector3 lp[4] = {{x0,yPlane,z0},{x1,yPlane,z0},{x1,yPlane,z1},{x0,yPlane,z1}};
            float xs[4] = {x0,x1,x1,x0}, zs[4] = {z0,z0,z1,z1};
            Color cc[4];
            for (int k = 0; k < 4; k++)
                cc[k] = shade(marbleColor(xs[k], zs[k]), nrm);
            int tri[2][3] = {{0,1,2},{0,2,3}};
            for (int t = 0; t < 2; t++)
                for (int k = 0; k < 3; k++) {
                    int idx = tri[t][k];
                    m.vertices[n*3]   = lp[idx].x;
                    m.vertices[n*3+1] = lp[idx].y;
                    m.vertices[n*3+2] = lp[idx].z;
                    m.colors[n*4]   = cc[idx].r;
                    m.colors[n*4+1] = cc[idx].g;
                    m.colors[n*4+2] = cc[idx].b;
                    m.colors[n*4+3] = cc[idx].a;
                    n++;
                }
        }
    }
    m.vertexCount = n; m.triangleCount = n/3;
    UploadMesh(&m, false);
    return m;
}

// ── Sphere placement ──────────────────────────────────────────────────────
// Treat each apple as a sphere of radius APPLE_R and pack them properly.

#define MAX_APPLES 12
#define EPS 1e-3f

// Sphere resting on bowl from above: centre at bowl_y(r) + R
static float bowlSphereY(float x, float z) {
    return bowlYprof(sqrtf(x*x + z*z)) + APPLE_R;
}

// Highest y at which a sphere centred at (x,z) does NOT overlap any of the
// already-placed apples (and also rests on the bowl if nothing else stops it).
// Equivalent to:  y = max( bowl_y(r)+R ,  max_i  y_i + sqrt((2R)^2 − d_xz^2) )
static float restingY(float x, float z, AppleInst *ap, int n) {
    float y = bowlSphereY(x, z);
    const float twoR2 = 4.0f * APPLE_R * APPLE_R;
    for (int i = 0; i < n; i++) {
        float dx = x - ap[i].pos.x;
        float dz = z - ap[i].pos.z;
        float d2 = dx*dx + dz*dz;
        if (d2 < twoR2) {
            float y_i = ap[i].pos.y + sqrtf(twoR2 - d2);
            if (y_i > y) y = y_i;
        }
    }
    return y;
}

// Verify a candidate doesn't intersect any placed apple (3D check)
static int sphereOK(float x, float y, float z, AppleInst *ap, int n) {
    const float twoR2 = 4.0f * APPLE_R * APPLE_R - 0.001f;  // tiny tolerance
    for (int i = 0; i < n; i++) {
        float dx = x - ap[i].pos.x;
        float dy = y - ap[i].pos.y;
        float dz = z - ap[i].pos.z;
        if (dx*dx + dy*dy + dz*dz < twoR2) return 0;
    }
    return 1;
}

static AppleInst makeApple(float x, float y, float z) {
    int vi = (int)(rf() * 3.0f); if (vi >= 3) vi = 2;
    float bs = 0.95f + rf() * 0.08f;       // 0.95..1.03 — modest variation
    return (AppleInst){
        .pos = {x, y, z},
        .yaw = rf() * 2*PI,
        .tilt = 0.04f + rf() * 0.16f,
        .tiltDir = rf() * 2*PI,
        .scale = {bs, bs * (0.96f + rf()*0.07f), bs},
        .blushAngle = rf() * 2*PI,
        .variant = vi
    };
}

static int placeApples(AppleInst *out) {
    int n = 0;

    // ── Bottom layer: 2D Poisson disk in the bowl-mouth disk ────────────
    // 2D distance ≥ 2R guarantees 3D distance ≥ 2R when both rest on bowl.
    int wantBottom = 7;
    for (int a = 0; a < 25000 && n < wantBottom; a++) {
        float ang = rf() * 2.0f * PI;
        float r   = BOWL_USABLE_R * sqrtf(rf());
        float x   = r * cosf(ang);
        float z   = r * sinf(ang);

        // 2D Poisson check (centres ≥ 2R + ε apart)
        int ok = 1;
        const float minD2 = (2.0f*APPLE_R + EPS) * (2.0f*APPLE_R + EPS);
        for (int i = 0; i < n; i++) {
            float dx = x - out[i].pos.x;
            float dz = z - out[i].pos.z;
            if (dx*dx + dz*dz < minD2) { ok = 0; break; }
        }
        if (!ok) continue;

        float y = bowlSphereY(x, z);
        if (!sphereOK(x, y, z, out, n)) continue;     // belt and braces
        out[n] = makeApple(x, y, z);
        n++;
    }

    // ── Top layer: rest on existing apples, not on bowl ─────────────────
    int wantTop = 3;
    int bottomCount = n;
    for (int a = 0; a < 15000 && n < bottomCount + wantTop; a++) {
        float ang = rf() * 2.0f * PI;
        float r   = rf() * 1.10f;                 // sample near centre
        float x   = r * cosf(ang);
        float z   = r * sinf(ang);

        float y       = restingY(x, z, out, n);
        float bowlOnly = bowlSphereY(x, z);

        // require the sphere to rest ON apples, not just on the bowl
        if (y < bowlOnly + 0.10f) continue;

        // it must be tangent to ≥ 2 existing apples (geometric stability)
        int supporters = 0;
        const float twoR2 = 4.0f * APPLE_R * APPLE_R;
        for (int i = 0; i < n; i++) {
            float dx = x - out[i].pos.x;
            float dy = y - out[i].pos.y;
            float dz = z - out[i].pos.z;
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 - twoR2 < 0.04f) supporters++;
        }
        if (supporters < 2) continue;

        // limit how high the pile can climb
        if (y - APPLE_R > BOWL_RIM_Y + 1.20f) continue;

        if (!sphereOK(x, y, z, out, n)) continue;
        out[n] = makeApple(x, y, z);
        n++;
    }
    return n;
}

// ── Main ──────────────────────────────────────────────────────────────────

int main(void) {
    srand(11);
    const int W = 1280, H = 800;
    InitWindow(W, H, "Apples in a Bowl");
    SetTargetFPS(60);

    Mesh marble = buildMarble();
    Mesh bowl   = buildBowl();

    AppleInst apples[MAX_APPLES];
    int nA = placeApples(apples);

    Mesh aMesh[MAX_APPLES], sMesh[MAX_APPLES];
    for (int i = 0; i < nA; i++) {
        aMesh[i] = buildAppleInstance(apples[i]);
        sMesh[i] = buildStemInstance(apples[i]);
    }

    Material mat = LoadMaterialDefault();
    mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    float camYaw   = 0.85f;
    float camPitch = 0.42f;       // ~24° — three-quarter still-life
    float camDist  = 11.5f;
    Camera3D cam = {.target = {0, 0.10f, 0}, .up = {0,1,0},
                    .fovy = 36.0f, .projection = CAMERA_PERSPECTIVE};

    while (!WindowShouldClose()) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 d = GetMouseDelta();
            camYaw  -= d.x * 0.006f;
            camPitch -= d.y * 0.006f;
            if (camPitch < 0.05f) camPitch = 0.05f;
            if (camPitch > 1.40f) camPitch = 1.40f;
        }
        float wh = GetMouseWheelMove();
        camDist = fmaxf(5.0f, fminf(25.0f, camDist - wh*0.7f));
        cam.position = (Vector3){
            cosf(camYaw)*cosf(camPitch)*camDist,
            sinf(camPitch)*camDist,
            sinf(camYaw)*cosf(camPitch)*camDist
        };

        BeginDrawing();
        ClearBackground((Color){38, 35, 42, 255});
        BeginMode3D(cam);
        rlDisableBackfaceCulling();

        DrawMesh(marble, mat, MatrixIdentity());
        DrawMesh(bowl,   mat, MatrixIdentity());
        for (int i = 0; i < nA; i++) {
            DrawMesh(aMesh[i], mat, MatrixIdentity());
            DrawMesh(sMesh[i], mat, MatrixIdentity());
        }

        rlEnableBackfaceCulling();
        EndMode3D();

        DrawText("Apples in a Bowl", 22, H-44, 18, (Color){220,215,205,220});
        DrawText("parametric surfaces · normal field · Lambert shading",
                 22, H-22, 12, (Color){170,160,150,180});

        EndDrawing();
    }

    UnloadMesh(marble); UnloadMesh(bowl);
    for (int i = 0; i < nA; i++) { UnloadMesh(aMesh[i]); UnloadMesh(sMesh[i]); }
    UnloadMaterial(mat);
    CloseWindow();
    return 0;
}
