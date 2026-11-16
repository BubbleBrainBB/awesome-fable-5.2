// world2.h — entrance (fence + hedge), flower border, path + bench, pond, trees, labels
#pragma once
#include "world.h"
#include "plants2.h"

inline void Builder::entry() {
  // ---- fence at x = -0.80
  Mesh board; addBox(board, V3(-0.011f, 0, -0.068f), V3(0.011f, 1.72f, 0.068f), MT.wood);
  Mesh boardShort; addBox(boardShort, V3(-0.011f, 0.19f, -0.068f), V3(0.011f, 1.72f, 0.068f), MT.wood);
  int pb = S.addProto(board), pbs = S.addProto(boardShort);
  for (float z = -2.5f; z < 26.5f; z += 0.149f) {
    bool hole = fabsf(z - 6.03f) < 0.16f;
    S.put(hole ? pbs : pb, place(V3(-0.80f + R.r(-0.004f, 0.004f), 0.03f + R.r(-0.01f, 0.02f) + (hole ? R.r(0, 0.05f) : 0), z), R.r(-0.02f, 0.02f), 1, R.r(-0.012f, 0.012f), R.r(-0.01f, 0.01f)), 0, R.f(), tintN(-0.8f, z, 0.25f));
  }
  Mesh rail; addBox(rail, V3(-0.02f, -0.045f, -14.5f), V3(0.02f, 0.045f, 14.5f), MT.wood);
  int pr = S.addProto(rail);
  S.put(pr, mTrans(V3(-0.835f, 0.42f, 12)), 0, 0.3f); S.put(pr, mTrans(V3(-0.835f, 1.42f, 12)), 0, 0.6f);
  Mesh post; addBox(post, V3(-0.045f, 0, -0.045f), V3(0.045f, 1.85f, 0.045f), MT.wood);
  int pp = S.addProto(post);
  for (float z = -2.4f; z < 26.5f; z += 2.4f) S.put(pp, place(V3(-0.89f, 0, z + 0.9f), 0, 1), 0, R.f());
  // ---- hedge
  int mCore = S.addMat(0, V3(0.012f, 0.03f, 0.01f), 0.9f, V3(0.02f, 0.045f, 0.012f), 0, V3(0, 0, 0), 0.8f, 20, 0.2f);
  Mesh core; addBox(core, V3(-0.62f, 0.46f, -2.5f), V3(0.02f, 1.66f, 26.5f), mCore);
  S.put(S.addProto(core), M34());
  auto sprigs = variants(5, [](Rng& r) { return sprig(r, r.r(0.07f, 0.11f), 11 + r.i(5), 0.024f, 0.52f, MT.sprig, MT.twig, 0.15f); });
  auto litter = variants(8, [&](Rng& r) { return deadLeaf(r, r.chance(0.6f) ? MT.litter : MT.litter2); });
  auto twigs = variants(4, [](Rng& r) { return twigMesh(r); });
  auto talls = variants(5, [](Rng& r) { return tallGrass(r, r.r(0.2f, 0.34f), 20, 2); });
  size_t n = 0;
  auto shell = [&](int face, float dens, float sc, float zc, float zr0, float zr1) { // face 0 east, 1 top, 2 west
    float len = 29.0f, hgt = face == 1 ? 0.84f : 1.63f;
    int cnt = (int)(dens * len * hgt);
    for (int i = 0; i < cnt; i++) {
      float z = -2.5f + R.f() * len, q = R.f();
      float dz = fabsf(z - zc);
      if (dz < zr0 || dz >= zr1) continue;
      V3 p, nrm;
      float bulge = 0.07f * (fbm(V3(z * 1.7f, q * 3, face * 7.0f), 2) - 0.5f) * 2;
      if (face == 0) { p = V3(0.10f + bulge, 0.12f + q * hgt, z); nrm = V3(1, 0.25f, 0); }
      else if (face == 1) { p = V3(-0.72f + q * hgt, 1.74f + bulge, z); nrm = V3(0, 1, 0); }
      else { p = V3(-0.70f - bulge, 0.12f + q * hgt, z); nrm = V3(-1, 0.25f, 0); }
      if (face != 1 && p.y < 0.36f && fabsf(z - 6.04f) < 0.20f && p.y < 0.22f) continue; // keep the cat's tunnel mouth passable
      V3 d = norm(nrm + V3(R.r(-0.8f, 0.8f), R.r(-0.5f, 0.9f), R.r(-0.8f, 0.8f)));
      p -= nrm * (R.f() * 0.10f * sc);
      S.put(sprigs[R.i(5)], mAlignY(d, p) * mRotY(R.f() * TAU) * mScale(sc * R.r(0.8f, 1.25f)), 0.5f, R.f(), tintN(p.y * 3, z, 0.3f));
      n++;
    }
  };
  for (int f = 0; f < 3; f++) {
    shell(f, f == 1 ? 600 : 1150, 1.0f, 6.05f, 0, 4.2f);
    shell(f, f == 1 ? 160 : 300, 1.9f, 6.05f, 4.2f, 9.5f);
    shell(f, f == 1 ? 50 : 85, 3.6f, 6.05f, 9.5f, 99);
  }
  // low drooping sprays in front of the tunnel mouth: these brush the lens as the cat pushes out
  for (int i = 0; i < 46; i++) {
    V3 p(R.r(0.06f, 0.24f), R.r(0.13f, 0.34f), 6.05f + R.r(-0.22f, 0.22f));
    V3 d = norm(V3(R.r(0.2f, 1.0f), R.r(-1.0f, -0.2f), R.r(-0.6f, 0.6f)));
    S.put(sprigs[R.i(5)], mAlignY(d, p) * mRotY(R.f() * TAU) * mScale(R.r(0.9f, 1.3f)), 0.7f, R.f(), tintN(p.x, p.z, 0.3f));
  }
  // trunks and low branches
  Mesh trunks;
  for (float z = -2.3f; z < 26.3f; z += R.r(0.3f, 0.5f)) {
    if (fabsf(z - 6.04f) < 0.24f) continue;
    V3 b(-0.30f + R.r(-0.12f, 0.12f), -0.02f, z);
    float r = R.r(0.014f, 0.03f);
    V3 t = b + V3(R.r(-0.1f, 0.1f), 0.62f, R.r(-0.1f, 0.1f));
    addStem(trunks, b, t, R.r(-0.08f, 0.08f), r, r * 0.7f, MT.barkDark, R, nullptr, 7);
    for (int k = 0; k < 3; k++) {
      V3 s = lerp(b, t, R.r(0.15f, 0.8f));
      addStem(trunks, s, s + V3(R.r(-0.4f, 0.42f), R.r(0.15f, 0.5f), R.r(-0.3f, 0.3f)), 0.1f, r * 0.45f, r * 0.2f, MT.barkDark, R, nullptr, 5);
    }
  }
  S.put(S.addProto(trunks), M34());
  // leaf litter, twigs under and beside the hedge; rough grass outside the fence
  scatter(-1.6f, 0.45f, 2.5f, 10.0f, 0.045f, [&](float x, float z) {
    float hs = G::hedgeSD(x, z);
    float p = hs < 0 ? 0.55f : smooth(0.4f, 0.0f, hs) * 0.3f;
    if (x < -0.85f) p = 0.10f;
    if (R.f() > p) return;
    onGround(litter[R.i(8)], x, z, R.f() * TAU, R.r(0.6f, 1.15f), 0, -0.004f - R.f() * 0.008f, tintN(x, z, 0.35f), 0.3f);
  });
  scatter(-1.6f, 0.2f, 3.0f, 9.5f, 0.2f, [&](float x, float z) {
    if (R.f() < 0.5f) onGround(twigs[R.i(4)], x, z, R.f() * TAU, R.r(0.08f, 0.22f), 0, 0.0f, TINT1, 0.1f);
  });
  scatter(-1.7f, -0.86f, 3.5f, 9.0f, 0.09f, [&](float x, float z) {
    if (fabsf(z - 6.03f) < 0.14f && x > -1.2f) return;
    if (R.f() < 0.6f) onGround(talls[R.i(5)], x, z, R.f() * TAU, R.r(0.7f, 1.2f), 1.0f, 0, tintN(x, z, 0.2f, 0.2f));
  });
  fprintf(stderr, "hedge: %zu sprigs\n", n);
}

