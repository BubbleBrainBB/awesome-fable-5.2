// shot_bud.h — time-lapse: beech buds split and the pleated young leaves unfold.
#pragma once
#include "plants.h"

namespace bud {
static float envf(const char* n, float d) { const char* v = getenv(n); return v ? (float)atof(v) : d; }
struct Bud { float s; int side; float t0, t1; uint32_t seed; int nleaf; };       // s = position along the twig (0..1)
struct Twig { std::vector<V3> ctrl; float rad; std::vector<Bud> buds; int detail; uint32_t seed; };
static std::vector<Twig> TW;
static int mTwig, mScale, mLeaf, mCilia, mShoot, mWater, mStreak, mLeafWet;
static BlasH dropB;
static bool gRain = false; static float gRainT = 0;
struct Splash { V3 p; float t0; }; static std::vector<Splash> gSplash;   // filled while building leaves
struct LeafDrop { V3 p, n; float r; }; static std::vector<LeafDrop> gLeafDrops;
// geometry check (env BUDCHECK=1): sampled leaf / scale surfaces and the twig centre line, tested for intersections each frame
struct ChkSurf { int bud, k; std::vector<V3> pts; }; static std::vector<ChkSurf> gChkLeaf, gChkScale; static std::vector<V3> gChkTwigP; static std::vector<float> gChkTwigR;
static bool gChk = false;
static void chkSample(std::vector<ChkSurf>& dst, int bud, int k, const Blade& bl, const M34& xf, float u0) {
    BladePts mid = bladeMidrib(bl); ChkSurf c; c.bud = bud; c.k = k;
    for (int i = 0; i < bl.nu; i++) { if ((float)i / (bl.nu - 1) < u0) continue; for (int j = -4; j <= 4; j++) c.pts.push_back(xf.pt(bladePoint(bl, mid, i, j * 0.25f))); }
    dst.push_back(c);
}
static float segDist(V3 p, V3 a, V3 b) { V3 ab = b - a; float t = sat(dot(p - a, ab) / std::max(dot(ab, ab), 1e-12f)); return len(p - (a + ab * t)); }
static void chkReport(int frame) {
    int bad = 0;
    for (const ChkSurf& L : gChkLeaf) {   // leaf vs twig
        float dm = 1e9f; for (const V3& q : L.pts) for (size_t i = 0; i + 1 < gChkTwigP.size(); i += 2) dm = std::min(dm, segDist(q, gChkTwigP[i], gChkTwigP[std::min(i + 2, gChkTwigP.size() - 1)]) - gChkTwigR[i]);
        if (dm < 0.0006f) { fprintf(stderr, "CHK f%d leaf %d.%d TWIG clearance %.2f mm\n", frame, L.bud, L.k, dm * 1000); bad++; }
    }
    auto minD = [](const ChkSurf& A, const ChkSurf& B) { float dm = 1e9f; for (const V3& a : A.pts) for (const V3& b : B.pts) { V3 d = a - b; dm = std::min(dm, dot(d, d)); } return std::sqrt(dm); };
    for (size_t i = 0; i < gChkLeaf.size(); i++) for (size_t j = i + 1; j < gChkLeaf.size(); j++) { float d = minD(gChkLeaf[i], gChkLeaf[j]);
        if (d < 0.0009f) { int ia = 0, ib = 0; float dm = 1e9f; for (size_t x = 0; x < gChkLeaf[i].pts.size(); x++) for (size_t y = 0; y < gChkLeaf[j].pts.size(); y++) { V3 q = gChkLeaf[i].pts[x] - gChkLeaf[j].pts[y]; float dd = dot(q, q); if (dd < dm) { dm = dd; ia = (int)x; ib = (int)y; } }
            fprintf(stderr, "CHK f%d leaf %d.%d x leaf %d.%d  [row %d col %d | row %d col %d]  %.2f mm\n", frame, gChkLeaf[i].bud, gChkLeaf[i].k, gChkLeaf[j].bud, gChkLeaf[j].k, ia / 9, ia % 9 - 4, ib / 9, ib % 9 - 4, d * 1000); bad++; } }
    for (const ChkSurf& L : gChkLeaf) for (const ChkSurf& S : gChkScale) { float d = minD(L, S);
        if (d < 0.0005f) { fprintf(stderr, "CHK f%d leaf %d.%d x scale %d.%d  %.2f mm\n", frame, L.bud, L.k, S.bud, S.k, d * 1000); bad++; } }
    for (const ChkSurf& L : gChkScale) {   // scale vs twig (outer 60 % of the scale)
        float dm = 1e9f; for (const V3& q : L.pts) for (size_t i = 0; i + 1 < gChkTwigP.size(); i += 2) dm = std::min(dm, segDist(q, gChkTwigP[i], gChkTwigP[std::min(i + 2, gChkTwigP.size() - 1)]) - gChkTwigR[i]);
        if (dm < 0.0003f) { fprintf(stderr, "CHK f%d scale %d.%d TWIG clearance %.2f mm\n", frame, L.bud, L.k, dm * 1000); bad++; }
    }
    fprintf(stderr, "CHK f%d: %d leaves, %d problems\n", frame, (int)gChkLeaf.size(), bad);
}
// damped bounce of a leaf after a raindrop hits it; returns extra droop (rad) and reports recent hits
static float leafDip(uint32_t id, float t, float* lastHit = nullptr) {
    float dip = 0; float period = 1.1f + 1.6f * hash1(id * 7 + 3);
    float ph = hash1(id * 13 + 1) * period;
    for (int k = 0; k < 3; k++) {
        float n = std::floor((t + ph) / period) - k; float th = n * period - ph + 0.35f * period * (hash1(id * 31 + (uint32_t)(n + 100) * 17) - 0.5f);
        float tau = t - th; if (tau < 0) continue;
        float A = 0.28f + 0.4f * hash1(id * 11 + (uint32_t)(n + 100) * 5);
        dip += A * std::exp(-3.4f * tau) * std::sin(2 * PI * 2.7f * tau);
        if (lastHit && k == 0) *lastHit = tau;
    }
    return dip;
}

static Mat sc_dummy() { Mat m = mkMat(V3(0.15f, 0.34f, 0.055f), 0.13f, 1.6f, 2); set4(m.base2, V3(0.50f, 0.74f, 0.10f)); m.trans = 0.5f; set4(m.p0, 7, 0.5f, 0.5f, 34); set4(m.p1, 0.2f, 0.4f, 0.2f, 0.00015f); set4(m.p2, V3(0.42f, 0.46f, 0.14f), 0.3f); m.bump = 1; return m; }
static void init(Scene& sc) {
    mTwig = sc.addMat([] { Mat m = mkMat(V3(0.24f, 0.19f, 0.15f), 0.7f, 0.35f, 7); set4(m.base2, V3(0.10f, 0.075f, 0.06f)); m.scale = 900; m.bump = 1; set4(m.p0, 5, 60, 0.00012f, 0.35f); set4(m.p2, V3(0.42f, 0.40f, 0.34f)); return m; }());
    mScale = sc.addMat([] { Mat m = mkMat(V3(0.32f, 0.16f, 0.06f), 0.35f, 0.8f, 3); set4(m.base2, V3(0.85f, 0.48f, 0.16f)); m.trans = 0.5f; set4(m.sh, V3(0.5f, 0.3f, 0.12f)); set4(m.p0, 16, 0.3f, 0.3f, 1.0f); set4(m.p1, 0.2f, 0.2f, 0, 0); set4(m.p2, V3(0.50f, 0.34f, 0.17f), 0.75f); return m; }());
    mLeaf = sc.addMat([] { Mat m = mkMat(V3(0.20f, 0.40f, 0.06f), 0.3f, 0.8f, 2); set4(m.base2, V3(0.62f, 0.84f, 0.12f)); m.trans = 0.58f; set4(m.p0, 7, 0.5f, 0.5f, 34); set4(m.p1, 0.5f, 0.4f, 0.2f, 0.00015f); set4(m.p2, V3(0.52f, 0.46f, 0.14f), 0.55f); m.bump = 1; return m; }());
    mCilia = sc.addMat([] { Mat m = mkMat(V3(0.9f, 0.92f, 0.86f), 0.4f, 0.4f, 0); set4(m.base2, V3(1.0f, 1.0f, 0.92f)); m.trans = 0.6f; set4(m.sh, V3(0.85f)); return m; }());
    mShoot = sc.addMat([] { Mat m = mkMat(V3(0.36f, 0.50f, 0.16f), 0.4f, 0.6f, 4); set4(m.base2, V3(0.42f, 0.56f, 0.18f)); m.trans = 0.3f; set4(m.p0, 0.0f, 1.0f, 40, 0); set4(m.p2, V3(0.5f, 0.3f, 0.2f), 0.2f); return m; }());
    mWater = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 1.0f, 0); m.type = MT_DIEL; m.ior = 1.33f; set4(m.sh, V3(0.9f)); return m; }());
    mStreak = sc.addMat([] { Mat m = mkMat(V3(0.16f, 0.175f, 0.18f), 0.0f, 0.0f, 11); m.type = MT_THIN; m.ior = 1.0f; m.trans = 0; set4(m.sh, V3(1.0f)); return m; }());
    mLeafWet = sc.addMat([] { Mat m = sc_dummy(); return m; }());
    { Mesh dm; addBlob(dm, V3(0), V3(1), 16, 24, mWater); dropB = sc.addBlas(dm); }
    TW.clear();
    Twig a; a.rad = 0.0015f; a.detail = 2; a.seed = 1;
    a.ctrl = {V3(-0.20f, -0.050f, 0.010f), V3(-0.11f, -0.022f, 0.0f), V3(-0.035f, -0.010f, 0.004f), V3(0.04f, 0.012f, 0.0f), V3(0.11f, 0.022f, -0.006f), V3(0.19f, 0.050f, -0.012f)};
    a.buds = {{0.20f, -1, 0.10f, 1.05f, 11, 3}, {0.36f, 1, -0.02f, 0.93f, 12, 3}, {0.535f, -1, 0.0f, 0.90f, 13, 4}, {0.70f, 1, 0.12f, 1.08f, 14, 3}, {0.86f, -1, 0.05f, 1.0f, 15, 3}, {1.0f, 0, -0.04f, 0.92f, 16, 4}};
    TW.push_back(a);
    Rng r(8);
    for (int k = 0; k < 5; k++) {
        Twig t; t.rad = 0.0022f; t.detail = 0; t.seed = 20 + k;
        V3 b(r.r(-0.35f, 0.1f), r.r(-0.16f, 0.12f), -r.r(0.22f, 0.7f)); V3 d = norm(V3(r.r(0.5f, 1), r.r(-0.2f, 0.6f), r.r(-0.3f, 0.3f)));
        for (int i = 0; i < 5; i++) t.ctrl.push_back(b + d * (0.09f * i) + V3(0, 0.012f * std::sin(i * 1.5f + k), 0.01f * std::cos(i + k)));
        for (int i = 0; i < 5; i++) t.buds.push_back({0.2f + 0.2f * i, (i & 1) ? 1 : -1, r.r(-0.1f, 0.2f), r.r(0.85f, 1.1f), (uint32_t)(100 + k * 10 + i), 3});
        TW.push_back(t);
    }
}

