// shared.h — code compiled both by clang++ (host) and by the Metal shader compiler.
// Everything that decides *where things are* (noise, ecology zones, far-forest tree cells)
// lives here so the CPU scene builder and the GPU shader always agree.
#ifndef SHARED_H
#define SHARED_H

#ifdef __METAL_VERSION__
  #define V2(a,b) float2(a,b)
  #define V3(a,b,c) float3(a,b,c)
  #define SH_FLOOR(x) floor(x)
  #define SH_SQRT(x) sqrt(x)
  #define SH_EXP(x) exp(x)
  #define SH_SIN(x) sin(x)
  #define SH_COS(x) cos(x)
  #define SH_ABS(x) fabs(x)
  #define SH_POW(x,y) pow(x,y)
#else
  #include <simd/simd.h>
  #include <cmath>
  #include <cstdint>
  typedef simd_float2 float2; typedef simd_float3 float3; typedef simd_float4 float4;
  typedef uint32_t uint;
  #define V2(a,b) simd_make_float2(a,b)
  #define V3(a,b,c) simd_make_float3(a,b,c)
  #define SH_FLOOR(x) floorf(x)
  #define SH_SQRT(x) sqrtf(x)
  #define SH_EXP(x) expf(x)
  #define SH_SIN(x) sinf(x)
  #define SH_COS(x) cosf(x)
  #define SH_ABS(x) fabsf(x)
  #define SH_POW(x,y) powf(x,y)
#endif

