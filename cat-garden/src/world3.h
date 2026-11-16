// world3.h — greenhouse, tropical + arid houses, end wall, creature prototypes, buildWorld
#pragma once
#include "world2.h"

// box running from a to b with square-ish section (w x h), local Z along the bar
static inline void addBar(Mesh& m, V3 a, V3 b, float w, float h, int mat) {
  float L = len(b - a);
  addBox(m, V3(-w * 0.5f, -h * 0.5f, 0), V3(w * 0.5f, h * 0.5f, L), mat, mLookZ(b - a, a));
}
static inline void addQuad(Mesh& m, V3 a, V3 b, V3 c, V3 d, int mat) {
  V3 n = norm(cross(b - a, d - a));
  u32 i0 = m.add(a, n, 0, 0), i1 = m.add(b, n, 1, 0), i2 = m.add(c, n, 1, 1), i3 = m.add(d, n, 0, 1);
  m.quad(i0, i1, i2, i3, mat);
}

inline void Builder::greenhouse() {
  using namespace G;
  const float E = GEAVE, RG = GRIDGE, D = GDWARF, zr = 0.5f * (GZ0 + GZ1);
  const float dS0 = DOOR_SX - 0.55f, dS1 = DOOR_SX + 0.55f, dE0 = DOOR_EZ - 0.55f, dE1 = DOOR_EZ + 0.55f, DH = 1.95f;
  Mesh w; // dwarf walls
  addBox(w, V3(GX0 - 0.11f, -0.1f, GZ0 - 0.11f), V3(dS0, D, GZ0 + 0.11f), MT.brick);
  addBox(w, V3(dS1, -0.1f, GZ0 - 0.11f), V3(GX1 + 0.11f, D, GZ0 + 0.11f), MT.brick);
  addBox(w, V3(GX0 - 0.11f, -0.1f, GZ1 - 0.11f), V3(GX1 + 0.11f, D, GZ1 + 0.11f), MT.brick);
  addBox(w, V3(GX0 - 0.11f, -0.1f, GZ0 + 0.11f), V3(GX0 + 0.11f, D, GZ1 - 0.11f), MT.brick);
  addBox(w, V3(GX1 - 0.11f, -0.1f, GZ0 + 0.11f), V3(GX1 + 0.11f, D, dE0), MT.brick);
  addBox(w, V3(GX1 - 0.11f, -0.1f, dE1), V3(GX1 + 0.11f, D, GZ1 - 0.11f), MT.brick);
  S.put(S.addProto(w), M34(), 0, 0.61f);
  Mesh f; // painted frame
  int FM = MT.whiteMetal;
  auto sill = [&](V3 a, V3 b) { addBar(f, a, b, 0.26f, 0.05f, FM); };
  sill(V3(GX0 - 0.13f, D + 0.025f, GZ0), V3(dS0, D + 0.025f, GZ0)); sill(V3(dS1, D + 0.025f, GZ0), V3(GX1 + 0.13f, D + 0.025f, GZ0));
  sill(V3(GX0 - 0.13f, D + 0.025f, GZ1), V3(GX1 + 0.13f, D + 0.025f, GZ1));
  sill(V3(GX0, D + 0.025f, GZ0), V3(GX0, D + 0.025f, GZ1)); sill(V3(GX1, D + 0.025f, GZ0), V3(GX1, D + 0.025f, dE0)); sill(V3(GX1, D + 0.025f, dE1), V3(GX1, D + 0.025f, GZ1));
  for (float z : {GZ0, GZ1}) addBar(f, V3(GX0 - 0.05f, E, z), V3(GX1 + 0.05f, E, z), 0.07f, 0.09f, FM);
  addBar(f, V3(GX0 - 0.05f, RG, zr), V3(GX1 + 0.05f, RG, zr), 0.07f, 0.1f, FM);
  for (float x = GX0; x <= GX1 + 0.01f; x += 0.7f) {
    bool doorS = x > dS0 + 0.05f && x < dS1 - 0.05f;
    if (!doorS) addBar(f, V3(x, D, GZ0), V3(x, E, GZ0), 0.04f, 0.05f, FM); else addBar(f, V3(x, DH, GZ0), V3(x, E, GZ0), 0.04f, 0.05f, FM);
    addBar(f, V3(x, D, GZ1), V3(x, E, GZ1), 0.04f, 0.05f, FM);
    addBar(f, V3(x, E, GZ0), V3(x, RG, zr), 0.04f, 0.06f, FM);
    addBar(f, V3(x, E, GZ1), V3(x, RG, zr), 0.04f, 0.06f, FM);
  }
  for (float x : {dS0, dS1}) addBar(f, V3(x, 0, GZ0), V3(x, DH, GZ0), 0.07f, 0.07f, FM);
  addBar(f, V3(dS0, DH, GZ0), V3(dS1, DH, GZ0), 0.07f, 0.07f, FM);
  for (float gx : {GX0, GX1}) {
    for (float z = GZ0; z <= GZ1 + 0.01f; z += 0.7f) {
      float top = E + (RG - E) * (1 - fabsf(z - zr) / (zr - GZ0));
      bool door = gx == GX1 && z > dE0 + 0.05f && z < dE1 - 0.05f;
      addBar(f, V3(gx, door ? DH : D, z), V3(gx, top, z), 0.04f, 0.05f, FM);
    }
    addBar(f, V3(gx, E, GZ0), V3(gx, E, GZ1), 0.05f, 0.06f, FM);
    addBar(f, V3(gx, E, GZ0), V3(gx, RG, zr), 0.06f, 0.07f, FM); addBar(f, V3(gx, E, GZ1), V3(gx, RG, zr), 0.06f, 0.07f, FM);
  }
  for (float z : {dE0, dE1}) addBar(f, V3(GX1, 0, z), V3(GX1, DH, z), 0.07f, 0.07f, FM);
  addBar(f, V3(GX1, DH, dE0), V3(GX1, DH, dE1), 0.07f, 0.07f, FM);
  // horizontal glazing bars half way up the walls and purlins on the roof
  addBar(f, V3(GX0, 1.3f, GZ0), V3(dS0, 1.3f, GZ0), 0.03f, 0.03f, FM); addBar(f, V3(dS1, 1.3f, GZ0), V3(GX1, 1.3f, GZ0), 0.03f, 0.03f, FM);
  addBar(f, V3(GX0, 1.3f, GZ1), V3(GX1, 1.3f, GZ1), 0.03f, 0.03f, FM);
  for (float t : {0.5f}) { float y = E + (RG - E) * t; addBar(f, V3(GX0, y, GZ0 + (zr - GZ0) * t), V3(GX1, y, GZ0 + (zr - GZ0) * t), 0.04f, 0.05f, FM); addBar(f, V3(GX0, y, GZ1 - (zr - GZ0) * t), V3(GX1, y, GZ1 - (zr - GZ0) * t), 0.04f, 0.05f, FM); }
  // open door leaves
  auto leaf = [&](V3 hinge, V3 dir) {
    V3 e = hinge + dir * 1.08f;
    addBar(f, hinge + V3(0, 0.05f, 0), hinge + V3(0, DH - 0.03f, 0), 0.05f, 0.05f, FM); addBar(f, e + V3(0, 0.05f, 0), e + V3(0, DH - 0.03f, 0), 0.05f, 0.05f, FM);
    for (float y : {0.08f, 0.75f, DH - 0.05f}) addBar(f, hinge + V3(0, y, 0), e + V3(0, y, 0), 0.05f, 0.06f, FM);
    addBox(f, V3(-0.008f, 0.1f, 0.03f), V3(0.008f, 0.73f, 1.05f), FM, mLookZ(dir, hinge));
  };
  V3 dirS = norm(V3(0.22f, 0, -1)), dirE = norm(V3(1, 0, 0.2f));
  leaf(V3(dS1 + 0.02f, 0, GZ0 - 0.03f), dirS); leaf(V3(GX1 + 0.03f, 0, dE1 + 0.02f), dirE);
  // heating pipes along the inside of the dwarf wall
  for (float y : {0.14f, 0.27f}) { addTube(f, {V3(GX0 + 0.2f, y, GZ1 - 0.2f), V3(GX1 - 0.2f, y, GZ1 - 0.2f)}, [](float) { return 0.035f; }, 8, MT.iron); addTube(f, {V3(dS1 + 0.3f, y, GZ0 + 0.2f), V3(GX1 - 0.2f, y, GZ0 + 0.2f)}, [](float) { return 0.035f; }, 8, MT.iron); }
  S.put(S.addProto(f), M34(), 0, 0.23f);
  Mesh g; // glazing
  int GM = MT.glass;
  addQuad(g, V3(GX0, D, GZ0), V3(dS0, D, GZ0), V3(dS0, E, GZ0), V3(GX0, E, GZ0), GM);
  addQuad(g, V3(dS1, D, GZ0), V3(GX1, D, GZ0), V3(GX1, E, GZ0), V3(dS1, E, GZ0), GM);
  addQuad(g, V3(dS0, DH, GZ0), V3(dS1, DH, GZ0), V3(dS1, E, GZ0), V3(dS0, E, GZ0), GM);
  addQuad(g, V3(GX0, D, GZ1), V3(GX1, D, GZ1), V3(GX1, E, GZ1), V3(GX0, E, GZ1), GM);
  addQuad(g, V3(GX0, E, GZ0), V3(GX1, E, GZ0), V3(GX1, RG, zr), V3(GX0, RG, zr), GM);
  addQuad(g, V3(GX0, E, GZ1), V3(GX1, E, GZ1), V3(GX1, RG, zr), V3(GX0, RG, zr), GM);
  addQuad(g, V3(GX0, D, GZ0), V3(GX0, D, GZ1), V3(GX0, E, GZ1), V3(GX0, E, GZ0), GM);
  { u32 a = g.add(V3(GX0, E, GZ0), V3(1, 0, 0), 0, 0), b = g.add(V3(GX0, E, GZ1), V3(1, 0, 0), 1, 0), c = g.add(V3(GX0, RG, zr), V3(1, 0, 0), 0.5f, 1); g.tri(a, b, c, GM); }
  addQuad(g, V3(GX1, D, GZ0), V3(GX1, D, dE0), V3(GX1, E, dE0), V3(GX1, E, GZ0), GM);
  addQuad(g, V3(GX1, D, dE1), V3(GX1, D, GZ1), V3(GX1, E, GZ1), V3(GX1, E, dE1), GM);
  addQuad(g, V3(GX1, DH, dE0), V3(GX1, DH, dE1), V3(GX1, E, dE1), V3(GX1, E, dE0), GM);
  { u32 a = g.add(V3(GX1, E, GZ0), V3(1, 0, 0), 0, 0), b = g.add(V3(GX1, E, GZ1), V3(1, 0, 0), 1, 0), c = g.add(V3(GX1, RG, zr), V3(1, 0, 0), 0.5f, 1); g.tri(a, b, c, GM); }
  { // glass in the open door leaves
    V3 h = V3(dS1 + 0.02f, 0, GZ0 - 0.03f); addQuad(g, h + V3(0, 0.75f, 0), h + dirS * 1.08f + V3(0, 0.75f, 0), h + dirS * 1.08f + V3(0, DH - 0.05f, 0), h + V3(0, DH - 0.05f, 0), GM);
    h = V3(GX1 + 0.03f, 0, dE1 + 0.02f); addQuad(g, h + V3(0, 0.75f, 0), h + dirE * 1.08f + V3(0, 0.75f, 0), h + dirE * 1.08f + V3(0, DH - 0.05f, 0), h + V3(0, DH - 0.05f, 0), GM);
  }
  S.put(S.addProto(g), M34(), 0, 0.5f, TINT1, F_NOBOUNCE);
  // paved aisle + door spur, thresholds
  Mesh pv;
  addBox(pv, V3(GX0 + 0.35f, -0.05f, 16.92f), V3(GX1 + 0.5f, 0.012f, 18.08f), MT.paving);
  addBox(pv, V3(dS0 - 0.02f, -0.05f, GZ0 - 0.45f), V3(dS1 + 0.02f, 0.012f, 16.92f), MT.paving);
  S.put(S.addProto(pv), M34());
  // brick kerbs
  Mesh brick; addBlob(brick, V3(0, 0, 0), V3(0.058f, 0.040f, 0.118f), 10, 8, 0.05f, 2.2f, 3.0f, MT.brickUnit);
  for (auto& v : brick.v) { V3 q = v.p; auto sq = [](float x, float r) { return r * squarish(x / r, 0.22f); }; v.p = V3(sq(q.x, 0.048f), sq(q.y, 0.034f), sq(q.z, 0.107f)); }
  int bp = S.addProto(brick);
  auto kerb = [&](V3 a, V3 b) {
    float L = len(b - a); V3 d = (b - a) / L; float yaw = atan2f(d.x, d.z);
    for (float t = 0.11f; t < L; t += 0.222f) {
      float c = R.r(0.7f, 1.15f);
      S.put(bp, place(a + d * t + V3(R.r(-0.004f, 0.004f), 0.03f + R.r(-0.004f, 0.006f), R.r(-0.004f, 0.004f)), yaw + R.r(-0.02f, 0.02f), 1, R.r(-0.03f, 0.03f), R.r(-0.04f, 0.04f)), 0, R.f(), packTint(V3(c, c * R.r(0.9f, 1.05f), c * R.r(0.85f, 1.05f))));
    }
  };
  kerb(V3(GX0 + 0.35f, 0, 16.87f), V3(dS0 - 0.07f, 0, 16.87f)); kerb(V3(dS1 + 0.07f, 0, 16.87f), V3(GX1 - 0.15f, 0, 16.87f)); kerb(V3(GX0 + 0.35f, 0, 18.13f), V3(GX1 - 0.15f, 0, 18.13f));
  kerb(V3(dS0 - 0.07f, 0, GZ0 + 0.15f), V3(dS0 - 0.07f, 0, 16.9f)); kerb(V3(dS1 + 0.07f, 0, GZ0 + 0.15f), V3(dS1 + 0.07f, 0, 16.9f));
  // staging bench in the arid house (north side)
  Mesh st;
  for (float x = 23.7f; x < 28.7f; x += 1.2f) for (float z : {19.95f, 20.7f}) addBar(st, V3(x, 0, z), V3(x, 0.74f, z), 0.045f, 0.045f, MT.galv);
  addBar(st, V3(23.6f, 0.72f, 19.95f), V3(28.7f, 0.72f, 19.95f), 0.04f, 0.05f, MT.galv); addBar(st, V3(23.6f, 0.72f, 20.7f), V3(28.7f, 0.72f, 20.7f), 0.04f, 0.05f, MT.galv);
  for (float x = 23.62f; x < 28.7f; x += 0.085f) addBox(st, V3(x, 0.745f, 19.9f), V3(x + 0.065f, 0.765f, 20.76f), MT.wood, M34(), x);
  S.put(S.addProto(st), M34(), 0, 0.8f);
}