inline void Builder::border() {
  using namespace G;
  // prototypes
  BigLeafP hp; hp.mat = MT.hosta; hp.petMat = MT.stemGreen;
  auto hosta = variants(5, [&](Rng& r) { BigLeafP p = hp; p.petLen = r.r(0.12f, 0.24f); p.len = r.r(0.17f, 0.25f); p.wid = p.len * r.r(0.62f, 0.75f); p.petPitch = r.r(0.7f, 1.2f); return bigLeaf(r, p); });
  auto hostaV = variants(4, [&](Rng& r) { BigLeafP p = hp; p.mat = MT.hostaVar; p.petLen = r.r(0.10f, 0.18f); p.len = r.r(0.13f, 0.19f); p.wid = p.len * r.r(0.55f, 0.65f); p.petPitch = r.r(0.7f, 1.2f); p.wave = 0.07f; return bigLeaf(r, p); });
  auto frond = variants(4, [](Rng& r) { return fernFrond(r, r.r(0.45f, 0.7f), 22 + r.i(8), MT.fern, MT.stemPale); });
  auto poppies = variants(5, [](Rng& r) { return poppy(r); });
  std::vector<V3> foxTop(4), alHead(4);
  std::vector<int> fox, al;
  for (int i = 0; i < 4; i++) { fox.push_back(S.addProto(foxglove(R, &foxTop[i]))); al.push_back(S.addProto(allium(R, &alHead[i]))); }
  auto lav = variants(4, [](Rng& r) { return lavender(r); });
  std::vector<int> daisy[4];
  for (int k = 0; k < 4; k++) daisy[k] = variants(3, [k](Rng& r) { return tallDaisy(r, k); });
  auto chips = variants(6, [](Rng& r) { return barkChip(r); });
  auto petal = variants(3, [](Rng& r) { Mesh m; LeafP L; L.len = 0.05f; L.wid = 0.06f; L.nu = 4; L.nv = 4; L.prof = profEll(0.5f, 0.42f, 0.25f); L.pitch = 0.1f; L.droop = r.r(-0.6f, 0.8f); L.curl = r.r(-0.5f, 0.5f); L.wave = 0.15f; L.w = r.f(); addLeaf(m, L, M34(), MT.petPoppy); return m; });
  auto litter = variants(4, [&](Rng& r) { return deadLeaf(r, MT.litter); });
  auto peb = variants(4, [](Rng& r) { return pebble(r, 10, 7); });

  auto rosette = [&](const std::vector<int>& protos, V3 c, int n, float sc, float flex, float tiltMin, float tiltMax) {
    float y0 = R.f() * TAU;
    for (int i = 0; i < n; i++) {
      float t = (float)i / n;
      S.put(protos[R.i((int)protos.size())], mTrans(c) * mRotY(y0 + i * 2.39996f) * mRotX(-lerpf(tiltMax, tiltMin, t) + R.r(-0.1f, 0.1f)) * mScale(sc * R.r(0.75f, 1.2f) * (0.7f + 0.5f * t)), flex, R.f(), tintN(c.x + i, c.z, 0.2f, R.f() < 0.15f ? 0.5f : 0.0f));
    }
  };
  auto hostaAt = [&](float x, float z, float sc, bool var) { rosette(var ? hostaV : hosta, V3(x, gh(x, z), z), 14 + R.i(8), sc, 0.45f, -0.25f, 0.55f); };
  auto fernAt = [&](float x, float z, float sc) { rosette(frond, V3(x, gh(x, z), z), 9 + R.i(5), sc, 0.6f, -0.1f, 0.75f); };
  auto single = [&](int proto, float x, float z, float sc, float flex) { onGround(proto, x, z, R.f() * TAU, sc, flex, 0.005f, tintN(x, z, 0.15f)); };

  // ---- hero planting along the cat's line (z ~ 7.0..7.4)
  hostaAt(9.60f, 6.70f, 1.15f, false); hostaAt(9.72f, 7.50f, 1.05f, true); fernAt(10.05f, 7.62f, 0.8f); hostaAt(9.95f, 8.75f, 1.2f, false); hostaAt(9.55f, 5.35f, 1.0f, true);
  hostaAt(10.15f, 6.05f, 0.8f, true);
  fernAt(9.75f, 9.75f, 1.0f); fernAt(10.35f, 8.35f, 0.75f); fernAt(9.9f, 4.4f, 1.1f);
  single(poppies[0], 10.72f, 7.72f, 1.0f, 0.5f); single(poppies[1], 10.95f, 7.38f, 1.1f, 0.5f); single(poppies[2], 10.62f, 6.72f, 0.95f, 0.5f);
  single(poppies[3], 10.45f, 8.15f, 1.0f, 0.5f); single(poppies[4], 11.25f, 7.95f, 1.05f, 0.5f); single(poppies[1], 11.05f, 8.5f, 0.9f, 0.5f);
  { // hero allium (bee visits), no random yaw so the head position is known
    float x = 11.02f, z = 6.86f; V3 b(x, gh(x, z), z);
    S.put(al[0], mTrans(b), 0.12f, R.f()); W.beeFlower = b + alHead[0];
    x = 11.38f; z = 7.05f; b = V3(x, gh(x, z), z); S.put(al[1], mTrans(b), 0.12f, R.f()); W.beeFlower2 = b + alHead[1];
    single(al[2], 10.85f, 6.35f, 1.0f, 0.3f); single(al[3], 11.45f, 6.45f, 1.05f, 0.3f); single(al[2], 11.2f, 6.1f, 0.9f, 0.3f); single(al[1], 10.62f, 5.9f, 0.95f, 0.3f);
  }
  single(fox[0], 11.62f, 8.25f, 1.0f, 0.3f); single(fox[1], 11.78f, 7.82f, 0.9f, 0.3f); single(fox[2], 11.42f, 8.85f, 1.05f, 0.3f); single(fox[3], 11.7f, 6.25f, 1.0f, 0.3f);
  single(fox[1], 11.95f, 6.6f, 0.85f, 0.3f); single(fox[0], 11.3f, 5.7f, 1.0f, 0.3f);
  for (float z : {5.9f, 6.55f, 8.0f, 8.6f, 9.25f, 5.2f}) single(lav[R.i(4)], 12.2f + R.r(-0.05f, 0.05f), z, R.r(0.9f, 1.15f), 0.6f);
  single(daisy[0][0], 10.95f, 9.05f, 1.0f, 0.5f); single(daisy[0][1], 11.3f, 9.5f, 1.0f, 0.5f); single(daisy[1][0], 10.35f, 5.55f, 1.0f, 0.5f); single(daisy[1][1], 10.75f, 5.25f, 1.0f, 0.5f);
  single(daisy[2][0], 11.45f, 9.9f, 1.0f, 0.6f); single(daisy[3][0], 10.1f, 9.3f, 1.0f, 0.5f); single(daisy[3][1], 10.5f, 9.6f, 1.0f, 0.5f); single(daisy[2][1], 11.9f, 5.4f, 1.0f, 0.6f);
  // ---- rest of the bed: drifts chosen by noise
  scatter(BED_X0 + 0.2f, BED_X1 - 0.25f, BED_Z0 + 0.2f, BED_Z1 - 0.2f, 0.42f, [&](float x, float z) {
    if (bedSD(x, z) > -0.15f) return;
    if (z > 4.6f && z < 10.2f) return; // hand planted above
    if (len(V2(x - 11.2f, z - 2.4f)) < 0.5f) return; // birch trunk
    float n = fbm(V3(x * 0.55f, 90, z * 0.55f), 3) * 9.0f + (x - BED_X0) * 0.4f;
    int sp = ((int)n) % 9;
    float fx = (x - BED_X0) / (BED_X1 - BED_X0);
    if (fx < 0.3f) { if (R.chance(0.5f)) hostaAt(x, z, R.r(0.8f, 1.2f), R.chance(0.4f)); else if (R.chance(0.4f)) fernAt(x, z, R.r(0.7f, 1.1f)); return; }
    if (fx > 0.85f) { if (R.chance(0.7f)) single(lav[R.i(4)], x, z, R.r(0.85f, 1.15f), 0.6f); return; }
    switch (sp) {
      case 0: case 1: single(poppies[R.i(5)], x, z, R.r(0.85f, 1.15f), 0.5f); break;
      case 2: single(fox[R.i(4)], x, z, R.r(0.85f, 1.1f), 0.3f); break;
      case 3: single(al[R.i(4)], x, z, R.r(0.85f, 1.1f), 0.3f); if (R.chance(0.6f)) single(al[R.i(4)], x + 0.2f, z + 0.15f, R.r(0.85f, 1.1f), 0.3f); break;
      case 4: single(daisy[0][R.i(3)], x, z, R.r(0.9f, 1.15f), 0.5f); break;
      case 5: single(daisy[1][R.i(3)], x, z, R.r(0.9f, 1.15f), 0.5f); break;
      case 6: single(daisy[2][R.i(3)], x, z, R.r(0.9f, 1.15f), 0.6f); break;
      case 7: single(daisy[3][R.i(3)], x, z, R.r(0.9f, 1.15f), 0.5f); break;
      default: single(fox[R.i(4)], x, z, R.r(0.8f, 1.0f), 0.3f); break;
    }
  });
  // ---- mulch, stones, fallen petals and leaves on the soil
  scatter(BED_X0 - 0.1f, BED_X1, BED_Z0, BED_Z1, 0.032f, [&](float x, float z) {
    if (bedSD(x, z) > -0.01f) return;
    float d = W.rd.at(x, z);
    if (d > 3.2f) { if (R.f() > 0.05f) return; }
    else if (R.f() > 0.62f) return;
    float s = (d > 3.2f ? 0.03f : 0.012f) * R.r(0.5f, 1.6f);
    if (R.f() < 0.07f) S.put(peb[R.i(4)], place(V3(x, gh(x, z) + s * 0.15f, z), R.f() * TAU, s * 0.7f, R.r(-0.4f, 0.4f), R.r(-0.4f, 0.4f)), 0, R.f(), tintN(x, z, 0.2f));
    else S.put(chips[R.i(6)], place(V3(x, gh(x, z) + 0.001f, z), R.f() * TAU, s, R.r(-0.35f, 0.35f), R.r(-0.35f, 0.35f)), 0, R.f(), tintN(x, z, 0.35f));
  });
  for (int i = 0; i < 26; i++) { float x = 10.8f + R.g() * 0.35f, z = 7.6f + R.g() * 0.45f; onGround(petal[R.i(3)], x, z, R.f() * TAU, R.r(0.8f, 1.1f), 0, -0.004f, TINT1, 0.2f); }
  scatter(BED_X0, BED_X1, 4.5f, 10.5f, 0.3f, [&](float x, float z) { if (R.f() < 0.5f && bedSD(x, z) < 0) onGround(litter[R.i(4)], x, z, R.f() * TAU, R.r(0.6f, 1.0f), 0, -0.006f, tintN(x, z, 0.3f), 0.3f); });
}