static inline float sh_min(float a, float b) { return a < b ? a : b; }
static inline float sh_max(float a, float b) { return a > b ? a : b; }
static inline float sh_clamp(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float sh_sat(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static inline float sh_mix(float a, float b, float t) { return a + (b - a) * t; }
static inline float sh_smooth(float e0, float e1, float x) { float t = sh_sat((x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }

// ---------- integer hashes ----------
static inline uint sh_hu(uint x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline uint sh_hc2(int x, int y, uint s) { return sh_hu(uint(x) * 0x9E3779B1U ^ sh_hu(uint(y) * 0x85EBCA6BU + s)); }
static inline uint sh_hc3(int x, int y, int z, uint s) { return sh_hu(uint(x) * 0x9E3779B1U ^ sh_hu(uint(y) * 0x85EBCA6BU ^ sh_hu(uint(z) * 0xC2B2AE35U + s))); }
static inline float sh_u2f(uint h) { return float(h >> 8) * (1.0f / 16777216.0f); }

// ---------- value noise, quintic, range 0..1 ----------
static inline float sh_vn2(float x, float y, uint s) {
    float fx0 = SH_FLOOR(x), fy0 = SH_FLOOR(y); int ix = int(fx0), iy = int(fy0);
    float fx = x - fx0, fy = y - fy0;
    float ux = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f), uy = fy * fy * fy * (fy * (fy * 6.0f - 15.0f) + 10.0f);
    float a = sh_u2f(sh_hc2(ix, iy, s)), b = sh_u2f(sh_hc2(ix + 1, iy, s)), c = sh_u2f(sh_hc2(ix, iy + 1, s)), d = sh_u2f(sh_hc2(ix + 1, iy + 1, s));
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
static inline float sh_vn3(float x, float y, float z, uint s) {
    float fx0 = SH_FLOOR(x), fy0 = SH_FLOOR(y), fz0 = SH_FLOOR(z); int ix = int(fx0), iy = int(fy0), iz = int(fz0);
    float fx = x - fx0, fy = y - fy0, fz = z - fz0;
    float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy), uz = fz * fz * (3.0f - 2.0f * fz);
    float a = sh_u2f(sh_hc3(ix, iy, iz, s)), b = sh_u2f(sh_hc3(ix + 1, iy, iz, s)), c = sh_u2f(sh_hc3(ix, iy + 1, iz, s)), d = sh_u2f(sh_hc3(ix + 1, iy + 1, iz, s));
    float e = sh_u2f(sh_hc3(ix, iy, iz + 1, s)), f = sh_u2f(sh_hc3(ix + 1, iy, iz + 1, s)), g = sh_u2f(sh_hc3(ix, iy + 1, iz + 1, s)), h = sh_u2f(sh_hc3(ix + 1, iy + 1, iz + 1, s));
    float k0 = a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
    float k1 = e + (f - e) * ux + (g - e) * uy + (e - f - g + h) * ux * uy;
    return k0 + (k1 - k0) * uz;
}
// fbm with rotation between octaves, range ~0..1
static inline float sh_fbm2(float x, float y, int oct, uint s) {
    float a = 0.5f, f = 0.0f, n = 0.0f;
    for (int i = 0; i < oct; i++) {
        f += a * sh_vn2(x, y, s + uint(i) * 17U); n += a; a *= 0.5f;
        float nx = 1.6f * x + 1.2f * y + 3.1f, ny = -1.2f * x + 1.6f * y + 1.7f; x = nx; y = ny;
    }
    return f / n;
}
static inline float sh_fbm3(float x, float y, float z, int oct, uint s) {
    float a = 0.5f, f = 0.0f, n = 0.0f;
    for (int i = 0; i < oct; i++) {
        f += a * sh_vn3(x, y, z, s + uint(i) * 17U); n += a; a *= 0.5f;
        float nx = 1.6f * x + 1.2f * z + 3.1f, nz = -1.2f * x + 1.6f * z + 1.7f; x = nx; z = nz; y = y * 2.0f + 5.3f;
    }
    return f / n;
}
// ridged value noise 0..1 (1 on ridges)
static inline float sh_ridge2(float x, float y, uint s) { float v = sh_vn2(x, y, s); return 1.0f - SH_ABS(2.0f * v - 1.0f); }

// 2D Worley: returns F1 in .x, F2 in .y, cell hash in .z (as float 0..1)
static inline float3 sh_worley2(float x, float y, uint s) {
    float fx0 = SH_FLOOR(x), fy0 = SH_FLOOR(y); int ix = int(fx0), iy = int(fy0);
    float f1 = 9.0f, f2 = 9.0f, id = 0.0f;
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
        uint h = sh_hc2(ix + i, iy + j, s);
        float px = float(ix + i) + sh_u2f(h), py = float(iy + j) + sh_u2f(sh_hu(h + 77U));
        float dx = px - x, dy = py - y, d = dx * dx + dy * dy;
        if (d < f1) { f2 = f1; f1 = d; id = sh_u2f(sh_hu(h + 991U)); } else if (d < f2) f2 = d;
    }
    return V3(SH_SQRT(f1), SH_SQRT(f2), id);
}

// ---------- ecology: what grows / lies where (alt in metres, ny = normal.y, north = -normal.z projected) ----------
struct Eco {
    float forest;   // tree cover probability
    float larch;    // fraction of trees that are larch (gold)
    float rock;     // bare bedrock
    float scree;    // loose talus
    float snow;     // fresh snow
    float grass;    // alpine turf
};
#define ECO_CLOUDTOP 1450.0f
static inline Eco sh_eco(float alt, float ny, float north, float hard, float flow, float n1, float n2) {
    Eco e;
    float steep = 1.0f - ny;                      // 0 flat .. 1 vertical
    float tl = 2110.0f + 110.0f * (n1 - 0.5f) - 70.0f * north;        // treeline
    e.rock = sh_sat(sh_smooth(0.26f, 0.42f, steep + 0.10f * hard + 0.08f * (n2 - 0.5f))
                    + sh_smooth(2300.0f, 2750.0f, alt) * sh_smooth(0.10f, 0.25f, steep) * 0.8f
                    + sh_smooth(2330.0f, 2470.0f, alt + 160.0f * (n2 - 0.5f)) * 0.8f);
    float screeBand = sh_smooth(0.12f, 0.20f, steep) * (1.0f - sh_smooth(0.26f, 0.36f, steep));
    e.scree = sh_sat(screeBand * sh_smooth(2150.0f, 2400.0f, alt + 300.0f * (n2 - 0.5f)) * 1.3f) * (1.0f - e.rock);
    float sl = 2400.0f - 200.0f * north + 140.0f * (n1 - 0.5f);
    e.snow = sh_smooth(sl, sl + 150.0f, alt) * sh_smooth(0.42f, 0.22f, steep);
    float f = (1.0f - sh_smooth(tl - 130.0f, tl, alt)) * sh_smooth(0.36f, 0.27f, steep) * (1.0f - sh_smooth(0.55f, 0.75f, flow));
    f *= sh_smooth(0.22f, 0.40f, n2 + 0.25f * (1.0f - sh_smooth(tl - 350.0f, tl - 60.0f, alt)));   // glades appear near treeline
    e.forest = sh_sat(f) * (1.0f - e.rock);
    e.larch = sh_sat(sh_smooth(1780.0f, 2050.0f, alt + 220.0f * (n1 - 0.5f)) * 0.9f + 0.06f);
    e.grass = sh_sat(1.0f - e.rock - e.scree - e.snow);
    return e;
}

// ---------- far-forest tree cells (used by the shader's canopy height field and by the CPU instancer) ----------
#define TREE_CELL 5.5f
struct TreeCell { float px, pz, height, radius, kind, rnd; };   // kind: 0 spruce 1 larch
static inline TreeCell sh_treeCell(int cx, int cz) {
    uint h = sh_hc2(cx, cz, 9001U); TreeCell t;
    t.px = (float(cx) + sh_u2f(h)) * TREE_CELL; t.pz = (float(cz) + sh_u2f(sh_hu(h + 1U))) * TREE_CELL;
    float r = sh_u2f(sh_hu(h + 2U)); t.rnd = sh_u2f(sh_hu(h + 3U)); t.kind = sh_u2f(sh_hu(h + 4U));
    t.height = 14.0f + 16.0f * r * (0.5f + 0.5f * r); t.radius = t.height * (0.13f + 0.05f * t.rnd);
    return t;
}
// ---------- terrain relief below the resolution of the eroded map ----------
// meso: 6..40 m rock ribs and jointed blocks, sampled on planes tilted with height so that
// features dip instead of lying in horizontal bands. Range about -1..1.
#define MESO_MAX 11.0f
static inline float sh_meso(float x, float z, float hb) {
    float xt = x + 0.55f * hb, zt = z + 0.30f * hb;
    float r = 0.60f * sh_ridge2(xt / 41.0f, zt / 41.0f, 11U) + 0.40f * sh_vn2((0.8f * xt + 0.6f * zt) / 17.0f, (-0.6f * xt + 0.8f * zt) / 17.0f, 12U);
    float gul = sh_ridge2((0.92f * xt + 0.39f * zt) / 150.0f, (-0.39f * xt + 0.92f * zt) / 95.0f, 15U);
    return (r - 0.55f) * 0.75f + (gul - 0.55f) * 0.95f;
}
// detail: 0.15..2.2 m lumps (metres, before the per-material roughness factor). t = viewing distance
// for footprint fading (0 on the CPU).
static inline float sh_detail(float x, float z, float t) {
    float fp = t * 0.0035f + 1e-4f;      // ~3 pixels at 1080p / 70 deg
    float d = 0.0f;
    d += 0.110f * (sh_vn2(x / 2.2f, z / 2.2f, 21U) - 0.5f) * sh_sat(2.2f / fp - 1.0f);
    d += 0.060f * (sh_vn2(x / 0.9f + 7.0f, z / 0.9f, 22U) - 0.5f) * sh_sat(0.9f / fp - 1.0f);
    float f3 = sh_sat(0.37f / fp - 1.0f); if (f3 > 0.0f) d += 0.030f * (sh_vn2(x / 0.37f, z / 0.37f + 3.0f, 23U) - 0.5f) * f3;
    float f4 = sh_sat(0.15f / fp - 1.0f); if (f4 > 0.0f) d += 0.014f * (sh_vn2(x / 0.15f, z / 0.15f, 24U) - 0.5f) * f4;
    return d;
}
// ---------- GPU <-> host structures ----------
struct Uniforms {
    float4 camPos0, camPos1;                       // shutter open / close
    float4 camR0, camU0, camF0, camR1, camU1, camF1;
    float4 lens;       // x tan(half hfov), y height/width, z distortion k, w time (s)
    float4 sunDir;     // xyz, w = angular radius (rad)
    float4 sunE;       // top-of-atmosphere irradiance rgb, w = mie scale
    float4 skyAmb;     // sky irradiance on an open horizontal surface rgb, w = occlusion guess for 2nd-bounce ambient
    float4 clip0, clip1, clip2, clip3;             // xy = world pos of texture corner, z = 1/size (1/m), w = texel (m)
    float4 mist;       // x density (/km) y reference altitude z scale height (m) w patchiness
    float4 cloud;      // x top altitude y density z time w unused
    float4 shot;       // xy shot centre, z radius inside which trees are real geometry, w camera altitude (km)
    float4 wind;       // xy direction, z strength, w gust frequency
    float4 misc;       // x cirrus cover 0..1, y debug mode, z water level enable, w per-sample radiance clamp
    float4 origin;     // world position that all shader-space positions are relative to (precision)
    uint width, height, spp0, sppN;                // this dispatch renders samples spp0 .. spp0+sppN-1
    uint frame, tlasCount, flags, sppTotal;
};
#define ATMO_N 64
#define ATMO_HMAX 9.0f
// ---------- fractured rock: tilted planar facets separated by joints (returns height m, edge distance, cell id) ----------
static inline float3 sh_facet(float x, float z, float lam, float rot, float joint, uint s) {
    float cr = SH_COS(rot), sr = SH_SIN(rot);
    float u = (cr * x + sr * z) / lam, v = (-sr * x + cr * z) / (lam * 0.62f);
    float fu = SH_FLOOR(u), fv = SH_FLOOR(v); int iu = int(fu), iv = int(fv);
    float f1 = 9.0f, f2 = 9.0f, cu = 0.0f, cv = 0.0f; uint hid = 0U;
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
        uint h = sh_hc2(iu + i, iv + j, s);
        float px = float(iu + i) + 0.15f + 0.7f * sh_u2f(h), py = float(iv + j) + 0.15f + 0.7f * sh_u2f(sh_hu(h + 77U));
        float dx = px - u, dy = py - v, d = dx * dx + dy * dy;
        if (d < f1) { f2 = f1; f1 = d; cu = px; cv = py; hid = h; } else if (d < f2) f2 = d;
    }
    float gx = (sh_u2f(sh_hu(hid + 3U)) - 0.5f) * 1.3f, gz = (sh_u2f(sh_hu(hid + 4U)) - 0.5f) * 1.3f, off = sh_u2f(sh_hu(hid + 5U)) - 0.5f;
    float edge = SH_SQRT(f2) - SH_SQRT(f1);
    float h = off * 0.9f + gx * (u - cu) + gz * (v - cv);
    h -= joint * (1.0f - sh_smooth(0.0f, 0.07f, edge));          // open joint
    return V3(h * lam * 0.42f, edge, sh_u2f(sh_hu(hid + 6U)));
}
static inline float sh_rock(float x, float z, float t) {
    float fp = t * 0.0035f + 1e-4f; float h = 0.0f;
    h += 0.42f * sh_facet(x, z, 5.5f, 0.5f, 0.10f, 201U).x * sh_sat(5.5f / fp - 1.0f);
    float f2 = sh_sat(1.7f / fp - 1.0f); if (f2 > 0.0f) h += 0.65f * sh_facet(x + 13.0f, z, 1.7f, 1.3f, 0.22f, 202U).x * f2;
    float f3 = sh_sat(0.5f / fp - 1.0f); if (f3 > 0.0f) h += sh_facet(x, z + 7.0f, 0.5f, 2.2f, 0.32f, 203U).x * f3;
    return h;
}
#define ROCK_MAXH 1.3f
// surface roots on the forest floor: winding ridges, 0..1 profile
static inline float sh_roots(float x, float z) {
    float a = sh_fbm2(x / 2.6f, z / 2.6f, 2, 211U) - 0.5f, b = sh_fbm2(x / 1.9f + 9.0f, z / 1.9f, 2, 212U) - 0.5f;
    float wa = 0.020f + 0.018f * sh_vn2(x / 0.9f, z / 0.9f, 215U), wb = 0.014f + 0.012f * sh_vn2(x / 0.7f + 4.0f, z / 0.7f, 216U);
    float ra = sh_sat(1.0f - SH_ABS(a) / wa), rb = sh_sat(1.0f - SH_ABS(b) / wb);
    float r = sh_max(SH_SQRT(ra), 0.7f * SH_SQRT(rb));
    float patch = sh_smooth(0.52f, 0.66f, sh_vn2(x / 5.5f, z / 5.5f, 213U));
    return r * patch;
}
// stones bedded in the trail tread: returns height 0..1 (x), stone id (y)
static inline float2 sh_treadStones(float x, float z) {
    float3 w = sh_worley2(x / 0.34f, z / 0.34f, 214U); float on = w.z > 0.74f ? 1.0f : 0.0f;
    float h = on * sh_min(1.0f, 1.8f * (1.0f - sh_smooth(0.05f, 0.40f, w.x))); return V2(h, w.z);
}
// buttresses and couloirs on big rock faces (metres, before the steepness weight)
#define BIGROCK_MAX 30.0f
static inline float sh_bigrock(float x, float z) {
    float a = sh_ridge2(x / 240.0f, z / 240.0f, 221U), b = sh_ridge2((0.8f * x + 0.6f * z) / 97.0f + 3.0f, (-0.6f * x + 0.8f * z) / 97.0f, 222U);
    return BIGROCK_MAX * (a * (0.55f + 0.45f * b) - 0.42f);
}
#endif