// ---------------------------------------------------------------- tropical house
inline void Builder::tropical() {
  using namespace G;
  int mStem = S.addMat(0, V3(0.17f, 0.22f, 0.07f), 0.5f, V3(0.26f, 0.20f, 0.10f), 0.1f, V3(0.1f, 0.14f, 0.05f), 0.8f, 9, 0.5f, 0.5f);
  auto rosette = [&](const std::vector<int>& protos, V3 c, int n, float sc, float flex, float tiltMin, float tiltMax, float r0 = 0) {
    float y0 = R.f() * TAU;
    for (int i = 0; i < n; i++) {
      float t = (float)i / n, yaw = y0 + i * 2.39996f;
      S.put(protos[R.i((int)protos.size())], mTrans(c + V3(sinf(yaw), 0, cosf(yaw)) * r0) * mRotY(yaw) * mRotX(-lerpf(tiltMax, tiltMin, t) + R.r(-0.1f, 0.1f)) * mScale(sc * R.r(0.75f, 1.2f) * (0.7f + 0.5f * t)), flex, R.f(), tintN(c.x + i, c.z, 0.2f, R.f() < 0.12f ? 0.45f : 0.0f));
    }
  };
  BigLeafP bn; bn.mat = MT.banana; bn.petMat = mStem; bn.petLen = 0.45f; bn.petR = 0.02f; bn.petPitch = 1.25f; bn.len = 1.35f; bn.wid = 0.42f; bn.base = 0.35f; bn.k = 0.9f; bn.e = 0.33f; bn.droop = 1.25f; bn.fold = 0.2f; bn.wave = 0.035f; bn.waveN = 5; bn.nu = 10; bn.nv = 26; bn.bladePitch = 0.5f;
  auto banana = variants(4, [&](Rng& r) { BigLeafP p = bn; p.len *= r.r(0.8f, 1.15f); p.droop = r.r(0.7f, 1.6f); return bigLeaf(r, p); });
  BigLeafP mn; mn.mat = MT.monstera; mn.petMat = mStem; mn.petLen = 0.34f; mn.petR = 0.008f; mn.petPitch = 0.9f; mn.len = 0.42f; mn.wid = 0.40f; mn.base = 0.8f; mn.k = 0.72f; mn.e = 0.5f; mn.droop = 1.0f; mn.fold = 0.12f; mn.wave = 0.02f; mn.nu = 14; mn.nv = 16;
  auto monstera = variants(4, [&](Rng& r) { BigLeafP p = mn; p.len *= r.r(0.75f, 1.15f); p.wid = p.len * r.r(0.85f, 1.0f); p.petLen = r.r(0.22f, 0.4f); return bigLeaf(r, p); });
  BigLeafP alo; alo.mat = MT.alocasia; alo.petMat = mStem; alo.petLen = 0.62f; alo.petR = 0.009f; alo.petPitch = 1.42f; alo.len = 0.46f; alo.wid = 0.29f; alo.base = 0.85f; alo.k = 0.62f; alo.e = 0.62f; alo.droop = 0.6f; alo.bladePitch = -0.75f; alo.fold = 0.22f; alo.wave = 0.06f; alo.waveN = 4; alo.nu = 10; alo.nv = 14;
  auto alocasia = variants(4, [&](Rng& r) { BigLeafP p = alo; p.petLen = r.r(0.4f, 0.75f); p.len *= r.r(0.8f, 1.15f); return bigLeaf(r, p); });
  BigLeafP cal; cal.mat = MT.calathea; cal.petMat = mStem; cal.petLen = 0.13f; cal.petR = 0.003f; cal.petPitch = 1.0f; cal.len = 0.19f; cal.wid = 0.085f; cal.base = 0.3f; cal.k = 0.9f; cal.e = 0.6f; cal.droop = 0.5f; cal.fold = 0.15f; cal.wave = 0.03f; cal.nu = 6; cal.nv = 10;
  auto calathea = variants(4, [&](Rng& r) { BigLeafP p = cal; p.petLen = r.r(0.08f, 0.18f); p.len *= r.r(0.8f, 1.2f); return bigLeaf(r, p); });
  auto frond = variants(4, [](Rng& r) { return fernFrond(r, r.r(0.45f, 0.7f), 22 + r.i(8), MT.fern, MT.stemPale); });
  auto bromL = variants(3, [](Rng& r) { Mesh m; LeafP L; L.len = r.r(0.2f, 0.3f); L.wid = 0.036f; L.nu = 2; L.nv = 9; L.prof = profStrap(2.0f, 0.9f); L.pitch = 0.9f; L.droop = r.r(0.8f, 1.5f); L.fold = 0.45f; L.w = r.f(); addLeaf(m, L, M34(), MT.bromeliad); return m; });
  auto mossB = variants(5, [](Rng& r) { Mesh m; addBlob(m, V3(0, 0, 0), V3(1, r.r(0.3f, 0.5f), r.r(0.7f, 1.0f)), 14, 8, 0.35f, 3.5f, r.f() * 40, MT.moss, M34(), 0, -0.1f); return m; });
  auto litter = variants(4, [&](Rng& r) { return deadLeaf(r, MT.litter); });
  auto peb = variants(4, [](Rng& r) { return pebble(r, 10, 7); });
  // sarracenia pitchers
  auto pitcher = variants(5, [](Rng& r) {
    Mesh m; float h = r.r(0.24f, 0.40f), mr = h * r.r(0.055f, 0.075f);
    addRevolve(m, {{0.003f, 0}, {0.005f, h * 0.25f}, {mr * 0.5f, h * 0.6f}, {mr * 0.85f, h * 0.88f}, {mr, h * 0.97f}, {mr * 1.25f, h}, {mr * 1.3f, h * 0.985f}}, 12, 14, MT.sarra, M34(), [](float th, float t) { return 1.0f + 0.12f * t * cosf(th); }, r.f());
    LeafP L; L.len = mr * 3.4f; L.wid = mr * 3.2f; L.nu = 6; L.nv = 6; L.prof = profEll(0.7f, 0.5f, 0.35f); L.pitch = 1.25f; L.droop = 1.9f; L.droopPow = 1.2f; L.fold = -0.15f; L.wave = 0.12f; L.waveN = 2.5f; L.w = r.f();
    addLeaf(m, L, mTrans(V3(0, h * 0.97f, -mr * 0.95f)), MT.sarraHood);
    return m;
  });
  auto trap = variants(3, [](Rng& r) {
    Mesh m;
    for (int i = 0; i < 9; i++) {
      float yaw = i * 2.4f + r.r(-0.3f, 0.3f); M34 x = mRotY(yaw);
      LeafP P; P.len = r.r(0.03f, 0.05f); P.wid = 0.012f; P.nu = 2; P.nv = 4; P.prof = profEll(1.6f, 0.6f, 0.3f); P.pitch = r.r(0.1f, 0.6f); P.droop = r.r(-0.1f, 0.4f); P.w = r.f();
      addLeaf(m, P, x, MT.flytrap);
      float a = P.pitch - P.droop; V3 tip(0, sinf(P.pitch - P.droop * 0.4f) * P.len, cosf(P.pitch - P.droop * 0.4f) * P.len);
      M34 tx = x * mTrans(tip) * mRotX(-a - 0.5f);
      float open = r.r(0.35f, 0.75f);
      for (int sgn = -1; sgn <= 1; sgn += 2) {
        M34 lx = tx * mRotZ(sgn * open);
        addGrid(m, 8, 3, 0, 1, 0, 1, [&](float u, float v) { float th = PI * u; return V3(0.004f * v * 0.0f, 0.011f * v * sinf(th) + 0.001f, 0.011f * (1 - cosf(th)) * (0.3f + 0.7f * v) - 0.004f * (1 - v)); }, MT.flytrap, lx * mRotZ(sgn * 0.0f), 0.8f);
        for (int k = 0; k < 11; k++) { float th = PI * (k + 0.5f) / 11; V3 p(0, 0.011f * sinf(th) + 0.001f, 0.011f * (1 - cosf(th))); V3 d = norm(V3((float)-sgn * 0.5f, sinf(th), -cosf(th))); addTube(m, {p, p + d * 0.005f}, [](float t) { return 0.0003f * (1 - t * 0.7f); }, 3, MT.stemGreen, lx, 0, false); }
      }
    }
    return m;
  });
  // orchid
  auto orchid = variants(3, [](Rng& r) {
    Mesh m;
    for (int i = 0; i < 4; i++) { LeafP L; L.len = r.r(0.12f, 0.2f); L.wid = 0.05f; L.nu = 4; L.nv = 8; L.prof = profEll(1.1f, 0.45f, 0.5f); L.pitch = r.r(0.2f, 0.6f); L.droop = r.r(0.4f, 1.0f); L.fold = 0.3f; L.w = r.f(); M34 x = mRotY(i * PI / 2 * 1.1f + (i % 2) * PI); addLeaf(m, L, x, MT.orchidLeaf); LeafP B = L; B.belly = 0.25f; addLeaf(m, B, x, MT.orchidLeaf); }
    float az = r.f() * TAU; V3 o(cosf(az), 0, sinf(az));
    auto pts = curvePts(V3(0, 0, 0), V3(0, 0.22f, 0) + o * 0.02f, V3(0, 0.36f, 0) + o * 0.12f, V3(0, 0.30f, 0) + o * 0.34f, 16);
    addTube(m, pts, [](float t) { return 0.0022f * (1.1f - 0.6f * t); }, 5, MT.stemGreen, M34(), r.f(), false);
    int nf = 6 + r.i(4);
    for (int i = 0; i < nf; i++) {
      float t = 0.45f + 0.55f * i / (float)(nf - 1); int k = std::min((int)(t * 16), 15);
      V3 p = lerp(pts[k], pts[k + 1], t * 16 - k);
      V3 side = norm(cross(o, V3(0, 1, 0))) * (i % 2 ? 1.0f : -1.0f);
      V3 face = norm(side * 0.8f + V3(0, -0.15f, 0) + o * 0.3f);
      V3 fp = p + face * 0.012f + V3(0, -0.012f, 0);
      addTube(m, {p, fp}, [](float) { return 0.0008f; }, 3, MT.stemGreen, M34(), 0, false);
      float sz = (i > nf - 2 ? 0.5f : 1.0f) * r.r(0.9f, 1.1f);
      M34 x = mAlignY(face, fp, V3(0, 1, 0));
      for (int s = 0; s < 3; s++) { LeafP L; L.len = 0.026f * sz; L.wid = 0.015f * sz; L.nu = 4; L.nv = 5; L.prof = profEll(1.0f, 0.6f, 0.2f); L.pitch = 0.08f; L.droop = 0.2f; L.w = r.f(); addLeaf(m, L, x * mRotY(s * TAU / 3 + PI), MT.petOrchid); }
      for (int s = -1; s <= 1; s += 2) { LeafP L; L.len = 0.028f * sz; L.wid = 0.032f * sz; L.nu = 6; L.nv = 5; L.prof = profEll(1.1f, 0.42f, 0.2f); L.pitch = 0.12f; L.droop = 0.25f; L.wave = 0.05f; L.w = r.f(); addLeaf(m, L, x * mTrans(V3(0, 0.001f, 0)) * mRotY(PI / 2 * s + PI), MT.petOrchid); }
      { LeafP L; L.len = 0.016f * sz; L.wid = 0.014f * sz; L.nu = 4; L.nv = 4; L.prof = profEll(0.8f, 0.5f, 0.4f); L.pitch = 0.9f; L.droop = 1.6f; L.fold = -0.5f; L.w = r.f(); addLeaf(m, L, x * mTrans(V3(0, 0.002f, 0)), MT.petOrchidLip); }
    }
    return m;
  });
  auto ground = [&](float x, float z) { return V3(x, 0.0f, z); };
  auto bananaAt = [&](float x, float z, float h) {
    Mesh m; std::vector<V3> pts;
    addTube(m, {V3(0, -0.05f, 0), V3(0.01f, h * 0.5f, 0.02f), V3(0.0f, h, 0.0f)}, [&](float t) { return 0.10f - 0.05f * t; }, 12, mStem);
    S.put(S.addProto(m), mTrans(ground(x, z)), 0, R.f());
    rosette(banana, V3(x, h - 0.05f, z), 7, 1.0f, 0.12f, 0.1f, 0.9f);
  };
  auto alocasiaAt = [&](float x, float z, float sc) { rosette(alocasia, ground(x, z), 6 + R.i(3), sc, 0.15f, -0.25f, 0.35f); };
  auto calAt = [&](float x, float z, float sc) { rosette(calathea, ground(x, z), 10 + R.i(6), sc, 0.15f, -0.2f, 0.6f); };
  auto fernAt = [&](float x, float z, float sc, float y = 0) { rosette(frond, V3(x, y, z), 9 + R.i(5), sc, 0.15f, -0.1f, 0.75f); };
  auto bromAt = [&](V3 c, float sc) { rosette(bromL, c, 16, sc, 0.05f, -0.75f, 0.7f); };
  auto monsteraAt = [&](float x, float z) {
    Mesh m; addTube(m, {V3(0, -0.05f, 0), V3(0, 1.75f, 0)}, [](float) { return 0.05f; }, 10, MT.moss);
    for (int i = 0; i < 7; i++) { float a = R.f() * TAU, y = R.r(0.4f, 1.6f); V3 p(cosf(a) * 0.05f, y, sinf(a) * 0.05f); addStem(m, p, V3(cosf(a) * R.r(0.1f, 0.3f), 0.0f, sinf(a) * R.r(0.1f, 0.3f)), 0.2f, 0.003f, 0.002f, MT.twig, R, nullptr, 4); }
    S.put(S.addProto(m), mTrans(ground(x, z)), 0, R.f());
    for (int i = 0; i < 13; i++) { float yaw = i * 2.39996f, y = 0.25f + 1.4f * i / 13.0f; S.put(monstera[R.i(4)], mTrans(V3(x + sinf(yaw) * 0.05f, y, z + cosf(yaw) * 0.05f)) * mRotY(yaw) * mRotX(R.r(-0.3f, 0.3f)) * mScale(R.r(0.75f, 1.2f)), 0.12f, R.f(), tintN(x + i, z, 0.2f)); }
  };
  // west of the door spur
  bananaAt(16.15f, 15.55f, 1.9f); bananaAt(15.75f, 16.25f, 1.3f); alocasiaAt(16.95f, 16.25f, 1.0f); alocasiaAt(16.9f, 14.75f, 0.85f); calAt(17.1f, 15.45f, 1.0f); calAt(16.6f, 16.6f, 0.9f);
  fernAt(17.15f, 14.5f, 0.7f); fernAt(15.6f, 14.8f, 1.0f);
  { // tree fern north-west
    Mesh m; addTube(m, {V3(0, -0.05f, 0), V3(0.03f, 0.7f, 0.02f), V3(0, 1.35f, 0)}, [](float t) { return 0.10f - 0.02f * t; }, 12, MT.barkDark);
    S.put(S.addProto(m), mTrans(V3(16.35f, 0, 19.25f)), 0, R.f());
    rosette(frond, V3(16.35f, 1.33f, 19.25f), 16, 2.9f, 0.15f, -0.25f, 0.5f);
    fernAt(17.3f, 18.5f, 0.8f); fernAt(15.7f, 18.45f, 0.9f); alocasiaAt(15.9f, 20.2f, 1.1f); calAt(17.0f, 19.4f, 1.1f); calAt(16.3f, 18.35f, 0.8f);
  }
  // east of the spur, south bed
  monsteraAt(19.25f, 15.85f); calAt(19.0f, 16.6f, 1.0f); calAt(19.8f, 16.55f, 0.85f); calAt(18.85f, 15.2f, 1.1f); fernAt(20.1f, 15.6f, 1.0f); fernAt(18.9f, 14.6f, 0.9f);
  alocasiaAt(20.3f, 14.9f, 1.1f); bananaAt(21.9f, 15.1f, 1.7f); fernAt(22.5f, 15.9f, 0.9f); alocasiaAt(22.8f, 14.8f, 0.9f);
  { // sarracenia bog + flytraps + bromeliads (cat level, right beside the aisle)
    W.sarraPos = V3(20.95f, 0.3f, 16.5f);
    for (int i = 0; i < 19; i++) { float a = R.f() * TAU, rr = 0.32f * sqrtf(R.f()); V3 p(20.95f + cosf(a) * rr * 1.5f, 0.01f, 16.42f + sinf(a) * rr * 0.9f); S.put(pitcher[R.i(5)], mTrans(p) * mRotY(R.f() * TAU) * mRotX(R.r(-0.22f, 0.22f)) * mRotZ(R.r(-0.22f, 0.22f)) * mScale(R.r(0.75f, 1.2f)), 0.08f, R.f(), tintN(p.x * 3, p.z * 3, 0.25f)); }
    for (int i = 0; i < 9; i++) S.put(trap[R.i(3)], place(V3(21.72f + R.g() * 0.12f, 0.035f, 16.66f + R.g() * 0.06f), R.f() * TAU, R.r(0.9f, 1.3f)), 0.05f, R.f());
    for (int i = 0; i < 80; i++) { float x = R.r(20.3f, 22.05f), z = R.r(16.0f, 16.82f); S.put(mossB[R.i(5)], place(V3(x, 0.01f, z), R.f() * TAU, R.r(0.03f, 0.09f)), 0, R.f(), tintN(x * 4, z * 4, 0.3f)); }
    bromAt(V3(22.45f, 0.03f, 16.55f), 0.9f); bromAt(V3(22.85f, 0.03f, 16.25f), 0.7f); bromAt(V3(20.1f, 0.03f, 16.35f), 0.65f);
  }
  // north bed: mossy log with orchids
  {
    Mesh lg; addTube(lg, curvePts(V3(-0.75f, 0.09f, 0), V3(-0.3f, 0.13f, 0.05f), V3(0.3f, 0.10f, -0.05f), V3(0.8f, 0.14f, 0.04f), 10), [](float t) { return 0.105f - 0.02f * t; }, 14, MT.bark);
    addStem(lg, V3(0.2f, 0.12f, 0), V3(0.45f, 0.75f, 0.1f), 0.1f, 0.045f, 0.03f, MT.bark, R, nullptr, 8);
    M34 lx = mTrans(V3(20.35f, 0, 18.62f)) * mRotY(0.12f);
    S.put(S.addProto(lg), lx, 0, R.f());
    W.orchidPos = lx.pt(V3(-0.1f, 0.45f, 0.0f));
    S.put(orchid[0], lx * mTrans(V3(-0.42f, 0.19f, 0.02f)) * mRotY(2.6f) * mRotX(-0.3f), 0.06f, R.f());
    S.put(orchid[1], lx * mTrans(V3(0.02f, 0.21f, -0.02f)) * mRotY(3.4f) * mRotX(-0.2f) * mScale(1.1f), 0.06f, R.f());
    S.put(orchid[2], lx * mTrans(V3(0.42f, 0.7f, 0.08f)) * mRotY(3.0f) * mRotX(-0.4f), 0.06f, R.f(), packTint(V3(1.0f, 0.55f, 0.85f)));
    S.put(orchid[0], lx * mTrans(V3(0.6f, 0.2f, -0.03f)) * mRotY(3.9f) * mRotX(-0.2f) * mScale(0.9f), 0.06f, R.f(), packTint(V3(1.0f, 0.6f, 0.9f)));
    bromAt(lx.pt(V3(-0.68f, 0.17f, 0.0f)), 0.55f); fernAt(lx.pt(V3(0.25f, 0.18f, 0.03f)).x, lx.pt(V3(0.25f, 0.18f, 0.03f)).z, 0.35f, 0.2f);
    for (int i = 0; i < 40; i++) { float t = R.r(-0.75f, 0.8f); S.put(mossB[R.i(5)], lx * place(V3(t, 0.17f + R.r(0, 0.04f), R.r(-0.08f, 0.08f)), R.f() * TAU, R.r(0.025f, 0.06f)), 0, R.f()); }
  }
  calAt(19.2f, 18.5f, 0.9f); calAt(21.5f, 18.45f, 1.0f); calAt(22.6f, 18.6f, 0.85f); fernAt(18.4f, 18.55f, 0.8f); fernAt(21.2f, 19.3f, 1.1f); fernAt(19.6f, 19.6f, 1.0f);
  alocasiaAt(19.0f, 19.5f, 1.2f); alocasiaAt(22.4f, 19.6f, 1.0f); bananaAt(20.6f, 20.1f, 2.0f); bananaAt(22.7f, 20.3f, 1.5f); monsteraAt(18.1f, 20.2f); fernAt(17.6f, 20.0f, 1.0f);
  // ground: moss cushions, litter and pebbles on the soil near the aisle
  scatter(GX0 + 0.3f, ARID_X, GZ0 + 0.3f, GZ1 - 0.3f, 0.09f, [&](float x, float z) {
    if ((z > 16.85f && z < 18.15f) || (x > DOOR_SX - 0.65f && x < DOOR_SX + 0.65f && z < 16.9f)) return;
    float n = fbm(V3(x * 1.3f, 5, z * 1.3f), 3);
    float nearA = fminf(fabsf(z - 16.85f), fabsf(z - 18.15f));
    if (R.f() < 0.08f + (n > 0.55f ? 0.22f : 0.0f)) { S.put(mossB[R.i(5)], place(V3(x, 0.005f, z), R.f() * TAU, R.r(0.02f, 0.07f)), 0, R.f(), tintN(x * 4, z * 4, 0.3f)); return; }
    if (R.f() < 0.07f) { onGround(litter[R.i(4)], x, z, R.f() * TAU, R.r(0.7f, 1.4f), 0, -0.01f, tintN(x, z, 0.3f), 0.3f); return; }
    if (R.f() < 0.10f && nearA < 0.8f) { float s = R.r(0.006f, 0.02f); S.put(peb[R.i(4)], place(V3(x, s * 0.3f, z), R.f() * TAU, s, R.r(-0.4f, 0.4f), R.r(-0.4f, 0.4f)), 0, R.f()); }
  });
  W.butterflyStart = V3(22.0f, 0.5f, 17.6f);
}