static void budGeom(Mesh& m, Mesh& hm, V3 org, V3 ax, V3 side, V3 away, const Bud& b, float g, int frame, int detail, float twigR) {
    float Lb = 0.019f * (1 + 0.15f * ease(0.0f, 0.25f, g));
    float openE = ease(0.14f, 0.62f, g);
    float shootL = envf("SHL", 0.031f) * ease(0.3f, 1.0f, g);
    int N = detail == 2 ? 13 : 8;
    V3 up(0, 1, 0);
    V3 w3 = norm(cross(ax, side));
    Rng r(b.seed * 7);
    // scales
    for (int i = 0; i < N; i++) {
        float fi = (float)i / (N - 1); float phi = i * 2.39996f + b.seed;
        V3 rad = side * std::cos(phi) + w3 * std::sin(phi);
        float hr = hash1(b.seed * 13 + i);
        float a = 0.05f + openE * (0.30f + 0.55f * hr) * (0.6f + 0.6f * fi) * (1 - envf("SCTW", 0.72f) * smooth(-0.1f, 0.8f, dot(rad, -away)));   // scales on the twig's side cannot fold back through it
        Blade bl; bl.len = Lb * (0.34f + 0.72f * fi) * (1 + 0.25f * fi * openE) * (1 - envf("SCIN", 0.2f) * smooth(0.8f, 1.0f, fi) * openE); /* the innermost scales stay short of the leaves */ bl.width = 0.0034f * (1.1f - 0.25f * fi) * (1 + 0.1f * ease(0, 0.25f, g)); bl.maxAt = 0.5f; bl.q = 0.42f; bl.baseW = 0.6f;
        bl.cup = 2.3f - 1.5f * openE * fi; bl.fold = 0; bl.curl = -0.42f * fi * (1 - openE) + (0.2f + 0.8f * hr) * openE * fi; bl.wave = 0.15f * openE; bl.waveFreq = 3; bl.wavePhase = i; bl.curlPow = 1.6f; bl.nu = detail == 2 ? 16 : 8; bl.nv = detail == 2 ? 4 : 2; bl.noise = 0.0002f; bl.seed = b.seed + i;
        V3 Y = norm(ax * std::cos(a) + rad * std::sin(a)), Z = norm(-rad * std::cos(a) + ax * std::sin(a)), X = cross(Y, Z);
        V3 o = org + ax * (Lb * (0.02f + 0.42f * std::pow(fi, 1.3f))) + rad * (0.0006f + 0.0010f * (1 - fi));
        addBlade(m, bl, M34::fromBasis(X, Y, Z, o), mScale, hr, fi * 0.8f + 0.5f * openE * fi);
        if (gChk && detail == 2) chkSample(gChkScale, (int)b.seed, i, bl, M34::fromBasis(X, Y, Z, o), 0.4f);
    }
    // new shoot
    V3 tipDir = norm(ax + up * (away.y > 0 ? envf("TIPU", 0.0f) : 0.35f));   // shoots above the twig follow the bud, shoots below it level off
    std::vector<V3> sp = {org + ax * (Lb * 0.3f), org + ax * (Lb * 0.5f) + tipDir * (shootL * 0.5f), org + ax * (Lb * 0.5f) + tipDir * shootL};
    if (shootL > 0.001f) addTube(m, spline(sp, 8), std::vector<float>{0.0011f, 0.0011f, 0.001f, 0.001f, 0.0009f, 0.0009f, 0.0008f, 0.0007f}, 7, mShoot, 0.1f);
    // leaves
    for (int k = 0; k < b.nleaf; k++) {
        float e = ease(0.24f + 0.07f * k, 0.86f + 0.045f * k, g);
        if (e <= 0.001f) continue;
        // Leaves stand alternately in front of and behind the twig's plane (never across the twig), point forward along the shoot,
        // and droop by bending along the petiole and the midrib (gravity sag), not by a hinge at the petiole.
        float sgn = ((k + (b.side > 0 ? 1 : 0)) & 1) ? 1.f : -1.f;
        float kf = (k + 1.f) / b.nleaf;
        V3 node = org + ax * (Lb * 0.5f) + tipDir * (shootL * (envf("NODE0", 0.42f) + (1 - envf("NODE0", 0.42f)) * kf));
        float hr = hash1(b.seed * 17 + k), hq = hash1(b.seed * 29 + k * 3 + 1);
        float cph = g * 2 * PI * 3.3f + b.seed + k;
        float jx = (hash1(frame * 131 + b.seed + k) - 0.5f) * 0.05f;
        V3 zc(0, 0, 1);
        float km = b.nleaf > 1 ? (float)k / (b.nleaf - 1) : 0.5f; if (tipDir.y < 0) km = 1 - km;   // fan: leaves further up the shoot are held higher
        V3 hd = tipDir * (0.50f + 0.45f * kf) + zc * (sgn * (0.74f - 0.16f * kf)) + away * 0.30f + side * (0.35f * (hr - 0.5f) + 0.05f * std::sin(cph) + jx);
        hd.y = 0; hd = norm(hd);                                        // horizontal heading of the blade
        float limp = std::sin(PI * std::pow(e, 0.75f));                  // freshly out: hangs limp; later stiffens and lifts
        float el = ((away.y > 0 ? envf("EL0U", -0.30f) : envf("EL0D", -0.55f)) + (away.y > 0 ? envf("EL1U", 0.66f) : envf("EL1D", 0.90f)) * km + 0.22f * (hq - 0.5f) + 0.12f * away.y) - envf("ELL", 0.36f) * limp * (0.7f + 0.3f * hr);   // elevation of the blade base (rad)
        float sag = envf("SAG0", 0.34f) + envf("SAGK", 0.22f) * (1 - km) + 0.06f * hr + envf("SAGL", 0.62f) * limp;   // lower leaves hang more: same-side leaves never cross                   // bend along the midrib (rad at the tip)
        uint32_t leafId = b.seed * 8 + k; float lastHit = 99;
        if (gRain) { float dip = leafDip(leafId, gRainT, &lastHit); el -= 0.10f + 0.55f * dip; sag += 0.14f + 0.40f * dip; }
        V3 tgt = hd * std::cos(el) + up * std::sin(el);
        V3 Y = norm(lerp(tipDir, tgt, ease(0.0f, 0.55f, e)));
        V3 zp = zc - Y * dot(zc, Y);                                     // tilt the upper face away from the camera: the lit underside shows
        V3 Zr = up - zp * ((sgn > 0 ? envf("ROLLF", 0.15f) : envf("ROLLB", 0.55f)) * e) - hd * (0.5f * (1 - e));
        V3 Z = norm(Zr - Y * dot(Zr, Y));
        V3 X = cross(Y, Z);
        float len = 0.047f * (0.22f + 0.78f * e) * (0.85f + 0.3f * hr);
        Blade bl; bl.len = len; bl.width = len * 0.30f * (0.45f + 0.55f * e); bl.maxAt = 0.47f; bl.q = 0.78f; bl.baseW = 0.12f; bl.tipQ = 0.3f;
        bl.fold = 1.15f * (1 - ease(0.1f, 0.85f, e)) + 0.10f; bl.pleat = len * 0.085f * (1 - 0.82f * ease(0.3f, 1.0f, e)); bl.pleatN = 7; bl.pleatSlope = 0.5f;
        bl.curl = sag; bl.curlPow = 1.25f; bl.cup = 0.25f; bl.wave = 0.035f; bl.waveFreq = 5; bl.wavePhase = k + b.seed; bl.twist = 0.35f * (hr - 0.5f);
        bl.nu = detail == 2 ? 56 : 14; bl.nv = detail == 2 ? 12 : 3; bl.noise = 0.0005f * e; bl.seed = b.seed * 3 + k;
        // petiole: leaves the shoot at an acute angle and curves into the blade's base direction
        float Lp = 0.004f + envf("LP", 0.008f) * e;
        V3 pt0 = norm(tipDir * 0.8f + Y * 0.6f); V3 pc = node + pt0 * (Lp * 0.5f); V3 pe = pc + Y * (Lp * 0.55f);
        { std::vector<V3> pp; std::vector<float> pr; for (int i = 0; i <= 6; i++) { float q = i / 6.f; pp.push_back(node * ((1 - q) * (1 - q)) + pc * (2 * q * (1 - q)) + pe * (q * q)); pr.push_back(0.00064f - 0.00018f * q); }
          addTube(m, pp, pr, 6, mShoot, 0.2f); }
        M34 xf = M34::fromBasis(X, Y, Z, pe);
        if (gChk && detail == 2) chkSample(gChkLeaf, (int)b.seed, k, bl, xf, 0.06f);
        addBlade(m, bl, xf, gRain ? mLeafWet : mLeaf, 0.21f * (b.seed + k), 1 - e);
        if (gRain && detail == 2) {
            Rng dr(leafId * 5 + 3);
            for (int q = 0; q < 16; q++) { float u = dr.r(0.12f, 0.95f), v = dr.r(-0.8f, 0.8f); float rr = dr.r(0.0003f, 0.0011f); V3 P, N; bladeSample(bl, xf, u, v, P, N); if (N.y < 0) N = -N; gLeafDrops.push_back({P, N, rr}); }
            V3 hp, hn; bladeSample(bl, xf, 0.3f + 0.5f * hash1(leafId * 3 + (uint32_t)(gRainT / 1.1f)), 0.3f * (hash1(leafId * 9) - 0.5f), hp, hn);
            if (lastHit < 0.4f) gSplash.push_back({hp, lastHit});
        }
        if (detail == 2) { // silky cilia along the margin
            BladePts mid = bladeMidrib(bl); Rng hr2(b.seed * 31 + k);
            int nc = 170;
            for (int c = 0; c < nc; c++) {
                float u = hr2.r(0.06f, 0.99f); int i = std::min(bl.nu - 1, (int)(u * (bl.nu - 1))); float sv = hr2.f() < 0.5f ? -1.f : 1.f;
                V3 p1 = xf.pt(bladePoint(bl, mid, i, sv)), p0 = xf.pt(bladePoint(bl, mid, i, sv * 0.85f));
                V3 d = norm(norm(p1 - p0) + Y * 0.5f + hr2.dir() * 0.35f); float L = hr2.r(0.0012f, 0.0026f) * (1.2f - 0.4f * e);
                float rn = hr2.f();
                hm.seg(p1, 0.000007f, p1 + d * (L * 0.5f), 0.000006f, mCilia, 0, 0.5f, rn); hm.seg(p1 + d * (L * 0.5f), 0.000006f, p1 + d * L + V3(0, -L * 0.15f, 0), 0.000003f, mCilia, 0.5f, 1, rn);
            }
        }
    }
    (void)twigR;
}

