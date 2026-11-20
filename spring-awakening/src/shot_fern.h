// shot_fern.h — time-lapse: fern croziers (fiddleheads) uncoil on the forest floor.
#pragma once
#include "plants.h"

namespace fern {
struct Frond { V3 base; float az, lean, L, t0, t1; uint32_t seed; int detail; };
static Soil soil; static Litter litter; static Moss moss;
static std::vector<Frond> F;
static int mRachis, mPinna, mScale, mFuzz, mCrown;
static BlasH decor;
struct MossPt { V3 p, n; float h; uint32_t seed; }; static std::vector<MossPt> mossPts;
static float ground(float x, float z) { return 0.015f * fbm(V3(x * 4, 0.3f, z * 4), 3) + 0.003f * fbm(V3(x * 30, 1.3f, z * 30), 3) + 0.02f * std::exp(-(x * x + z * z) / 0.01f); }

static void init(Scene& sc) {
    soil = Soil(); soil.height = ground;
    soilInit(sc, soil, 1.2f, -2.2f, 0.6f, 260, V3(0.11f, 0.07f, 0.045f), V3(0.04f, 0.026f, 0.018f));
    sc.mats[soil.matBase].p0[3] = 0.5f; sc.mats[soil.matBase + 1].p0[3] = 0.45f;
    soilScatter(soil, 50000, -0.5f, 0.5f, -0.8f, 0.45f, 0.0006f, 0.005f, 21, [](float x, float z) { return 0.15f + 0.85f * smooth(0.5f, 0.1f, std::sqrt(x * x + z * z)); }, 2.6f);
    litterInit(sc, litter, 8);
    litterScatter(litter, 2200, -1.0f, 1.0f, -1.9f, 0.5f, ground, 34);
    mossInit(sc, moss, V3(0.035f, 0.085f, 0.015f), V3(0.22f, 0.36f, 0.05f), 4);
    mRachis = sc.addMat([] { Mat m = mkMat(V3(0.30f, 0.17f, 0.07f), 0.4f, 0.7f, 4); set4(m.base2, V3(0.34f, 0.50f, 0.12f)); m.trans = 0.25f; set4(m.p0, 0.05f, 0.45f, 40, 0); set4(m.p2, V3(0.2f, 0.1f, 0.04f), 0.35f); return m; }());
    mPinna = sc.addMat([] { Mat m = mkMat(V3(0.16f, 0.36f, 0.05f), 0.35f, 0.8f, 2); set4(m.base2, V3(0.50f, 0.74f, 0.08f)); m.trans = 0.5f; set4(m.p0, 13, 0.25f, 0.45f, 0); set4(m.p1, 0.0f, 0.35f, 0.25f, 0.0001f); set4(m.p2, V3(0.5f, 0.5f, 0.12f), 0.6f); m.bump = 1; return m; }());
    mScale = sc.addMat([] { Mat m = mkMat(V3(0.26f, 0.14f, 0.055f), 0.5f, 0.3f, 0); set4(m.base2, V3(0.55f, 0.30f, 0.10f)); m.trans = 0.45f; set4(m.sh, V3(0.75f, 0.6f, 0.45f)); return m; }());
    mFuzz = sc.addMat([] { Mat m = mkMat(V3(0.80f, 0.72f, 0.55f), 0.5f, 0.3f, 0); set4(m.base2, V3(1.0f, 0.92f, 0.7f)); m.trans = 0.6f; set4(m.sh, V3(0.85f, 0.8f, 0.7f)); return m; }());
    mCrown = sc.addMat([] { Mat m = mkMat(V3(0.10f, 0.06f, 0.035f), 0.8f, 0.3f, 1); set4(m.base2, V3(0.04f, 0.025f, 0.015f)); m.scale = 120; m.bump = 1; set4(m.p0, 0.002f, 0.0003f, 0, 0); return m; }());
    F.clear();
    auto crown = [&](V3 c, int n, float L, float t0, uint32_t seed, int detail) {
        Rng r(seed);
        for (int i = 0; i < n; i++) { float az = i * 2.4f + r.r(-0.3f, 0.3f) + seed; float rr = 0.012f + 0.012f * r.f();
            V3 b = c + V3(std::cos(az) * rr, 0, std::sin(az) * rr); b.y = ground(b.x, b.z) + 0.004f;
            F.push_back({b, az, r.r(0.10f, 0.26f), L * r.r(0.8f, 1.1f), t0 + r.r(-0.12f, 0.16f), t0 + r.r(0.85f, 1.0f), seed * 100 + i, detail}); }
    };
    crown(V3(0, 0, 0), 6, 0.34f, 0.0f, 3, 2);
    crown(V3(-0.36f, 0, -0.34f), 5, 0.36f, 0.05f, 7, 0);
    crown(V3(0.40f, 0, -0.46f), 5, 0.38f, -0.03f, 9, 0);
    crown(V3(0.08f, 0, -0.85f), 6, 0.4f, 0.0f, 11, 0);
    crown(V3(-0.7f, 0, -1.0f), 6, 0.4f, 0.0f, 13, 0);
    crown(V3(0.75f, 0, -1.2f), 6, 0.4f, 0.0f, 15, 0);
    // hero frond faces the camera plane nicely
    F[0].az = 0.25f; F[0].lean = 0.14f; F[0].t0 = -0.02f; F[0].t1 = 0.94f;
    F[1].az = 2.75f; F[1].lean = 0.2f; F[1].t0 = 0.06f; F[1].t1 = 1.02f;
    Rng r(12); Mesh m;
    // crown stumps + old stipe stubs
    for (V3 c : {V3(0, 0, 0), V3(-0.36f, 0, -0.34f), V3(0.40f, 0, -0.46f), V3(0.08f, 0, -0.85f)}) {
        c.y = ground(c.x, c.z) + 0.004f;
        addBlob(m, c, V3(0.035f, 0.022f, 0.035f), 24, 36, mCrown, 0, [](V3 d) { return 0.9f + 0.3f * fbm(d * 3, 3); });
        for (int k = 0; k < 2500; k++) { V3 d = r.dir(); if (d.y < -0.1f) continue; V3 q = c + d * V3(0.035f, 0.022f, 0.035f) * 0.95f; V3 e = norm(d + r.dir() * 0.6f); float L = r.r(0.003f, 0.007f);
            m.seg(q, 0.00012f, q + e * L, 0.00003f, mScale, 0, 1, r.f()); }
    }
    decor = sc.addBlas(m);
    mossPts.clear();
    for (int k = 0; k < 60000; k++) { float x = r.r(-0.6f, 0.6f), z = r.r(-1.0f, 0.4f); float dn = fbm(V3(x * 5, 3, z * 5), 3) + 0.25f * std::exp(-(x * x + z * z) / 0.03f); if (dn < 0.08f) continue;
        float d = std::sqrt(x * x + z * z); if (r.f() > smooth(0.9f, 0.15f, d) + 0.08f) continue;
        mossPts.push_back({V3(x, ground(x, z) + 0.002f, z), V3(0, 1, 0), 0.006f + 0.006f * sat(dn * 2), (uint32_t)k}); }
}

static void frond(Mesh& m, Mesh& hm, const Frond& fr, float g, int frame) {
    float f = 0.22f + 0.78f * std::pow(ease(0.02f, 0.97f, g), 1.35f);         // unrolled fraction
    float L = fr.L * (0.36f + 0.64f * ease(0.0f, 0.95f, g));
    int n = fr.detail == 2 ? 120 : 50;
    float turns = 2.1f * std::pow(1 - f, 0.62f);
    float Theta = 2 * PI * turns;
    V3 out(std::cos(fr.az), 0, std::sin(fr.az));
    float jx = hash1(frame * 131 + fr.seed) - 0.5f, jz = hash1(frame * 173 + fr.seed) - 0.5f;
    float cph = g * 2 * PI * 3.7f + fr.seed;
    V3 side = cross(V3(0, 1, 0), out);
    V3 sway = side * (0.07f * std::sin(cph) + 0.02f * jx) + out * (0.03f * std::cos(cph * 1.3f) + 0.012f * jz);
    std::vector<V3> p(n); std::vector<float> rr(n); std::vector<V3> T(n), Nn(n);
    V3 q = fr.base; float ds = L / (n - 1);
    for (int i = 0; i < n; i++) {
        float u = (float)i / (n - 1);
        float th = fr.lean + 0.55f * f * u * u;                 // outward arch of the unrolled part
        if (u > f) { float x = (u - f) / (1 - f + 1e-4f); th -= Theta * (0.45f * x + 0.55f * x * x); }
        V3 t = norm(V3(0, 1, 0) * std::cos(th) + out * std::sin(th) + sway * std::pow(std::min(u, f), 1.5f));
        if (i > 0) q += t * ds;
        p[i] = q; T[i] = t; Nn[i] = norm(cross(side, t) * -1.0f);   // adaxial side (inside of the coil)
        rr[i] = fr.L * (0.0105f - 0.0075f * std::pow(u, 0.7f)) * (0.75f + 0.25f * g);
    }
    Frames frm; frm.T = T; frm.N = Nn; frm.B.resize(n); for (int i = 0; i < n; i++) frm.B[i] = cross(T[i], Nn[i]);
    addTube(m, p, rr, fr.detail == 2 ? 12 : 6, mRachis, 0.01f * fr.seed, 3, &frm);
    // pinnae
    int np = fr.detail == 2 ? 26 : 16;
    for (int k = 0; k < np; k++) for (int sgn = -1; sgn <= 1; sgn += 2) {
        float v = (k + (sgn > 0 ? 0.5f : 0.0f)) / np; float u = 0.22f + 0.77f * v;
        int i = std::min(n - 1, (int)(u * (n - 1)));
        float fp = sat((f - u + 0.05f) / 0.22f); fp = fp * fp * (3 - 2 * fp);
        float lp = fr.L * 0.23f * std::pow(std::sin(PI * std::pow(v, 0.62f)), 0.85f) * (0.22f + 0.78f * fp) * (0.75f + 0.25f * g);
        if (lp < 0.002f) continue;
        Blade b; b.len = lp; b.width = lp * 0.105f * (0.5f + 0.5f * fp); b.maxAt = 0.25f; b.q = 0.55f; b.baseW = 0.5f; b.serr = 0.62f; b.serrN = 13; b.fold = 0.35f - 0.2f * fp; b.cup = 0.3f;
        float pr = hash1(fr.seed * 31 + k * 2 + sgn + 5); lp *= 0.9f + 0.2f * pr;
        b.len = lp; b.curl = (0.2f + 0.45f * pr) * fp; b.curl2 = -6.5f * (1 - fp); b.curl2Pow = 1.15f; b.nu = fr.detail == 2 ? 46 : 14; b.nv = fr.detail == 2 ? 3 : 1; b.wave = 0.08f; b.waveFreq = 5; b.wavePhase = k;
        float beta = 0.22f + 0.2f * hash1(fr.seed * 37 + k * 2 + sgn) + 1.0f * (1 - fp);
        V3 B = frm.B[i] * (float)sgn;
        V3 Y = norm(B * std::cos(beta) + T[i] * std::sin(beta) + Nn[i] * (0.75f * (1 - fp) + 0.08f));
        V3 Z = norm(Nn[i] - Y * dot(Nn[i], Y)); V3 X = cross(Y, Z);
        addBlade(m, b, M34::fromBasis(X, Y, Z, p[i] + B * rr[i] * 0.6f), mPinna, 0.07f * (k * 2 + sgn + fr.seed), 1 - fp);
    }
    // chaffy scales on the stipe + pale fuzz on the coil
    if (fr.detail >= 1 || true) {
        Rng hr(fr.seed * 3 + 1);
        int ns = fr.detail == 2 ? 1500 : 200;
        addTubeHairs(hm, p, rr, ns, 0.0032f * (fr.L / 0.34f), fr.detail == 2 ? 0.00007f : 0.00016f, mScale, hr, 0.0f, 0.55f, 0.9f, 0.5f);
        int nf = fr.detail == 2 ? 3800 : 300;
        addTubeHairs(hm, p, rr, nf, 0.0026f * (fr.L / 0.34f), fr.detail == 2 ? 0.000028f : 0.00008f, mFuzz, hr, 0.25f, 1.0f, 0.6f, 0.2f);
    }
}

static void frame(Scene& sc, FrameCtx& fc) {
    float tt = (float)fc.f / (fc.nf - 1);
    soilEmit(sc, soil); litterEmit(sc, litter); sc.addInst(decor, M34::identity(), 0);
    for (const MossPt& mp : mossPts) { Rng r2(mp.seed * 7 + 1); mossAdd(sc, moss, mp.p, mp.n, mp.h, r2); }
    Mesh m, hm;
    for (const Frond& fr : F) { float g = sat((tt - fr.t0) / (fr.t1 - fr.t0)); frond(m, hm, fr, g, fc.f); }
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    sc.addInst(sc.addBlas(hm), M34::identity(), 0);
    Camera& c = fc.cam; Env& e = fc.env;
    float ce = ease(0, 1, tt);
    c.pos = lerp(V3(0.09f, 0.085f, 0.36f), V3(-0.06f, 0.21f, 0.98f), ce);
    c.target = lerp(V3(0.012f, 0.068f, 0), V3(0.02f, 0.185f, 0), ce);
    c.focal = 0.07f; c.fstop = 5.6f; c.focusDist = len(lerp(V3(0.02f, 0.07f, 0.01f), V3(0.04f, 0.18f, 0.01f), ce) - c.pos);
    float az = -0.7f + 0.45f * tt;
    e.sunDir = norm(V3(std::sin(az), 0.5f, -std::cos(az)));
    float cloud = 1 - 0.5f * (smooth(0.30f, 0.36f, tt) * smooth(0.50f, 0.44f, tt));
    float fl = 1 + 0.05f * (hash1(fc.f * 911 + 3) - 0.5f);
    e.sunE = V3(6.4f, 5.4f, 3.9f) * cloud * fl; e.sunAngR = 0.03f;
    e.type = 2; e.bd0 = V3(0.05f, 0.04f, 0.025f); e.bd1 = V3(0.10f, 0.12f, 0.04f); e.bdScale = 4.0f; e.bdMix = 1; e.bdP0 = -0.03f; e.bdP1 = 0.85f; e.bdP2 = 17; e.bdP3 = 0.93f; e.bdGap = 0.02f;
    e.zen = V3(0.16f, 0.30f, 0.60f) * 1.2f; e.hor = V3(0.50f, 0.62f, 0.22f) * 1.15f; e.gnd = V3(0.016f, 0.026f, 0.012f); e.horExp = 0.5f;
    e.glow = 0.03f; e.glowExp = 20; e.eps = 8e-6f; e.clampL = 16;
    Rng r(31);
    for (int i = 0; i < 30; i++) {
        V3 p(r.r(-1.4f, 1.4f), r.r(0.1f, 1.1f), r.r(-4.0f, -1.6f));
        float fl2 = 0.65f + 0.35f * std::sin(tt * 2 * PI * r.r(0.5f, 2.5f) + r.r(0, 6.28f));
        float br = r.f(); br = 0.08f + 0.92f * br * br * br; float cm = r.f();
        V3 col = (cm < 0.5f ? lerp(V3(1.0f, 0.9f, 0.62f), V3(1.0f, 0.97f, 0.85f), cm * 2) : lerp(V3(0.55f, 0.8f, 0.3f), V3(0.8f, 0.95f, 0.6f), cm * 2 - 1)) * (br * 1.0e-3f * fl2 * cloud);
        float dz = p.z - c.pos.z; sc.addGlint(p, col * (dz * dz), r.r(0.05f, 0.22f));
    }
}
} // namespace fern