inline void Builder::pathBench() {
  // brick edging between bed and path (individual bricks, slightly wonky)
  Mesh brick; addBlob(brick, V3(0, 0, 0), V3(0.058f, 0.040f, 0.118f), 10, 8, 0.05f, 2.2f, 3.0f, MT.brickUnit);
  for (auto& v : brick.v) { // square it up: push the blob towards a box
    V3 q = v.p; auto sq = [](float x, float r) { return r * squarish(x / r, 0.22f); };
    v.p = V3(sq(q.x, 0.052f), sq(q.y, 0.034f), sq(q.z, 0.107f));
  }
  auto bricks = variants(1, [&](Rng&) { return brick; });
  Mesh b2 = brick; for (auto& v : b2.v) v.p = v.p + V3(0.004f * sinf(v.p.z * 40), 0.003f * sinf(v.p.x * 70 + 1), 0);
  bricks.push_back(S.addProto(b2));
  for (float z = G::BED_Z0; z < G::BED_Z1; z += 0.222f) {
    float x = 12.645f + R.r(-0.006f, 0.006f);
    float c = R.r(0.75f, 1.2f);
    S.put(bricks[R.i(2)], place(V3(x, 0.006f + R.r(-0.006f, 0.008f), z + 0.11f), R.r(-0.03f, 0.03f), 1.0f, R.r(-0.04f, 0.04f), R.r(-0.05f, 0.05f)), 0, R.f(), packTint(V3(c * R.r(0.9f, 1.15f), c * R.r(0.85f, 1.05f), c * R.r(0.8f, 1.05f))));
  }
  // bench (faces east, towards the pond); the cat walks underneath
  Mesh bench;
  const float zc[2] = {-0.72f, 0.72f};
  for (int e = 0; e < 2; e++) {
    addBox(bench, V3(0.18f, 0, zc[e] - 0.025f), V3(0.225f, 0.40f, zc[e] + 0.025f), MT.iron);
    addBox(bench, V3(-0.245f, 0, zc[e] - 0.025f), V3(-0.20f, 0.86f, zc[e] + 0.025f), MT.iron);
    addBox(bench, V3(-0.245f, 0.36f, zc[e] - 0.022f), V3(0.225f, 0.40f, zc[e] + 0.022f), MT.iron);
    addBox(bench, V3(-0.245f, 0.60f, zc[e] - 0.028f), V3(0.25f, 0.635f, zc[e] + 0.028f), MT.iron);
    addBox(bench, V3(0.205f, 0.40f, zc[e] - 0.02f), V3(0.24f, 0.60f, zc[e] + 0.02f), MT.iron);
    addBox(bench, V3(-0.26f, 0.0f, zc[e] - 0.04f), V3(-0.18f, 0.012f, zc[e] + 0.04f), MT.iron);
    addBox(bench, V3(0.16f, 0.0f, zc[e] - 0.04f), V3(0.245f, 0.012f, zc[e] + 0.04f), MT.iron);
  }
  for (int i = 0; i < 5; i++) { float x = -0.185f + i * 0.096f; addBox(bench, V3(x - 0.038f, 0.402f, -0.80f), V3(x + 0.038f, 0.43f, 0.80f), MT.woodBench, M34(), 0.07f * i); }
  for (int i = 0; i < 3; i++) { float y = 0.53f + i * 0.125f; addBox(bench, V3(-0.262f - 0.02f * i, y - 0.042f, -0.80f), V3(-0.238f - 0.02f * i, y + 0.042f, 0.80f), MT.woodBench, M34(), 0.4f + 0.07f * i); }
  S.put(S.addProto(bench), mTrans(V3(15.06f, gh(15.06f, 7.98f) - 0.004f, 7.98f)), 0, 0.4f);
  // a few weeds growing at the bench feet and along the path edge
  auto weeds = variants(3, [](Rng& r) { return dandelion(r, 0); });
  auto tuft = variants(3, [](Rng& r) { return grassClump(r, r.r(0.08f, 0.15f), 24, 0.02f, 0.5f, MT.grass); });
  onGround(weeds[0], 14.83f, 7.22f, 1, 0.8f, 0.4f); onGround(weeds[1], 15.28f, 8.76f, 2, 0.9f, 0.4f); onGround(tuft[0], 14.84f, 8.74f, 0, 1.0f, 0.8f); onGround(tuft[1], 15.26f, 7.2f, 0, 1.2f, 0.8f);
  scatter(12.6f, 14.6f, 3.0f, 12.0f, 0.25f, [&](float x, float z) {
    float ps = G::pathSD(x, z);
    if (fabsf(z - 7.4f) < 0.75f && x < 13.3f) return; // keep the cat's crossing clear
    if (ps < -0.12f && ps > -0.9f) { if (R.f() < 0.02f) onGround(tuft[R.i(3)], x, z, R.f() * TAU, R.r(0.5f, 0.9f), 0.8f); return; }
    if (ps < 0.0f && R.f() < 0.22f) onGround(tuft[R.i(3)], x, z, R.f() * TAU, R.r(0.6f, 1.1f), 0.8f);
  });
}