// ---------------------------------------------------------------- arid house
static inline Mesh cactusBody(Rng& R, float rad, float hgt, int ribs, float ribDepth, float areoleFreq, float spineLen, int nRadial, int bodyMat, bool flowers) {
  Mesh m;
  float cap = fminf(0.5f, rad * 1.3f / hgt);
  auto prof = [=](float t) { // t 0 (base) .. 1 (apex): radius and height
    float r = rad, y = t * hgt;
    if (t > 1 - cap) { float q = (t - (1 - cap)) / cap; r = rad * sqrtf(fmaxf(1 - q * q, 0.0f)); y = (1 - cap) * hgt + sinf(q * PI * 0.5f) * cap * hgt * 0.72f; }
    if (t < 0.12f) r *= 0.75f + 0.25f * sinf(t / 0.12f * PI * 0.5f);
    return V2(r, y);
  };
  auto P = [&](float u, float v) {
    V2 q = prof(clampf(v, 0, 1)); float th = TAU * u;
    float r = q.x * (1.0f - ribDepth * (0.5f + 0.5f * cosf(ribs * th)));
    return V3(r * cosf(th), q.y, r * sinf(th));
  };
  addGrid(m, ribs * 6, 26, 0, 1, 0, 0.995f, P, bodyMat);
  int na = (int)areoleFreq;
  for (int rb = 0; rb < ribs; rb++)
    for (int j = 0; j < na; j++) {
      float u = (rb + 0.5f) / ribs, v = (j + 0.5f) / na;
      if (v > 0.985f) continue;
      V3 p = P(u, v), n = norm(cross(P(u, v + 0.01f) - P(u, v - 0.01f), P(u + 0.002f, v) - P(u - 0.002f, v)));
      if (dot(n, p - V3(0, p.y, 0)) < 0 && v < 0.9f) n = -n;
      if (n.y < -0.2f && v > 0.9f) n = -n;
      M34 x = mAlignY(n, p + n * 0.001f);
      float sl = spineLen * R.r(0.8f, 1.2f);
      addTube(m, {V3(0, 0, 0), V3(R.r(-0.1f, 0.1f) * sl, sl, R.r(-0.1f, 0.1f) * sl)}, [=](float t) { return 0.0007f * (1 - 0.8f * t); }, 3, MT.spine, x, R.f(), false);
      for (int k = 0; k < nRadial; k++) {
        float a = k * TAU / nRadial + R.r(-0.2f, 0.2f), tilt = R.r(0.9f, 1.3f);
        V3 d(cosf(a) * sinf(tilt), cosf(tilt), sinf(a) * sinf(tilt));
        addTube(m, {V3(0, 0, 0), d * (sl * 0.45f) + V3(0, 0.001f, 0), d * (sl * 0.75f)}, [=](float t) { return 0.0005f * (1 - 0.8f * t); }, 3, MT.spine, x, R.f(), false);
      }
    }
  if (flowers)
    for (int i = 0; i < 5; i++) {
      float a = i * TAU / 5 + R.r(-0.3f, 0.3f); V2 q = prof(0.93f);
      V3 p(q.x * 0.8f * cosf(a), q.y + 0.005f, q.x * 0.8f * sinf(a));
      addDaisyHead(m, R, mAlignY(norm(V3(cosf(a) * 0.4f, 1, sinf(a) * 0.4f)), p), 16, rad * 0.22f, rad * 0.06f, rad * 0.05f, rad * 0.04f, MT.cactusFl, MT.discYellow, -0.5f, 2);
    }
  return m;
}
static inline void addThick(Mesh& m, LeafP L, const M34& x, int mat, float belly) { addLeaf(m, L, x, mat); L.belly = belly; addLeaf(m, L, x, mat); }
static inline Mesh echeveria(Rng& R) {
  Mesh m; int n = 44;
  for (int i = 0; i < n; i++) {
    float t = (float)i / n;
    LeafP L; L.len = lerpf(0.058f, 0.014f, t) * R.r(0.9f, 1.1f); L.wid = L.len * lerpf(0.55f, 0.75f, t); L.nu = 4; L.nv = 6; L.prof = profEll(1.7f, 0.45f, 0.35f);
    L.pitch = lerpf(0.12f, 1.35f, powf(t, 0.8f)) + R.r(-0.05f, 0.05f); L.droop = -0.25f; L.fold = -0.12f; L.curl = -0.3f; L.w = t;
    addThick(m, L, mTrans(V3(0, 0.004f + 0.012f * t, 0)) * mRotY(i * 2.39996f) * mTrans(V3(0, 0, 0.004f * (1 - t))), MT.echeveria, 0.5f);
  }
  return m;
}
static inline Mesh agave(Rng& R, int n, float len0, float wid0, int mat, bool spine) {
  Mesh m;
  for (int i = 0; i < n; i++) {
    float t = (float)i / n;
    LeafP L; L.len = len0 * lerpf(1.0f, 0.45f, t) * R.r(0.9f, 1.1f); L.wid = wid0 * lerpf(1.0f, 0.6f, t); L.nu = 4; L.nv = 10; L.prof = profEll(0.75f, 0.55f, 0.65f);
    L.pitch = lerpf(0.15f, 1.4f, powf(t, 0.7f)) + R.r(-0.06f, 0.06f); L.droop = R.r(-0.1f, 0.35f) * (1 - t); L.fold = -0.2f; L.curl = -0.4f; L.twist = R.r(-0.15f, 0.15f); L.w = R.f();
    M34 x = mTrans(V3(0, 0.01f + 0.03f * t, 0)) * mRotY(i * 2.39996f);
    addThick(m, L, x, mat, 0.38f);
    if (spine) { V3 pos[LEAF_NS + 1], T[LEAF_NS + 1], S2[LEAF_NS + 1], N[LEAF_NS + 1]; leafSpine(L, pos, T, S2, N); addTube(m, {pos[LEAF_NS] - T[LEAF_NS] * 0.01f, pos[LEAF_NS] + T[LEAF_NS] * 0.022f}, [](float tt) { return 0.0022f * (1 - tt * 0.9f); }, 4, MT.barkDark, x, 0, false); }
  }
  return m;
}
static inline void opuntiaRec(Mesh& m, Rng& R, const M34& x, float s, int depth) {
  addBlob(m, V3(0, 0.115f * s, 0), V3(0.085f * s, 0.12f * s, 0.016f * s), 16, 12, 0.08f, 2.0f, R.f() * 30, MT.opuntia, x, R.f());
  for (int i = 0; i < 26; i++) { // spines / glochids
    float a = R.f() * TAU, rr = sqrtf(R.f()); V3 p(cosf(a) * 0.075f * s * rr, 0.115f * s + sinf(a) * 0.105f * s * rr, (R.chance(0.5f) ? 1 : -1) * 0.015f * s * sqrtf(fmaxf(1 - rr * rr, 0.05f)));
    V3 d = norm(V3(R.r(-0.5f, 0.5f), R.r(-0.5f, 0.5f), p.z > 0 ? 1.0f : -1.0f));
    addTube(m, {p, p + d * (0.016f * s)}, [](float t) { return 0.0005f * (1 - t * 0.7f); }, 3, MT.spine, x, 0, false);
  }
  if (depth <= 0) { if (R.chance(0.6f)) addBlob(m, V3(R.r(-0.04f, 0.04f) * s, 0.24f * s, 0), V3(0.016f * s, 0.024f * s, 0.016f * s), 8, 6, 0.05f, 3, 1, MT.petPink, x); return; }
  int nk = 1 + R.i(2) + (depth > 1 ? 1 : 0);
  for (int k = 0; k < nk; k++) {
    float a = R.r(-1.0f, 1.0f);
    M34 cx = x * mTrans(V3(sinf(a) * 0.07f * s, 0.115f * s + cosf(a) * 0.105f * s, 0)) * mRotZ(-a * 0.8f) * mRotY(R.r(-0.9f, 0.9f)) * mRotX(R.r(-0.25f, 0.25f));
    opuntiaRec(m, R, cx, s * R.r(0.7f, 0.92f), depth - 1);
  }
}
inline void Builder::arid() {
  using namespace G;
  int hero = S.addProto(cactusBody(R, 0.125f, 0.21f, 21, 0.16f, 11, 0.030f, 6, MT.cactus, true));
  auto barrel = variants(3, [](Rng& r) { return cactusBody(r, 0.1f, r.r(0.14f, 0.2f), 17 + r.i(5), 0.16f, 9, 0.026f, 5, MT.cactus, r.chance(0.5f)); });
  auto column = variants(3, [](Rng& r) { return cactusBody(r, 0.065f, r.r(0.7f, 1.3f), 8 + r.i(3), 0.3f, 30, 0.018f, 4, MT.cactusCol, false); });
  auto opun = variants(3, [](Rng& r) { Mesh m; opuntiaRec(m, r, M34(), 1.0f, 3); return m; });
  auto eche = variants(4, [](Rng& r) { return echeveria(r); });
  auto agv = variants(2, [](Rng& r) { return agave(r, 26, 0.42f, 0.085f, MT.agave, true); });
  auto aloe = variants(2, [](Rng& r) { return agave(r, 16, 0.3f, 0.05f, MT.aloe, false); });
  auto lith = variants(3, [](Rng& r) { Mesh m; for (int s = -1; s <= 1; s += 2) addBlob(m, V3(s * 0.0085f, 0.006f, 0), V3(0.0085f, 0.011f, 0.012f), 10, 7, 0.08f, 2, r.f() * 20, MT.lithops, M34(), 0, -0.4f); return m; });
  auto rocks = variants(5, [](Rng& r) { return rock(r, MT.sandRock, 0.6f); });
  auto grit = variants(5, [](Rng& r) { return pebble(r, 10, 7); });
  Mesh potM; addRevolve(potM, {{0.07f, 0}, {0.078f, 0.02f}, {0.10f, 0.15f}, {0.112f, 0.155f}, {0.114f, 0.18f}, {0.104f, 0.182f}, {0.10f, 0.165f}, {0.0f, 0.162f}}, 20, 14, MT.terracotta);
  int pot = S.addProto(potM);
  auto at = [&](int proto, float x, float z, float sc, float y = 0) { S.put(proto, place(V3(x, y, z), R.f() * TAU, sc, R.r(-0.04f, 0.04f), R.r(-0.04f, 0.04f)), 0, R.f(), tintN(x * 2, z * 2, 0.18f)); };
  S.put(hero, place(V3(CACTUS.x, -0.012f, CACTUS.z), 0.3f, 1.0f), 0, 0.4f);
  at(barrel[0], 25.78f, 16.22f, 0.8f); at(barrel[1], 25.62f, 16.02f, 0.55f); at(barrel[2], 25.95f, 16.05f, 0.65f); at(barrel[1], 27.0f, 18.55f, 1.2f); at(barrel[0], 24.9f, 19.3f, 0.9f);
  at(column[0], 24.35f, 15.35f, 1.0f); at(column[1], 24.6f, 15.15f, 0.75f); at(column[2], 24.15f, 15.6f, 0.55f); at(column[0], 27.6f, 18.95f, 1.2f); at(column[2], 27.9f, 19.3f, 0.8f); at(column[1], 28.3f, 15.0f, 1.1f);
  at(opun[0], 26.65f, 15.6f, 1.0f); at(opun[1], 26.25f, 19.05f, 1.15f); at(opun[2], 23.75f, 14.8f, 1.0f);
  at(agv[0], 26.95f, 16.28f, 1.0f); at(agv[1], 24.65f, 18.75f, 1.15f); at(agv[0], 28.1f, 16.1f, 0.6f);
  at(aloe[0], 27.75f, 16.6f, 1.0f); at(aloe[1], 24.0f, 19.25f, 1.1f); at(aloe[1], 25.2f, 15.2f, 0.9f);
  for (int i = 0; i < 7; i++) at(eche[R.i(4)], 24.22f + R.g() * 0.13f, 16.70f + R.g() * 0.07f, R.r(0.7f, 1.4f), 0.004f);
  for (int i = 0; i < 6; i++) at(eche[R.i(4)], 25.35f + R.g() * 0.15f, 18.36f + R.g() * 0.08f, R.r(0.8f, 1.5f), 0.004f);
  for (int i = 0; i < 9; i++) at(lith[R.i(3)], 25.92f + R.g() * 0.07f, 16.80f + R.g() * 0.03f, R.r(0.9f, 1.5f), 0.002f);
  for (int i = 0; i < 6; i++) at(lith[R.i(3)], 23.75f + R.g() * 0.06f, 18.3f + R.g() * 0.04f, R.r(0.9f, 1.5f), 0.002f);
  // rocks + grit top dressing
  for (int i = 0; i < 46; i++) {
    float x = R.r(ARID_X + 0.2f, GX1 - 0.4f), z = R.chance(0.5f) ? R.r(GZ0 + 0.4f, 16.75f) : R.r(18.25f, 19.8f);
    if (len(V2(x - CACTUS.x, z - CACTUS.z)) < 0.3f) continue;
    float s = R.r(0.04f, 0.16f);
    S.put(rocks[R.i(5)], place(V3(x, s * 0.2f, z), R.f() * TAU, s, R.r(-0.2f, 0.2f), R.r(-0.2f, 0.2f)), 0, R.f(), tintN(x, z, 0.2f));
  }
  scatter(ARID_X, GX1 - 0.15f, GZ0 + 0.15f, GZ1 - 0.15f, 0.022f, [&](float x, float z) {
    if (z > 16.86f && z < 18.14f) return;
    float d = fminf(fabsf(z - 16.86f), fabsf(z - 18.14f));
    if (R.f() > (d < 1.0f ? 0.8f : 0.25f) || len(V2(x - CACTUS.x, z - CACTUS.z)) < 0.11f) return;
    float s = R.r(0.004f, 0.011f) * (d < 1.0f ? 1.0f : 1.8f) * (R.chance(0.03f) ? 2.5f : 1.0f);
    float c = R.r(0.9f, 1.5f);
    S.put(grit[R.i(5)], place(V3(x, s * 0.3f, z), R.f() * TAU, s, R.r(-0.4f, 0.4f), R.r(-0.4f, 0.4f)), 0, R.f(), packTint(V3(c * 1.15f, c * 1.0f, c * 0.82f)));
  });
  // pots on and under the staging
  int mSoil = S.addMat(0, V3(0.07f, 0.05f, 0.035f), 0.95f, V3(0.12f, 0.09f, 0.06f), 0, V3(0, 0, 0), 0.8f, 60, 0.2f);
  (void)mSoil;
  for (int i = 0; i < 11; i++) {
    float x = 23.9f + i * 0.44f + R.r(-0.05f, 0.05f), z = R.r(20.1f, 20.55f), sc = R.r(0.7f, 1.1f);
    S.put(pot, place(V3(x, 0.765f, z), R.f() * TAU, sc), 0, R.f(), tintN(x * 3, z, 0.25f));
    int k = R.i(5); float py = 0.765f + 0.155f * sc;
    if (k == 0) at(barrel[R.i(3)], x, z, 0.55f * sc, py); else if (k == 1) at(eche[R.i(4)], x, z, 1.6f * sc, py); else if (k == 2) at(column[R.i(3)], x, z, 0.5f * sc, py); else if (k == 3) at(aloe[R.i(2)], x, z, 0.6f * sc, py); else at(opun[R.i(3)], x, z, 0.5f * sc, py);
  }
  for (int i = 0; i < 5; i++) { float x = 24.2f + i * 0.9f + R.r(-0.2f, 0.2f), z = R.r(20.15f, 20.5f), sc = R.r(0.9f, 1.4f); S.put(pot, place(V3(x, 0, z), R.f() * TAU, sc), 0, R.f()); at(i % 2 ? agv[R.i(2)] : barrel[R.i(3)], x, z, (i % 2 ? 0.45f : 0.7f) * sc, 0.155f * sc); }
  S.put(pot, place(V3(28.35f, 0, 16.45f), 0, 1.3f), 0, 0.3f); at(eche[0], 28.35f, 16.45f, 2.3f, 0.2f);
}

