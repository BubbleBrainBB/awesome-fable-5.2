// world.h — assembles the botanical garden: ground, scatter, structures
#pragma once
#include "geom.h"
#include "layout.h"
#include "plants.h"

struct CreatureProtos {
  int ladyBody = -1, ladyWing = -1, beeBody = -1, beeWing = -1, bfBody = -1, bfWing = -1, bfBody2 = -1, bfWing2 = -1, dfBody = -1, dfWing = -1;
  int koiSeg[5] = {-1, -1, -1, -1, -1}, koiTail = -1, koiFin = -1;
  int shoe = -1, shin = -1, thigh = -1, ant = -1, catProxy = -1, whisker = -1, paw = -1, drop = -1, heroBlade = -1, robin = -1, robinWing = -1;
};
struct World {
  // lighting / atmosphere
  V3 sunDir = norm(V3(0.80f, 0.47f, -0.37f));
  float sunRadius = 0.011f;
  V3 sunCol = V3(1.0f, 0.86f, 0.66f) * 5.2f;
  float skyGain = 1.0f, wet = 1.0f, fogIn = 0.026f, fogOut = 0.0045f;
  float hfov = 96.0f * PI / 180.0f;
  V3 ghLo = V3(G::GX0, -0.5f, G::GZ0), ghHi = V3(G::GX1, G::GRIDGE, G::GZ1);
  float waterY = G::WATER_Y;
  float windAt(double t) const { return 0.62f + 0.30f * sinf((float)t * 0.31f + 1.0f) * sinf((float)t * 0.113f + 0.4f) + 0.12f * sinf((float)t * 0.9f); }
  // ground map
  int mapW = 0, mapH = 0; float mapRes = 0.025f, mapX0 = G::X0, mapZ0 = G::Z0;
  std::vector<uint8_t> map;
  int labW = 4, labH = 4; std::vector<uint8_t> lab = std::vector<uint8_t>(16, 0);
  int labRows = 16;
  CreatureProtos cp;
  G::RouteDist rd;
  // hero data shared with the animation code
  std::vector<V3> heroBladePts;   // world-space midrib of the ladybird's grass blade
  std::vector<V3> lilyPads;       // pad centres (for dragonfly perches)
  V3 beeFlower, beeFlower2, orchidPos, sarraPos, butterflyStart;
};

struct Builder {
  Scene& S; World& W; Rng R{20261116};
  Builder(Scene& s, World& w) : S(s), W(w) {}
  std::vector<int> variants(int n, const std::function<Mesh(Rng&)>& gen) {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.push_back(S.addProto(gen(R)));
    return v;
  }
  float gh(float x, float z) { return G::groundH(x, z); }
  void put(int proto, V3 p, float yaw, float s, float flex = 0, float tiltX = 0, float tiltZ = 0, u32 tint = TINT1, u32 flags = 0) {
    S.put(proto, place(p, yaw, s, tiltX, tiltZ), flex, R.f(), tint, flags);
  }
  void onGround(int proto, float x, float z, float yaw, float s, float flex = 0, float sink = 0, u32 tint = TINT1, float tilt = 0) {
    S.put(proto, place(V3(x, gh(x, z) - sink, z), yaw, s, tilt * R.r(-1, 1), tilt * R.r(-1, 1)), flex, R.f(), tint, 0);
  }
  // jittered-grid scatter over a rectangle; f(x,z) called for each candidate
  void scatter(float x0, float x1, float z0, float z1, float cell, const std::function<void(float, float)>& f) {
    int nx = (int)ceilf((x1 - x0) / cell), nz = (int)ceilf((z1 - z0) / cell);
    for (int j = 0; j < nz; j++)
      for (int i = 0; i < nx; i++) f(x0 + (i + R.f()) * cell, z0 + (j + R.f()) * cell);
  }
  u32 tintN(float x, float z, float amt = 0.25f, float age = 0) {
    float n = fbm(V3(x * 0.9f, 11, z * 0.9f), 3), n2 = vnoise(V3(x * 5, 13, z * 5));
    float b = 1.0f + amt * ((n - 0.5f) * 2.0f + (n2 - 0.5f) * 0.6f + R.r(-0.3f, 0.3f));
    return packTint(V3(b * (1 + 0.1f * (n2 - 0.5f)), b, b * (1 - 0.15f * (n - 0.5f))), age);
  }

