// plants2.h — border perennials, shrubs, trees, pond plants
#pragma once
#include "plants.h"

// twig with leaves; local +Y along the twig
static inline Mesh sprig(Rng& R, float len, int nl, float leafLen, float widRatio, int leafMat, int twigMat, float droop = 0.3f, int nu = 2) {
  Mesh m;
  V3 a(0, 0, 0), b(R.r(-0.15f, 0.15f) * len, len, R.r(-0.15f, 0.15f) * len);
  std::vector<V3> pts;
  addStem(m, a, b, R.r(-0.1f, 0.1f), len * 0.012f + 0.0006f, 0.0005f, twigMat, R, &pts, 4);
  for (int i = 0; i < nl; i++) {
    float t = 0.12f + 0.88f * (i + R.f() * 0.6f) / nl; int k = std::min((int)(t * 8), 7);
    V3 p = lerp(pts[k], pts[k + 1], t * 8 - k);
    LeafP L; L.len = leafLen * R.r(0.65f, 1.15f) * (0.75f + 0.25f * sinf(PI * t)); L.wid = L.len * widRatio * R.r(0.85f, 1.15f); L.nu = nu; L.nv = nu > 2 ? 6 : 4;
    L.prof = profEll(R.r(0.75f, 1.0f), 0.65f);
    L.pitch = R.r(0.1f, 0.9f) - droop; L.droop = R.r(0.0f, 0.7f) + droop; L.fold = R.r(0.1f, 0.45f); L.twist = R.r(-0.4f, 0.4f); L.curl = R.r(-0.2f, 0.3f); L.w = R.f();
    addLeaf(m, L, mTrans(p) * mRotY(i * 2.4f + R.r(-0.5f, 0.5f)) * mTrans(V3(0, 0, len * 0.01f)), leafMat);
  }
  return m;
}

