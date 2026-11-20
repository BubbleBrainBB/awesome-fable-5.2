// shot_crocus.h — melting snow over the forest floor; crocuses push through and open (two shots share the set).
#pragma once
#include "plants.h"

namespace cro {
struct Crocus { V3 base; float t0, t1, size, az; uint32_t seed; int detail; float hue; };
static Soil soil; static Litter litter; static Moss moss;
static std::vector<Crocus> C;
static int mSnow, mTepal, mTube, mAnther, mStyle, mLeaf, mSheath, mTwig, mStone;
static BlasH decor;
struct MossPt { V3 p, n; float h; uint32_t seed; }; static std::vector<MossPt> mossPts;
struct Spark { float x, z; V3 n; float s; }; static std::vector<Spark> sparks;
struct Stone { V3 c; float r; }; static std::vector<Stone> stones;

static float ground(float x, float z) { return 0.012f * fbm(V3(x * 5, 0.3f, z * 5), 3) + 0.0025f * fbm(V3(x * 40, 1.3f, z * 40), 3); }
// snow thickness; melt in [0,1]
static float snowTh(float x, float z, float melt) {
    float base = 0.034f * (0.45f + 0.75f * (fbm(V3(x * 3.2f, 5.0f, z * 3.2f), 3) + 0.5f)) + 0.006f * fbm(V3(x * 15, 2, z * 15), 2);
    float m = melt * 0.062f * (0.75f + 0.6f * (fbm(V3(x * 7, 9, z * 7), 3) + 0.5f));
    float th = base - m;
    // melt wells around warm dark things (stems, stones)
    for (const Crocus& c : C) { float d2 = (x - c.base.x) * (x - c.base.x) + (z - c.base.z) * (z - c.base.z); float w = 0.009f + 0.026f * melt; th -= 0.035f * (0.3f + melt) * std::exp(-d2 / (w * w)); }
    for (const Stone& s : stones) { float d = std::sqrt((x - s.c.x) * (x - s.c.x) + (z - s.c.z) * (z - s.c.z)); th -= 0.05f * (0.3f + melt) * smooth(s.r * (1.2f + 0.8f * melt), s.r * 0.6f, d); }
    return th;
}

static void init(Scene& sc) {
    soil = Soil(); soil.height = ground;
    soilInit(sc, soil, 1.2f, -2.2f, 0.6f, 300, V3(0.11f, 0.07f, 0.045f), V3(0.04f, 0.026f, 0.018f));
    sc.mats[soil.matBase].p0[3] = 0.5f; sc.mats[soil.matBase + 1].p0[3] = 0.45f;
    soilScatter(soil, 80000, -0.5f, 0.5f, -0.8f, 0.45f, 0.0005f, 0.0045f, 21, [](float x, float z) { return 0.15f + 0.85f * smooth(0.5f, 0.1f, std::sqrt(x * x + z * z)); }, 2.6f);
    litterInit(sc, litter, 5);
    litterScatter(litter, 2600, -1.0f, 1.0f, -1.9f, 0.5f, ground, 33);
    mossInit(sc, moss, V3(0.035f, 0.085f, 0.015f), V3(0.20f, 0.33f, 0.045f), 9);
    mSnow = sc.addMat([] { Mat m = mkMat(V3(0.93f, 0.95f, 0.97f), 0.55f, 0.7f, 5); set4(m.base2, V3(0.42f, 0.40f, 0.38f)); m.trans = 0.12f; m.bump = 1; set4(m.p0, 650, 0.0005f, 0.22f, 0); return m; }());
    mTepal = sc.addMat([] { Mat m = mkMat(V3(0.33f, 0.17f, 0.56f), 0.38f, 0.5f, 3); set4(m.base2, V3(0.60f, 0.30f, 0.86f)); m.trans = 0.55f; set4(m.sh, V3(0.25f, 0.1f, 0.35f)); set4(m.p0, 11, 0.5f, 0.22f, 0.42f); set4(m.p1, 0.12f, 0.45f, 0, 0); set4(m.p2, V3(0.85f, 0.80f, 0.92f), 0.02f); return m; }());
    mTube = sc.addMat([] { Mat m = mkMat(V3(0.86f, 0.85f, 0.80f), 0.4f, 0.6f, 4); set4(m.base2, V3(0.66f, 0.56f, 0.80f)); m.trans = 0.4f; set4(m.p0, 0.6f, 1.0f, 30, 0); set4(m.p2, V3(0.5f), 0.0f); return m; }());
    mAnther = sc.addMat([] { Mat m = mkMat(V3(0.95f, 0.58f, 0.04f), 0.8f, 0.1f, 1); set4(m.base2, V3(0.85f, 0.42f, 0.02f)); m.scale = 1500; m.bump = 1; set4(m.p0, 0.00008f, 0.00002f, 0, 0); return m; }());
    mStyle = sc.addMat([] { Mat m = mkMat(V3(0.95f, 0.30f, 0.03f), 0.45f, 0.5f, 0); set4(m.base2, V3(1.0f, 0.45f, 0.05f)); m.trans = 0.35f; return m; }());
    mLeaf = sc.addMat([] { Mat m = mkMat(V3(0.05f, 0.15f, 0.04f), 0.28f, 1.0f, 2); set4(m.base2, V3(0.20f, 0.40f, 0.06f)); m.trans = 0.3f; set4(m.p0, 0, 0, 1.6f, 0); set4(m.p1, 0.0f, 0.3f, 0.15f, 0); set4(m.p2, V3(0.6f, 0.6f, 0.4f), 0.0f); return m; }());
    mSheath = sc.addMat([] { Mat m = mkMat(V3(0.80f, 0.76f, 0.66f), 0.5f, 0.4f, 4); set4(m.base2, V3(0.86f, 0.84f, 0.76f)); m.trans = 0.5f; set4(m.p0, 0.0f, 1.0f, 50, 0); set4(m.p2, V3(0.5f, 0.4f, 0.3f), 0.3f); return m; }());
    mTwig = sc.addMat([] { Mat m = mkMat(V3(0.09f, 0.065f, 0.045f), 0.6f, 0.6f, 4); set4(m.base2, V3(0.14f, 0.10f, 0.07f)); set4(m.p0, 0.0f, 1.0f, 60, 0); set4(m.p2, V3(0.28f, 0.3f, 0.22f), 0.5f); return m; }());
    mStone = sc.addMat([] { Mat m = mkMat(V3(0.30f, 0.29f, 0.27f), 0.6f, 0.7f, 1); set4(m.base2, V3(0.13f, 0.13f, 0.125f)); m.scale = 45; m.bump = 1; set4(m.p0, 0.0015f, 0.0003f, 0, 0.5f); return m; }());
    C.clear();
    C.push_back({V3(0.0f, 0, 0.0f), 0.02f, 0.92f, 1.0f, 0.3f, 1, 2, 0.0f});
    C.push_back({V3(0.047f, 0, -0.035f), 0.10f, 1.02f, 0.9f, 1.4f, 2, 2, 0.5f});
    C.push_back({V3(-0.062f, 0, -0.055f), -0.06f, 0.86f, 1.05f, 2.2f, 3, 1, 0.2f});
    C.push_back({V3(-0.025f, 0, 0.065f), 0.25f, 1.25f, 0.85f, 4.0f, 4, 1, 0.8f});
    C.push_back({V3(0.13f, 0, -0.12f), 0.0f, 0.95f, 1.0f, 5.1f, 5, 1, 0.3f});
    C.push_back({V3(-0.17f, 0, -0.16f), 0.08f, 1.0f, 1.0f, 0.9f, 6, 0, 0.6f});
    C.push_back({V3(0.03f, 0, -0.22f), -0.05f, 0.9f, 1.1f, 3.0f, 7, 0, 0.1f});
    C.push_back({V3(0.24f, 0, -0.3f), 0.0f, 0.9f, 1.0f, 2.0f, 8, 0, 0.9f});
    C.push_back({V3(-0.3f, 0, -0.38f), 0.05f, 0.95f, 1.0f, 1.0f, 9, 0, 0.4f});
    C.push_back({V3(-0.11f, 0, -0.42f), 0.02f, 0.9f, 1.0f, 4.4f, 10, 0, 0.7f});
    C.push_back({V3(0.16f, 0, -0.5f), 0.0f, 0.88f, 1.0f, 5.6f, 11, 0, 0.2f});
    for (auto& c : C) c.base.y = ground(c.base.x, c.base.z);
    Rng r(52); Mesh m;
    stones.clear();
    stones.push_back({V3(-0.14f, 0, 0.02f), 0.05f}); stones.push_back({V3(0.2f, 0, -0.2f), 0.07f}); stones.push_back({V3(-0.33f, 0, -0.55f), 0.11f}); stones.push_back({V3(0.42f, 0, -0.75f), 0.14f});
    for (auto& s : stones) { s.c.y = ground(s.c.x, s.c.z) + s.r * 0.15f; float sd = s.r * 100;
        addBlob(m, s.c, V3(s.r, s.r * 0.6f, s.r * 0.85f), 40, 60, mStone, 0, [sd](V3 d) { return 0.9f + 0.25f * fbm(d * 1.3f + V3(sd), 3) + 0.05f * fbm(d * 6 + V3(sd), 2); });
        for (int k = 0; k < (int)(s.r * s.r * 1.6e6f); k++) { V3 d = r.dir(); if (d.y < 0.05f) continue; float dn = fbm(d * 2.5f + V3(sd * 2), 2); if (dn < 0.0f) continue;
            float kk = 0.9f + 0.25f * fbm(d * 1.3f + V3(sd), 3) + 0.05f * fbm(d * 6 + V3(sd), 2);
            mossPts.push_back({s.c + d * V3(s.r, s.r * 0.6f, s.r * 0.85f) * kk, d, 0.004f + 0.004f * dn, (uint32_t)mossPts.size()}); } }
    for (int k = 0; k < 40; k++) { // twigs lying around
        V3 b(r.r(-0.6f, 0.6f), 0, r.r(-1.2f, 0.4f)); float L = r.r(0.06f, 0.28f); float az = r.r(0, 6.28f); V3 d(std::cos(az), 0, std::sin(az));
        std::vector<V3> c(5); for (int i = 0; i < 5; i++) { V3 q = b + d * (L * i / 4.f) + V3(-d.z, 0, d.x) * (0.08f * L * std::sin(i * 1.7f + k)); q.y = ground(q.x, q.z) + 0.006f + 0.006f * hash1(k * 7 + i); c[i] = q; }
        std::vector<V3> p = spline(c, 24); std::vector<float> rr(24); float r0 = r.r(0.0012f, 0.004f); for (int i = 0; i < 24; i++) rr[i] = r0 * (1 - 0.5f * i / 23.f);
        addTube(m, p, rr, 8, mTwig, r.f(), 5);
    }
    decor = sc.addBlas(m);
    sparks.clear();
    for (int k = 0; k < 9000; k++) { float x = r.r(-0.45f, 0.45f), z = r.r(-0.9f, 0.3f); sparks.push_back({x, z, norm(V3(0, 1, 0) + r.dir() * 0.8f), r.f()}); }
}

// one crocus at growth g (0..1); light = 0..1 how open the weather lets it be
static void crocus(Mesh& m, const Crocus& c, float g, float light, int frame) {
    float sz = c.size;
    float rise = ease(0.0f, 0.62f, g);
    float H = (0.008f + 0.034f * rise) * sz;                 // height of the tepal base above ground
    float open = ease(0.56f, 0.97f, g) * (0.55f + 0.45f * light);
    float tl = (0.018f + 0.016f * ease(0.1f, 0.7f, g)) * sz; // tepal length
    V3 jit(hash1(frame * 131 + c.seed) - 0.5f, 0, hash1(frame * 171 + c.seed) - 0.5f);
    float cph = g * 2 * PI * 3.1f + c.seed;
    V3 lean = norm(V3(0.10f * std::cos(c.az) + 0.035f * std::cos(cph) + jit.x * 0.012f, 1, 0.10f * std::sin(c.az) + 0.035f * std::sin(cph) + jit.z * 0.012f));
    int nt = c.detail >= 1 ? 28 : 14;
    // perianth tube
    std::vector<V3> p(nt); std::vector<float> rr(nt);
    for (int i = 0; i < nt; i++) { float u = i / (nt - 1.f); V3 d = norm(V3(0, 1, 0) * (1 - u * 0.6f) + lean * (u * 0.6f)); p[i] = c.base + V3(0, -0.01f, 0) + d * ((H + 0.01f) * u) + V3(lean.x, 0, lean.z) * (H * u * u * 0.5f); rr[i] = (0.0013f + 0.0004f * u + 0.0014f * std::pow(u, 5.0f)) * sz; }
    addTube(m, p, rr, c.detail >= 1 ? 14 : 8, mTube, 0.2f * c.seed, 1);
    V3 top = p[nt - 1]; V3 ax = norm(p[nt - 1] - p[nt - 2]);
    M34 fr = frameY(top, ax, 1, c.az);
    // sheath
    { int ns = 10; std::vector<V3> q(ns); std::vector<float> qr(ns); float sh = std::min(0.022f * sz, H * 0.6f + 0.006f);
      for (int i = 0; i < ns; i++) { float u = i / (ns - 1.f); q[i] = c.base + V3(0, -0.004f + sh * u, 0) + V3(lean.x, 0, lean.z) * (sh * u * 0.3f); qr[i] = (0.0034f - 0.0010f * u) * sz * (i == ns - 1 ? 0.75f : 1.f); }
      addTube(m, q, qr, 10, mSheath, 0.3f); }
    // tepals: 3 outer + 3 inner
    for (int k = 0; k < 6; k++) {
        bool inner = k & 1; float phi = k * (PI / 3) + 0.15f * std::sin(c.seed * 3.f + k);
        float a = (0.07f + (inner ? 0.74f : 0.90f) * open) * (1 + 0.1f * std::sin(c.seed + k * 2.1f));
        Blade b; b.len = tl * (inner ? 0.93f : 1.0f); b.width = b.len * (0.215f + 0.03f * open); b.maxAt = 0.58f; b.q = 0.72f; b.baseW = 0.22f;
        b.cup = 1.35f - 0.75f * open; b.fold = 0.02f; b.curl = -0.62f * (1 - open) + 0.32f * open; b.curlPow = 1.6f; b.wave = 0.06f; b.waveFreq = 2.5f; b.wavePhase = k + c.seed;
        b.noise = 0.0004f; b.seed = c.seed * 10 + k; b.nu = c.detail == 2 ? 30 : 14; b.nv = c.detail == 2 ? 8 : 4;
        V3 rad = fr.dir(V3(std::cos(phi), 0, std::sin(phi)));
        V3 Y = norm(ax * std::cos(a) + rad * std::sin(a)), Z = norm(-rad * std::cos(a) + ax * std::sin(a)), X = cross(Y, Z);
        V3 org = top + rad * ((inner ? 0.0016f : 0.0024f) * sz) + ax * (inner ? 0.0008f : 0.0f);
        addBlade(m, b, M34::fromBasis(X, Y, Z, org), mTepal, 0.13f * (c.seed * 7 + k), 0);
    }
    // stamens + style (only worth it when open and close enough)
    if (c.detail >= 1 && open > 0.05f) {
        for (int k = 0; k < 3; k++) {
            float phi = k * (2 * PI / 3) + 0.5f; V3 rad = fr.dir(V3(std::cos(phi), 0, std::sin(phi)));
            V3 d = norm(ax + rad * (0.12f + 0.2f * open));
            std::vector<V3> q = {top + rad * 0.001f, top + d * (0.007f * sz) + rad * 0.001f}; std::vector<float> qr = {0.00035f, 0.0003f};
            addTube(m, q, qr, 6, mTube, 0);
            M34 af = frameY(q[1] + d * (0.0045f * sz), d, 1, k);
            addBlob(m, V3(0), V3(0.00085f * sz, 0.0052f * sz, 0.00065f * sz), 10, 10, mAnther, 0.3f * k, nullptr, &af);
        }
        std::vector<V3> q = {top, top + ax * (0.017f * sz)}; std::vector<float> qr = {0.0003f, 0.00035f}; addTube(m, q, qr, 6, mStyle, 0);
        for (int k = 0; k < 3; k++) {
            float phi = k * (2 * PI / 3) + 1.6f; V3 rad = fr.dir(V3(std::cos(phi), 0, std::sin(phi)));
            Blade b; b.len = 0.0058f * sz; b.width = 0.0011f * sz; b.maxAt = 0.8f; b.q = 0.5f; b.baseW = 0.3f; b.fold = 0.7f; b.wave = 0.5f; b.waveFreq = 5; b.curl = 0.5f; b.nu = 10; b.nv = 2;
            float a = 0.35f + 0.25f * open; V3 Y = norm(ax * std::cos(a) + rad * std::sin(a)), Z = norm(-rad * std::cos(a) + ax * std::sin(a));
            addBlade(m, b, M34::fromBasis(cross(Y, Z), Y, Z, q[1]), mStyle, 0, 0);
        }
    }
    // leaves
    int nl = 4;
    for (int k = 0; k < nl; k++) {
        float phi = k * 1.7f + c.seed; float L = (0.02f + 0.05f * ease(0.0f, 0.9f, g)) * sz * (0.75f + 0.4f * hash1(c.seed * 13 + k));
        Blade b; b.len = L; b.width = 0.0013f * sz; b.maxAt = 0.3f; b.q = 0.25f; b.tipQ = 1.2f; b.baseW = 0.9f; b.fold = 0.55f; b.curl = 0.5f + 0.5f * hash1(c.seed * 17 + k); b.curlPow = 2; b.twist = 0.5f; b.nu = 18; b.nv = 2;
        float out = 0.16f + 0.2f * hash1(c.seed * 19 + k) + 0.03f * std::sin(cph + k);
        V3 rad(std::cos(phi), 0, std::sin(phi));
        V3 Y = norm(V3(0, 1, 0) * std::cos(out) + rad * std::sin(out)), Z = norm(-rad * std::cos(out) + V3(0, 1, 0) * std::sin(out));
        addBlade(m, b, M34::fromBasis(cross(Y, Z), Y, Z, c.base + rad * 0.003f + V3(0, -0.006f, 0)), mLeaf, 0.1f * k, 0);
    }
}

static void buildSnow(Scene& sc, float melt, float x0, float x1, float z0, float z1, int nx, int nz) {
    Mesh m; std::vector<V3> P(nx * nz); std::vector<float> A(nx * nz);
    for (int i = 0; i < nx; i++) for (int j = 0; j < nz; j++) {
        float x = x0 + (x1 - x0) * i / (nx - 1.f), z = z0 + (z1 - z0) * j / (nz - 1.f);
        float th = snowTh(x, z, melt); float gy = ground(x, z);
        float y = th > 0 ? gy + th + 0.0018f * fbm(V3(x * 70, 3, z * 70), 2) * sat(th / 0.01f) : gy + std::max(th, -0.01f) * 1.2f - 0.001f;
        P[i * nz + j] = V3(x, y, z); A[i * nz + j] = 1 - sat(th / 0.007f);
    }
    addGrid(m, nx, nz, P, nullptr, &A, mSnow, false, false, 0, true);
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
}

// mode 0: close time-lapse of the hero crocus; mode 1: wider look at the snow drawing back
static void frameCommon(Scene& sc, FrameCtx& fc, int mode) {
    float tt = (float)fc.f / (fc.nf - 1);
    float T = mode == 0 ? 0.30f + 0.70f * tt : 0.42f * tt;          // global season time
    float melt = mode == 0 ? 0.16f + 0.36f * ease(0, 1, tt) : 0.05f + 0.62f * ease(0.0f, 1.0f, tt);
    float cloud = 1 - 0.55f * (smooth(0.20f, 0.26f, tt) * smooth(0.40f, 0.33f, tt)) - 0.45f * (smooth(0.68f, 0.72f, tt) * smooth(0.82f, 0.77f, tt));
    float light = sat(cloud * 1.2f - 0.2f);
    soilEmit(sc, soil); litterEmit(sc, litter); sc.addInst(decor, M34::identity(), 0);
    for (const MossPt& mp : mossPts) { Rng r2(mp.seed * 7 + 1); mossAdd(sc, moss, mp.p, mp.n, mp.h, r2); }
    Mesh m;
    for (const Crocus& c : C) { float g = sat((T - c.t0) / (c.t1 - c.t0)); if (g <= 0) continue; crocus(m, c, g, light, fc.f); }
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    if (mode == 0) { buildSnow(sc, melt, -0.22f, 0.22f, -0.3f, 0.2f, 330, 370); buildSnow(sc, melt, -1.1f, 1.1f, -2.0f, -0.29f, 220, 170); buildSnow(sc, melt, -1.1f, -0.21f, -0.3f, 0.5f, 90, 80); buildSnow(sc, melt, 0.21f, 1.1f, -0.3f, 0.5f, 90, 80); }
    else buildSnow(sc, melt, -1.1f, 1.1f, -2.0f, 0.55f, 560, 640);
    Camera& c = fc.cam; Env& e = fc.env;
    if (mode == 0) {
        float ce = ease(0, 1, tt);
        c.pos = lerp(V3(0.065f, 0.082f, 0.42f), V3(0.02f, 0.088f, 0.385f), ce);
        c.target = lerp(V3(0.006f, 0.030f, 0), V3(0.006f, 0.042f, 0), ease(0.1f, 0.8f, tt));
        c.focal = 0.075f; c.fstop = 5.6f; c.focusDist = len(V3(0.002f, 0.045f, 0.004f) - c.pos);
    } else {
        float ce = ease(0, 1, tt);
        c.pos = lerp(V3(-0.12f, 0.34f, 0.62f), V3(-0.04f, 0.30f, 0.56f), ce);
        c.target = lerp(V3(-0.02f, 0.0f, -0.12f), V3(0.0f, 0.0f, -0.10f), ce);
        c.focal = 0.04f; c.fstop = 5.6f; c.focusDist = len(V3(0, 0.01f, -0.06f) - c.pos);
    }
    float az = -0.75f + 0.55f * tt;
    e.sunDir = norm(V3(std::sin(az), mode == 0 ? 0.40f + 0.12f * tt : 0.55f, -std::cos(az)));
    float fl = 1 + 0.05f * (hash1(fc.f * 911 + 7) - 0.5f);
    e.sunE = V3(6.2f, 5.3f, 4.1f) * cloud * fl; e.sunAngR = 0.035f;
    e.zen = V3(0.15f, 0.28f, 0.60f) * 1.25f; e.hor = V3(0.55f, 0.62f, 0.70f) * 1.15f; e.gnd = V3(0.018f, 0.02f, 0.02f); e.horExp = 0.5f;
    e.type = 2; e.bd0 = V3(0.10f, 0.14f, 0.22f); e.bd1 = V3(0.42f, 0.42f, 0.42f) * (0.5f + 0.5f * cloud); e.bdScale = 4; e.bdMix = 1; e.bdP0 = 0.02f; e.bdP1 = 0.8f; e.bdP2 = 26; e.bdP3 = 0.95f; e.bdGap = 0.05f;
    e.glow = 0.05f; e.glowExp = 24; e.eps = mode == 0 ? 4e-6f : 1e-5f; e.clampL = 16;
    if (mode == 1) { e.cloudStr = 0.75f; e.cloudScale = 2.2f; e.cloudOfs = V2(tt * 4.5f, tt * 1.3f); e.sunE = V3(6.2f, 5.3f, 4.1f) * fl; }
    // snow sparkles (surface facets catching the sun)
    for (const Spark& s : sparks) {
        float th = snowTh(s.x, s.z, melt); if (th < 0.003f) continue;
        V3 p(s.x, ground(s.x, s.z) + th + 0.0006f, s.z);
        V3 v = norm(c.pos - p); V3 hv = norm(v + e.sunDir); float al = dot(hv, s.n);
        float k = std::pow(std::max(al, 0.f), 1200.0f); if (k < 0.03f) continue;
        sc.addGlint(p, e.sunE * (1.6e-8f * k * (0.3f + s.s) * (mode ? 4.0f : 1.0f)), 0.15f);
    }
    Rng r(71);
    if (mode == 0) for (int i = 0; i < 18; i++) {
        V3 p(r.r(-0.7f, 0.7f), r.r(0.02f, 0.5f), r.r(-2.6f, -0.9f));
        float br = r.f(); br = 0.06f + br * br * br; float tw = 0.6f + 0.4f * std::sin(tt * 14 * r.r(0.5f, 2) + r.r(0, 6));
        V3 col = lerp(V3(0.8f, 0.88f, 1.0f), V3(1.0f, 0.9f, 0.7f), r.f()) * (br * tw * 1.0e-3f * cloud);
        float dz = p.z - c.pos.z; sc.addGlint(p, col * (dz * dz), r.r(0.05f, 0.2f));
    }
}
static void frameClose(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 0); }
static void frameWide(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 1); }
} // namespace cro