  void ground();
  void lawn();
  void gravel();
  void entry();
  void border();
  void pathBench();
  void pond();
  void greenhouse();
  void tropical();
  void arid();
  void endWall();
  void trees();
  void creatures();
  void labels();
};

// ---------------------------------------------------------------- ground mesh + region map
inline void Builder::ground() {
  const float cs = 0.05f;
  int nx = (int)((G::X1 - G::X0) / cs), nz = (int)((G::Z1 - G::Z0) / cs);
  Mesh m;
  m.v.reserve((size_t)(nx + 1) * (nz + 1));
  for (int j = 0; j <= nz; j++)
    for (int i = 0; i <= nx; i++) {
      float x = G::X0 + i * cs, z = G::Z0 + j * cs, h = gh(x, z);
      float e = 0.02f;
      V3 n = norm(V3(gh(x - e, z) - gh(x + e, z), 2 * e, gh(x, z - e) - gh(x, z + e)));
      m.add(V3(x, h, z), n, x, z, 0);
    }
  for (int j = 0; j < nz; j++)
    for (int i = 0; i < nx; i++) {
      u32 a = j * (nx + 1) + i;
      m.quad(a, a + nx + 1, a + nx + 2, a + 1, MT.ground);
    }
  S.put(S.addProto(m), M34());
  // region map: R lawn, G soil, B gravel, A sand
  W.mapW = (int)((G::X1 - G::X0) / W.mapRes); W.mapH = (int)((G::Z1 - G::Z0) / W.mapRes);
  W.map.assign((size_t)W.mapW * W.mapH * 4, 0);
  for (int j = 0; j < W.mapH; j++)
    for (int i = 0; i < W.mapW; i++) {
      float x = G::X0 + (i + 0.5f) * W.mapRes, z = G::Z0 + (j + 0.5f) * W.mapRes;
      float nz2 = (vnoise(V3(x * 14, 21, z * 14)) - 0.5f) * 0.05f;
      float gravelM = smooth(0.04f, -0.03f, G::pathSD(x, z) + nz2);
      float soil = smooth(0.03f, -0.04f, G::bedSD(x, z) + nz2);
      soil = fmaxf(soil, smooth(0.25f, -0.05f, G::hedgeSD(x, z) + nz2 * 3));
      float sand = 0, lawnM = 1;
      if (G::inGH(x, z, 0.1f)) { lawnM = 0; gravelM = 0; if (x > G::ARID_X) { sand = 1; soil = 0; } else soil = 1; }
      float pd = G::pondSD(x, z);
      if (pd < 0.10f) { lawnM *= smooth(0.0f, 0.10f, pd); soil = fmaxf(soil, smooth(0.10f, 0.0f, pd)); }
      if (x > G::WALL_X0 - 0.05f && x < G::WALL_X1 && z > G::WALL_Z0 - 2.4f && z < G::WALL_Z1) { soil = 1; }
      // worn bare patches in the lawn
      float bare = smooth(0.70f, 0.78f, fbm(V3(x * 0.8f, 31, z * 0.8f), 3));
      lawnM *= (1 - gravelM) * (1 - soil) * (1 - 0.0f * bare);
      uint8_t* p = &W.map[((size_t)j * W.mapW + i) * 4];
      p[0] = (uint8_t)(lawnM * 255); p[1] = (uint8_t)(soil * 255); p[2] = (uint8_t)(gravelM * 255); p[3] = (uint8_t)(sand * 255);
    }
}