// ---------------------------------------------------------------- big basal leaf on a petiole (hosta, calathea, alocasia...)
struct BigLeafP { float petLen = 0.18f, petR = 0.004f, petPitch = 1.1f, len = 0.2f, wid = 0.13f, base = 0.5f, k = 0.8f, e = 0.55f, droop = 0.9f, fold = 0.25f, wave = 0.04f, waveN = 3, curl = 0.0f, bladePitch = 0.3f; int nu = 8, nv = 12; int mat = 0, petMat = 0; };
static inline Mesh bigLeaf(Rng& R, const BigLeafP& P) {
  Mesh m;
  float a = P.petPitch;
  V3 end(0, sinf(a) * P.petLen, cosf(a) * P.petLen);
  auto pts = curvePts(V3(0, 0, 0), V3(0, P.petLen * 0.45f, 0.02f * P.petLen), end * 0.75f + V3(0, 0.1f * P.petLen, 0), end, 8);
  addTube(m, pts, [&](float t) { return P.petR * (1.25f - 0.5f * t); }, 5, P.petMat, M34(), R.f(), false);
  LeafP L; L.len = P.len; L.wid = P.wid; L.nu = P.nu; L.nv = P.nv; L.prof = profEll(P.k, P.e, P.base);
  L.pitch = P.bladePitch; L.droop = P.droop * R.r(0.7f, 1.3f); L.droopPow = 1.4f; L.fold = P.fold * R.r(0.6f, 1.4f); L.wave = P.wave; L.waveN = P.waveN; L.curl = P.curl; L.twist = R.r(-0.25f, 0.25f); L.side = R.r(-0.2f, 0.2f); L.w = R.f(); L.phase = R.f() * 6;
  addLeaf(m, L, mTrans(end) * mRotY(R.r(-0.15f, 0.15f)), P.mat);
  return m;
}
// ---------------------------------------------------------------- fern frond along +Z (arching)
static inline Mesh fernFrond(Rng& R, float Lf, int pairs, int mat, int stemMat, float pinnaRatio = 0.2f) {
  Mesh m;
  auto pts = curvePts(V3(0, 0, 0), V3(0, 0.30f * Lf, 0.25f * Lf), V3(0, 0.42f * Lf, 0.7f * Lf), V3(0, 0.22f * Lf * R.r(0.5f, 1.3f), Lf), 16);
  addTube(m, pts, [&](float t) { return Lf * 0.006f * (1.1f - t); }, 4, stemMat, M34(), R.f(), false);
  for (int i = 0; i < pairs; i++) {
    float t = 0.14f + 0.85f * (i + 0.5f) / pairs; int k = std::min((int)(t * 16), 15);
    V3 p = lerp(pts[k], pts[k + 1], t * 16 - k), T = norm(pts[k + 1] - pts[k]);
    V3 N = norm(cross(T, V3(1, 0, 0)) * -1.0f);
    float pl = Lf * pinnaRatio * powf(sinf(PI * powf(t, 0.75f)), 0.8f) * R.r(0.9f, 1.1f) + 0.004f;
    for (int sgn = -1; sgn <= 1; sgn += 2) {
      V3 d = norm(V3((float)sgn, 0, 0) * 0.92f + T * 0.32f + N * R.r(-0.12f, 0.1f));
      LeafP L; L.len = pl; L.wid = pl * 0.24f; L.nu = 2; L.nv = 5; L.prof = profStrap(1.6f, 1.0f); L.pitch = 0.05f; L.droop = R.r(0.1f, 0.5f); L.fold = 0.2f; L.wave = 0.12f; L.waveN = 6; L.w = R.f(); L.phase = R.f() * 6;
      V3 Z = d, X = norm(cross(N, Z)), Y = cross(Z, X);
      addLeaf(m, L, mBasis(X, Y, Z, p), mat);
    }
  }
  return m;
}
// ---------------------------------------------------------------- perennials (single prototypes, origin at ground)
static inline Mesh poppy(Rng& R) {
  Mesh m;
  int nl = 7 + R.i(5);
  for (int i = 0; i < nl; i++) {
    LeafP L; L.len = R.r(0.12f, 0.24f); L.wid = L.len * R.r(0.25f, 0.35f); L.nu = 4; L.nv = 14;
    float teeth = R.r(6, 9), ph = R.f();
    L.prof = [=](float v) { float s = powf(fmaxf(sinf(PI * powf(v, 0.9f)), 0.0f), 0.7f); float t = fabsf(fmodf(v * teeth + ph, 1.0f) - 0.4f) / 0.6f; return s * (0.3f + 0.7f * fminf(1.0f, t * 1.4f)) + 0.05f * (1 - v); };
    L.pitch = R.r(0.3f, 1.0f); L.droop = R.r(0.5f, 1.3f); L.fold = R.r(0.1f, 0.4f); L.wave = 0.06f; L.waveN = teeth; L.twist = R.r(-0.4f, 0.4f); L.w = R.f();
    addLeaf(m, L, mRotY(i * 2.4f + R.r(-0.3f, 0.3f)) * mTrans(V3(0, 0.01f, 0.01f)), MT.foxLeaf);
  }
  int ns = 3 + R.i(3);
  for (int s = 0; s < ns; s++) {
    int kind = s == 0 ? 0 : (R.f() < 0.55f ? 0 : (R.f() < 0.5f ? 1 : 2)); // 0 open flower, 1 nodding bud, 2 pod
    float h = R.r(0.42f, 0.78f);
    float az = R.f() * TAU; V3 lean(cosf(az) * R.r(0.05f, 0.25f), 0, sinf(az) * R.r(0.05f, 0.25f));
    V3 top = lean * h + V3(0, h, 0);
    std::vector<V3> pts;
    if (kind == 1) { // hooked stem
      V3 hook = top + lean * 0.3f + V3(cosf(az) * 0.035f, -0.03f, sinf(az) * 0.035f);
      pts = curvePts(V3(0, 0, 0), lean * (h * 0.3f) + V3(0, h * 0.5f, 0), top + V3(0, 0.05f, 0), hook, 12);
      addTube(m, pts, [](float t) { return 0.0024f - 0.0008f * t; }, 5, MT.stemGreen, M34(), R.f(), false);
      M34 x = mAlignY(norm(pts[12] - pts[11]), hook);
      addBlob(m, V3(0, 0.011f, 0), V3(0.008f, 0.014f, 0.008f), 10, 6, 0.06f, 4, R.f() * 9, MT.poppyPod, x);
      continue;
    }
    pts = curvePts(V3(0, 0, 0), lean * (h * 0.2f) + V3(0, h * 0.4f, 0), lean * (h * 0.7f) + V3(0, h * 0.75f, 0), top, 10);
    addTube(m, pts, [](float t) { return 0.0026f - 0.0009f * t; }, 5, MT.stemGreen, M34(), R.f(), false);
    V3 up = norm(pts[10] - pts[9] + V3(R.r(-0.2f, 0.2f), 0.3f, R.r(-0.2f, 0.2f)) + V3(0.25f, 0, -0.1f));
    M34 x = mAlignY(up, top);
    // seed capsule
    addRevolve(m, {{0.001f, 0.0f}, {0.005f, 0.002f}, {0.0068f, 0.008f}, {0.006f, 0.014f}, {0.0085f, 0.0155f}, {0.0085f, 0.017f}, {0.001f, 0.019f}}, 12, 8, MT.poppyPod, x, [](float th, float t) { return 1.0f + (t > 0.7f ? 0.06f * sinf(th * 10) : 0.0f); });
    if (kind == 2) continue;
    for (int i = 0; i < 40; i++) { // stamens
      float a2 = R.f() * TAU;
      LeafP St; St.len = 0.011f; St.wid = 0.0012f; St.nu = 2; St.nv = 2; St.prof = profStrap(8, 1); St.pitch = R.r(0.6f, 1.2f); St.droop = 0; St.w = R.f();
      addLeaf(m, St, x * mRotY(a2) * mTrans(V3(0, 0.002f, 0.004f)), MT.discDark);
    }
    int np = 4 + R.i(3);
    float open = R.r(0.55f, 1.15f);
    for (int i = 0; i < np; i++) {
      LeafP L; L.len = R.r(0.045f, 0.058f); L.wid = L.len * R.r(1.15f, 1.4f); L.nu = 8; L.nv = 7; L.prof = profEll(0.5f, 0.42f, 0.25f);
      L.pitch = 1.25f - open * 0.65f + (i >= 4 ? 0.2f : 0.0f) + R.r(-0.08f, 0.08f); L.droop = open * R.r(0.5f, 0.95f); L.droopPow = 1.3f; L.fold = -0.18f; L.curl = -0.45f; L.wave = R.r(0.06f, 0.14f); L.waveN = R.r(1.5f, 3.0f); L.twist = R.r(-0.15f, 0.15f); L.w = R.f(); L.phase = R.f() * 6;
      addLeaf(m, L, x * mRotY(i * TAU / 4 + (i >= 4 ? 0.8f : 0) + R.r(-0.12f, 0.12f)) * mTrans(V3(0, 0.001f, 0.003f)), MT.petPoppy);
    }
  }
  return m;
}
static inline Mesh foxglove(Rng& R, V3* outTop = nullptr) {
  Mesh m;
  float h = R.r(1.0f, 1.45f);
  V3 lean(R.r(-0.08f, 0.08f), 0, R.r(-0.08f, 0.08f));
  auto pts = curvePts(V3(0, 0, 0), V3(0, h * 0.3f, 0), lean * h * 0.5f + V3(0, h * 0.7f, 0), lean * h + V3(0, h, 0), 20);
  addTube(m, pts, [](float t) { return 0.006f * (1.05f - 0.8f * t); }, 6, MT.stemGreen, M34(), R.f(), false);
  auto at = [&](float t) { float f = t * 20; int k = std::min((int)f, 19); return lerp(pts[k], pts[k + 1], f - k); };
  for (int i = 0; i < 9; i++) { // basal rosette
    LeafP L; L.len = R.r(0.16f, 0.26f); L.wid = L.len * R.r(0.36f, 0.46f); L.nu = 6; L.nv = 10; L.prof = profEll(0.85f, 0.6f); L.pitch = R.r(0.2f, 0.8f); L.droop = R.r(0.5f, 1.2f); L.fold = 0.25f; L.wave = 0.05f; L.waveN = 4; L.w = R.f();
    addLeaf(m, L, mRotY(i * 2.4f) * mTrans(V3(0, 0.01f, 0.012f)), MT.foxLeaf);
  }
  for (int i = 0; i < 8; i++) { // stem leaves
    float t = 0.08f + 0.36f * i / 8.0f;
    LeafP L; L.len = 0.15f * (1 - t * 1.4f) + 0.03f; L.wid = L.len * 0.36f; L.nu = 4; L.nv = 8; L.prof = profEll(0.9f, 0.6f); L.pitch = R.r(0.3f, 0.8f); L.droop = R.r(0.5f, 1.1f); L.fold = 0.3f; L.w = R.f();
    addLeaf(m, L, mTrans(at(t)) * mRotY(i * 2.4f + 1.0f) * mTrans(V3(0, 0, 0.005f)), MT.foxLeaf);
  }
  int nb = 30 + R.i(12); float faceAz = R.f() * TAU;
  for (int i = 0; i < nb; i++) {
    float t = 0.46f + 0.52f * i / (float)nb;
    float sz = (1.0f - 0.55f * powf((float)i / nb, 1.6f)) * R.r(0.9f, 1.1f);
    bool bud = i > nb * 0.78f;
    float az = faceAz + R.r(-1.3f, 1.3f);
    V3 out(cosf(az), 0, sinf(az));
    float down = bud ? R.r(-0.5f, 0.1f) : R.r(0.25f, 0.6f);
    V3 dir = norm(out * cosf(down) - V3(0, 1, 0) * sinf(down));
    V3 p = at(t) + out * 0.004f;
    M34 x = mAlignY(dir, p, V3(0, 1, 0));
    float l = 0.046f * sz;
    if (bud) addRevolve(m, {{0.001f, 0}, {0.004f * sz, l * 0.15f}, {0.006f * sz, l * 0.4f}, {0.0045f * sz, l * 0.62f}, {0.0008f, l * 0.75f}}, 7, 5, i > nb * 0.9f ? MT.stemGreen : MT.petFox, x, nullptr, R.f());
    else addRevolve(m, {{0.0015f, 0}, {0.004f * sz, l * 0.12f}, {0.0075f * sz, l * 0.4f}, {0.010f * sz, l * 0.75f}, {0.0115f * sz, l * 0.93f}, {0.0145f * sz, l * 1.0f}}, 10, 7, MT.petFox, x, [](float th, float tt) { return 1.0f + 0.10f * tt * sinf(th * 5 + 1) + 0.12f * tt * cosf(th); }, R.f());
    for (int k = 0; k < 3; k++) { // calyx
      LeafP L; L.len = 0.008f * sz; L.wid = 0.004f * sz; L.nu = 2; L.nv = 2; L.pitch = 1.0f; L.droop = 0.3f; L.w = R.f();
      addLeaf(m, L, x * mRotY(k * TAU / 3), MT.stemGreen);
    }
  }
  if (outTop) *outTop = at(0.75f);
  return m;
}
static inline Mesh allium(Rng& R, V3* outHead = nullptr) {
  Mesh m;
  float h = R.r(0.62f, 0.92f);
  V3 top(R.r(-0.05f, 0.05f), h, R.r(-0.05f, 0.05f));
  addStem(m, V3(0, 0, 0), top, 0.03f, 0.0042f, 0.003f, MT.stemGreen, R, nullptr, 6);
  for (int i = 0; i < 5; i++) { // strap leaves flopping
    LeafP L; L.len = R.r(0.3f, 0.5f); L.wid = R.r(0.03f, 0.045f); L.nu = 2; L.nv = 10; L.prof = profStrap(2.0f, 0.8f); L.pitch = R.r(0.9f, 1.3f); L.droop = R.r(1.6f, 2.6f); L.droopPow = 1.6f; L.fold = 0.35f; L.twist = R.r(-0.6f, 0.6f); L.w = R.f();
    addLeaf(m, L, mRotY(i * 1.3f + R.f()), MT.irisLeaf);
  }
  float Rr = R.r(0.04f, 0.055f);
  int nf = 130;
  for (int i = 0; i < nf; i++) {
    float u = 1 - 1.85f * (i + 0.5f) / nf, a = i * 2.39996f, sn = sqrtf(fmaxf(1 - u * u, 0.0f));
    V3 d = norm(V3(sn * cosf(a), u, sn * sinf(a)));
    float rl = Rr * R.r(0.85f, 1.05f);
    M34 sx = mLookZ(d, top + d * 0.003f);
    LeafP St; St.len = rl; St.wid = 0.0007f; St.nu = 2; St.nv = 1; St.prof = [](float) { return 1.0f; }; St.pitch = 0; St.droop = 0; St.fold = 0.6f; St.w = R.f();
    addLeaf(m, St, sx, MT.stemGreen);
    M34 fx = mAlignY(d, top + d * rl);
    for (int k = 0; k < 6; k++) {
      LeafP L; L.len = 0.0075f; L.wid = 0.0024f; L.nu = 2; L.nv = 2; L.prof = profEll(1, 0.7f); L.pitch = R.r(0.15f, 0.55f); L.droop = -0.2f; L.w = R.f();
      addLeaf(m, L, fx * mRotY(k * TAU / 6 + R.f() * 0.3f), MT.petAllium);
    }
  }
  if (outHead) *outHead = top;
  return m;
}
static inline Mesh lavender(Rng& R) {
  Mesh m;
  int ns = 70 + R.i(30);
  for (int s = 0; s < ns; s++) {
    float az = R.f() * TAU, sp = powf(R.f(), 0.7f) * 0.75f;
    V3 dir = norm(V3(cosf(az) * sp, 1, sinf(az) * sp));
    float l = R.r(0.26f, 0.44f);
    V3 a(cosf(az) * 0.03f * R.f(), 0.02f, sinf(az) * 0.03f * R.f()), b = a + dir * l;
    std::vector<V3> pts;
    addStem(m, a, b, R.r(-0.06f, 0.06f), 0.0011f, 0.0007f, MT.stemGreen, R, &pts, 3);
    for (int i = 0; i < 7; i++) {
      float t = 0.05f + 0.5f * (i + R.f()) / 7; int k = std::min((int)(t * 8), 7);
      LeafP L; L.len = R.r(0.022f, 0.04f); L.wid = 0.0032f; L.nu = 2; L.nv = 3; L.prof = profStrap(2, 0.9f); L.pitch = R.r(0.3f, 1.0f); L.droop = R.r(0, 0.5f); L.w = R.f();
      addLeaf(m, L, mTrans(lerp(pts[k], pts[k + 1], t * 8 - k)) * mRotY(R.f() * TAU), MT.lavLeaf);
    }
    float sl = R.r(0.03f, 0.055f);
    std::vector<V2> pr; int nw = 5 + R.i(3);
    pr.push_back({0.0008f, 0});
    for (int i = 0; i < nw; i++) { float y = sl * (i + 0.5f) / nw; pr.push_back({0.0042f * (1 - 0.4f * i / nw), y}); pr.push_back({0.0018f, y + sl * 0.4f / nw}); }
    pr.push_back({0.0005f, sl * 1.05f});
    addRevolve(m, pr, 6, nw * 3, MT.petLav, mAlignY(norm(pts[8] - pts[7]), b), [](float th, float) { return 1.0f + 0.25f * sinf(th * 3); }, R.f());
  }
  return m;
}
// tall daisy-type border flowers: kind 0 rudbeckia, 1 echinacea, 2 cosmos, 3 oxeye
static inline Mesh tallDaisy(Rng& R, int kind) {
  Mesh m;
  int ns = 4 + R.i(4);
  for (int s = 0; s < ns; s++) {
    float h = (kind == 2 ? R.r(0.6f, 0.95f) : R.r(0.4f, 0.75f));
    float az = R.f() * TAU, sp = R.r(0.05f, 0.3f);
    V3 top(cosf(az) * sp * h, h, sinf(az) * sp * h);
    std::vector<V3> pts;
    addStem(m, V3(cosf(az) * 0.01f, 0, sinf(az) * 0.01f), top, R.r(-0.06f, 0.06f), 0.0026f, 0.0016f, MT.stemGreen, R, &pts, 5);
    int nl = kind == 2 ? 10 : 6;
    for (int i = 0; i < nl; i++) {
      float t = 0.05f + 0.7f * (i + R.f()) / nl; int k = std::min((int)(t * 8), 7);
      LeafP L; L.w = R.f();
      if (kind == 2) { L.len = R.r(0.05f, 0.09f); L.wid = 0.004f; L.nu = 2; L.nv = 4; L.prof = profStrap(2, 0.9f); L.pitch = R.r(0.2f, 0.9f); L.droop = R.r(0.2f, 0.9f); }
      else { L.len = R.r(0.07f, 0.13f) * (1.2f - t); L.wid = L.len * 0.32f; L.nu = 4; L.nv = 6; L.prof = profEll(0.9f, 0.65f); L.pitch = R.r(0.2f, 0.8f); L.droop = R.r(0.4f, 1.0f); L.fold = 0.3f; }
      addLeaf(m, L, mTrans(lerp(pts[k], pts[k + 1], t * 8 - k)) * mRotY(R.f() * TAU), kind == 2 ? MT.fern : MT.foxLeaf);
    }
    V3 up = norm(pts[8] - pts[7] + V3(0.5f, 0.5f, -0.25f) * R.r(0.2f, 0.9f));
    M34 x = mAlignY(up, top);
    if (kind == 0) addDaisyHead(m, R, x, 13 + R.i(4), 0.036f, 0.011f, 0.010f, 0.009f, MT.petOrange, MT.discDark, R.r(0.1f, 0.5f));
    else if (kind == 1) addDaisyHead(m, R, x, 14 + R.i(5), 0.042f, 0.009f, 0.013f, 0.013f, MT.petPink, MT.discDark, R.r(0.6f, 1.1f));
    else if (kind == 2) addDaisyHead(m, R, x, 8, 0.034f, 0.024f, 0.006f, 0.003f, MT.petCosmos, MT.discYellow, R.r(-0.1f, 0.3f));
    else addDaisyHead(m, R, x, 20 + R.i(6), 0.024f, 0.006f, 0.009f, 0.004f, MT.petWhite, MT.discYellow, R.r(-0.05f, 0.3f));
  }
  return m;
}
// ---------------------------------------------------------------- pond plants
static inline Mesh lilyPadMesh(Rng& R, bool stem) {
  Mesh m;
  float notch = R.r(0.12f, 0.3f), cup = R.r(-0.02f, 0.05f), ph = R.f() * 6, wv = R.r(0.01f, 0.035f), lift = R.chance(0.2f) ? R.r(0.05f, 0.15f) : 0.0f;
  auto P = [&](float u, float v) {
    float th = notch + (TAU - 2 * notch) * u, r = v * (1.0f + 0.04f * sinf(th * 3 + ph) + 0.02f * sinf(th * 7));
    float y = cup * v * v + wv * sinf(th * 5 + ph) * v * v * v + lift * powf(v, 4) * fmaxf(sinf(th + ph), 0.0f);
    return V3(r * cosf(th), y + 0.004f, r * sinf(th));
  };
  addGrid(m, 30, 4, 0, 1, 0.02f, 1, P, MT.lilyPad, M34(), R.f(), true);
  if (stem) addTube(m, {V3(0, 0, 0), V3(R.r(-0.5f, 0.5f), -1.5f, R.r(-0.5f, 0.5f)), V3(R.r(-1.5f, 1.5f), -4.5f, R.r(-1.5f, 1.5f))}, [](float) { return 0.035f; }, 5, MT.stemPale, M34(), 0, false);
  return m;
}
static inline Mesh lilyFlower(Rng& R, bool bud) {
  Mesh m;
  const int nw[4] = {8, 8, 7, 6}; const float pit[4] = {0.22f, 0.62f, 0.98f, 1.28f}, ln[4] = {0.052f, 0.048f, 0.041f, 0.032f};
  for (int w = 0; w < 4; w++)
    for (int i = 0; i < nw[w]; i++) {
      LeafP L; L.len = ln[w] * R.r(0.92f, 1.08f); L.wid = L.len * 0.36f; L.nu = 4; L.nv = 6; L.prof = profEll(0.9f, 0.62f, 0.25f);
      L.pitch = bud ? 1.35f : pit[w] + R.r(-0.06f, 0.06f); L.droop = bud ? -0.15f : -0.25f; L.fold = -0.25f; L.curl = -0.3f; L.w = w * 0.25f + R.f() * 0.2f;
      addLeaf(m, L, mTrans(V3(0, 0.006f + 0.002f * w, 0)) * mRotY((i + 0.5f * w) * TAU / nw[w]) * mTrans(V3(0, 0, 0.006f)), MT.petLily);
    }
  if (!bud)
    for (int i = 0; i < 36; i++) {
      LeafP St; St.len = R.r(0.012f, 0.018f); St.wid = 0.0018f; St.nu = 2; St.nv = 2; St.prof = profStrap(8, 1); St.pitch = R.r(0.9f, 1.45f); St.droop = R.r(-0.3f, 0.3f); St.w = R.f();
      addLeaf(m, St, mTrans(V3(0, 0.012f, 0)) * mRotY(R.f() * TAU) * mTrans(V3(0, 0, R.r(0.001f, 0.006f))), MT.petYellow);
    }
  return m;
}
static inline Mesh reedClump(Rng& R, bool cattails) {
  Mesh m;
  int nl = 14 + R.i(8);
  for (int i = 0; i < nl; i++) {
    LeafP L; L.len = R.r(0.8f, 1.7f); L.wid = R.r(0.010f, 0.018f); L.nu = 2; L.nv = 12; L.prof = profStrap(2.5f, 0.9f);
    L.pitch = R.r(1.2f, 1.55f); L.droop = R.r(0.1f, 1.3f); L.droopPow = R.r(2.5f, 4.5f); L.fold = 0.3f; L.twist = R.r(-1.5f, 1.5f); L.side = R.r(-0.2f, 0.2f); L.w = R.f();
    addLeaf(m, L, mTrans(V3(R.r(-0.07f, 0.07f), -0.02f, R.r(-0.07f, 0.07f))) * mRotY(R.f() * TAU), MT.reed);
  }
  if (cattails)
    for (int i = 0; i < 2 + R.i(3); i++) {
      float h = R.r(1.1f, 1.6f);
      V3 b(R.r(-0.05f, 0.05f), 0, R.r(-0.05f, 0.05f)), t = b + V3(R.r(-0.08f, 0.08f), h, R.r(-0.08f, 0.08f));
      addStem(m, b, t, 0.02f, 0.004f, 0.003f, MT.stemGreen, R, nullptr, 5);
      V3 d = norm(t - b);
      addTube(m, {t, t + d * 0.02f, t + d * 0.14f, t + d * 0.16f}, [](float tt) { return tt < 0.06f || tt > 0.94f ? 0.004f : 0.0115f; }, 8, MT.cattail, M34(), R.f(), true);
      addTube(m, {t + d * 0.16f, t + d * 0.24f}, [](float tt) { return 0.0025f * (1 - tt * 0.8f); }, 4, MT.stemPale, M34(), 0, true);
    }
  return m;
}
static inline Mesh irisClump(Rng& R, bool flower) {
  Mesh m;
  int nf = 3 + R.i(3);
  for (int f = 0; f < nf; f++) {
    V3 b(R.r(-0.08f, 0.08f), 0, R.r(-0.08f, 0.08f)); float yaw = R.f() * TAU;
    int nl = 6 + R.i(3);
    for (int i = 0; i < nl; i++) {
      float s = (i - (nl - 1) * 0.5f) / (nl * 0.5f);
      LeafP L; L.len = R.r(0.45f, 0.75f) * (1 - 0.25f * fabsf(s)); L.wid = R.r(0.022f, 0.032f); L.nu = 2; L.nv = 9; L.prof = profStrap(2.2f, 0.95f);
      L.pitch = 1.45f; L.droop = R.r(0.05f, 0.5f); L.droopPow = 3; L.fold = 0.12f; L.twist = R.r(-0.3f, 0.3f); L.w = R.f();
      addLeaf(m, L, mTrans(b) * mRotY(yaw) * mRotZ(-s * 0.38f) * mRotY(PI / 2), MT.irisLeaf);
    }
    if (flower && f < 2) {
      float h = R.r(0.55f, 0.8f);
      V3 t = b + V3(R.r(-0.04f, 0.04f), h, R.r(-0.04f, 0.04f));
      addStem(m, b, t, 0.02f, 0.004f, 0.003f, MT.stemGreen, R, nullptr, 5);
      for (int k = 0; k < 3; k++) {
        LeafP F; F.len = 0.07f; F.wid = 0.042f; F.nu = 4; F.nv = 7; F.prof = profEll(1.3f, 0.6f, 0.2f); F.pitch = 0.5f; F.droop = 2.2f; F.droopPow = 1.2f; F.fold = 0.1f; F.wave = 0.06f; F.w = R.f();
        addLeaf(m, F, mTrans(t) * mRotY(k * TAU / 3 + yaw), MT.petIris);
        LeafP St; St.len = 0.06f; St.wid = 0.034f; St.nu = 4; St.nv = 6; St.prof = profEll(1.2f, 0.6f, 0.2f); St.pitch = 1.25f; St.droop = -0.5f; St.fold = -0.3f; St.curl = -0.3f; St.w = 0.9f;
        addLeaf(m, St, mTrans(t) * mRotY(k * TAU / 3 + yaw + PI / 3), MT.petIris);
      }
    }
  }
  return m;
}
// ---------------------------------------------------------------- trees: trunk + branches in one mesh, returns tip frames for foliage
struct TreeP { float h = 6, r = 0.12f; int levels = 3, kids = 4; float spread = 0.7f, shrink = 0.62f, droop = 0.0f, firstBranch = 0.4f; int bark = 0; float wob = 0.06f; };
struct Tip { V3 p, d; float s; };
static inline void treeBranch(Mesh& m, Rng& R, const TreeP& P, V3 p, V3 dir, float L, float r, int level, std::vector<Tip>& tips) {
  int seg = level == 0 ? 10 : 6;
  std::vector<V3> pts; pts.push_back(p);
  V3 d = dir;
  for (int i = 0; i < seg; i++) {
    d = norm(d + V3(R.r(-1, 1), R.r(-1, 1) + (level > 0 ? -P.droop * 0.4f + 0.12f : 0.15f), R.r(-1, 1)) * P.wob);
    p += d * (L / seg); pts.push_back(p);
  }
  float r1 = level == 0 ? r * 0.45f : r * 0.35f;
  addTube(m, pts, [=](float t) { return lerpf(r, r1, t) * (level == 0 ? 1.0f + 0.5f * powf(1 - t, 12.0f) : 1.0f); }, level == 0 ? 12 : (level == 1 ? 7 : 4), P.bark, M34(), R.f(), true);
  if (level >= P.levels) {
    for (int i = 2; i <= seg; i++) tips.push_back({pts[i], norm(pts[i] - pts[i - 1]), L});
    return;
  }
  int nk = P.kids + (level == 0 ? 2 : 0) + R.i(2);
  for (int k = 0; k < nk; k++) {
    float t = level == 0 ? P.firstBranch + (1 - P.firstBranch) * (k + R.f()) / nk : 0.3f + 0.7f * (k + R.f()) / nk;
    int i = std::min((int)(t * seg), seg - 1);
    V3 bp = lerp(pts[i], pts[i + 1], t * seg - i), bd = norm(pts[i + 1] - pts[i]);
    V3 side = norm(cross(bd, V3(R.r(-1, 1), R.r(-1, 1), R.r(-1, 1))));
    float sp = P.spread * R.r(0.7f, 1.3f);
    V3 nd = norm(bd * cosf(sp) + side * sinf(sp) + V3(0, level == 0 ? 0.25f : -P.droop * 0.3f, 0));
    treeBranch(m, R, P, bp, nd, L * P.shrink * R.r(0.8f, 1.15f) * (level == 0 ? (1.25f - 0.6f * t) : 1.0f), lerpf(r, r1, t) * 0.6f, level + 1, tips);
  }
  if (level > 0 || true) tips.push_back({pts[seg], norm(pts[seg] - pts[seg - 1]), L});
}
