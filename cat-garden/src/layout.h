// layout.h — garden plan: regions, ground height, hero positions and the cat's route keys
#pragma once
#include "base.h"

namespace G {
// ---- extents (metres). x: east, z: north, y: up
static const float X0 = -3.0f, X1 = 37.0f, Z0 = -4.0f, Z1 = 28.0f;
static const float WATER_Y = -0.07f;
// pond
static const float PCX = 19.6f, PCZ = 8.2f, PA = 3.3f, PB = 2.5f;
// greenhouse footprint
static const float GX0 = 15.0f, GX1 = 29.0f, GZ0 = 14.0f, GZ1 = 21.0f, GEAVE = 2.1f, GRIDGE = 3.7f, GDWARF = 0.42f;
static const float DOOR_SX = 18.0f;   // south door centre x (at z = GZ0)
static const float DOOR_EZ = 17.5f;   // east door centre z (at x = GX1)
static const float ARID_X = 23.2f;    // tropical | arid boundary
// end wall (dry stone), cat finishes on top of it
static const float WALL_X0 = 29.6f, WALL_X1 = 34.0f, WALL_Z0 = 11.55f, WALL_Z1 = 11.98f, WALL_H = 0.50f;
// flower border
static const float BED_X0 = 9.2f, BED_X1 = 12.58f, BED_Z0 = 1.0f, BED_Z1 = 13.6f;
// heroes
static const V3 HERO_BLADE(2.52f, 0, 6.93f);
static const V3 CACTUS(25.32f, 0, 16.70f);
static const float HUMAN_X = 13.92f;

static inline float pondSD(float x, float z) { // metres, <0 inside the water
  float dx = (x - PCX) / PA, dz = (z - PCZ) / PB;
  float r = sqrtf(dx * dx + dz * dz), th = atan2f(dz, dx);
  float R = 1.0f + 0.06f * sinf(3 * th + 1.0f) + 0.035f * sinf(5 * th + 2.3f);
  return (r - R) * 2.7f;
}
static inline float shoreX(float z) { // west shore x at given z
  float a = 14.5f, b = PCX;
  for (int i = 0; i < 40; i++) { float m = 0.5f * (a + b); if (pondSD(m, z) > 0) a = m; else b = m; }
  return 0.5f * (a + b);
}
static inline float pathSD(float x, float z) {
  float wob = 0.05f * (vnoise(V3(x * 1.3f, 5.0f, z * 1.3f)) - 0.5f);
  float m = fabsf(x - 13.6f) - 0.92f + wob;
  float b = fmaxf(fabsf(z - 13.0f) - 0.70f + wob, fmaxf(14.0f - x, x - 24.2f));
  float s = fmaxf(fabsf(x - DOOR_SX) - 0.62f, fmaxf(13.5f - z, z - 14.05f));
  return fminf(m, fminf(b, s));
}
static inline float bedSD(float x, float z) {
  float w = BED_X0 + 0.28f * sinf(z * 0.85f + 0.6f) + 0.1f * sinf(z * 2.3f);
  return fmaxf(fmaxf(w - x, x - BED_X1), fmaxf(BED_Z0 - z, z - BED_Z1));
}
static inline bool inGH(float x, float z, float m = 0) { return x > GX0 - m && x < GX1 + m && z > GZ0 - m && z < GZ1 + m; }
static inline float hedgeSD(float x, float z) { return fmaxf(fabsf(x + 0.30f) - 0.42f, fmaxf(-2.5f - z, z - 26.5f)); }

static inline float groundH(float x, float z) {
  float h = 0.022f * (fbm(V3(x * 0.33f, 0, z * 0.33f), 3) - 0.47f) * 2 + 0.007f * (fbm(V3(x * 2.3f, 1, z * 2.3f), 2) - 0.47f) * 2;
  float pm = smooth(0.06f, -0.05f, pathSD(x, z));
  h = lerpf(h, -0.022f, pm);
  float bm = smooth(0.04f, -0.12f, bedSD(x, z));
  h += bm * (0.04f + 0.022f * (fbm(V3(x * 8, 2, z * 8), 3) - 0.47f) * 2);
  float hm = smooth(0.15f, -0.1f, hedgeSD(x, z));
  h += hm * (0.015f + 0.02f * (fbm(V3(x * 6, 3, z * 6), 3) - 0.47f) * 2);
  float pd = pondSD(x, z);
  if (pd < 0.45f) {
    h = lerpf(h, 0.0f, smooth(0.45f, 0.15f, pd));
    if (pd < 0.12f) {
      float bank = smooth(0.12f, -0.02f, pd);
      float dep = 0.10f * bank + fminf(0.50f, fmaxf(-pd, 0.0f) * 0.55f) + 0.03f * (fbm(V3(x * 3, 9, z * 3), 3) - 0.5f) * smooth(0.0f, -0.4f, pd);
      h = -dep;
    }
  }
  if (inGH(x, z, 0.12f)) h = 0.0f;
  return h;
}

// ---- cat route keys: t, x, z, h (eye height above ground unless abs)
struct PK { float t, x, z, h; bool abs; };
static inline std::vector<PK> routeKeys() {
  float zs = 8.12f, xs = shoreX(zs);
  float wy = WALL_H + 0.045f;
  std::vector<PK> k = {
      {0.0f, -1.00f, 6.02f, 0.105f, false},
      {1.5f, -0.90f, 6.025f, 0.100f, false},
      {2.7f, -0.60f, 6.03f, 0.098f, false},
      {3.3f, -0.50f, 6.035f, 0.100f, false},
      {4.3f, 0.02f, 6.07f, 0.125f, false},
      {5.6f, 0.60f, 6.14f, 0.175f, false},
      {6.9f, 1.30f, 6.32f, 0.215f, false},
      {8.3f, 2.14f, 6.60f, 0.220f, false},
      {9.3f, 2.552f, 6.846f, 0.147f, false},
      {12.7f, 2.560f, 6.850f, 0.146f, false},
      {13.5f, 2.62f, 6.80f, 0.210f, false},
      {15.0f, 3.60f, 6.95f, 0.220f, false},
      {17.2f, 5.55f, 7.06f, 0.220f, false},
      {18.6f, 6.72f, 6.88f, 0.210f, false},
      {19.5f, 7.02f, 6.80f, 0.170f, false},
      {20.9f, 7.08f, 6.80f, 0.175f, false},
      {22.9f, 8.75f, 6.98f, 0.210f, false},
      {24.0f, 9.60f, 7.08f, 0.160f, false},
      {25.3f, 10.22f, 7.21f, 0.170f, false},
      {29.7f, 10.27f, 7.23f, 0.185f, false},
      {31.1f, 11.20f, 7.35f, 0.190f, false},
      {32.5f, 12.16f, 7.33f, 0.200f, false},
      {33.9f, 12.49f, 7.335f, 0.105f, false},
      {35.1f, 12.45f, 7.33f, 0.215f, false},
      {36.0f, 12.47f, 7.34f, 0.240f, false},
      {39.7f, 12.48f, 7.34f, 0.232f, false},
      {40.5f, 12.80f, 7.40f, 0.215f, false},
      {42.1f, 14.30f, 7.74f, 0.205f, false},
      {43.1f, 15.02f, 7.94f, 0.185f, false},
      {45.1f, 15.12f, 7.98f, 0.190f, false},
      {46.5f, xs - 0.52f, 8.08f, 0.215f, false},
      {47.5f, xs - 0.22f, zs, 0.215f, false},
      {52.6f, xs - 0.21f, zs, 0.225f, false},
      {54.2f, xs + 0.02f, zs + 0.01f, WATER_Y + 0.062f, true},
      {58.6f, xs + 0.025f, zs + 0.01f, WATER_Y + 0.058f, true},
      {59.05f, xs - 0.34f, zs - 0.03f, 0.245f, false},
      {60.2f, xs - 0.38f, zs + 0.05f, 0.220f, false},
      {61.6f, xs - 0.30f, 9.10f, 0.215f, false},
      {63.6f, 16.75f, 11.30f, 0.215f, false},
      {65.1f, 17.75f, 12.75f, 0.215f, false},
      {66.3f, 18.00f, 13.72f, 0.205f, false},
      {68.3f, 18.00f, 14.15f, 0.210f, false},
      {70.0f, 18.04f, 15.55f, 0.215f, false},
      {71.6f, 18.50f, 17.15f, 0.215f, false},
      {73.2f, 20.00f, 17.50f, 0.215f, false},
      {74.5f, 20.92f, 17.30f, 0.180f, false},
      {77.7f, 21.00f, 17.28f, 0.200f, false},
      {79.6f, 23.20f, 17.50f, 0.215f, false},
      {81.0f, 24.85f, 17.36f, 0.210f, false},
      {82.1f, CACTUS.x - 0.09f, CACTUS.z + 0.36f, 0.165f, false},
      {83.6f, CACTUS.x - 0.06f, CACTUS.z + 0.245f, 0.150f, false},
      {84.0f, CACTUS.x - 0.055f, CACTUS.z + 0.225f, 0.150f, false},
      {84.28f, CACTUS.x - 0.17f, CACTUS.z + 0.56f, 0.225f, false},
      {85.6f, CACTUS.x - 0.20f, CACTUS.z + 0.66f, 0.210f, false},
      {86.6f, 25.30f, 17.50f, 0.150f, false},
      {87.3f, 25.62f, 17.50f, 0.140f, false},
      {88.55f, 27.95f, 17.50f, 0.200f, false},
      {88.95f, 28.72f, 17.50f, 0.430f, false},
      {89.35f, 29.42f, 17.50f, 0.140f, false},
      {90.4f, 29.70f, 17.45f, 0.225f, false},
      {92.4f, 29.80f, 17.10f, 0.230f, false},
      {94.5f, 29.35f, 15.00f, 0.220f, false},
      {96.4f, 29.02f, 11.55f, 0.215f, false},
      {97.4f, 28.98f, 10.72f, 0.130f, false},
      {97.72f, 28.99f, 10.70f, 0.180f, false},
      {98.00f, 29.30f, 10.64f, wy + 0.36f, true},
      {98.26f, 29.60f, 10.60f, wy + 0.12f, true},
      {98.9f, 29.61f, 10.60f, wy + 0.215f, true},
      {100.6f, 29.62f, 10.60f, wy + 0.250f, true},
      {103.4f, 29.62f, 10.60f, wy + 0.245f, true},
      {105.2f, 29.60f, 10.60f, wy + 0.105f, true},
      {112.0f, 29.60f, 10.60f, wy + 0.095f, true},
  };
  return k;
}
static const float T_END = 110.0f;

// distance from (x,z) to the cat's route polyline (cached on a coarse grid)
struct RouteDist {
  int W, H; float res = 0.1f; std::vector<float> d;
  RouteDist() {
    auto k = routeKeys();
    W = (int)((X1 - X0) / res) + 1; H = (int)((Z1 - Z0) / res) + 1; d.assign((size_t)W * H, 1e9f);
    for (int j = 0; j < H; j++)
      for (int i = 0; i < W; i++) {
        float x = X0 + i * res, z = Z0 + j * res, best = 1e9f;
        for (size_t s = 0; s + 1 < k.size(); s++) {
          float ax = k[s].x, az = k[s].z, bx = k[s + 1].x - ax, bz = k[s + 1].z - az;
          float l2 = bx * bx + bz * bz, t = l2 > 1e-9f ? clampf(((x - ax) * bx + (z - az) * bz) / l2, 0, 1) : 0;
          float ex = x - ax - bx * t, ez = z - az - bz * t;
          best = fminf(best, ex * ex + ez * ez);
        }
        d[(size_t)j * W + i] = sqrtf(best);
      }
  }
  float at(float x, float z) const {
    float fx = clampf((x - X0) / res, 0, W - 1.001f), fz = clampf((z - Z0) / res, 0, H - 1.001f);
    int i = (int)fx, j = (int)fz; float a = fx - i, b = fz - j;
    return lerpf(lerpf(d[(size_t)j * W + i], d[(size_t)j * W + i + 1], a), lerpf(d[(size_t)(j + 1) * W + i], d[(size_t)(j + 1) * W + i + 1], a), b);
  }
};
} // namespace G