static void twigGeom(Mesh& m, Mesh& hm, const Twig& t, float tt, int frame) {
    int n = t.detail == 2 ? 120 : 40;
    std::vector<V3> p = spline(t.ctrl, n); std::vector<float> rr(n);
    for (int i = 0; i < n; i++) {
        float u = i / (n - 1.f); float zz = 0;
        for (size_t k = 0; k < t.buds.size(); k++) { const Bud& b = t.buds[k]; if (!b.side) continue;
            float prev = k > 0 ? t.buds[k - 1].s : 0.0f; float x = sat((u - prev) / std::max(b.s - prev, 1e-4f));
            if (u <= b.s && u >= prev) zz += b.side * 0.0045f * (x - 0.5f); }
        p[i].y += zz + 0.0006f * fbm(V3(u * 40, t.seed, 0), 2);
        float node = 0; for (const Bud& b : t.buds) node += std::exp(-(u - b.s) * (u - b.s) / 0.00006f);
        rr[i] = t.rad * (1.2f - 0.5f * u) * (1 + 0.28f * node) * (1 + 0.05f * fbm(V3(u * 90, t.seed, 3), 2));
    }
    addTube(m, p, rr, t.detail == 2 ? 16 : 7, mTwig, 0.1f * t.seed, 40);
    if (gChk && t.detail == 2) { gChkTwigP = p; gChkTwigR = rr; }
    for (const Bud& b : t.buds) {
        int i = std::min(n - 2, (int)(b.s * (n - 1)));
        V3 T = norm(p[std::min(n - 1, i + 1)] - p[std::max(0, i - 1)]);
        V3 nrm = norm(cross(V3(0, 0, 1), T)); // in-plane normal (up-ish)
        V3 ax = b.side == 0 ? T : norm(T * 0.72f + nrm * (0.68f * b.side) + V3(0, 0, 0.12f * std::sin(b.seed)));
        V3 side = norm(cross(ax, V3(0, 0, 1)));
        float g = sat((tt - b.t0) / (b.t1 - b.t0));
        V3 away = b.side == 0 ? V3(0, 1, 0) : nrm * (float)b.side;
        budGeom(m, hm, p[i] + ax * (rr[i] * 0.6f), ax, side, away, b, g, frame, t.detail, rr[i]);
    }
}