inline void Builder::pond() {
  using namespace G;
  // water surface
  Mesh wm;
  u32 a = wm.add(V3(PCX - PA - 0.6f, WATER_Y, PCZ - PB - 0.6f), V3(0, 1, 0), 0, 0), b = wm.add(V3(PCX + PA + 0.6f, WATER_Y, PCZ - PB - 0.6f), V3(0, 1, 0), 1, 0);
  u32 c = wm.add(V3(PCX + PA + 0.6f, WATER_Y, PCZ + PB + 0.6f), V3(0, 1, 0), 1, 1), d = wm.add(V3(PCX - PA - 0.6f, WATER_Y, PCZ + PB + 0.6f), V3(0, 1, 0), 0, 1);
  wm.quad(a, d, c, b, MT.water);
  S.put(S.addProto(wm), M34(), 0, 0, TINT1, F_NOSHADOW);
  auto flat = variants(7, [](Rng& r) { return rock(r, MT.stoneMoss, 0.28f); });
  auto rocks = variants(6, [](Rng& r) { return rock(r, MT.stoneMoss, 0.7f); });
  auto peb = variants(5, [](Rng& r) { return pebble(r, 12, 8, MT.stoneWet); });
  auto reeds = variants(5, [](Rng& r) { return reedClump(r, r.chance(0.6f)); });
  auto iris = variants(4, [](Rng& r) { return irisClump(r, r.chance(0.7f)); });
  auto pads = variants(7, [](Rng& r) { return lilyPadMesh(r, true); });
  auto flw = variants(3, [](Rng& r) { return lilyFlower(r, false); });
  int bud = S.addProto(lilyFlower(R, true));
  float zs = 8.12f, xs = shoreX(zs);
  // rim stones
  for (int i = 0; i < 150; i++) {
    float th = i / 150.0f * TAU + R.r(-0.02f, 0.02f);
    float deg = th * 180 / PI;
    bool planted = (deg > 285 || deg < 80); // east / north-east margin left to reeds
    if (planted && R.f() < 0.75f) continue;
    // find rim point along the ray from the centre
    float lo = 0.5f, hi = 1.6f;
    for (int k = 0; k < 24; k++) { float m = 0.5f * (lo + hi); if (pondSD(PCX + cosf(th) * PA * m, PCZ + sinf(th) * PB * m) < 0.09f) lo = m; else hi = m; }
    float x = PCX + cosf(th) * PA * lo, z = PCZ + sinf(th) * PB * lo;
    if (len(V2(x - (xs - 0.22f), z - zs)) < 0.34f) continue; // hero stone goes here
    float s = R.r(0.10f, 0.19f);
    S.put(flat[R.i(7)], place(V3(x + R.r(-0.03f, 0.03f), 0.0f + R.r(-0.015f, 0.01f), z + R.r(-0.03f, 0.03f)), th + PI / 2 + R.r(-0.4f, 0.4f), s, R.r(-0.06f, 0.06f), R.r(-0.06f, 0.06f)), 0, R.f(), tintN(x, z, 0.2f));
  }
  S.put(flat[0], place(V3(xs - 0.225f, 0.004f, zs), 0.2f, 0.185f, 0.0f, -0.03f), 0, 0.31f); // the cat's drinking stone
  S.put(flat[2], place(V3(xs - 0.20f, -0.005f, zs + 0.42f), 1.2f, 0.16f, 0.03f, 0.0f), 0, 0.55f);
  S.put(flat[3], place(V3(xs - 0.17f, -0.004f, zs - 0.40f), 2.4f, 0.17f, 0.0f, 0.03f), 0, 0.72f);
  // rocks in / by the water, bottom stones
  for (int i = 0; i < 26; i++) {
    float th = R.f() * TAU, m = R.r(0.55f, 0.98f);
    float x = PCX + cosf(th) * PA * m, z = PCZ + sinf(th) * PB * m;
    if (len(V2(x - xs, z - zs)) < 0.7f) continue;
    float s = R.r(0.08f, 0.22f);
    S.put(rocks[R.i(6)], place(V3(x, gh(x, z) + s * 0.2f, z), R.f() * TAU, s, R.r(-0.2f, 0.2f), R.r(-0.2f, 0.2f)), 0, R.f(), tintN(x, z, 0.2f));
  }
  scatter(PCX - PA, PCX + PA, PCZ - PB, PCZ + PB, 0.07f, [&](float x, float z) {
    float pd = pondSD(x, z);
    if (pd > 0.02f || R.f() > (len(V2(x - xs, z - zs)) < 1.6f ? 0.85f : 0.2f)) return;
    float s = R.r(0.012f, 0.05f);
    S.put(peb[R.i(5)], place(V3(x, gh(x, z) + s * 0.2f, z), R.f() * TAU, s, R.r(-0.3f, 0.3f), R.r(-0.3f, 0.3f)), 0, R.f(), tintN(x, z, 0.2f));
  });
  // marginal plants
  for (int i = 0; i < 44; i++) {
    float deg = R.chance(0.5f) ? R.r(-85, -28) : R.r(30, 85), th = deg * PI / 180, m = R.r(0.86f, 1.10f);
    float x = PCX + cosf(th) * PA * m, z = PCZ + sinf(th) * PB * m;
    onGround(reeds[R.i(5)], x, z, R.f() * TAU, R.r(0.75f, 1.15f), 0.7f, 0.02f, tintN(x, z, 0.15f, 0.15f));
  }
  for (int i = 0; i < 22; i++) {
    float deg = R.chance(0.6f) ? R.r(80, 150) : R.r(200, 290), th = deg * PI / 180, m = R.r(0.97f, 1.14f);
    float x = PCX + cosf(th) * PA * m, z = PCZ + sinf(th) * PB * m;
    if (len(V2(x - xs, z - zs)) < 0.8f) continue;
    onGround(iris[R.i(4)], x, z, R.f() * TAU, R.r(0.8f, 1.1f), 0.5f, 0.01f, tintN(x, z, 0.15f));
  }
  onGround(iris[0], xs - 0.12f, zs + 0.95f, 1.0f, 0.9f, 0.5f, 0.01f); onGround(iris[1], xs - 0.02f, zs - 1.0f, 2.0f, 0.85f, 0.5f, 0.01f);
  // water lilies
  auto padAt = [&](float x, float z, float r) {
    if (pondSD(x, z) > -0.15f) return;
    S.put(pads[R.i(7)], place(V3(x, WATER_Y, z), R.f() * TAU, r), 0, R.f(), tintN(x, z, 0.2f, R.f() < 0.2f ? 0.5f : 0.0f));
    W.lilyPads.push_back(V3(x, WATER_Y + 0.006f, z));
  };
  padAt(xs + 0.62f, zs + 0.30f, 0.105f); padAt(xs + 0.80f, zs - 0.22f, 0.12f); padAt(xs + 1.02f, zs + 0.52f, 0.11f); padAt(xs + 1.15f, zs + 0.05f, 0.095f); padAt(xs + 0.92f, zs + 0.86f, 0.085f);
  padAt(xs + 1.38f, zs - 0.38f, 0.115f); padAt(xs + 0.45f, zs + 0.78f, 0.075f);
  S.put(flw[0], place(V3(xs + 1.0f, WATER_Y + 0.004f, zs + 0.22f), 0.4f, 1.15f), 0, 0.2f);
  S.put(bud, place(V3(xs + 0.70f, WATER_Y + 0.0f, zs + 0.60f), 1.4f, 1.0f, 0.12f), 0, 0.5f);
  const float cl[4][2] = {{PCX - 0.9f, PCZ + 0.5f}, {PCX + 0.9f, PCZ - 0.9f}, {PCX + 0.2f, PCZ + 1.3f}, {PCX + 1.7f, PCZ + 0.5f}};
  for (int k = 0; k < 4; k++) {
    for (int i = 0; i < 16; i++) padAt(cl[k][0] + R.g() * 0.42f, cl[k][1] + R.g() * 0.36f, R.r(0.07f, 0.13f));
    for (int i = 0; i < 3; i++) S.put(i == 2 ? bud : flw[R.i(3)], place(V3(cl[k][0] + R.g() * 0.3f, WATER_Y + 0.004f, cl[k][1] + R.g() * 0.3f), R.f() * TAU, R.r(0.9f, 1.25f), R.r(-0.1f, 0.1f)), 0, R.f(), k % 2 ? packTint(V3(1.05f, 0.75f, 0.85f)) : TINT1);
  }
}

