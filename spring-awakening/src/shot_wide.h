// shot_wide.h — the clearing: one camera position seen in deep winter and again in full spring.
#pragma once
#include "plants.h"

namespace wid {
static int season = 0;       // 0 winter, 1 spring
static BlasH groundFar, groundNear, waterB, treeB[5], spruceB[3], tuft[3], flowerB[2], stalkB;
static int mGround, mBark, mTwig, mLeafT, mNeedle, mWater, mGrass, mPetal, mCentre, mStalk, mStem;
struct TreeI { M34 xf; int k; bool spruce; V3 tint; }; static std::vector<TreeI> trees;
struct Sc { M34 xf; int k; V3 tint; float rnd; }; static std::vector<Sc> tufts, flowers, stalks;

static float brookZ(float x) { return -15.0f + 4.5f * std::sin(x / 8.0f + 0.6f) + 0.42f * x + 1.2f * std::sin(x / 2.7f); }
static float baseH(float x, float z) { return 0.7f * fbm(V3(x / 20, 0.3f, z / 20), 3) + 0.016f * std::pow(std::max(0.f, -z - 40), 1.45f) * (1 + 0.3f * fbm(V3(x / 30, 2, z / 30), 2)); }
static float height(float x, float z) {
    float h = baseH(x, z) + 0.16f * fbm(V3(x / 3.5f, 1.3f, z / 3.5f), 3) + 0.03f * fbm(V3(x * 1.3f, 2.3f, z * 1.3f), 2);
    float d = std::fabs(z - brookZ(x));
    h -= 0.62f * smooth(2.4f, 0.5f, d);
    return h;
}
static float waterY(float x) { return baseH(x, brookZ(x)) - 0.36f; }

struct TreeP { float H, R; int levels; uint32_t seed; bool leaves; };
static void branch(Mesh& m, Mesh* leaf, Rng& r, V3 p, V3 d, float len, float rad, int depth, const TreeP& tp) {
    int nseg = depth == 0 ? 12 : (depth <= 2 ? 6 : 4);
    std::vector<V3> pts(nseg + 1); std::vector<float> rr(nseg + 1);
    V3 wander = r.dir();
    for (int i = 0; i <= nseg; i++) {
        float u = (float)i / nseg;
        pts[i] = p; rr[i] = rad * (1 - (depth == 0 ? 0.55f : 0.4f) * u);
        V3 w = r.dir();
        d = norm(d + w * (depth == 0 ? 0.035f : 0.10f + 0.03f * depth) + wander * 0.03f + V3(0, depth == 0 ? 0.02f : 0.07f, 0));
        p += d * (len / nseg);
    }
    if (depth >= 3) { for (int i = 0; i < nseg; i++) m.seg(pts[i], rr[i], pts[i + 1], rr[i + 1], 1, 0, 1, r.f()); }
    else addTube(m, pts, rr, depth == 0 ? 12 : (depth == 1 ? 7 : 5), 0, r.f(), len * 2);
    if (leaf && depth >= tp.levels - 1) {
        int nl = (int)(len / 0.07f) + 2;
        for (int i = 0; i < nl; i++) {
            float u = r.f(); int k = std::min(nseg - 1, (int)(u * nseg)); V3 c = lerp(pts[k], pts[k + 1], u * nseg - k) + r.dir() * 0.03f;
            float s = r.r(0.03f, 0.055f); V3 n = norm(V3(r.r(-0.6f, 0.6f), 1, r.r(-0.6f, 0.6f))); V3 a = norm(cross(n, r.dir())); V3 b = cross(n, a);
            V3 q0 = c, q1 = c + a * s + b * (s * 0.42f) + n * (s * 0.12f), q2 = c + a * (s * 2.0f), q3 = c + a * s - b * (s * 0.42f) + n * (s * 0.12f);
            float rn = r.f();
            leaf->tri(q0, q1, q2, n, n, n, V2(0, 0), V2(0.5f, 1), V2(1, 0), 0, 0, 0, 2, rn); leaf->tri(q0, q2, q3, n, n, n, V2(0, 0), V2(1, 0), V2(0.5f, -1), 0, 0, 0, 2, rn);
        }
    }
    if (depth >= tp.levels) return;
    int nc = depth == 0 ? 11 : (depth == 1 ? 6 : 4);
    float az = r.r(0, 6.28f);
    for (int c = 0; c < nc; c++) {
        float t = depth == 0 ? 0.30f + 0.68f * (c + r.f() * 0.6f) / nc : 0.25f + 0.75f * (c + r.f() * 0.6f) / nc;
        int k = std::min(nseg - 1, (int)(t * nseg)); V3 bp = lerp(pts[k], pts[k + 1], t * nseg - k); V3 bd = norm(pts[k + 1] - pts[k]);
        az += 2.4f + r.r(-0.4f, 0.4f);
        float ang = depth == 0 ? (1.15f - 0.55f * t) + r.r(-0.15f, 0.15f) : r.r(0.5f, 0.95f);
        V3 a = std::fabs(bd.y) < 0.95f ? norm(cross(bd, V3(0, 1, 0))) : V3(1, 0, 0); V3 b = cross(bd, a);
        V3 cd = norm(bd * std::cos(ang) + (a * std::cos(az) + b * std::sin(az)) * std::sin(ang));
        float cl = len * (depth == 0 ? (0.62f - 0.30f * t) : 0.62f * (1 - 0.35f * t)) * r.r(0.8f, 1.15f);
        float cr = rr[k] * (depth == 0 ? 0.42f : 0.55f);
        branch(m, leaf, r, bp, cd, cl, cr, depth + 1, tp);
    }
    if (depth > 0 && depth < tp.levels) branch(m, leaf, r, p, d, len * 0.6f, rr[nseg] * 0.9f, depth + 1, tp);
}
static void spruce(Mesh& m, Rng& r, float H) {
    int n = 14; std::vector<V3> tp(n); std::vector<float> tr(n);
    for (int i = 0; i < n; i++) { float u = i / (n - 1.f); tp[i] = V3(0.05f * std::sin(u * 3), u * H, 0); tr[i] = 0.16f * (H / 18) * (1 - u) + 0.01f; }
    addTube(m, tp, tr, 8, 0, 0, H);
    for (float h = 1.6f; h < H - 0.3f; h += 0.42f + 0.1f * r.f()) {
        float u = h / H; float L = (0.26f * H * std::pow(1 - u, 0.75f) + 0.25f) * r.r(0.8f, 1.1f);
        int nb = 5 + (int)(r.u() % 2); float az0 = r.r(0, 6.28f);
        for (int b = 0; b < nb; b++) {
            float az = az0 + b * 2 * PI / nb + r.r(-0.25f, 0.25f); V3 out(std::cos(az), 0, std::sin(az));
            int ns = 7; V3 prev = V3(0, h, 0); float droop = 0.25f + 0.5f * (1 - u);
            for (int s = 1; s <= ns; s++) {
                float t = s / (float)ns; V3 p = V3(0, h, 0) + out * (L * t) + V3(0, -droop * L * t * (1 - 0.75f * t * t), 0);
                float rad = 0.05f * (1 - 0.7f * t) * (0.5f + L / 3.5f);
                m.seg(prev, rad, p, rad * 0.8f, 1, 0, 1, r.f());
                // hanging side sprays (the needle masses)
                for (int q = 0; q < 7; q++) { V3 sd = norm(cross(out, V3(0, 1, 0)) * r.r(-1.2f, 1.2f) + out * r.r(0.0f, 0.7f) + V3(0, -0.35f - 0.5f * r.f(), 0)); float sl = L * 0.24f * (1 - 0.5f * t) * r.r(0.6f, 1.3f);
                    V3 b0 = lerp(prev, p, r.f()); V3 e = b0 + sd * sl; m.seg(b0, 0.10f * (0.4f + L / 4), e, 0.03f, 2, 0, 1, r.f());
                    V3 e2 = e + norm(sd + V3(0, -0.6f, 0) + r.dir() * 0.5f) * (sl * 0.6f); m.seg(e, 0.07f * (0.4f + L / 4), e2, 0.015f, 2, 0, 1, r.f()); }
                prev = p;
            }
        }
    }
    m.seg(V3(0, H - 0.4f, 0), 0.08f, V3(0, H + 0.5f, 0), 0.01f, 2, 0, 1, 0.5f);
}

static void initCommon(Scene& sc) {
    trees.clear(); tufts.clear(); flowers.clear(); stalks.clear();
    bool sp = season == 1;
    mGround = sc.addMat(sp ? [] { Mat m = mkMat(V3(0.10f, 0.17f, 0.045f), 0.7f, 0.3f, 16); set4(m.base2, V3(0.16f, 0.14f, 0.07f)); set4(m.p0, 0.6f, 1, 0, 0); return m; }()
                           : [] { Mat m = mkMat(V3(0.90f, 0.93f, 0.97f), 0.55f, 0.5f, 5); set4(m.base2, V3(0.6f, 0.62f, 0.66f)); m.trans = 0.0f; m.bump = 1; set4(m.p0, 60, 0.004f, 0.1f, 0); return m; }());
    // tree materials: 0 bark, 1 twig segs, 2 leaves / needles
    mBark = sc.addMat([sp] { Mat m = mkMat(V3(0.16f, 0.14f, 0.12f), 0.8f, 0.3f, 15); m.scale = 6; set4(m.p0, 0.35f, 0.8f, sp ? 0.0f : 0.85f, 5); return m; }());
    mTwig = sc.addMat([sp] { Mat m = mkMat(V3(0.12f, 0.10f, 0.09f), 0.8f, 0.3f, 15); m.scale = 6; set4(m.p0, 0.2f, 0.7f, sp ? 0.0f : 0.5f, 9); return m; }());
    mLeafT = sc.addMat([] { Mat m = mkMat(V3(0.22f, 0.40f, 0.06f), 0.4f, 0.5f, 0); set4(m.base2, V3(0.62f, 0.80f, 0.12f)); m.trans = 0.55f; return m; }());
    // spruce: bark, branch, needles
    int mSB = sc.addMat([sp] { Mat m = mkMat(V3(0.11f, 0.08f, 0.06f), 0.8f, 0.3f, 15); m.scale = 6; set4(m.p0, 0.4f, 0.8f, sp ? 0.0f : 0.5f, 5); return m; }());
    sc.addMat([sp] { Mat m = mkMat(V3(0.07f, 0.055f, 0.04f), 0.8f, 0.3f, 15); m.scale = 6; set4(m.p0, 0.1f, 0.6f, sp ? 0.0f : 0.9f, 3); return m; }());
    mNeedle = sc.addMat([sp] { Mat m = mkMat(sp ? V3(0.030f, 0.065f, 0.028f) : V3(0.022f, 0.045f, 0.03f), 0.7f, 0.4f, 15); m.scale = 9; set4(m.p0, -0.1f, 0.6f, sp ? 0.0f : 0.95f, 2.5f); return m; }());
    (void)mSB;
    mWater = sc.addMat([sp] { Mat m = mkMat(V3(0.012f, 0.016f, 0.02f), 0.04f, 4.0f, 14); set4(m.base2, V3(0.8f, 0.84f, 0.9f)); m.bump = 1; set4(m.p0, 2.2f, 0.05f, 0.5f, sp ? 0.0f : 0.5f); set4(m.p1, 1.0f, 0.1f, 0, 0); return m; }());
    mGrass = sc.addMat([] { Mat m = mkMat(V3(0.12f, 0.27f, 0.05f), 0.35f, 0.8f, 0); set4(m.base2, V3(0.45f, 0.66f, 0.10f)); m.trans = 0.45f; return m; }());
    mStalk = sc.addMat([] { Mat m = mkMat(V3(0.50f, 0.40f, 0.24f), 0.6f, 0.4f, 0); set4(m.base2, V3(0.6f, 0.5f, 0.3f)); m.trans = 0.3f; return m; }());
    mStem = sc.addMat([] { Mat m = mkMat(V3(0.14f, 0.28f, 0.06f), 0.4f, 0.6f, 17); set4(m.base2, V3(0.3f, 0.5f, 0.1f)); m.trans = 0.3f; return m; }());
    mPetal = sc.addMat([] { Mat m = mkMat(V3(0.88f, 0.88f, 0.84f), 0.5f, 0.3f, 0); set4(m.base2, V3(0.95f, 0.95f, 0.9f)); m.trans = 0.45f; return m; }());
    mCentre = sc.addMat([] { Mat m = mkMat(V3(0.85f, 0.62f, 0.05f), 0.7f, 0.2f, 17); return m; }());
    sc.addMat([] { Mat m = mkMat(V3(0.90f, 0.72f, 0.06f), 0.35f, 0.8f, 0); set4(m.base2, V3(1.0f, 0.85f, 0.1f)); m.trans = 0.4f; return m; }());   // yellow petals (mStem + 3)
    Mesh m;
    { int nx = 420, nz = 520; std::vector<V3> P(nx * nz); for (int i = 0; i < nx; i++) for (int j = 0; j < nz; j++) { float x = -70 + 140.f * i / (nx - 1), z = -150 + 162.f * j / (nz - 1); P[i * nz + j] = V3(x, height(x, z), z); }
      addGrid(m, nx, nz, P, nullptr, nullptr, 0, false, false, 0, true); groundFar = sc.addBlas(m); m.clear(); }
    { int nx = 400, nz = 400; std::vector<V3> P(nx * nz); for (int i = 0; i < nx; i++) for (int j = 0; j < nz; j++) { float x = -7 + 14.f * i / (nx - 1), z = -12 + 14.f * j / (nz - 1);
          float e = std::max(std::fabs(i / (nx - 1.f) * 2 - 1), std::fabs(j / (nz - 1.f) * 2 - 1)); P[i * nz + j] = V3(x, height(x, z) + 0.012f * fbm(V3(x * 6, 4, z * 6), 3) + 0.004f - 0.03f * smooth(0.9f, 1.0f, e), z); }
      addGrid(m, nx, nz, P, nullptr, nullptr, 0, false, false, 0, true); groundNear = sc.addBlas(m); m.clear(); }
    { int nx = 500, nw = 12; std::vector<V3> P(nx * nw); for (int i = 0; i < nx; i++) for (int j = 0; j < nw; j++) { float x = -70 + 140.f * i / (nx - 1); float z = brookZ(x) + (j / (nw - 1.f) * 2 - 1) * 2.6f; P[i * nw + j] = V3(x, waterY(x), z); }
      addGrid(m, nx, nw, P, nullptr, nullptr, 0, false, false, 0, true); waterB = sc.addBlas(m); m.clear(); }
    for (int k = 0; k < 5; k++) {
        Rng r(100 + k * 7 + season * 0); TreeP tp; tp.H = r.r(13, 19); tp.R = tp.H * 0.016f; tp.levels = 5; tp.seed = k; tp.leaves = sp;
        Mesh lf; branch(m, sp ? &lf : nullptr, r, V3(0, -0.3f, 0), norm(V3(r.r(-0.06f, 0.06f), 1, r.r(-0.06f, 0.06f))), tp.H * 0.62f, tp.R, 0, tp);
        if (sp) { for (size_t i = 0; i < lf.t.size(); i++) { m.t.push_back(lf.t[i]); m.a.push_back(lf.a[i]); } }
        treeB[k] = sc.addBlas(m); m.clear();
    }
    for (int k = 0; k < 3; k++) { Rng r(300 + k); spruce(m, r, 15.0f + 4.0f * k); spruceB[k] = sc.addBlas(m); m.clear(); }
    Rng r(2026);
    auto place = [&](float x, float z, bool spr, float sc2) {
        if (std::fabs(z - brookZ(x)) < 3.0f) return;
        TreeI t; t.spruce = spr; t.k = (int)(r.u() % (spr ? 3 : 5));
        float tv = r.r(0.8f, 1.2f); t.tint = V3(tv, tv * r.r(0.95f, 1.05f), tv * r.r(0.9f, 1.05f));
        t.xf = M34::translate(V3(x, height(x, z), z)) * M34::rotY(r.r(0, 6.28f)) * M34::scale(sc2);
        trees.push_back(t);
    };
    // forest edge beyond the clearing, a few trees standing forward, a dark conifer belt behind
    for (int i = 0; i < 46; i++) { float x = r.r(-60, 60), z = r.r(-74, -46); place(x, z, false, r.r(0.85f, 1.3f)); }
    for (int i = 0; i < 18; i++) { float x = (r.f() < 0.5f ? -1.f : 1.f) * r.r(17, 50), z = r.r(-46, -20); place(x, z, false, r.r(0.9f, 1.2f)); }
    place(-9.5f, -31.0f, false, 1.15f); place(13.5f, -38.0f, false, 1.25f); place(-21.0f, -24.0f, false, 1.0f);
    for (int i = 0; i < 18; i++) { float x = r.r(-60, 60), z = r.r(-80, -52); place(x, z, true, r.r(0.8f, 1.25f)); }
    for (int i = 0; i < 48; i++) { float x = r.r(-70, 70), z = r.r(-120, -80); place(x, z, r.f() < 0.7f, r.r(1.0f, 1.5f)); }
    // near field: grass tufts and flowers (spring), dry stalks (winter)
    const float TILE = 0.5f;
    for (int k = 0; k < 3; k++) {       // sod tiles: hundreds of blades per tile (k = 2 is the thin far version)
        Rng rg(40 + k); int nb = k == 2 ? 170 : 600;
        for (int i = 0; i < nb; i++) { Blade b; b.len = rg.r(0.035f, 0.13f) * (k == 2 ? 1.2f : 1.0f); b.width = rg.r(0.0016f, 0.003f) * (k == 2 ? 1.8f : 1.0f); b.maxAt = 0.3f; b.q = 0.3f; b.tipQ = 1.5f; b.baseW = 0.8f; b.fold = 0.5f; b.curl = rg.r(0.2f, 1.5f); b.curlPow = 2; b.twist = rg.r(-1, 1); b.nu = k == 2 ? 5 : 7; b.nv = 1;
            V3 o(rg.r(-TILE / 2, TILE / 2), 0, rg.r(-TILE / 2, TILE / 2)); float tv = rg.r(0.75f, 1.25f);
            addBlade(m, b, M34::translate(o) * M34::rotY(rg.r(0, 6.28f)) * M34::rotX(-rg.r(0.0f, 0.45f)), 0, tv, 0); }
        tuft[k] = sc.addBlas(m); m.clear();
    }
    for (int k = 0; k < 2; k++) {       // flower tiles: small six-tepalled woodland flowers, mostly white, some yellow
        Rng rg(60 + k); int nfl = k == 0 ? 20 : 8;
        for (int f = 0; f < nfl; f++) {
            V3 o(rg.r(-TILE / 2, TILE / 2), 0, rg.r(-TILE / 2, TILE / 2)); float H = rg.r(0.10f, 0.16f); V3 lean(rg.r(-0.02f, 0.02f), 0, rg.r(-0.02f, 0.02f)); float sz = rg.r(0.9f, 1.4f); bool yel = rg.f() < 0.22f;
            std::vector<V3> p = {o, o + V3(0, H * 0.5f, 0) + lean * 0.4f, o + V3(0, H, 0) + lean}; addTube(m, spline(p, 5), std::vector<float>{0.0012f, 0.0011f, 0.001f, 0.001f, 0.0009f}, 4, 0);
            for (int i = 0; i < 3; i++) { Blade b; b.len = 0.035f; b.width = 0.012f; b.maxAt = 0.5f; b.q = 0.5f; b.serr = 0.45f; b.serrN = 4; b.fold = 0.2f; b.curl = 0.5f; b.nu = 9; b.nv = 1; addBlade(m, b, M34::translate(o + V3(0, H * 0.62f, 0) + lean * 0.6f) * M34::rotY(i * 2.1f + f) * M34::rotX(-1.25f), 0, 0.3f * i, 0); }
            float az = rg.r(0, 6.28f);
            for (int i = 0; i < 6; i++) { Blade b; b.len = 0.019f * sz; b.width = 0.0065f * sz; b.maxAt = 0.55f; b.q = 0.6f; b.cup = 0.4f; b.curl = -0.25f; b.nu = 6; b.nv = 1; addBlade(m, b, M34::translate(o + V3(0, H, 0) + lean) * M34::rotY(az + i * (PI / 3)) * M34::rotX(-1.05f + 0.1f * (i & 1)), yel ? 3 : 1, 0.2f * i, 0); }
            addBlob(m, o + V3(0, H + 0.002f, 0) + lean, V3(0.003f * sz, 0.002f, 0.003f * sz), 5, 6, 2);
        }
        flowerB[k] = sc.addBlas(m); m.clear();
    }
    { Rng rg(80); for (int i = 0; i < 9; i++) { V3 b(rg.r(-0.06f, 0.06f), 0, rg.r(-0.06f, 0.06f)); V3 d = norm(V3(rg.r(-0.35f, 0.35f), 1, rg.r(-0.35f, 0.35f))); float L = rg.r(0.2f, 0.6f);
          std::vector<V3> c = {b, b + d * (L * 0.5f), b + d * L + V3(rg.r(-0.1f, 0.1f), -0.12f * L, rg.r(-0.1f, 0.1f))}; addTube(m, spline(c, 8), std::vector<float>{0.0016f, 0.0015f, 0.0014f, 0.0013f, 0.0012f, 0.001f, 0.0009f, 0.0007f}, 5, 0, rg.f());
          if (rg.f() < 0.6f) { V3 tip = c[2]; for (int q = 0; q < 14; q++) { V3 e = norm(d + rg.dir() * 0.8f); m.seg(tip - d * (0.004f * q), 0.0007f, tip - d * (0.004f * q) + e * 0.02f, 0.0003f, 0, 0, 1, rg.f()); } } }
      stalkB = sc.addBlas(m); m.clear(); }
    for (float z = -0.25f; z > -19.0f; z -= TILE) for (float x = -13.0f; x < 13.0f; x += TILE) {
        float d = std::sqrt(x * x + z * z); if (std::fabs(x) > 1.0f + 0.62f * d || d > 18.5f) continue;
        if (std::fabs(z - brookZ(x)) < 1.3f) continue;
        float hx = (height(x + 0.25f, z) - height(x - 0.25f, z)) / 0.5f, hz = (height(x, z + 0.25f) - height(x, z - 0.25f)) / 0.5f;
        V3 n = norm(V3(-hx, 1, -hz)); float tv = r.r(0.8f, 1.2f);
        M34 base = frameY(V3(x, height(x, z) + 0.008f, z), n, 1, 0) * M34::rotY((float)(r.u() % 4) * (PI / 2));
        tufts.push_back({base, d > 10.0f ? 2 : (int)(r.u() % 2), V3(tv * r.r(0.9f, 1.15f), tv, tv * r.r(0.7f, 1.0f)), r.f()});
        float fl = fbm(V3(x * 0.5f, 7, z * 0.5f), 3);
        if (fl > -0.06f && d < 15.0f) flowers.push_back({base * M34::rotY(1.57f), fl > 0.1f ? 0 : 1, V3(1), r.f()});
    }
    for (int i = 0; i < 260; i++) {
        float a = r.r(-0.5f, 0.5f), d = 0.9f + 10.0f * std::pow(r.f(), 1.6f); float x = std::sin(a) * d * 1.2f, z = -std::cos(a) * d;
        if (std::fabs(z - brookZ(x)) < 1.0f) continue;
        stalks.push_back({M34::translate(V3(x, height(x, z) - 0.02f, z)) * M34::rotY(r.r(0, 6.28f)) * M34::scale(r.r(0.7f, 1.3f)), 0, V3(1), r.f()});
    }
}
static void initWinter(Scene& sc) { season = 0; initCommon(sc); }
static void initSpring(Scene& sc) { season = 1; initCommon(sc); }

static void frame(Scene& sc, FrameCtx& fc) {
    float t = (float)fc.t; float tt = (float)fc.f / (fc.nf - 1);
    bool sp = season == 1;
    sc.addInst(groundFar, M34::identity(), mGround); sc.addInst(groundNear, M34::identity(), mGround);
    sc.addInst(waterB, M34::identity(), mWater);
    for (const TreeI& tr : trees) sc.addInst(tr.spruce ? spruceB[tr.k] : treeB[tr.k], tr.xf, tr.spruce ? mNeedle - 2 : mBark, tr.tint, 0.1f * tr.k);
    if (sp) {
        float wind = 0.04f * std::sin(t * 1.3f) + 0.02f * std::sin(t * 2.9f);
        // a light breeze: each tile leans by a shear that travels across the meadow
        auto shear = [&](const Sc& s, float k) { V3 p = s.xf.T(); float ph = t * 1.6f - p.x * 0.5f - p.z * 0.3f; float sx = k * (0.05f * std::sin(ph) + 0.025f * std::sin(ph * 2.3f + s.rnd * 6) + wind); M34 sh = M34::identity(); sh.m[1] = sx; sh.m[9] = sx * 0.4f; return s.xf * sh; };
        for (const Sc& s : tufts) sc.addInst(tuft[s.k], shear(s, 1.0f), mGrass, s.tint, s.rnd, nullptr, IF_NOFOGSHADOW);
        for (const Sc& s : flowers) sc.addInst(flowerB[s.k], shear(s, 1.4f), mStem, s.tint, s.rnd, nullptr, IF_NOFOGSHADOW);
    } else for (const Sc& s : stalks) sc.addInst(stalkB, s.xf * M34::rotX(0.02f * std::sin(t * 1.1f + s.rnd * 9)), mStalk, V3(1), s.rnd);
    Camera& c = fc.cam; Env& e = fc.env;
    float ce = ease(0, 1, tt);
    if (!sp) {
        c.pos = lerp(V3(-0.9f, 1.25f, 0.0f), V3(0.5f, 1.30f, 0.0f), ce); c.pos.y += height(c.pos.x, c.pos.z);
        c.target = V3(0.6f, 5.2f, -40.0f); c.focal = 0.036f; c.fstop = 8; c.focusDist = 30;
        e.sunDir = norm(V3(-0.25f, 0.03f + 0.02f * tt, -0.97f)); e.sunE = V3(1.6f, 0.9f, 0.5f) * (0.3f + 0.4f * tt); e.sunAngR = 0.02f;
        e.zen = V3(0.06f, 0.12f, 0.30f) * 1.6f; e.hor = V3(0.95f, 0.60f, 0.46f) * 1.25f; e.gnd = V3(0.2f, 0.24f, 0.32f); e.horExp = 0.38f;
        e.glow = 1.2f; e.glowExp = 14; e.fogDen = 0.0075f; e.fogFall = 0.13f; e.fogY0 = 0; e.fogG = 0.7f; e.fogAmb = V3(0.40f, 0.50f, 0.74f) * 2.4f; e.fogMax = 260; e.fogSun = 0; e.fogN = 1;
    } else {
        c.pos = lerp(V3(-0.5f, 0.30f, 0.0f), V3(0.5f, 0.62f, 0.0f), ce); c.pos.y += height(c.pos.x, c.pos.z);
        c.target = lerp(V3(0.0f, 0.9f, -9.0f), V3(0.6f, 5.2f, -40.0f), ease(0.15f, 0.9f, tt)); c.focal = 0.036f; c.fstop = 2.8f;
        c.focusDist = lerp(1.6f, 34.0f, std::pow(ease(0.3f, 0.85f, tt), 2.0f));
        e.sunDir = norm(V3(-0.34f, 0.30f, -0.90f)); e.sunE = V3(7.5f, 5.8f, 3.7f); e.sunAngR = 0.02f;
        e.zen = V3(0.16f, 0.30f, 0.62f) * 1.2f; e.hor = V3(0.95f, 0.80f, 0.55f) * 1.25f; e.gnd = V3(0.06f, 0.09f, 0.04f); e.horExp = 0.45f;
        e.glow = 0.5f; e.glowExp = 30; e.fogDen = 0.011f; e.fogFall = 0.10f; e.fogY0 = 0; e.fogG = 0.62f; e.fogAmb = V3(0.75f, 0.72f, 0.6f) * 1.4f; e.fogMax = 220; e.fogN = 2; e.fogSun = 0.5f; e.fogClamp = 3.0f;
        // sparkle on the brook
        Rng r(5 + fc.f / 2);
        for (int i = 0; i < 60; i++) { float x = r.r(-9, 9); V3 p(x, waterY(x) + 0.01f, brookZ(x) + r.r(-1.2f, 1.2f)); sc.addGlint(p, e.sunE * (3e-5f * r.f() * r.f()), 0.1f); }
    }
    e.eps = 4e-4f; e.clampL = 20; e.time = t; e.maxDiff = 2;
}
} // namespace wid