// ---------------------------------------------------------------- lawn
static inline bool isLawn(float x, float z) {
  if (G::pathSD(x, z) < 0.03f || G::bedSD(x, z) < 0.02f || G::pondSD(x, z) < 0.10f || G::inGH(x, z, 0.12f) || G::hedgeSD(x, z) < 0.0f) return false;
  if (x > G::WALL_X0 - 0.1f && x < G::WALL_X1 + 0.1f && z > G::WALL_Z0 - 2.5f && z < G::WALL_Z1 + 0.05f) return false;
  if (x < -0.2f || x > 35.2f || z < -1.6f || z > 25.2f) return false;
  if (x > G::BED_X1 - 0.06f && x < 12.80f && z > G::BED_Z0 - 0.2f && z < G::BED_Z1 + 0.2f) return false; // brick edging strip
  return true;
}
inline void Builder::lawn() {
  auto tiles = variants(14, [](Rng& r) { return grassTile(r, 0.085f, r.r(0.055f, 0.085f), 600, r.r(0.0f, 0.5f), MT.grass); });
  auto tufts = variants(6, [](Rng& r) { return grassClump(r, r.r(0.10f, 0.16f), 26, 0.022f, 0.4f, MT.grass); });
  auto talls = variants(8, [](Rng& r) { return tallGrass(r, r.r(0.20f, 0.36f), 20 + r.i(10), 1 + r.i(4)); });
  auto clovers = variants(6, [](Rng& r) { return cloverPatch(r, 12 + r.i(8), r.chance(0.4f)); });
  auto daisies = variants(5, [](Rng& r) { return lawnDaisy(r); });
  auto dandF = variants(3, [](Rng& r) { return dandelion(r, 0); });
  auto dandC = variants(3, [](Rng& r) { return dandelion(r, 1); });
  auto dandB = variants(2, [](Rng& r) { return dandelion(r, 2); });
  auto mush = variants(3, [](Rng& r) { return mushroom(r); });
  // sod tiles, density levels by distance to the cat's route
  struct Lv { float cell, scale, lo, hi; };
  const Lv lv[4] = {{0.078f, 1.0f, -1, 2.6f}, {0.15f, 1.9f, 2.6f, 6.0f}, {0.27f, 3.4f, 6.0f, 12.0f}, {0.50f, 6.2f, 12.0f, 1e9f}};
  size_t count = 0;
  for (int L = 0; L < 4; L++) {
    const Lv& v = lv[L];
    scatter(-0.2f, 35.2f, -1.6f, 25.2f, v.cell, [&](float x, float z) {
      float d = W.rd.at(x, z);
      float band = 0.15f * (v.lo > 0 ? v.lo : 1);
      float pIn = v.lo < 0 ? 1.0f : smooth(v.lo - band, v.lo + band, d), pOut = smooth(v.hi + 0.15f * v.hi, v.hi - 0.15f * v.hi, d);
      if (R.f() > pIn * pOut) return;
      if (!isLawn(x, z)) return;
      float age = clampf((fbm(V3(x * 0.6f, 60, z * 0.6f), 3) - 0.45f) * 1.4f, 0, 0.6f);
      float sx = v.scale * R.r(0.9f, 1.15f), sy = powf(v.scale, 0.3f) * R.r(0.9f, 1.15f);
      S.put(tiles[R.i(14)], mTrans(V3(x, gh(x, z) - 0.001f, z)) * mRotY(R.f() * TAU) * mScale(V3(sx, sy, sx)), 0.8f, R.f(), tintN(x, z, 0.22f, age));
      count++;
    });
  }
  // tufts, tall grass at the edges, clover patches
  scatter(-0.2f, 35.2f, -1.6f, 25.2f, 0.06f, [&](float x, float z) {
    float d = W.rd.at(x, z);
    if (d > 9 || !isLawn(x, z)) return;
    float far = smooth(3.0f, 9.0f, d);
    if (R.f() < far * 0.9f) return;
    float cl = fbm(V3(x * 1.1f, 40, z * 1.1f), 3), tl = fbm(V3(x * 0.5f, 50, z * 0.5f), 3);
    float edge = fminf(fminf(G::hedgeSD(x, z), G::pondSD(x, z) - 0.1f), fminf(35.0f - x, fminf(z + 1.5f, 25.0f - z)));
    float tallP = (smooth(1.15f, 0.15f, edge) * 0.55f + smooth(0.68f, 0.76f, tl) * 0.06f) * (x < 1.6f ? 1.0f : smooth(0.4f, 1.1f, d));
    u32 tint = tintN(x, z, 0.22f, 0.1f);
    float yaw = R.f() * TAU, sc = (1 + 0.6f * far) * R.r(0.8f, 1.25f);
    if (R.f() < tallP) { onGround(talls[R.i(8)], x, z, yaw, sc * R.r(0.7f, 1.2f), 1.0f, 0.0f, tint); count++; return; }
    if (cl > 0.60f && R.f() < (cl - 0.60f) * 5.0f) { onGround(clovers[R.i(6)], x, z, yaw, sc, 0.5f, 0, tint); count++; return; }
    if (R.f() < 0.05f) { onGround(tufts[R.i(6)], x, z, yaw, sc, 0.9f, 0.002f, tint); count++; }
  });
  // daisies in drifts, dandelions, mushrooms (only near the route; these are too small to matter far away)
  scatter(0.3f, 35.0f, -1.0f, 25.0f, 0.11f, [&](float x, float z) {
    if (!isLawn(x, z)) return;
    float d = W.rd.at(x, z);
    if (d > 7) return;
    float n = fbm(V3(x * 0.7f, 70, z * 0.7f), 3);
    if (n > 0.56f && R.f() < (n - 0.56f) * 5.0f) onGround(daisies[R.i(5)], x, z, R.f() * TAU, R.r(0.9f, 1.4f), 0.6f);
    float n2 = fbm(V3(x * 0.45f, 80, z * 0.45f), 3);
    if (n2 > 0.52f && R.f() < 0.012f && d > 0.12f) { int k = R.i(8); onGround(k < 3 ? dandF[k] : (k < 6 ? dandC[k - 3] : dandB[k - 6]), x, z, R.f() * TAU, R.r(0.8f, 1.2f), 0.5f); }
  });
  // hero dandelion clocks where the cat stops to sniff (t ~ 19-21 s)
  onGround(dandB[0], 7.24f, 6.80f, 1.0f, 1.15f, 0.35f);
  onGround(dandC[1], 7.33f, 6.62f, 2.0f, 1.0f, 0.35f);
  onGround(dandC[2], 7.12f, 7.02f, 4.0f, 0.9f, 0.35f);
  onGround(dandF[1], 6.6f, 7.15f, 0.5f, 1.1f, 0.35f);
  for (int i = 0; i < 5; i++) onGround(mush[R.i(3)], 4.3f + R.r(-0.5f, 0.5f), 7.75f + R.r(-0.3f, 0.3f), R.f() * TAU, R.r(0.8f, 1.3f));
  fprintf(stderr, "lawn: %zu clumps\n", count);
}