// ---------------------------------------------------------------- end: dry-stone raised bed the cat jumps onto
inline void Builder::endWall() {
  using namespace G;
  auto blocks = variants(7, [](Rng& r) {
    Mesh m; addBlob(m, V3(0, 0, 0), V3(1, 1, 1), 16, 12, 0.16f, 1.9f, r.f() * 50, MT.wallStone);
    for (auto& v : m.v) { auto sq = [](float x) { return squarish(x, 0.26f); }; float k = 1.0f + 0.05f * sinf(v.p.x * 5 + v.p.z * 3); v.p = V3(sq(v.p.x) * k, sq(v.p.y), sq(v.p.z)); }
    return m;
  });
  const float bx0 = WALL_X0, bx1 = WALL_X1, bz0 = 9.2f, bz1 = WALL_Z1, th = 0.2f;
  auto run = [&](V3 a, V3 b, bool alongX) {
    float L = len(b - a);
    for (int c = 0; c < 5; c++) {
      float y = 0.05f + c * 0.098f; float t = R.r(-0.1f, 0.0f);
      while (t < L) {
        float sl = R.r(0.12f, 0.40f), hh = 0.049f * R.r(0.85f, 1.12f), dd = th * R.r(0.85f, 1.12f);
        V3 p = a + (b - a) * ((t + sl * 0.5f) / L); p.y = y + R.r(-0.004f, 0.004f);
        V3 sc = alongX ? V3(sl * 0.5f, hh, dd) : V3(dd, hh, sl * 0.5f);
        S.put(blocks[R.i(7)], mTrans(p) * mRotY(R.r(-0.04f, 0.04f)) * mRotX(R.r(-0.03f, 0.03f)) * mScale(sc), 0, R.f(), tintN(p.x * 2 + c, p.z * 2, 0.2f));
        t += sl + R.r(0.003f, 0.012f);
      }
    }
    // cap stones
    float t = -0.1f;
    while (t < L) {
      float sl = R.r(0.3f, 0.55f);
      V3 p = a + (b - a) * ((t + sl * 0.5f) / L); p.y = WALL_H + 0.018f;
      V3 sc = alongX ? V3(sl * 0.5f, 0.026f, th * 1.25f) : V3(th * 1.25f, 0.026f, sl * 0.5f);
      S.put(blocks[R.i(7)], mTrans(p) * mRotY(R.r(-0.03f, 0.03f)) * mScale(sc), 0, R.f(), tintN(p.x * 2, p.z * 2, 0.2f));
      t += sl + R.r(0.004f, 0.012f);
    }
  };
  float zc = 0.5f * (WALL_Z0 + WALL_Z1);
  run(V3(bx0, 0, zc), V3(bx1, 0, zc), true); run(V3(bx0, 0, bz0), V3(bx1, 0, bz0), true);
  run(V3(bx0 + 0.02f, 0, bz0), V3(bx0 + 0.02f, 0, zc), false); run(V3(bx1 - 0.02f, 0, bz0), V3(bx1 - 0.02f, 0, zc), false);
  // soil fill
  int mSoil = S.addMat(0, V3(0.065f, 0.045f, 0.03f), 0.95f, V3(0.12f, 0.085f, 0.055f), 0, V3(0, 0, 0), 0.8f, 30, 0.2f);
  Mesh soil; addBox(soil, V3(bx0 + 0.15f, 0, bz0 + 0.15f), V3(bx1 - 0.15f, WALL_H - 0.03f, WALL_Z0 + 0.1f), mSoil);
  S.put(S.addProto(soil), M34());
  auto mossB = variants(4, [](Rng& r) { Mesh m; addBlob(m, V3(0, 0, 0), V3(1, r.r(0.3f, 0.5f), r.r(0.7f, 1.0f)), 12, 7, 0.4f, 4.0f, r.f() * 40, MT.moss, M34(), 0, -0.1f); return m; });
  auto lav = variants(3, [](Rng& r) { return lavender(r); });
  auto dz = variants(3, [](Rng& r) { return tallDaisy(r, 3); });
  auto dz2 = variants(2, [](Rng& r) { return tallDaisy(r, 1); });
  auto eche = variants(3, [](Rng& r) { return echeveria(r); });
  auto tuft = variants(3, [](Rng& r) { return grassClump(r, r.r(0.06f, 0.12f), 22, 0.02f, 0.5f, MT.grass); });
  float ys = WALL_H - 0.03f;
  {
    auto chips = variants(4, [](Rng& r) { return barkChip(r); });
    scatter(bx0 + 0.25f, bx1 - 0.25f, bz0 + 0.25f, WALL_Z0 + 0.05f, 0.04f, [&](float x, float z) {
      if (R.f() > (x < 31.6f ? 0.55f : 0.12f)) return;
      float s = R.r(0.008f, 0.02f) * (x < 31.6f ? 1.0f : 2.0f);
      if (R.f() < 0.12f) S.put(mossB[R.i(4)], place(V3(x, ys - 0.002f, z), R.f() * TAU, R.r(0.02f, 0.06f)), 0, R.f(), tintN(x * 4, z * 4, 0.3f));
      else S.put(chips[R.i(4)], place(V3(x, ys + 0.001f, z), R.f() * TAU, s, R.r(-0.3f, 0.3f), R.r(-0.3f, 0.3f)), 0, R.f(), tintN(x, z, 0.35f));
    });
  }
  for (int i = 0; i < 16; i++) { float x = R.r(bx0 + 0.4f, bx1 - 0.4f), z = R.r(bz0 + 0.4f, WALL_Z0 - 0.15f); if (len(V2(x - 30.8f, z - 11.4f)) < 0.5f) continue; S.put(lav[R.i(3)], place(V3(x, ys, z), R.f() * TAU, R.r(0.85f, 1.2f)), 0.6f, R.f()); }
  for (int i = 0; i < 9; i++) { float x = R.r(bx0 + 0.4f, bx1 - 0.4f), z = R.r(bz0 + 0.4f, WALL_Z0 - 0.3f); S.put(i % 3 ? dz[R.i(3)] : dz2[R.i(2)], place(V3(x, ys, z), R.f() * TAU, R.r(0.8f, 1.1f)), 0.5f, R.f()); }
  // informal planting along the east wall (seen through the greenhouse's east door)
  auto fx = variants(2, [](Rng& r) { return foxglove(r); });
  auto dz0 = variants(2, [](Rng& r) { return tallDaisy(r, 0); });
  auto dz3 = variants(2, [](Rng& r) { return tallDaisy(r, 2); });
  for (int i = 0; i < 46; i++) {
    float x = R.r(32.6f, 35.3f), z = R.r(13.0f, 23.5f); int k = R.i(5);
    int pr = k == 0 ? fx[R.i(2)] : (k == 1 ? dz0[R.i(2)] : (k == 2 ? dz3[R.i(2)] : (k == 3 ? dz[R.i(3)] : lav[R.i(3)])));
    S.put(pr, place(V3(x, gh(x, z), z), R.f() * TAU, R.r(0.9f, 1.25f)), 0.5f, R.f(), tintN(x, z, 0.15f));
  }
  // creeping plants and moss on the caps near where the cat sits (x ~ 30.8)
  for (int i = 0; i < 90; i++) {
    bool westW = i % 2 == 0;
    float x = westW ? bx0 + 0.02f + R.r(-0.24f, 0.24f) : R.r(bx0, bx0 + 3.0f), z = westW ? R.r(bz0, zc) : zc + R.r(-0.24f, 0.24f);
    if (fabsf(x - (bx0 + 0.02f)) < 0.26f && fabsf(z - 10.6f) < 0.24f) continue;
    if (R.chance(0.35f)) S.put(mossB[R.i(4)], place(V3(x, WALL_H + 0.038f, z), R.f() * TAU, R.r(0.012f, 0.03f)), 0, R.f(), tintN(x * 5, z * 5, 0.3f));
    else if (R.chance(0.5f)) S.put(eche[R.i(3)], place(V3(x, WALL_H + 0.042f, z), R.f() * TAU, R.r(0.35f, 0.7f)), 0, R.f(), packTint(V3(0.75f, 1.05f, 0.7f)));
    else S.put(tuft[R.i(3)], place(V3(x, WALL_H + 0.035f, z), R.f() * TAU, R.r(0.5f, 0.9f)), 0.8f, R.f());
  }
}

