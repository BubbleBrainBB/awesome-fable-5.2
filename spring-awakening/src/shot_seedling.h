// shot_seedling.h — time-lapse: seedlings break the soil, straighten and open their cotyledons.
#pragma once
#include "plants.h"

namespace sdl {
struct Sdl { V3 base; float az; float t0, t1; float size; uint32_t seed; bool hero; };
struct Shoot { V3 base; float az, t0, len, lean; uint32_t seed; };
static Soil soil;
static int mStem, mCot, mHair, mCoat, mGrass, mDead, mTwig, mStraw, mRoot;
static std::vector<Sdl> S;
static std::vector<Shoot> G;
static BlasH decor;

static void init(Scene& sc) {
    soil = Soil();
    soilInit(sc, soil, 0.4f, -0.9f, 0.3f, 220, V3(0.17f, 0.098f, 0.055f), V3(0.065f, 0.038f, 0.022f));
    sc.mats[soil.matBase].p0[3] = 0.5f; sc.mats[soil.matBase + 1].p0[3] = 0.6f;
    auto dens = [](float x, float z) { float d = std::sqrt(x * x * 0.6f + z * z); return 0.12f + 0.88f * smooth(0.22f, 0.05f, d); };
    soilScatter(soil, 150000, -0.3f, 0.3f, -0.45f, 0.25f, 0.0004f, 0.006f, 11, dens, 3.2f);
    soilScatter(soil, 60000, -0.09f, 0.09f, -0.06f, 0.09f, 0.00025f, 0.0012f, 13, nullptr, 1.5f);
    soilScatter(soil, 500, -0.3f, 0.3f, -0.6f, 0.25f, 0.006f, 0.016f, 12, nullptr, 1.2f);
    mStem = sc.addMat([] { Mat m = mkMat(V3(0.78f, 0.60f, 0.52f), 0.32f, 1.0f, 4); set4(m.base2, V3(0.50f, 0.66f, 0.26f)); m.trans = 0.3f; set4(m.p0, 0.25f, 0.75f, 30, 0); set4(m.p2, V3(0.55f, 0.3f, 0.3f), 0.0f); return m; }());
    mCot = sc.addMat([] { Mat m = mkMat(V3(0.13f, 0.30f, 0.045f), 0.22f, 1.0f, 2); set4(m.base2, V3(0.42f, 0.62f, 0.06f)); m.trans = 0.42f; set4(m.sh, V3(0.45f, 0.62f, 0.15f)); set4(m.p0, 4, 0.5f, 0.3f, 0); set4(m.p1, 0.0f, 0.5f, 0.2f, 0.00012f); set4(m.p2, V3(0.5f, 0.5f, 0.1f), 0.5f); m.bump = 1; return m; }());
    mHair = sc.addMat([] { Mat m = mkMat(V3(0.85f, 0.88f, 0.8f), 0.4f, 0.4f, 0); set4(m.base2, V3(0.95f, 0.97f, 0.9f)); m.trans = 0.6f; set4(m.sh, V3(0.75f)); return m; }());
    mCoat = sc.addMat([] { Mat m = mkMat(V3(0.20f, 0.12f, 0.06f), 0.45f, 0.8f, 1); set4(m.base2, V3(0.09f, 0.05f, 0.03f)); m.scale = 500; m.bump = 1; set4(m.p0, 0.0002f, 0.00003f, 0, 0.3f); return m; }());
    mGrass = sc.addMat([] { Mat m = mkMat(V3(0.15f, 0.33f, 0.05f), 0.3f, 1.0f, 2); set4(m.base2, V3(0.45f, 0.65f, 0.08f)); m.trans = 0.45f; set4(m.p0, 0, 0, 0.35f, 0); set4(m.p1, 0.0f, 0.3f, 0.2f, 0); set4(m.p2, V3(0.75f, 0.72f, 0.45f), 1.0f); return m; }());
    mDead = sc.addMat([] { Mat m = mkMat(V3(0.20f, 0.12f, 0.06f), 0.6f, 0.4f, 2); set4(m.base2, V3(0.35f, 0.16f, 0.05f)); m.trans = 0.22f; set4(m.p0, 7, 0.55f, 0.6f, 22); set4(m.p1, 0.5f, 0.3f, 0.5f, 0.0002f); set4(m.p2, V3(0.07f, 0.045f, 0.03f), 0.0f); m.bump = 1; return m; }());
    mTwig = sc.addMat([] { Mat m = mkMat(V3(0.11f, 0.075f, 0.05f), 0.6f, 0.5f, 4); set4(m.base2, V3(0.16f, 0.11f, 0.07f)); set4(m.p0, 0.0f, 1.0f, 60, 0); set4(m.p2, V3(0.3f, 0.26f, 0.2f), 0.5f); return m; }());

    mStraw = sc.addMat([] { Mat m = mkMat(V3(0.42f, 0.33f, 0.19f), 0.5f, 0.5f, 4); set4(m.base2, V3(0.5f, 0.42f, 0.26f)); m.trans = 0.2f; set4(m.p0, 0.0f, 1.0f, 40, 0); set4(m.p2, V3(0.2f, 0.15f, 0.1f), 0.4f); return m; }());
    mRoot = sc.addMat([] { Mat m = mkMat(V3(0.6f, 0.55f, 0.45f), 0.5f, 0.5f, 0); set4(m.base2, V3(0.7f, 0.65f, 0.5f)); m.trans = 0.3f; return m; }());
    S.clear(); G.clear();
    S.push_back({V3(-0.006f, 0, 0.0f), 0.65f, -0.20f, 0.74f, 1.0f, 3, true});
    S.push_back({V3(0.047f, 0, -0.045f), 2.5f, 0.02f, 0.98f, 0.92f, 5, false});
    S.push_back({V3(-0.058f, 0, -0.075f), -0.4f, -0.28f, 0.66f, 1.05f, 9, false});
    S.push_back({V3(0.024f, 0, 0.05f), 1.9f, 0.22f, 1.22f, 0.85f, 14, false});
    S.push_back({V3(-0.11f, 0, -0.16f), 1.1f, 0.05f, 0.95f, 1.0f, 21, false});
    S.push_back({V3(0.13f, 0, -0.2f), 0.2f, 0.1f, 1.0f, 1.1f, 27, false});
    Rng r(77);
    for (int i = 0; i < 46; i++) {
        V3 b(r.r(-0.2f, 0.2f), 0, r.r(-0.4f, 0.12f));
        if (std::fabs(b.x + 0.006f) < 0.022f && std::fabs(b.z) < 0.03f) continue;
        G.push_back({b, r.r(0, 2 * PI), r.r(-0.1f, 0.6f), r.r(0.022f, 0.06f), r.r(0.05f, 0.3f), (uint32_t)(100 + i)});
    }
    // static decor: twig, dead leaves, pebbles handled by the soil
    Mesh m;
    { std::vector<V3> c = {V3(0.03f, 0.004f, 0.035f), V3(0.07f, 0.006f, 0.02f), V3(0.115f, 0.004f, 0.012f), V3(0.16f, 0.007f, -0.01f)};
      for (auto& p : c) p.y += soil.height(p.x, p.z);
      std::vector<V3> p = spline(c, 40); std::vector<float> rr(40); for (int i = 0; i < 40; i++) rr[i] = 0.0016f * (1 - 0.5f * i / 39.f) * (1 + 0.15f * fbm(V3(i * 0.4f, 0, 0)));
      addTube(m, p, rr, 10, mTwig, 0.3f, 6); }
    for (int k = 0; k < 7; k++) {
        Blade b; b.len = r.r(0.03f, 0.055f); b.width = b.len * r.r(0.26f, 0.36f); b.maxAt = 0.45f; b.q = 0.75f; b.fold = r.r(-0.1f, 0.3f); b.cup = r.r(-0.6f, 0.9f);
        b.curl = r.r(-0.8f, 1.2f); b.wave = 0.25f; b.waveFreq = r.r(2, 4); b.noise = 0.0025f; b.seed = 40 + k; b.nu = 40; b.nv = 10; b.serr = 0.12f; b.serrN = 14;
        V3 p(r.r(-0.19f, 0.19f), 0, r.r(-0.3f, 0.06f)); if (std::fabs(p.x) < 0.03f && std::fabs(p.z) < 0.03f) p.x += 0.07f;
        p.y = soil.height(p.x, p.z) + 0.004f;
        M34 xf = M34::translate(p) * M34::rotY(r.r(0, 6.28f)) * M34::rotX(-PI / 2 + r.r(-0.25f, 0.25f)) * M34::rotY(r.r(-0.3f, 0.3f));
        addBlade(m, b, xf, mDead, r.f(), r.f() * 0.5f);
    }
    for (int k = 0; k < 36; k++) { // dry stalks from last year
        V3 b(r.r(-0.3f, 0.3f), 0, r.r(-0.75f, -0.14f)); b.y = soil.height(b.x, b.z) - 0.002f;
        float L = r.r(0.05f, 0.2f); V3 dir = norm(V3(r.r(-0.6f, 0.6f), 1, r.r(-0.6f, 0.6f)));
        std::vector<V3> c = {b, b + dir * (L * 0.4f) + r.dir() * 0.004f, b + dir * (L * 0.75f) + r.dir() * 0.01f + V3(0, -0.1f * L, 0), b + dir * L + r.dir() * 0.012f + V3(0, -0.25f * L, 0)};
        std::vector<V3> p = spline(c, 14); std::vector<float> rr(14); float r0 = r.r(0.0006f, 0.0013f); for (int i = 0; i < 14; i++) rr[i] = r0 * (1 - 0.6f * i / 13.f);
        addTube(m, p, rr, 6, mStraw, r.f(), 3);
    }
    for (int k = 0; k < 16; k++) { // young broad leaves far behind, only ever seen out of focus
        V3 b(r.r(-0.35f, 0.35f), 0, r.r(-0.85f, -0.3f)); b.y = soil.height(b.x, b.z);
        int nl = 2 + (int)(r.u() % 3);
        for (int j = 0; j < nl; j++) {
            Blade bl; bl.len = r.r(0.03f, 0.07f); bl.width = bl.len * r.r(0.25f, 0.4f); bl.maxAt = 0.4f; bl.q = 0.8f; bl.fold = 0.2f; bl.curl = r.r(0.3f, 1.2f); bl.nu = 14; bl.nv = 3; bl.wave = 0.1f;
            M34 xf = M34::translate(b) * M34::rotY(r.r(0, 6.28f)) * M34::rotX(-r.r(0.1f, 0.6f));
            addBlade(m, bl, xf, mGrass, r.f(), 0);
        }
    }
    for (int k = 0; k < 420; k++) { // straw bits and pale rootlets lying on the soil
        V3 c(r.r(-0.22f, 0.22f), 0, r.r(-0.4f, 0.14f)); float L = r.r(0.004f, 0.022f); float az = r.r(0, 6.28f);
        V3 d(std::cos(az), 0, std::sin(az)); bool root = r.f() < 0.3f; float rad = root ? r.r(0.00008f, 0.00018f) : r.r(0.00022f, 0.0005f);
        V3 prev; float rn = r.f();
        for (int i = 0; i <= 5; i++) { float u = i / 5.f - 0.5f; V3 q = c + d * (u * L) + V3(-d.z, 0, d.x) * (0.15f * L * std::sin(u * 3 + rn * 6)); q.y = soil.height(q.x, q.z) + rad + 0.0012f * (0.5f + 0.5f * std::sin(u * 5 + rn * 9));
            if (i > 0) m.seg(prev, rad, q, rad, root ? mRoot : mStraw, 0, 1, rn); prev = q; }
    }
    decor = sc.addBlas(m);
}

static float bulge(const Sdl& s, float d, float g) {
    float ea = ease(0.03f, 0.3f, g) * (1 - 0.65f * ease(0.3f, 0.6f, g));
    return 0.0038f * s.size * ea * std::exp(-(d * d) / (0.0085f * 0.0085f * s.size * s.size));
}
static float capDist(const Sdl& s, float g, float x, float z, V3* dirOut = nullptr) {
    V3 hd(std::cos(s.az), 0, std::sin(s.az));
    float hk = 0.0075f * s.size * ease(0.18f, 0.4f, g) * (1 - 0.6f * ease(0.6f, 0.85f, g));
    V3 a = s.base, b = s.base + hd * hk; V3 p(x, 0, z);
    V3 ab = b - a; float t = hk > 1e-6f ? sat(dot(p - a, ab) / dot(ab, ab)) : 0;
    V3 c = a + ab * t; V3 dd = p - c; dd.y = 0; float d = len(dd);
    if (dirOut) *dirOut = d > 1e-6f ? dd / d : V3(1, 0, 0);
    return d;
}

static void seedling(Mesh& m, Mesh& hm, const Sdl& s, float g, int frame, V3& coatPos, M34& coatXf, bool& coatOn) {
    float sz = s.size;
    float L = (0.006f + 0.047f * ease(0.04f, 0.98f, g)) * sz;
    float H = 2.95f * (1 - ease(0.42f, 0.80f, g));
    float hl = std::min(0.011f * sz, L * 0.75f);
    int n = s.hero ? 56 : 30;
    V3 hd(std::cos(s.az), 0, std::sin(s.az));
    float ca = 0.11f * ease(0.3f, 0.75f, g); float cph = g * 2 * PI * 4.3f + s.seed;
    V3 jit(hash1(frame * 131 + s.seed) - 0.5f, 0, hash1(frame * 173 + s.seed * 7) - 0.5f);
    V3 tilt = V3(std::cos(cph), 0, std::sin(cph)) * ca + jit * 0.022f * ease(0.3f, 0.6f, g) + hd * (-0.06f);
    std::vector<V3> p(n); std::vector<float> rr(n);
    V3 q = s.base + V3(0, soil.height(s.base.x, s.base.z) - 0.012f * sz, 0); float ds = L / (n - 1); V3 d(0, 1, 0);
    for (int i = 0; i < n; i++) {
        float sa = (float)i / (n - 1) * L;
        float th = H * smooth(0.0f, 1.0f, (sa - (L - hl)) / hl);
        V3 wob(fbm(V3(sa * 55, s.seed * 3.1f, 0.5f), 2), 0, fbm(V3(sa * 55, s.seed * 3.1f, 7.5f), 2));
        d = norm(V3(0, 1, 0) * std::cos(th) + hd * std::sin(th) + tilt * std::pow(sa / L, 1.6f) + wob * 0.22f);
        if (i > 0) q += d * ds;
        p[i] = q; rr[i] = 0.00085f * sz * (1.0f + 0.12f * sa / L - 0.25f * smooth(0.25f, 0.0f, sa / L)) * (0.8f + 0.2f * ease(0.0f, 0.6f, g));
    }
    float green = ease(0.45f, 0.95f, g);
    // stem attr drives base(pink-white) -> green; use uScale to carry "greenness" through attr remap
    {
        Frames f = ptFrames(p, hd);
        int sides = s.hero ? 14 : 8;
        std::vector<V3> P(n * sides); std::vector<V2> UV(n * sides); std::vector<float> A(n * sides);
        for (int i = 0; i < n; i++) for (int j = 0; j < sides; j++) {
            float a = 2 * PI * j / sides; float u = (float)i / (n - 1);
            P[i * sides + j] = p[i] + (f.N[i] * std::cos(a) + f.B[i] * std::sin(a)) * rr[i];
            UV[i * sides + j] = V2(u * L * 400, (float)j / sides);
            A[i * sides + j] = sat(u * 1.1f - 0.25f) * green + 0.12f * green;
        }
        addGrid(m, n, sides, P, &UV, &A, mStem, false, true, 0.1f * s.seed, true);
        if (s.hero || true) { Rng hr(s.seed * 5 + 1); addTubeHairs(hm, p, rr, s.hero ? 420 : 120, 0.00060f * sz, 0.000008f, mHair, hr, 0.35f, 0.97f, 0.5f, 0.2f); }
    }
    // cotyledons
    V3 T = d; V3 e = norm(cross(hd, V3(0, 1, 0)));
    e = norm(e - T * dot(e, T));
    float al = 1.46f * ease(0.70f, 0.96f, g) + 0.05f * std::sin(g * 40 + s.seed);
    float cl = (0.0075f + 0.0095f * ease(0.5f, 1.0f, g)) * sz;
    V3 tip = p[n - 1];
    for (int k = 0; k < 2; k++) {
        V3 ee = k ? -e : e;
        V3 Y = T * std::cos(al) + ee * std::sin(al), Z = -ee * std::cos(al) + T * std::sin(al), X = cross(Y, Z);
        Blade b; b.len = cl; b.width = cl * 0.34f; b.maxAt = 0.52f; b.q = 0.55f; b.baseW = 0.25f; b.fold = 0.05f; b.cup = 0.35f - 0.2f * ease(0.8f, 1, g);
        b.curl = 0.35f * ease(0.8f, 1.0f, g); b.thick = 0.0010f * sz; b.nu = s.hero ? 26 : 14; b.nv = s.hero ? 7 : 4; b.noise = 0.0002f; b.seed = s.seed + k;
        M34 xf = M34::fromBasis(X, Y, Z, tip + ee * (0.00045f * sz) - T * 0.0003f);
        addBlade(m, b, xf, mCot, 0.37f * (s.seed + k), 1 - green);
    }
    // first true leaves
    float tg = ease(0.86f, 1.0f, g);
    if (tg > 0.01f) for (int k = 0; k < 2; k++) {
        V3 w = norm(cross(T, e)) * (k ? -1.f : 1.f);
        float a2 = 0.25f + 0.25f * tg;
        V3 Y = T * std::cos(a2) + w * std::sin(a2), Z = -w * std::cos(a2) + T * std::sin(a2), X = cross(Y, Z);
        Blade b; b.len = 0.004f * tg * sz; b.width = b.len * 0.3f; b.maxAt = 0.45f; b.q = 0.9f; b.fold = 0.5f; b.nu = 10; b.nv = 3;
        addBlade(m, b, M34::fromBasis(X, Y, Z, tip), mGrass, 0.5f, 0);
    }
    // seed coat
    coatOn = g < 0.66f;
    if (coatOn) {
        V3 c = tip + T * (cl * 0.72f);
        coatXf = frameY(c, T, 1, s.seed) * M34::scale(V3(cl * 0.36f, cl * 0.42f, 0.0019f * sz));
    } else {
        V3 c = s.base + hd * (0.013f * sz) + e * (0.006f * sz); c.y = soil.height(c.x, c.z) + 0.0016f;
        coatXf = M34::translate(c) * M34::rotY(s.seed * 1.3f) * M34::rotX(1.2f) * M34::scale(V3(cl * 0.36f, cl * 0.42f, 0.0019f * sz) * 0.9f);
    }
    (void)coatPos;
}

static BlasH coatB; static bool coatInit = false;

static void frame(Scene& sc, FrameCtx& fc) {
    float tt = (float)fc.f / (fc.nf - 1);
    if (!coatInit) { coatInit = true; }
    Mesh m, hm, cm;
    // seedlings
    std::vector<float> gs(S.size());
    Mesh coat; addBlob(coat, V3(0), V3(1), 12, 20, mCoat, 0, [](V3 d) { return d.y < -0.2f ? 0.82f : 1.0f; });
    for (size_t i = 0; i < S.size(); i++) {
        float g = sat((tt - S[i].t0) / (S[i].t1 - S[i].t0)); gs[i] = g;
        V3 cp; M34 cx; bool on;
        seedling(m, hm, S[i], g, fc.f, cp, cx, on);
        m.append(coat, cx);
        // mound patch
        int n = 26; std::vector<V3> P(n * n);
        for (int a = 0; a < n; a++) for (int b = 0; b < n; b++) {
            float x = S[i].base.x + (a / (n - 1.f) * 2 - 1) * 0.03f, z = S[i].base.z + (b / (n - 1.f) * 2 - 1) * 0.03f;
            float d = capDist(S[i], g, x, z); float rim = std::max(std::fabs(a / (n - 1.f) * 2 - 1), std::fabs(b / (n - 1.f) * 2 - 1));
            P[a * n + b] = V3(x, soil.height(x, z) + bulge(S[i], d, g) * 0.85f - 0.0012f * smooth(0.6f, 1.0f, rim) - 0.0002f, z);
        }
        addGrid(cm, n, n, P, nullptr, nullptr, 0, false, false, 0, true);
    }
    // grass shoots
    for (const Shoot& gsh : G) {
        float g = ease(gsh.t0, gsh.t0 + 0.55f, tt); if (g <= 0.001f) continue;
        Blade b; b.len = gsh.len * g; b.width = 0.0011f + 0.0006f * g; b.maxAt = 0.3f; b.q = 0.35f; b.tipQ = 1.5f; b.baseW = 0.8f; b.fold = 0.75f - 0.3f * g; b.curl = 0.5f * g * gsh.lean * 4; b.twist = 0.8f * g; b.nu = 18; b.nv = 2;
        float ph = tt * 2 * PI * 3.1f + gsh.seed; V3 tl(std::cos(ph) * 0.07f + (hash1(fc.f * 31 + gsh.seed) - 0.5f) * 0.03f, 0, std::sin(ph) * 0.07f);
        V3 base = gsh.base; base.y = soil.height(base.x, base.z) - 0.002f;
        M34 xf = M34::translate(base) * M34::rotY(gsh.az) * M34::rotX(-gsh.lean - tl.x) * M34::rotZ(tl.z);
        addBlade(m, b, xf, mGrass, 0.1f * gsh.seed, 0, [g](float u, float) { return sat(1 - u * (1.5f + 2 * g)) * 0.8f; });
    }
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    sc.addInst(sc.addBlas(hm), M34::identity(), 0);
    sc.addInst(sc.addBlas(cm), M34::identity(), soil.matBase);
    sc.addInst(decor, M34::identity(), 0);
    soilEmit(sc, soil, [&](SoilItem& it) {
        for (size_t i = 0; i < S.size(); i++) {
            float dx = it.p.x - S[i].base.x, dz = it.p.z - S[i].base.z;
            if (dx * dx + dz * dz > 0.04f * 0.04f) continue;
            float g = gs[i]; V3 dir; float d = capDist(S[i], g, it.p.x, it.p.z, &dir);
            float rc = (0.0006f + 0.0032f * ease(0.18f, 0.5f, g)) * S[i].size + it.s * 0.8f;
            float y0 = soil.height(it.p.x, it.p.z);
            if (d < rc) {
                float nd = rc + (rc - d) * (0.3f + 0.9f * it.rnd);
                float mv = nd - d;
                it.p.x += dir.x * mv; it.p.z += dir.z * mv; d = nd;
                float y1 = soil.height(it.p.x, it.p.z);
                it.p.y += (y1 - y0) + 0.0012f * it.rnd * ease(0.2f, 0.5f, g); y0 = y1;
                it.ang += mv * 500;
            }
            it.p.y += bulge(S[i], d, g) * (0.9f + 0.4f * it.rnd);
        }
        return true;
    });
    // bokeh lights in the far background
    Rng r(31);
    for (int i = 0; i < 34; i++) {
        V3 p(r.r(-1.0f, 1.0f), r.r(0.02f, 0.6f), r.r(-2.6f, -0.55f));
        float fl = 0.65f + 0.35f * std::sin(tt * 2 * PI * r.r(0.5f, 2.5f) + r.r(0, 6.28f));
        float br = r.f(); br = 0.08f + 0.92f * br * br * br;
        float cm = r.f();
        V3 col = (cm < 0.5f ? lerp(V3(1.0f, 0.9f, 0.62f), V3(1.0f, 0.97f, 0.85f), cm * 2) : lerp(V3(0.55f, 0.8f, 0.3f), V3(0.8f, 0.95f, 0.6f), cm * 2 - 1)) * (br * 1.1e-3f * fl);
        sc.addGlint(p, col * (p.z * p.z), r.r(0.05f, 0.22f));
    }
    // camera: slow slider move
    float ce = ease(0.0f, 1.0f, tt);
    fc.cam.pos = lerp(V3(-0.045f, 0.046f, 0.37f), V3(0.012f, 0.05f, 0.345f), ce);
    fc.cam.target = lerp(V3(-0.004f, 0.016f, 0.0f), V3(-0.002f, 0.027f, 0.0f), ease(0.2f, 1.0f, tt));
    fc.cam.focal = 0.100f; fc.cam.fstop = 8.0f; fc.cam.focusDist = len(V3(-0.004f, 0.02f, 0.002f) - fc.cam.pos);
    // light: low back-left sun drifting, clouds passing
    float az = -0.55f + 0.5f * tt;
    fc.env.sunDir = norm(V3(std::sin(az), 0.42f + 0.1f * tt, -std::cos(az)));
    float cloud = 1 - 0.6f * (smooth(0.17f, 0.22f, tt) * smooth(0.33f, 0.27f, tt)) - 0.5f * (smooth(0.62f, 0.66f, tt) * smooth(0.74f, 0.7f, tt));
    float fl = 1 + 0.05f * (hash1(fc.f * 911) - 0.5f);
    fc.env.sunE = V3(6.0f, 5.0f, 3.6f) * cloud * fl; fc.env.sunAngR = 0.03f;
    Env& e = fc.env;
    e.type = 2; e.bd0 = V3(0.05f, 0.04f, 0.025f); e.bd1 = V3(0.10f, 0.12f, 0.04f); e.bdScale = 4.0f; e.bdMix = 1; e.bdP0 = -0.03f; e.bdP1 = 0.85f; e.bdP2 = 17; e.bdP3 = 0.93f; e.bdGap = 0.02f;
    e.zen = V3(0.16f, 0.30f, 0.60f) * 1.2f; e.hor = V3(0.50f, 0.62f, 0.22f) * 1.15f; e.gnd = V3(0.016f, 0.026f, 0.012f); e.horExp = 0.5f;
    fc.env.glow = 0.02f; fc.env.glowExp = 20; fc.env.eps = 3e-6f; fc.env.clampL = 14;
}
} // namespace sdl