inline void Builder::trees() {
  auto birchSprig = variants(4, [](Rng& r) { return sprig(r, r.r(0.28f, 0.4f), 13 + r.i(5), 0.055f, 0.72f, MT.birchLeaf, MT.twig, 0.5f); });
  auto leafSprig = variants(4, [](Rng& r) { return sprig(r, r.r(0.3f, 0.42f), 12 + r.i(5), 0.085f, 0.6f, MT.treeLeaf, MT.twig, 0.3f, 4); });
  auto farSprig = variants(4, [](Rng& r) { return sprig(r, r.r(0.3f, 0.42f), 9 + r.i(4), 0.10f, 0.62f, MT.treeLeaf, MT.twig, 0.3f); });
  size_t n = 0;
  auto tree = [&](V3 pos, const TreeP& P, const std::vector<int>& sp, float perTip, float sc, float droop, float flex, V3 tintC) {
    Mesh m; std::vector<Tip> tips;
    treeBranch(m, R, P, V3(0, -0.05f, 0), norm(V3(R.r(-0.05f, 0.05f), 1, R.r(-0.05f, 0.05f))), P.h * 0.55f, P.r, 0, tips);
    M34 x = mTrans(pos) * mRotY(R.f() * TAU);
    S.put(S.addProto(m), x, 0, R.f());
    for (auto& t : tips) {
      int k = (int)perTip + (R.f() < perTip - (int)perTip ? 1 : 0);
      for (int i = 0; i < k; i++) {
        V3 d = norm(t.d * 0.5f + V3(R.r(-1, 1), R.r(-0.6f, 0.8f) - droop, R.r(-1, 1)));
        V3 p = x.pt(t.p + V3(R.r(-1, 1), R.r(-1, 1), R.r(-1, 1)) * (0.12f * sc));
        float c = R.r(0.8f, 1.2f);
        S.put(sp[R.i((int)sp.size())], mAlignY(x.dir(d), p) * mRotY(R.f() * TAU) * mScale(sc * R.r(0.8f, 1.25f)), flex, R.f(), packTint(tintC * c));
        n++;
      }
    }
  };
  TreeP birch; birch.h = 7.0f; birch.r = 0.095f; birch.levels = 3; birch.kids = 4; birch.spread = 0.72f; birch.shrink = 0.6f; birch.droop = 0.9f; birch.firstBranch = 0.36f; birch.bark = MT.barkBirch; birch.wob = 0.07f;
  tree(V3(11.2f, 0, 2.4f), birch, birchSprig, 2.6f, 1.0f, 0.8f, 0.9f, V3(1, 1, 1));
  TreeP maple; maple.h = 6.5f; maple.r = 0.14f; maple.levels = 3; maple.kids = 4; maple.spread = 0.85f; maple.shrink = 0.62f; maple.droop = 0.1f; maple.firstBranch = 0.32f; maple.bark = MT.bark; maple.wob = 0.09f;
  tree(V3(4.5f, 0, 17.5f), maple, leafSprig, 3.0f, 1.15f, 0.2f, 0.7f, V3(1, 1, 1));
  tree(V3(29.0f, 0, 0.5f), maple, leafSprig, 3.0f, 1.1f, 0.2f, 0.7f, V3(1.15f, 1.0f, 0.8f));
  tree(V3(32.5f, 0, 21.5f), birch, birchSprig, 2.4f, 1.0f, 0.8f, 0.9f, V3(1, 1, 1));
  // backdrop trees beyond the walls
  TreeP big; big.h = 13.0f; big.r = 0.3f; big.levels = 3; big.kids = 4; big.spread = 0.8f; big.shrink = 0.62f; big.droop = 0.1f; big.firstBranch = 0.3f; big.bark = MT.bark; big.wob = 0.08f;
  const float bt[][3] = {{-6, 2, 1.0f}, {-7.5f, 9, 1.15f}, {-5.5f, 15, 0.9f}, {-8, 22, 1.2f}, {3, 31, 1.1f}, {11, 32, 0.95f}, {19, 31, 1.2f}, {27, 33, 1.0f}, {34, 31, 1.1f}, {41, 28, 1.0f}, {31, -9, 0.8f}, {20, -9, 1.0f}, {9, -8, 1.2f}, {0, -7, 0.95f}, {-9, -4, 1.1f}};
  for (auto& b : bt) {
    TreeP p = big; p.h *= b[2] * R.r(0.7f, 1.35f); p.spread = R.r(0.45f, 1.0f); p.shrink = R.r(0.55f, 0.68f); p.kids = 3 + R.i(3); p.firstBranch = R.r(0.2f, 0.45f); p.droop = R.r(0.0f, 0.5f); p.r = 0.02f * p.h + 0.05f;
    float c = R.r(0.6f, 1.15f);
    tree(V3(b[0] + R.r(-1, 1), 0, b[1] + R.r(-1, 1)), p, farSprig, R.r(1.6f, 2.6f), R.r(2.4f, 3.8f), R.r(0.1f, 0.5f), 0.6f, V3(c * R.r(0.8f, 1.3f), c, c * R.r(0.6f, 1.0f)));
  }
  // garden walls (brick) on south, north, east
  Mesh wl;
  addBox(wl, V3(-1.0f, -0.1f, -2.35f), V3(36.0f, 2.1f, -2.05f), MT.brick);
  addBox(wl, V3(-1.0f, -0.1f, 25.8f), V3(36.0f, 2.1f, 26.1f), MT.brick);
  addBox(wl, V3(35.7f, -0.1f, -2.35f), V3(36.0f, 2.1f, 26.1f), MT.brick);
  int mCap = S.addMat(5, V3(0.36f, 0.35f, 0.33f), 0.8f, V3(0, 0, 0), 0, V3(0, 0, 0), 1.2f, 0, 1, 1.2f);
  addBox(wl, V3(-1.05f, 2.1f, -2.42f), V3(36.05f, 2.17f, -1.98f), mCap);
  addBox(wl, V3(-1.05f, 2.1f, 25.73f), V3(36.05f, 2.17f, 26.17f), mCap);
  addBox(wl, V3(35.63f, 2.1f, -2.42f), V3(36.07f, 2.17f, 26.17f), mCap);
  S.put(S.addProto(wl), M34(), 0, 0.37f);
  // clipped box balls and loose shrubs
  auto bx = variants(4, [](Rng& r) { return sprig(r, r.r(0.07f, 0.1f), 12, 0.022f, 0.55f, MT.sprig, MT.twig, 0.1f); });
  int mCore = S.addMat(0, V3(0.012f, 0.03f, 0.01f), 0.9f, V3(0.02f, 0.045f, 0.012f), 0, V3(0, 0, 0), 0.8f, 20, 0.2f);
  Mesh coreM; addBlob(coreM, V3(0, 0, 0), V3(1, 1, 1), 16, 10, 0.05f, 2, 1, mCore);
  int core = S.addProto(coreM);
  auto ball = [&](V3 c, float rad, float dens, float sc) {
    S.put(core, place(c, 0, rad * 0.88f));
    int cnt = (int)(dens * 4 * PI * rad * rad);
    for (int i = 0; i < cnt; i++) {
      V3 d = norm(V3(R.g(), R.g() + 0.3f, R.g()));
      if (d.y < -0.75f) continue;
      V3 p = c + d * (rad * R.r(0.9f, 1.0f));
      S.put(bx[R.i(4)], mAlignY(norm(d + V3(R.r(-0.5f, 0.5f), R.r(-0.5f, 0.5f), R.r(-0.5f, 0.5f))), p) * mRotY(R.f() * TAU) * mScale(sc * R.r(0.8f, 1.2f)), 0.3f, R.f(), tintN(p.x, p.z, 0.25f));
      n++;
    }
  };
  ball(V3(15.5f, 0.36f, 6.35f), 0.42f, 900, 1.0f); ball(V3(15.45f, 0.40f, 9.95f), 0.46f, 900, 1.0f);
  ball(V3(8.6f, 0.45f, 1.3f), 0.55f, 250, 2.0f); ball(V3(8.6f, 0.45f, 13.4f), 0.55f, 250, 2.0f); ball(V3(15.6f, 0.5f, 3.0f), 0.6f, 250, 2.0f);
  for (int i = 0; i < 16; i++) { // loose shrubs along the walls
    float x = R.r(1, 34), z = R.chance(0.5f) ? R.r(-1.5f, -0.6f) : R.r(24.2f, 25.2f);
    ball(V3(x, R.r(0.5f, 0.9f), z), R.r(0.6f, 1.1f), 60, 3.4f);
  }
  for (int i = 0; i < 6; i++) ball(V3(R.r(34.2f, 35.0f), R.r(0.5f, 0.9f), R.r(0, 24)), R.r(0.6f, 1.0f), 60, 3.4f);
  fprintf(stderr, "trees/shrubs: %zu sprigs\n", n);
}