static void frame(Scene& sc, FrameCtx& fc) {
    float tt = (float)fc.f / (fc.nf - 1);
    Mesh m, hm;
    gChk = getenv("BUDCHECK") != nullptr; gChkLeaf.clear(); gChkScale.clear();
    for (const Twig& t : TW) twigGeom(m, hm, t, tt, fc.f);
    if (gChk) chkReport(fc.f);
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    if (!hm.s.empty()) sc.addInst(sc.addBlas(hm), M34::identity(), 0);
    Camera& c = fc.cam; Env& e = fc.env;
    float ce = ease(0, 1, tt);
    c.pos = lerp(V3(-0.03f, -0.06f, 0.52f), V3(0.035f, -0.075f, 0.47f), ce);
    c.target = lerp(V3(0.0f, -0.004f, 0), V3(0.012f, -0.014f, 0), ce);
    c.focal = 0.1f; c.fstop = 5.6f; c.focusDist = len(V3(0.01f, -0.01f, 0.004f) - c.pos);
    float az = -0.8f + 0.4f * tt;
    e.sunDir = norm(V3(std::sin(az), 0.62f, -std::cos(az)));
    float cloud = 1 - 0.5f * (smooth(0.42f, 0.48f, tt) * smooth(0.62f, 0.56f, tt));
    float fl = 1 + 0.05f * (hash1(fc.f * 911 + 5) - 0.5f);
    e.sunE = V3(6.4f, 5.5f, 4.2f) * cloud * fl; e.sunAngR = 0.03f;
    e.type = 2; e.bd0 = V3(0.05f, 0.04f, 0.025f); e.bd1 = V3(0.10f, 0.12f, 0.04f); e.bdScale = 3.5f; e.bdMix = 1; e.bdP0 = -0.3f; e.bdP1 = 0.42f; e.bdP2 = 15; e.bdP3 = 0.7f; e.bdGap = 0.1f;
    e.zen = V3(0.10f, 0.26f, 0.70f) * 1.25f; e.hor = V3(0.52f, 0.66f, 0.30f) * 1.0f; e.gnd = V3(0.016f, 0.026f, 0.012f); e.horExp = 0.5f;
    e.glow = 0.04f; e.glowExp = 20; e.eps = 4e-6f; e.clampL = 16;
    Rng r(33);
    for (int i = 0; i < 26; i++) {
        V3 p(r.r(-1.2f, 1.2f), r.r(-0.5f, 0.9f), r.r(-4.0f, -1.2f));
        float fl2 = 0.65f + 0.35f * std::sin(tt * 2 * PI * r.r(0.5f, 2.5f) + r.r(0, 6.28f));
        float br = r.f(); br = 0.08f + 0.92f * br * br * br; float cm = r.f();
        V3 col = (cm < 0.5f ? lerp(V3(1.0f, 0.9f, 0.62f), V3(1.0f, 0.97f, 0.85f), cm * 2) : lerp(V3(0.55f, 0.8f, 0.3f), V3(0.8f, 0.95f, 0.6f), cm * 2 - 1)) * (br * 0.9e-3f * fl2 * cloud);
        float dz = p.z - c.pos.z; sc.addGlint(p, col * (dz * dz), r.r(0.05f, 0.22f));
    }
}
static void frameRain(Scene& sc, FrameCtx& fc) {
    float t = (float)fc.t; float tt = (float)fc.f / (fc.nf - 1);
    gRain = true; gRainT = t; gSplash.clear(); gLeafDrops.clear();
    Mesh m, hm, dm;
    std::vector<Twig> tw = TW;
    for (Twig& w : tw) for (size_t i = 0; i < w.ctrl.size(); i++) w.ctrl[i].y += 0.0018f * std::sin(t * 2.3f + i * 0.9f + w.seed) * (i / (float)w.ctrl.size()) - 0.004f * (i / (float)w.ctrl.size());
    gChk = getenv("BUDCHECK") != nullptr; gChkLeaf.clear(); gChkScale.clear();
    for (const Twig& w : tw) twigGeom(m, hm, w, 3.0f, 0);
    if (gChk) chkReport(fc.f);
    gRain = false;
    for (const LeafDrop& d : gLeafDrops) addDrop(dm, d.p, d.n, d.r, 0.55f, mWater, 8);
    // drips hanging from the twig
    Rng r(5);
    for (int k = 0; k < 7; k++) { float u = r.r(0.1f, 0.95f); V3 p = splineAt(tw[0].ctrl, u) + V3(0, -0.0016f, 0); float per = r.r(1.6f, 3.5f); float ph = fract((t + r.r(0, 3)) / per);
        float rad = 0.0009f + 0.0011f * std::pow(ph, 0.5f); addBlob(dm, p + V3(0, -rad * 0.75f, 0), V3(rad, rad * 1.15f, rad), 12, 16, mWater);
        float tf = ph * per; if (tf < 0.25f) { V3 q0 = p + V3(0, -0.004f - 4.9f * tf * tf, 0), q1 = p + V3(0, -0.004f - 4.9f * (tf + 0.008f) * (tf + 0.008f), 0); M34 a0 = M34::translate(q0) * M34::scale(0.0016f), a1 = M34::translate(q1) * M34::scale(0.0016f); sc.addInst(dropB, a0, 0, V3(1), 0, &a1); } }
    // splash droplets
    for (const Splash& sp : gSplash) { Rng sr((uint32_t)(sp.p.x * 1e6f)); for (int k = 0; k < 9; k++) { V3 v = norm(V3(sr.r(-1, 1), sr.r(0.6f, 1.6f), sr.r(-1, 1))) * sr.r(0.25f, 0.7f); float ta = sp.t0, tb = sp.t0 + 0.006f;
        V3 q0 = sp.p + v * ta + V3(0, -4.9f * ta * ta, 0), q1 = sp.p + v * tb + V3(0, -4.9f * tb * tb, 0); float rr = sr.r(0.00015f, 0.00045f);
        M34 a0 = M34::translate(q0) * M34::scale(rr), a1 = M34::translate(q1) * M34::scale(rr); sc.addInst(dropB, a0, 0, V3(1), 0, &a1); } }
    // rain streaks (one shutter's worth of fall drawn as a faint line)
    Mesh rm; Rng rr(99);
    for (int k = 0; k < 800; k++) {
        V3 p0(rr.r(-0.45f, 0.45f), 0, rr.r(-1.1f, 0.42f)); float sp = rr.r(5.0f, 8.0f); float h = 0.9f; float y = h * (1 - fract((t * sp / h) + rr.f())) - 0.4f;
        float L = sp / 48.0f * 0.5f; V3 w(0.10f, -1, 0.02f);
        float rs = rr.r(0.00015f, 0.00042f); rm.seg(p0 + V3(0, y, 0), rs, p0 + V3(0, y, 0) + w * L, rs, mStreak, 0, 1, rr.f());
    }
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    if (!hm.s.empty()) sc.addInst(sc.addBlas(hm), M34::identity(), 0);
    if (!dm.t.empty()) sc.addInst(sc.addBlas(dm), M34::identity(), 0);
    sc.addInst(sc.addBlas(rm), M34::identity(), 0, V3(1), 0, nullptr, IF_NOSHADOW);
    Camera& c = fc.cam; Env& e = fc.env;
    c.pos = lerp(V3(0.02f, -0.07f, 0.44f), V3(0.03f, -0.072f, 0.42f), ease(0, 1, tt));
    c.target = V3(0.02f, -0.022f, 0); c.focal = 0.1f; c.fstop = 4.5f; c.focusDist = len(V3(0.02f, -0.02f, 0.004f) - c.pos);
    e.sunDir = norm(V3(-0.3f, 0.8f, -0.4f)); e.sunE = V3(0.5f, 0.5f, 0.48f); e.sunAngR = 0.25f;
    e.type = 2; e.bd0 = V3(0.05f, 0.05f, 0.035f); e.bd1 = V3(0.10f, 0.13f, 0.06f); e.bdScale = 3.5f; e.bdMix = 1; e.bdP0 = -0.3f; e.bdP1 = 0.7f; e.bdP2 = 15; e.bdP3 = 0.92f; e.bdGap = 0.05f;
    e.zen = V3(0.62f, 0.66f, 0.70f) * 1.7f; e.hor = V3(0.34f, 0.42f, 0.36f) * 1.0f; e.gnd = V3(0.02f, 0.03f, 0.02f); e.horExp = 0.5f;
    e.eps = 4e-6f; e.clampL = 14;
}
} // namespace bud
