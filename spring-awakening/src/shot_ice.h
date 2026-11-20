// shot_ice.h — icicles under a snow-capped branch; the first drops of meltwater (two shots share the scene).
#pragma once
#include "plants.h"

namespace ice {
struct Icicle { float x, z, len, R; uint32_t seed; float t0, period; };
static std::vector<Icicle> IC;
static std::vector<BlasH> inner;
static BlasH branchB, snowB, dropB, twigB;
static Moss moss;
static int mIce, mWater, mBark, mSnow, mCore, mBubble, mTwig;
static const float BR = 0.034f;
struct Spark { V3 p, n; float s; }; static std::vector<Spark> sparks;
struct MossPt { V3 p, n; float h; uint32_t seed; }; static std::vector<MossPt> mossPts;

static V3 branchC(float x) { return V3(x, 0.006f * std::sin(x * 9) + 0.01f * x, 0.01f * std::sin(x * 5 + 1)); }
static float branchR(float x) { return BR * (1 + 0.08f * fbm(V3(x * 12, 0.3f, 0), 2)) * (1 - 0.25f * (x + 0.3f)); }

static float bodyR(const Icicle& ic, float s) {
    float tipR = 0.0011f; float L = ic.len;
    if (s > L) return tipR * std::sqrt(std::max(0.f, 1 - (s - L) * (s - L) / (tipR * tipR)));
    float r = tipR * (0.8f + 0.2f * sat((L - s) / 0.01f)) + (ic.R - tipR) * std::pow(1 - s / L, 0.85f);
    r *= 1 + 0.22f * std::exp(-s / 0.004f);
    return r;
}
// drop state: bulb radius b and neck factor; `phi` in [0,1) growth, pinch in [1,1+pinch)
static void icicleMesh(Mesh& m, const Icicle& ic, float b, float pinch, V3& tipOut, float& scOut) {
    float L = ic.len;
    float s0 = L - 0.0006f; float tr0 = bodyR(ic, s0);
    float h = std::max(b, 0.0003f) * (1.25f + 1.9f * pinch);
    float sc = s0 + h;
    float send = sc + b;
    int n1 = 72, n2 = 56, n = n1 + n2; int sides = 28;
    std::vector<V3> p(n); std::vector<float> rr(n);
    V3 A = branchC(ic.x) + V3(0, -branchR(ic.x) * 0.9f, ic.z);
    for (int i = 0; i < n; i++) {
        float s = i < n1 ? (float)i / n1 * s0 : s0 + (send - s0) * (float)(i - n1) / (n2 - 1);
        float r;
        if (s < s0) {
            r = bodyR(ic, s);
            r *= 1 + (0.038f * std::sin(2 * PI * s / 0.0105f + 5.0f * fbm(V3(s * 45, ic.seed, 0), 2)) + 0.06f * fbm(V3(s * 90, ic.seed * 3.f, 1), 2)) * sat((L - s) / 0.012f);
        } else if (s < sc) {
            float x = (s - s0) / h;
            r = tr0 + (b - tr0) * 0.5f * (1 - std::cos(PI * std::pow(x, 0.85f)));
            r *= 1 - 0.985f * std::pow(pinch, 1.4f) * std::exp(-(x - 0.24f) * (x - 0.24f) / (0.17f * 0.17f));
        } else r = b * std::sqrt(std::max(0.f, 1 - (s - sc) * (s - sc) / (b * b)));
        rr[i] = i == n - 1 ? 0 : r;
        float sw = sat(s / L);
        p[i] = A + V3(0.0025f * std::sin(sw * 2.2f + ic.seed) * sw, -s, 0.002f * std::sin(sw * 1.7f + ic.seed * 2.f) * sw);
    }
    Frames f = ptFrames(p, V3(1, 0, 0));
    uint32_t sd = ic.seed;
    addTube(m, p, rr, sides, mIce, 0.1f * ic.seed, 1, &f, [sd](float u, float v) { return 1 + 0.05f * fbm(V3(u * 25, std::cos(v * 2 * PI) * 1.5f + sd, std::sin(v * 2 * PI) * 1.5f), 2) * (1 - u); });
    tipOut = p[n1 - 1]; tipOut.y = A.y - L; scOut = sc;
}

static void init(Scene& sc) {
    mIce = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 1.0f, 6); m.type = MT_DIEL; m.ior = 1.31f; m.scale = 420; m.bump = 1; set4(m.p0, 0.00010f, 0.05f, 0, 0); set4(m.p1, V3(3.0f, 1.2f, 0.5f)); set4(m.sh, V3(0.8f, 0.86f, 0.92f)); return m; }());
    mWater = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 1.0f, 0); m.type = MT_DIEL; m.ior = 1.33f; set4(m.sh, V3(0.85f, 0.9f, 0.95f)); return m; }());
    mBark = sc.addMat([] { Mat m = mkMat(V3(0.10f, 0.078f, 0.06f), 0.8f, 0.4f, 7); set4(m.base2, V3(0.028f, 0.022f, 0.018f)); m.scale = 90; m.bump = 1; set4(m.p0, 9, 14, 0.0012f, 0.55f); set4(m.p2, V3(0.30f, 0.36f, 0.27f)); return m; }());
    mSnow = sc.addMat([] { Mat m = mkMat(V3(0.93f, 0.95f, 0.97f), 0.55f, 0.6f, 5); set4(m.base2, V3(0.55f, 0.5f, 0.42f)); m.trans = 0.18f; m.bump = 1; set4(m.p0, 900, 0.00035f, 0.25f, 0); return m; }());
    mCore = sc.addMat([] { Mat m = mkMat(V3(0.85f, 0.9f, 0.95f), 0.6f, 0.0f, 0); set4(m.base2, V3(0.9f, 0.95f, 1.0f)); m.trans = 0.8f; set4(m.sh, V3(0.85f)); return m; }());
    mBubble = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 0.0f, 0); m.type = MT_DIEL; m.ior = 1.0f / 1.31f; set4(m.sh, V3(0.92f)); return m; }());
    mTwig = sc.addMat([] { Mat m = mkMat(V3(0.07f, 0.05f, 0.04f), 0.6f, 0.5f, 4); set4(m.base2, V3(0.11f, 0.08f, 0.06f)); set4(m.p0, 0.0f, 1.0f, 60, 0); set4(m.p2, V3(0.25f, 0.25f, 0.22f), 0.4f); return m; }());
    mossInit(sc, moss, V3(0.03f, 0.065f, 0.014f), V3(0.13f, 0.21f, 0.035f), 5);
    Mesh m; Rng r(42);
    // branch
    { int n = 90; std::vector<V3> p(n); std::vector<float> rr(n);
      for (int i = 0; i < n; i++) { float x = -0.4f + 0.8f * i / (n - 1.f); p[i] = branchC(x); rr[i] = branchR(x); }
      Frames f = ptFrames(p, V3(0, 1, 0));
      addTube(m, p, rr, 64, mBark, 0.3f, 8, &f, [](float u, float v) { return 1 + 0.06f * ridged(V3(std::cos(v * 2 * PI) * 2.5f, std::sin(v * 2 * PI) * 2.5f, u * 30), 3) - 0.04f; });
      branchB = sc.addBlas(m); m.clear(); }
    // snow cap: angle a from the top (+y), towards +z (camera side) positive
    { int nx = 300, na = 60; std::vector<V3> P(nx * na); std::vector<float> A(nx * na);
      for (int i = 0; i < nx; i++) for (int j = 0; j < na; j++) {
          float x = -0.4f + 0.8f * i / (nx - 1.f); float t = j / (na - 1.f) * 2 - 1; float amax = 1.25f + 0.25f * fbm(V3(x * 8, t > 0 ? 1.f : 5.f, 0), 2);
          float a = t * amax; V3 c = branchC(x); float R = branchR(x);
          float th = 0.024f * std::pow(std::max(0.f, std::cos(a * (PI / 2) / amax)), 0.55f) * (0.75f + 0.5f * fbm(V3(x * 7, 0.2f, 3), 3)) + 0.0025f * fbm(V3(x * 60, a * 3, 1), 3);
          float lip = 0.006f * smooth(0.55f, 1.0f, std::fabs(t)) * (0.5f + fbm(V3(x * 25, t > 0 ? 2.f : 8.f, 0), 2));
          float rad = R * 0.97f + th;
          P[i * na + j] = c + V3(0, std::cos(a) * rad - lip * 0.3f, std::sin(a) * (rad + lip));
          A[i * na + j] = 1 - std::fabs(t) < 0.12f ? 1.0f : 0.0f;
      }
      addGrid(m, nx, na, P, nullptr, &A, mSnow, false, false, 0, false);
      // sparkle candidates
      for (int k = 0; k < 1500; k++) { int i = 1 + (int)(r.f() * (nx - 2)), j = 1 + (int)(r.f() * (na - 2)); V3 p0 = P[i * na + j]; V3 nn = norm(cross(P[(i + 1) * na + j] - p0, P[i * na + j + 1] - p0)); if (nn.y < 0 && std::fabs(nn.z) < 0.3f) nn = -nn; sparks.push_back({p0 + nn * 0.0003f, norm(nn + r.dir() * 0.9f), r.f()}); }
      snowB = sc.addBlas(m); m.clear(); }
    // moss on the camera-facing flank and underside
    for (int k = 0; k < 30000; k++) {
        float x = r.r(-0.34f, 0.34f); float a = r.r(1.15f, 2.7f);
        float dn = fbm(V3(x * 9, a * 1.5f, 7), 3);
        if (dn < 0.04f + 0.1f * hash1(k)) continue;
        V3 c = branchC(x); float R = branchR(x); V3 n(0, std::cos(a), std::sin(a));
        mossPts.push_back({c + n * (R * 0.98f), n, 0.0022f + 0.0028f * sat(dn * 3), (uint32_t)k});
    }
    // icicles
    IC.clear();
    IC.push_back({0.020f, 0.004f, 0.118f, 0.0085f, 1, 0.0f, 3.2f});   // hero
    IC.push_back({-0.045f, 0.000f, 0.074f, 0.0065f, 2, 1.1f, 4.1f});
    IC.push_back({-0.098f, 0.006f, 0.135f, 0.0095f, 3, 2.0f, 3.6f});
    IC.push_back({0.074f, -0.003f, 0.052f, 0.0055f, 4, 0.6f, 5.0f});
    IC.push_back({0.118f, 0.005f, 0.096f, 0.0075f, 5, 2.6f, 3.0f});
    IC.push_back({-0.150f, -0.002f, 0.045f, 0.0050f, 6, 1.7f, 4.6f});
    IC.push_back({0.165f, 0.000f, 0.030f, 0.0042f, 7, 0.9f, 5.5f});
    IC.push_back({-0.010f, -0.008f, 0.026f, 0.0040f, 8, 2.2f, 6.0f});
    IC.push_back({-0.205f, 0.004f, 0.082f, 0.0070f, 9, 0.3f, 3.9f});
    IC.push_back({0.215f, 0.003f, 0.060f, 0.0060f, 10, 1.4f, 4.3f});
    IC.push_back({-0.072f, 0.007f, 0.020f, 0.0035f, 11, 3.0f, 7.0f});
    inner.clear();
    for (const Icicle& ic : IC) {
        Mesh im; Rng rr2(ic.seed * 91);
        V3 A = branchC(ic.x) + V3(0, -branchR(ic.x) * 0.9f, ic.z);
        auto axis = [&](float s) { float sw = sat(s / ic.len); return A + V3(0.0025f * std::sin(sw * 2.2f + ic.seed) * sw, -s, 0.002f * std::sin(sw * 1.7f + ic.seed * 2.f) * sw); };
        { int n = 40; std::vector<V3> p(n); std::vector<float> rad(n);
          for (int i = 0; i < n; i++) { float s = ic.len * (0.02f + 0.72f * i / (n - 1.f)); p[i] = axis(s) + V3(fbm(V3(s * 80, ic.seed, 0), 2), 0, fbm(V3(s * 80, ic.seed, 5), 2)) * bodyR(ic, s) * 0.2f; rad[i] = bodyR(ic, s) * (0.10f + 0.10f * fbm(V3(s * 120, ic.seed, 2), 2)) * (1 - std::pow(i / (n - 1.f), 3.0f)); }
          addTube(im, p, rad, 8, mCore, 0); }
        int nb = (int)(ic.len * 1400);
        for (int k = 0; k < nb; k++) {
            float s = ic.len * rr2.r(0.02f, 0.9f); float br = bodyR(ic, s); float rad = rr2.r(0.00012f, 0.00045f) * (0.6f + br / 0.006f);
            float a = rr2.r(0, 6.28f), d = br * 0.6f * std::sqrt(rr2.f());
            V3 c = axis(s) + V3(std::cos(a) * d, 0, std::sin(a) * d);
            addBlob(im, c, V3(rad, rad * rr2.r(1.0f, 3.5f), rad), 7, 10, mBubble, 0);
        }
        inner.push_back(sc.addBlas(im));
    }
    { addBlob(m, V3(0), V3(1), 24, 40, mWater); dropB = sc.addBlas(m); m.clear(); }
    // a few thin twigs for depth
    for (int k = 0; k < 6; k++) {
        V3 b(r.r(-0.3f, 0.3f), r.r(-0.3f, 0.12f), k < 3 ? r.r(0.25f, 0.45f) : r.r(-0.7f, -0.3f)); V3 d = norm(V3(r.r(-1, 1), r.r(-0.3f, 0.6f), r.r(-0.2f, 0.2f)));
        std::vector<V3> c = {b, b + d * 0.12f + r.dir() * 0.02f, b + d * 0.25f + r.dir() * 0.04f, b + d * 0.36f + r.dir() * 0.05f};
        std::vector<V3> p = spline(c, 24); std::vector<float> rr3(24); for (int i = 0; i < 24; i++) rr3[i] = 0.0035f * (1 - 0.7f * i / 23.f);
        addTube(m, p, rr3, 8, mTwig, r.f(), 4);
    }
    twigB = sc.addBlas(m); m.clear();
}