// ---------------------------------------------------------------- gravel paths
inline void Builder::gravel() {
  auto peb = variants(10, [](Rng& r) { return pebble(r); });
  auto pebLo = variants(4, [](Rng& r) { return pebble(r, 8, 5); });
  auto litter = variants(6, [&](Rng& r) { return deadLeaf(r, r.chance(0.5f) ? MT.litter : MT.litter2); });
  size_t n = 0;
  struct Lv { float cell, size, lo, hi; };
  const Lv lv[3] = {{0.0125f, 0.0078f, -1, 3.0f}, {0.03f, 0.016f, 3.0f, 8.0f}, {0.08f, 0.036f, 8.0f, 1e9f}};
  for (int L = 0; L < 3; L++) {
    const Lv& v = lv[L];
    scatter(12.4f, 24.5f, -2.0f, 25.5f, v.cell, [&](float x, float z) {
      if (x > 14.9f && (z < 12.0f || z > 14.3f)) return;
      float ps = G::pathSD(x, z);
      if (ps > 0.05f) return;
      if (ps > -0.04f && R.f() < smooth(-0.04f, 0.05f, ps)) return; // thin out at the edge
      float d = W.rd.at(x, z);
      float pIn = v.lo < 0 ? 1.0f : smooth(v.lo * 0.85f, v.lo * 1.15f, d), pOut = smooth(v.hi * 1.15f, v.hi * 0.85f, d);
      if (R.f() > pIn * pOut) return;
      float s = v.size * R.r(0.55f, 1.5f) * (R.chance(0.04f) ? 1.8f : 1.0f);
      float y = gh(x, z) + s * 0.25f + (L == 0 ? R.r(0, 0.006f) : 0);
      S.put(L == 0 ? peb[R.i(10)] : pebLo[R.i(4)], place(V3(x, y, z), R.f() * TAU, s, R.r(-0.5f, 0.5f), R.r(-0.5f, 0.5f)), 0, R.f(), tintN(x, z, 0.15f));
      n++;
    });
  }
  // leaves blown onto the path
  scatter(12.6f, 14.6f, 2.0f, 14.0f, 0.22f, [&](float x, float z) {
    if (G::pathSD(x, z) > -0.05f || R.f() > 0.35f) return;
    onGround(litter[R.i(6)], x, z, R.f() * TAU, R.r(0.7f, 1.2f), 0, -0.012f, tintN(x, z, 0.3f), 0.25f);
  });
  fprintf(stderr, "gravel: %zu pebbles\n", n);
}