inline void Builder::labels() {
  static const char* names[16] = {"PAPAVER ORIENTALE", "DIGITALIS PURPUREA", "ALLIUM GIGANTEUM", "LAVANDULA", "HOSTA SIEBOLDIANA", "NYMPHAEA ALBA", "IRIS PSEUDACORUS", "SARRACENIA FLAVA",
                                  "DIONAEA MUSCIPULA", "ECHINOCACTUS", "MONSTERA DELICIOSA", "MUSA BASJOO", "PHALAENOPSIS", "AGAVE AMERICANA", "ECHEVERIA ELEGANS", "DICKSONIA"};
  static const char* F[26] = {"01110100011000111111100011000110001", "11110100011000111110100011000111110", "01110100011000010000100001000101110", "11110100011000110001100011000111110",
                              "11111100001000011110100001000011111", "11111100001000011110100001000010000", "01110100011000010111100011000101111", "10001100011000111111100011000110001",
                              "01110001000010000100001000010001110", "00111000100001000010000101001001100", "10001100101010011000101001001010001", "10000100001000010000100001000011111",
                              "10001110111010110101100011000110001", "10001110011010110011100011000110001", "01110100011000110001100011000101110", "11110100011000111110100001000010000",
                              "01110100011000110001101011001001101", "11110100011000111110101001001010001", "01111100001000001110000010000111110", "11111001000010000100001000010000100",
                              "10001100011000110001100011000101110", "10001100011000110001100010101000100", "10001100011000110101101011010101010", "10001100010101000100010101000110001",
                              "10001100010101000100001000010000100", "11111000010001000100010001000011111"};
  const int cols = 20, up = 4;
  W.labW = cols * 6 * up; W.labH = 16 * 8 * up; W.lab.assign((size_t)W.labW * W.labH, 0);
  for (int r = 0; r < 16; r++) {
    int L = (int)strlen(names[r]); int x0 = (cols - L) * 3;
    for (int i = 0; i < L; i++) {
      char ch = names[r][i]; if (ch < 'A' || ch > 'Z') continue;
      for (int gy = 0; gy < 7; gy++)
        for (int gx = 0; gx < 5; gx++)
          if (F[ch - 'A'][gy * 5 + gx] == '1')
            for (int sy = 0; sy < up; sy++)
              for (int sx = 0; sx < up; sx++) W.lab[(size_t)((r * 8 + gy) * up + sy) * W.labW + (x0 + i * 6 + gx) * up + sx] = 255;
    }
  }
  Mesh m;
  addBox(m, V3(-0.006f, -0.08f, -0.0035f), V3(0.006f, 0.128f, -0.0005f), MT.labelStake);
  M34 px = mTrans(V3(0, 0.125f, 0.002f)) * mRotX(-0.55f);
  addBox(m, V3(-0.058f, -0.002f, -0.004f), V3(0.058f, 0.062f, -0.0005f), MT.labelStake, px);
  addGrid(m, 1, 1, 0, 1, 0, 1, [](float u, float v) { return V3((u - 0.5f) * 0.112f, v * 0.058f, 0); }, MT.label, px);
  int lp = S.addProto(m);
  auto lab = [&](float x, float z, float yaw, int row, float y = -1e9f) { S.put(lp, place(V3(x, y > -1e8f ? y : gh(x, z), z), yaw, 1.0f, R.r(-0.08f, 0.08f), R.r(-0.1f, 0.1f)), 0, (row + 0.5f) / 16.0f); };
  lab(10.42f, 7.50f, -1.75f, 0); lab(11.0f, 8.0f, -1.5f, 1); lab(10.72f, 6.95f, -1.9f, 2); lab(12.0f, 7.0f, -1.4f, 3); lab(9.42f, 7.38f, -1.6f, 4);
  float xs = G::shoreX(8.12f);
  lab(xs - 0.42f, 8.62f, -1.3f, 5); lab(xs - 0.3f, 7.2f, -1.9f, 6);
  lab(20.75f, 16.86f, 0.1f, 7); lab(21.6f, 16.88f, -0.2f, 8); lab(G::CACTUS.x + 0.28f, G::CACTUS.z + 0.2f, 0.3f, 9); lab(19.3f, 16.8f, 0.2f, 10); lab(17.2f, 16.6f, 0.9f, 11);
  lab(20.3f, 18.25f, 3.0f, 12); lab(26.6f, 16.85f, 0.0f, 13); lab(24.2f, 16.9f, 0.2f, 14); lab(16.4f, 18.3f, 2.4f, 15);
}