// mode 0: wide shot (real time), mode 1: tip close-up (slow motion)
static void frameCommon(Scene& sc, FrameCtx& fc, int mode) {
    float t = (float)fc.t; float tt = (float)fc.f / (fc.nf - 1);
    Mesh m;
    sc.addInst(branchB, M34::identity(), 0);
    sc.addInst(snowB, M34::identity(), 0);
    sc.addInst(twigB, M34::identity(), 0);
    { Rng r(9); for (const MossPt& mp : mossPts) { Rng r2(mp.seed * 7 + 1); mossAdd(sc, moss, mp.p, mp.n, mp.h, r2, V3(0.9f, 1.0f, 1.0f)); } }
    V3 heroTip; float heroB = 0, heroSc = 0; V3 heroDrop(0, -100, 0); float heroDropR = 0;
    for (size_t i = 0; i < IC.size(); i++) {
        const Icicle& ic = IC[i];
        float bmax = 0.0021f + 0.0004f * hash1(ic.seed * 77);
        float grow, pinch = 0, fallT = -1; // fallT: seconds of (slowed) fall since detachment
        float gscale = 1;
        if (mode == 0) {
            float ph = (t + 10 - ic.t0) / ic.period; float cyc = ph - std::floor(ph); float tin = cyc * ic.period;
            float Tp = 0.09f; float tg = ic.period - Tp;
            if (tin < tg) grow = tin / tg; else { grow = 1; pinch = (tin - tg) / Tp; }
            // previous drop still falling?
            if (tin < 0.5f) fallT = tin;
        } else {
            // hero timeline for the close-up: grow until 1.55 s, pinch 0.55 s, then fall slowed 22x
            float Tg = 2.0f, Tp = 0.6f; gscale = 1.0f / 30.0f;
            float tl = t + (i == 0 ? 0.0f : -100.0f);
            if (i != 0) { grow = 0.5f; }
            else if (tl < Tg) grow = 0.5f + 0.5f * tl / Tg;
            else if (tl < Tg + Tp) { grow = 1; pinch = (tl - Tg) / Tp; }
            else { fallT = tl - Tg - Tp; grow = std::min(1.0f, 0.02f + 0.06f * fallT); }
        }
        float b = bmax * std::pow(0.08f + 0.92f * grow, 1.0f / 2.2f); if (fallT >= 0 && mode == 0) b = bmax * 0.3f;
        if (fallT >= 0 && mode == 1 && i == 0) b = bmax * (0.30f + 0.05f * fallT + 0.07f * std::exp(-fallT * 3) * std::cos(fallT * 26));
        V3 tip; float scc;
        Mesh im; icicleMesh(im, ic, b, sat(pinch), tip, scc);
        sc.addInst(sc.addBlas(im), M34::identity(), 0);
        sc.addInst(inner[i], M34::identity(), 0);
        V3 A = branchC(ic.x) + V3(0, -branchR(ic.x) * 0.9f, ic.z);
        if (i == 0) { heroTip = V3(A.x, A.y - ic.len, A.z); heroB = b; heroSc = scc; }
        if (fallT >= 0) {
            float tr = fallT * gscale; // real seconds
            float y0 = A.y - (ic.len - 0.0006f + bmax * 3.15f);
            float sh = 1.0f / 48.0f * gscale * 0.5f; // shutter (real s): 180 deg
            auto pos = [&](float trr) { return V3(A.x + 0.0025f * std::sin(1.0f * 2.2f + ic.seed), y0 - (mode ? 0.02f : 0.2f) * trr - 0.5f * 9.81f * trr * trr, A.z + 0.002f * std::sin(1.7f + ic.seed * 2.f)); };
            float os = 0.16f * std::exp(-fallT * (mode ? 1.6f : 30.f)) * std::cos(fallT * (mode ? 21.f : 400.f));
            V3 s0(bmax * (1 - os * 0.5f), bmax * (1 + os), bmax * (1 - os * 0.5f));
            M34 x0 = M34::translate(pos(tr)) * M34::scale(s0 * 0.96f), x1 = M34::translate(pos(tr + sh)) * M34::scale(s0 * 0.96f);
            if (pos(tr).y > -1.0f) sc.addInst(dropB, x0, 0, V3(1), 0, &x1);
            if (i == 0) { heroDrop = pos(tr); heroDropR = bmax * 0.96f; }
        }
    }
    // ---- camera ----
    Camera& c = fc.cam;
    if (mode == 0) {
        float e = ease(0, 1, tt);
        c.pos = lerp(V3(-0.045f, -0.085f, 1.04f), V3(0.035f, -0.10f, 1.0f), e);
        c.target = lerp(V3(-0.012f, -0.052f, 0), V3(0.012f, -0.064f, 0), e);
        c.focal = 0.092f; c.fstop = 3.2f;
        float fz = lerp(-0.006f, 0.004f, ease(0.25f, 0.7f, tt)); // focus pull from the back icicle to the hero
        c.focusDist = len(V3(c.target.x, c.target.y, fz) - c.pos);
    } else {
        c.pos = V3(heroTip.x - 0.012f, heroTip.y - 0.004f + 0.002f * tt, 0.215f);
        c.target = heroTip + V3(0.0f, -0.0058f, 0);
        c.focal = 0.2f; c.fstop = 9.0f; c.focusDist = len(heroTip + V3(0, -0.004f, 0.0f) - c.pos);
    }
    // ---- light ----
    Env& e = fc.env;
    e.sunDir = norm(V3(-0.52f, 0.25f, -0.82f));
    float sunIn = mode == 0 ? ease(0.12f, 0.62f, tt) : 1.0f;
    e.sunE = V3(6.4f, 4.6f, 2.9f) * (0.02f + 0.98f * sunIn); e.sunAngR = 0.02f;
    e.zen = V3(0.10f, 0.19f, 0.42f) * 1.3f; e.hor = lerp(V3(0.30f, 0.38f, 0.52f), V3(0.75f, 0.60f, 0.46f), sunIn) * (mode ? 0.8f : 1.2f); e.gnd = V3(0.012f, 0.016f, 0.022f); e.horExp = 0.6f;
    e.glow = 0.14f * sunIn; e.glowExp = 24;
    e.type = 2; e.bd0 = V3(0.045f, 0.075f, 0.15f) * (mode ? 0.6f : 1.0f); e.bd1 = lerp(V3(0.10f, 0.15f, 0.27f), V3(0.42f, 0.38f, 0.36f), sunIn) * (mode ? 0.6f : 1.0f); e.bdScale = 5; e.bdMix = 1;
    e.bdP0 = -0.06f; e.bdP1 = 0.75f; e.bdP2 = 30; e.bdP3 = 0.97f;
    if (mode == 1) { e.bdScale = 1.5f; e.bdP2 = 11; e.bdGap = 0.22f; }   // the drop shows the whole surroundings in miniature: keep them large and simple
    e.eps = 4e-6f; e.clampL = 25; e.bdGap = mode ? 0.22f : 0.12f; e.sunDisc = 6.0f * sunIn;
    // snow sparkles
    V3 fw = norm(c.target - c.pos);
    for (const Spark& s : sparks) {
        V3 v = norm(c.pos - s.p); V3 hv = norm(v + e.sunDir); float al = dot(hv, s.n);
        float k = std::pow(std::max(al, 0.f), 900.0f) * sunIn; if (k < 0.02f) continue;
        sc.addGlint(s.p, e.sunE * (2.5e-8f * k * (0.3f + s.s)), 0.15f);
    }
    // sun glints on hanging / falling hero drop
    auto dropGlint = [&](V3 cc, float r) {
        V3 v = norm(c.pos - cc); V3 n = norm(v + e.sunDir); float F = 0.02f + 0.98f * std::pow(1 - std::max(dot(v, n), 0.f), 5.0f);
        sc.addGlint(cc + n * r, e.sunE * (F * r * r * 0.25f), 0.1f);
    };
    if (mode == 1) { if (heroB > 0.0003f) dropGlint(heroTip + V3(0, -(heroSc - IC[0].len), 0), heroB); if (heroDropR > 0) dropGlint(heroDrop, heroDropR); }
    // far background bokeh: glittering snow / wet twigs
    Rng r(71);
    for (int i = 0; i < 16; i++) {
        V3 p(r.r(-0.5f, 0.5f), r.r(-0.45f, 0.25f), r.r(-3.5f, -1.2f));
        float br = r.f(); br = 0.05f + br * br * br;
        float tw = 0.6f + 0.4f * std::sin(t * (mode ? 0.05f : 1.0f) * r.r(1, 4) + r.r(0, 6));
        V3 col = lerp(V3(0.75f, 0.85f, 1.0f), V3(1.0f, 0.78f, 0.5f), (mode ? 0.4f + 0.6f * r.f() : r.f()) * sunIn) * (br * tw * 1.2e-3f * (0.25f + 0.75f * sunIn));
        float dz = (p.z - c.pos.z); sc.addGlint(p, col * (dz * dz) * (mode ? 1.1f : 1.0f), r.r(0.05f, 0.2f));
    }
    (void)fw;
}
static void frameWide(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 0); }
static void frameTip(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 1); }
} // namespace ice