// ---------------------------------------------------------------- creature / prop prototypes (animated in cat.h)
inline void Builder::creatures() {
  CreatureProtos& C = W.cp;
  { // ladybird: 7 mm, +Z forward, feet at y = 0
    Mesh m; const float Wd = 0.0027f, H = 0.0023f, Ln = 0.0035f;
    addGrid(m, 12, 12, 0, 1, 0.02f, 0.98f, [&](float u, float v) { float z = Ln * (1 - 2 * v), s = sqrtf(fmaxf(1 - (1 - 2 * v) * (1 - 2 * v), 0.0f)), th = PI * u; return V3(Wd * s * cosf(th), 0.0006f + H * s * sinf(th), z * 0.92f - 0.0004f); }, MT.ladyShell, M34(), 0, true);
    addBlob(m, V3(0, 0.0012f, Ln * 0.82f), V3(0.0017f, 0.0012f, 0.0011f), 8, 6, 0, 1, 1, MT.ladyBlack);
    addBlob(m, V3(0, 0.0009f, Ln * 1.08f), V3(0.0009f, 0.0007f, 0.0006f), 6, 5, 0, 1, 1, MT.ladyBlack);
    addBox(m, V3(-Wd * 0.8f, 0.0004f, -Ln * 0.8f), V3(Wd * 0.8f, 0.0007f, Ln * 0.8f), MT.ladyBlack);
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 3; k++) { V3 a(s * 0.0012f, 0.0006f, (k - 1) * 0.0018f); addTube(m, {a, a + V3(s * 0.0022f, 0.0006f, (k - 1) * 0.0008f), a + V3(s * 0.0032f, -0.0005f, (k - 1) * 0.0014f)}, [](float) { return 0.00014f; }, 3, MT.ladyBlack, M34(), 0, false); }
    for (int s = -1; s <= 1; s += 2) addTube(m, {V3(s * 0.0004f, 0.001f, Ln * 1.15f), V3(s * 0.0012f, 0.0014f, Ln * 1.45f)}, [](float) { return 0.0001f; }, 3, MT.ladyBlack, M34(), 0, false);
    C.ladyBody = S.addProto(m);
    Mesh w; LeafP L; L.len = 0.008f; L.wid = 0.0034f; L.nu = 2; L.nv = 3; L.prof = profEll(1.3f, 0.6f); L.pitch = 0; L.droop = 0; L.fold = 0; addLeaf(w, L, mRotY(PI / 2), MT.wingClear);
    C.ladyWing = S.addProto(w);
  }
  { // bumblebee 18 mm
    Mesh m; M34 rz = mRotX(PI / 2);
    addBlob(m, V3(0, 0, 0), V3(0.0042f, 0.0048f, 0.0040f), 12, 10, 0.05f, 4, 2, MT.beeBody, mTrans(V3(0, 0, 0.001f)) * rz, 0.2f);
    addBlob(m, V3(0, 0, 0), V3(0.0046f, 0.0068f, 0.0044f), 12, 10, 0.04f, 4, 3, MT.beeBody, mTrans(V3(0, -0.0006f, -0.0085f)) * mRotX(PI / 2 + 0.25f), 0.6f);
    addBlob(m, V3(0, 0.0002f, 0.0062f), V3(0.0027f, 0.0025f, 0.0022f), 8, 6, 0, 1, 1, MT.ladyBlack);
    for (int i = 0; i < 260; i++) { // fuzz
      float u = R.f() * 2 - 1, a = R.f() * TAU, sn = sqrtf(1 - u * u); V3 d(sn * cosf(a), sn * sinf(a), u);
      bool ab = R.chance(0.55f); V3 c = ab ? V3(0, -0.0006f, -0.0085f) : V3(0, 0, 0.001f); V3 rr = ab ? V3(0.0046f, 0.0044f, 0.0068f) : V3(0.0042f, 0.0040f, 0.0048f);
      V3 p = c + rr * d; LeafP Hh; Hh.len = 0.0016f; Hh.wid = 0.00025f; Hh.nu = 2; Hh.nv = 1; Hh.pitch = 0; Hh.droop = 0; Hh.w = R.f();
      float band = ab ? fabsf(fmodf((d.z * 0.5f + 0.5f) * 2.6f + 0.15f, 1.0f) - 0.5f) : fabsf(d.z);
      addLeaf(m, Hh, mLookZ(norm(d + V3(0, 0, -0.5f)), p), (band > 0.3f) ? MT.petYellow : MT.ladyBlack);
    }
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 3; k++) { V3 a(s * 0.002f, -0.003f, 0.003f - k * 0.003f); addTube(m, {a, a + V3(s * 0.002f, -0.003f, -0.001f), a + V3(s * 0.0025f, -0.007f, -0.003f)}, [](float) { return 0.00028f; }, 3, MT.ladyBlack, M34(), 0, false); }
    C.beeBody = S.addProto(m);
    Mesh w; LeafP L; L.len = 0.013f; L.wid = 0.005f; L.nu = 2; L.nv = 3; L.prof = profEll(1.4f, 0.6f); L.pitch = 0; L.droop = 0; L.fold = 0; addLeaf(w, L, mRotY(PI / 2), MT.wingClear);
    C.beeWing = S.addProto(w);
  }
  for (int kind = 0; kind < 2; kind++) { // butterflies: 0 orange (tropical house), 1 white (garden)
    Mesh m; float sc = kind == 0 ? 1.0f : 0.62f;
    addTube(m, {V3(0, 0, -0.013f * sc), V3(0, 0.0005f, 0), V3(0, 0, 0.011f * sc)}, [=](float t) { return (0.0013f + 0.0012f * sinf(PI * t)) * sc; }, 6, MT.bfBody);
    for (int s = -1; s <= 1; s += 2) addTube(m, {V3(s * 0.0006f, 0.001f, 0.011f * sc), V3(s * 0.004f * sc, 0.004f * sc, 0.022f * sc)}, [](float) { return 0.00018f; }, 3, MT.bfBody, M34(), 0, false);
    (kind == 0 ? C.bfBody : C.bfBody2) = S.addProto(m);
    Mesh w;
    addGrid(w, 18, 5, -1, 1, 0, 1, [&](float u, float v) {
      float rr = 0.046f * sc * (0.72f + 0.36f * cosf(1.7f * (u + 0.42f))) * (1.0f - 0.30f * expf(-sq((u - 0.08f) / 0.13f))) * (1 + 0.03f * sinf(u * 23));
      float ph = u * 1.28f - 0.25f;
      return V3(rr * v * cosf(ph), 0.0015f * sc * sinf(u * 5) * v, -rr * v * sinf(ph));
    }, kind == 0 ? MT.bfWing : MT.bfWing2);
    (kind == 0 ? C.bfWing : C.bfWing2) = S.addProto(w);
  }
  { // dragonfly 6 cm
    Mesh m;
    addBlob(m, V3(0, 0, 0.026f), V3(0.0032f, 0.003f, 0.0028f), 8, 6, 0, 1, 1, MT.dfEye);
    for (int s = -1; s <= 1; s += 2) addBlob(m, V3(s * 0.0022f, 0.0008f, 0.0265f), V3(0.0024f, 0.0024f, 0.0022f), 8, 6, 0, 1, 1, MT.dfEye);
    addBlob(m, V3(0, 0.0005f, 0.018f), V3(0.0036f, 0.0038f, 0.0065f), 10, 8, 0.03f, 3, 1, MT.dfBody);
    addTube(m, {V3(0, 0.0005f, 0.013f), V3(0, 0, -0.01f), V3(0, -0.0008f, -0.036f)}, [](float t) { return 0.0017f - 0.0006f * t; }, 7, MT.dfBody);
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 3; k++) { V3 a(s * 0.002f, -0.002f, 0.021f - k * 0.003f); addTube(m, {a, a + V3(s * 0.004f, -0.003f, 0.001f), a + V3(s * 0.005f, -0.007f, 0.003f)}, [](float) { return 0.0002f; }, 3, MT.ladyBlack, M34(), 0, false); }
    C.dfBody = S.addProto(m);
    Mesh w; LeafP L; L.len = 0.034f; L.wid = 0.0085f; L.nu = 2; L.nv = 5; L.prof = profEll(1.2f, 0.45f, 0.3f); L.pitch = 0; L.droop = 0; L.fold = 0; addLeaf(w, L, mRotY(PI / 2), MT.wingClear);
    C.dfWing = S.addProto(w);
  }
  { // koi: unit length 1 along -Z from the nose at z = 0; five rigid segments + fins
    auto half = [](float s) { float a = 0.115f * powf(fmaxf(sinf(PI * powf(s, 0.62f)), 0.0f), 0.75f) * (1 - 0.72f * s * s) + 0.012f; return a; };
    const float cut[6] = {0.0f, 0.24f, 0.44f, 0.62f, 0.78f, 0.93f};
    for (int k = 0; k < 5; k++) {
      Mesh m; float s0 = cut[k], s1 = cut[k + 1] + (k < 4 ? 0.03f : 0.0f);
      addGrid(m, 16, 6, 0, 1, 0, 1, [&](float u, float v) {
        float s = lerpf(s0, s1, v), a = half(s), th = TAU * u;
        if (k == 0 && v < 0.02f) a *= 0.2f;
        return V3(a * 0.62f * cosf(th), a * sinf(th) + 0.01f * sinf(PI * s), -(s - s0));
      }, MT.koi, M34(), 0, false);
      // remap uv so the pattern is continuous along the whole fish
      for (auto& v : m.v) v.v = lerpf(s0, s1, v.v);
      if (k == 1) { LeafP L; L.len = 0.20f; L.wid = 0.10f; L.nu = 2; L.nv = 6; L.prof = profEll(0.6f, 0.5f, 0.4f); L.pitch = 0.0f; L.droop = 0.5f; L.fold = 0; L.w = 0.5f; addLeaf(m, L, mTrans(V3(0, 0.095f, 0.02f)) * mRotZ(PI / 2) * mRotY(PI), MT.koiFin); }
      W.cp.koiSeg[k] = S.addProto(m);
    }
    Mesh t; LeafP L; L.len = 0.2f; L.wid = 0.24f; L.nu = 6; L.nv = 6; L.prof = [](float v) { return 0.12f + 0.88f * powf(v, 0.8f); }; L.pitch = 0; L.droop = 0; L.fold = 0; L.wave = 0.05f; L.waveN = 1.5f; L.w = 0.3f;
    addLeaf(t, L, mRotZ(PI / 2) * mRotY(PI), MT.koiFin);
    C.koiTail = S.addProto(t);
    Mesh f; LeafP Fp; Fp.len = 0.13f; Fp.wid = 0.085f; Fp.nu = 4; Fp.nv = 5; Fp.prof = profEll(1.3f, 0.5f, 0.2f); Fp.pitch = 0; Fp.droop = 0.3f; Fp.fold = 0; Fp.w = 0.7f;
    addLeaf(f, Fp, M34(), MT.koiFin);
    C.koiFin = S.addProto(f);
  }
  { // person: shoe, shin, thigh (trousers), all with the joint at the local origin, extending down (-Y)
    Mesh sh;
    addBlob(sh, V3(0, 0.045f, 0.045f), V3(0.05f, 0.05f, 0.145f), 16, 10, 0.03f, 2, 1, MT.shoe, M34(), 0, -0.55f);
    addBox(sh, V3(-0.052f, 0.0f, -0.10f), V3(0.052f, 0.022f, 0.19f), MT.shoeSole);
    addBlob(sh, V3(0, 0.085f, -0.04f), V3(0.042f, 0.05f, 0.055f), 10, 8, 0.03f, 2, 2, MT.shoe);
    C.shoe = S.addProto(sh);
    Mesh sn; addTube(sn, {V3(0, 0, 0), V3(0, -0.22f, 0.005f), V3(0, -0.40f, 0.0f)}, [](float t) { return 0.078f + 0.012f * t + 0.006f * sinf(t * 23); }, 14, MT.denim, M34(), 0, false);
    C.shin = S.addProto(sn);
    Mesh thg; addTube(thg, {V3(0, 0.05f, 0), V3(0, -0.22f, 0), V3(0, -0.46f, 0)}, [](float t) { return 0.105f - 0.02f * t + 0.004f * sinf(t * 17); }, 14, MT.denim, M34(), 0, true);
    // upper body rides with the thigh proto of the left leg (hips at origin): only its shadow and blur matter
    C.thigh = S.addProto(thg);
    int mCoat = S.addMat(15, V3(0.10f, 0.11f, 0.09f), 0.95f, V3(0.16f, 0.17f, 0.14f), 0, V3(0, 0, 0), 0.6f);
    int mSkin = S.addMat(0, V3(0.46f, 0.30f, 0.22f), 0.55f, V3(0.40f, 0.25f, 0.18f), 0.05f, V3(0, 0, 0), 0.4f, 30, 0.5f);
    int mHair = S.addMat(0, V3(0.05f, 0.035f, 0.025f), 0.6f, V3(0.09f, 0.06f, 0.04f), 0, V3(0, 0, 0), 0.6f, 200, 0.4f);
    Mesh ub; addBlob(ub, V3(0, 0.33f, 0), V3(0.19f, 0.36f, 0.13f), 14, 10, 0.05f, 2, 3, mCoat);
    addBlob(ub, V3(0, 0.84f, 0.015f), V3(0.078f, 0.105f, 0.092f), 12, 10, 0.02f, 2, 3, mSkin);
    addBlob(ub, V3(0, 0.875f, -0.012f), V3(0.086f, 0.092f, 0.098f), 12, 10, 0.06f, 4, 5, mHair);
    addTube(ub, {V3(0, 0.66f, 0), V3(0, 0.76f, 0.01f)}, [](float) { return 0.045f; }, 8, mSkin);
    C.robin = S.addProto(ub); // (slot reused for the person's upper body)
    Mesh arm; addTube(arm, {V3(0, 0.02f, 0), V3(0.015f, -0.30f, 0.0f), V3(0.01f, -0.57f, 0.05f)}, [](float t) { return 0.055f - 0.012f * t; }, 9, mCoat);
    addBlob(arm, V3(0.01f, -0.62f, 0.06f), V3(0.035f, 0.055f, 0.03f), 8, 6, 0.03f, 2, 1, mSkin);
    C.robinWing = S.addProto(arm); // (slot reused for the person's arm)
  }
  { // ant 4 mm
    Mesh m; addBlob(m, V3(0, 0.0012f, 0.0015f), V3(0.0005f, 0.00045f, 0.0006f), 6, 5, 0, 1, 1, MT.antBody); addBlob(m, V3(0, 0.0012f, 0.0004f), V3(0.00038f, 0.00038f, 0.0007f), 6, 5, 0, 1, 1, MT.antBody); addBlob(m, V3(0, 0.0013f, -0.0011f), V3(0.0007f, 0.0006f, 0.001f), 6, 5, 0, 1, 1, MT.antBody);
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 3; k++) { V3 a(s * 0.0003f, 0.0011f, 0.0008f - k * 0.0005f); addTube(m, {a, a + V3(s * 0.0009f, 0.0005f, (1 - k) * 0.0006f), a + V3(s * 0.0016f, -0.0011f, (1 - k) * 0.0012f)}, [](float) { return 0.00007f; }, 3, MT.antBody, M34(), 0, false); }
    for (int s = -1; s <= 1; s += 2) addTube(m, {V3(s * 0.0002f, 0.0014f, 0.002f), V3(s * 0.0006f, 0.0018f, 0.0027f), V3(s * 0.0008f, 0.0012f, 0.0033f)}, [](float) { return 0.00005f; }, 3, MT.antBody, M34(), 0, false);
    C.ant = S.addProto(m);
  }
  { // the cat itself: only seen as a shadow and as a reflection (origin = eye position, +Z = gaze)
    Mesh m;
    addBlob(m, V3(0, -0.012f, -0.05f), V3(0.052f, 0.048f, 0.055f), 14, 10, 0.02f, 2, 1, MT.fur);
    addBlob(m, V3(0, -0.035f, -0.005f), V3(0.028f, 0.022f, 0.028f), 10, 8, 0.02f, 2, 2, MT.fur);
    for (int s = -1; s <= 1; s += 2) addRevolve(m, {{0.022f, 0}, {0.014f, 0.022f}, {0.001f, 0.045f}}, 8, 4, MT.fur, mTrans(V3(s * 0.034f, 0.022f, -0.06f)) * mRotZ(-s * 0.3f));
    addBlob(m, V3(0, -0.075f, -0.27f), V3(0.07f, 0.075f, 0.19f), 14, 10, 0.03f, 2, 3, MT.fur);
    for (int s = -1; s <= 1; s += 2) { addTube(m, {V3(s * 0.04f, -0.09f, -0.14f), V3(s * 0.04f, -0.2f, -0.13f)}, [](float) { return 0.016f; }, 6, MT.fur); addTube(m, {V3(s * 0.045f, -0.09f, -0.40f), V3(s * 0.045f, -0.2f, -0.42f)}, [](float) { return 0.018f; }, 6, MT.fur); }
    addTube(m, curvePts(V3(0, -0.04f, -0.45f), V3(0, 0.02f, -0.56f), V3(0, 0.12f, -0.60f), V3(0, 0.2f, -0.56f), 8), [](float) { return 0.012f; }, 6, MT.fur);
    C.catProxy = S.addProto(m);
    Mesh wsk; // whiskers
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 5; k++) {
      V3 a(s * 0.022f, -0.040f + 0.004f * k, 0.012f), b = a + V3(s * R.r(0.055f, 0.075f), R.r(-0.03f, 0.012f) - 0.006f * k, R.r(0.03f, 0.055f));
      addTube(wsk, curvePts(a, lerp(a, b, 0.4f) + V3(0, 0.006f, 0), lerp(a, b, 0.8f) + V3(0, 0.002f, 0), b, 6), [](float t) { return 0.00028f * (1 - 0.7f * t); }, 3, MT.whisker, M34(), 0, false);
    }
    C.whisker = S.addProto(wsk);
    Mesh pw; // fore paw with fur (origin at the wrist, toes towards +Z)
    addTube(pw, {V3(0, 0.012f, -0.16f), V3(0, 0.006f, -0.05f), V3(0, 0.0f, 0.0f)}, [](float t) { return 0.021f - 0.002f * t; }, 10, MT.furPaw, M34(), 0.9f);
    addBlob(pw, V3(0, -0.002f, 0.022f), V3(0.024f, 0.017f, 0.03f), 12, 8, 0.02f, 2, 1, MT.furPaw, M34(), 0.3f);
    for (int k = 0; k < 4; k++) addBlob(pw, V3((k - 1.5f) * 0.0105f, -0.004f, 0.044f - 0.004f * fabsf(k - 1.5f)), V3(0.0068f, 0.008f, 0.011f), 8, 6, 0.02f, 2, 1, MT.furPaw, M34(), 0.2f);
    for (int i = 0; i < 5200; i++) { // fur strands
      float z = R.r(-0.16f, 0.055f), a = R.r(-0.2f, PI + 0.2f);
      float rad = z < 0 ? 0.021f : 0.024f * sqrtf(fmaxf(1 - sq((z - 0.022f) / 0.033f), 0.05f));
      V3 n(cosf(a), sinf(a) * (z < 0 ? 1.0f : 0.72f), 0);
      V3 p(n.x * rad, n.y * rad + (z < 0 ? 0.006f : -0.002f), z);
      LeafP Hh; Hh.len = R.r(0.006f, 0.011f); Hh.wid = 0.0005f; Hh.nu = 2; Hh.nv = 2; Hh.pitch = 0; Hh.droop = R.r(0.2f, 0.8f); Hh.w = clampf((z + 0.16f) / 0.215f, 0, 1);
      addLeaf(pw, Hh, mLookZ(norm(n * 0.55f + V3(R.r(-0.2f, 0.2f), R.r(-0.2f, 0.2f), 0.85f)), p, n), MT.furPaw);
    }
    C.paw = S.addProto(pw);
  }
  { Mesh d; addBlob(d, V3(0, 0, 0), V3(1, 1, 1), 14, 10, 0, 1, 1, MT.dew); C.drop = S.addProto(d); }
  { // the ladybird's grass blade (static, with dew drops) — spine exported for the animation
    LeafP L; L.len = 0.245f; L.wid = 0.0105f; L.nu = 2; L.nv = 22; L.prof = profStrap(2.2f, 0.9f); L.pitch = 1.32f; L.droop = 1.05f; L.droopPow = 2.0f; L.fold = 0.32f; L.twist = 0.25f; L.w = 0.37f;
    Mesh m; addLeaf(m, L, M34(), MT.grassTall);
    V3 hb = G::HERO_BLADE; hb.y = gh(hb.x, hb.z) - 0.004f;
    M34 x = mTrans(hb) * mRotY(1.875f);
    S.put(S.addProto(m), x, 0, 0.37f);
    V3 pos[LEAF_NS + 1], T[LEAF_NS + 1], S2[LEAF_NS + 1], N[LEAF_NS + 1]; leafSpine(L, pos, T, S2, N);
    for (int i = 0; i <= LEAF_NS; i++) { W.heroBladePts.push_back(x.pt(pos[i])); W.heroBladePts.push_back(x.dir(N[i])); W.heroBladePts.push_back(x.dir(T[i])); }
    for (float v : {0.30f, 0.44f, 0.52f, 0.86f, 0.93f}) { int i = (int)(v * LEAF_NS); float r = R.r(0.0009f, 0.0016f); S.put(C.drop, mTrans(x.pt(pos[i] + N[i] * (r * 0.8f) + S2[i] * R.r(-0.002f, 0.002f))) * mScale(V3(r, r * 0.85f, r)), 0, R.f()); }
    // a few more tall blades around it so it does not stand alone
    Rng r2(77);
    S.put(S.addProto(tallGrass(r2, 0.26f, 14, 2)), place(hb + V3(-0.03f, 0, 0.07f), 1.0f, 1.0f), 0.15f, 0.2f);
  }
}

static inline void buildWorld(Scene& S, World& W) {
  makeMats(S);
  Builder B(S, W);
  B.ground();
  if (!getenv("NO_LAWN")) B.lawn();
  B.gravel();
  if (!getenv("NO_HEDGE")) B.entry();
  B.border();
  B.pathBench();
  B.pond();
  B.greenhouse();
  B.tropical();
  B.arid();
  B.endWall();
  if (!getenv("NO_TREES")) B.trees();
  B.creatures();
  B.labels();
}
