// plants.h — reusable plant parts: blades (leaves, petals, cotyledons), hairs, drops, soil.
#pragma once
#include "geom.h"

static inline float tri01(float x) { float f = x - std::floor(x); return 1 - std::fabs(2 * f - 1); } // 0..1..0

struct Blade {
    float len = 0.03f, width = 0.012f;   // metres (half-width is `width`)
    float maxAt = 0.4f, q = 0.8f;        // outline: sin(pi*u^p)^q with the maximum at u = maxAt
    float tipQ = 0;                      // extra tip sharpening
    float baseW = 0;                     // width fraction kept at the base (petiole junction)
    float fold = 0.15f, cup = 0.0f;      // V-fold and transverse cupping (rad)
    float curl = 0.0f, curlPow = 1.5f;   // longitudinal bend towards the lower face (rad at tip)
    float curl2 = 0, curl2Pow = 1;       // second bend term (e.g. hook at the tip)
    float twist = 0;
    float wave = 0, waveFreq = 4, wavePhase = 0;
    float serr = 0, serrN = 16;
    float pleat = 0, pleatN = 7, pleatSlope = 0.6f;
    float rollIn = 0;                    // margins rolled towards the upper face (rad at the margin)
    float thick = 0;                     // >0: closed lens-shaped body
    float noise = 0, noiseF = 3; uint32_t seed = 1;  // low-frequency surface warping (m)
    int nu = 24, nv = 10;
    float widthAt(float u) const {
        float p = std::log(0.5f) / std::log(clampf(maxAt, 0.05f, 0.95f));
        float s = std::sin(PI * std::pow(sat(u), p));
        float w = std::pow(std::max(s, 0.f), q);
        if (tipQ > 0) w *= std::pow(1 - std::pow(sat(u), 6.0f), tipQ);
        if (baseW > 0) w = std::max(w, baseW * (1 - sat(u * 4)));
        return width * w;
    }
};
// local frame: midrib along +Y from the origin, width along X, upper face +Z.
struct BladePts { std::vector<V3> C, T, Nn; };
static inline BladePts bladeMidrib(const Blade& b) {
    BladePts o; o.C.resize(b.nu); o.T.resize(b.nu); o.Nn.resize(b.nu);
    V3 c(0); float ds = b.len / (b.nu - 1);
    for (int i = 0; i < b.nu; i++) {
        float u = (float)i / (b.nu - 1);
        float a = b.curl * std::pow(u, b.curlPow) + b.curl2 * std::pow(u, b.curl2Pow);
        V3 t(0, std::cos(a), -std::sin(a));
        if (i > 0) { float um = ((float)i - 0.5f) / (b.nu - 1); float am = b.curl * std::pow(um, b.curlPow) + b.curl2 * std::pow(um, b.curl2Pow); c += V3(0, std::cos(am), -std::sin(am)) * ds; }
        o.C[i] = c; o.T[i] = t; o.Nn[i] = V3(0, std::sin(a), std::cos(a));
    }
    return o;
}
static inline V3 bladePoint(const Blade& b, const BladePts& m, int i, float v, float zoff = 0) {
    float u = (float)i / (b.nu - 1);
    float w = b.widthAt(u);
    float av = std::fabs(v);
    if (b.serr > 0 && av > 0.999f) { float sw = fract(u * b.serrN); w *= 1 - b.serr * (1 - sw) * smooth(0.02f, 0.15f, u) * smooth(1.0f, 0.9f, u); }
    float s = v * w;
    float ph = b.fold + b.cup * av * 0.5f + b.rollIn * av * av * av;
    float dx = s * std::cos(ph), dz = std::fabs(s) * std::sin(ph);
    if (b.wave != 0) dz += b.wave * w * std::pow(av, 1.5f) * std::sin(2 * PI * b.waveFreq * u + b.wavePhase + (v > 0 ? 0.f : 1.7f));
    if (b.pleat != 0) dz += b.pleat * (tri01((u - b.pleatSlope * av) * b.pleatN) - 0.5f) * smooth(0.0f, 0.25f, av) * smooth(0.0f, 0.1f, u) * smooth(1.0f, 0.85f, u);
    if (b.noise != 0) dz += b.noise * fbm(V3(u * b.noiseF, v * b.noiseF * 0.6f, (float)b.seed * 3.7f), 3);
    dz += zoff;
    float tw = b.twist * u; float c = std::cos(tw), sn = std::sin(tw);
    float x2 = dx * c - dz * sn, z2 = dx * sn + dz * c;
    return m.C[i] + V3(1, 0, 0) * x2 + m.Nn[i] * z2;
}
// attr = attrFn(u) or constant. UV = (u, v in [-1,1]).
static inline void addBlade(Mesh& m, const Blade& b, const M34& xf, uint32_t mat, float rnd = 0, float attr = 0, std::function<float(float, float)> attrFn = nullptr) {
    BladePts mid = bladeMidrib(b);
    int nu = b.nu, nv = b.nv * 2 + 1;
    auto build = [&](float side) {
        std::vector<V3> P(nu * nv); std::vector<V2> UV(nu * nv); std::vector<float> A(nu * nv);
        for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
            float u = (float)i / (nu - 1), v = (float)j / (nv - 1) * 2 - 1;
            float zo = 0;
            if (b.thick > 0) zo = side * 0.5f * b.thick * std::pow(std::max(0.f, 1 - v * v), 0.6f) * std::pow(std::sin(PI * std::pow(u, 0.7f)), 0.5f);
            P[i * nv + j] = xf.pt(bladePoint(b, mid, i, v, zo));
            UV[i * nv + j] = V2(u, v); A[i * nv + j] = attrFn ? attrFn(u, v) : attr;
        }
        addGrid(m, nu, nv, P, &UV, &A, mat, false, false, rnd, side < 0);
    };
    if (b.thick > 0) { build(1); build(-1); } else build(1);
}
// sample a point + frame on a blade (for attaching drops / hairs): returns position, normal out
static inline void bladeSample(const Blade& b, const M34& xf, float u, float v, V3& P, V3& N) {
    BladePts mid = bladeMidrib(b);
    int i = std::min(b.nu - 2, std::max(1, (int)(u * (b.nu - 1))));
    V3 p = bladePoint(b, mid, i, v), pu = bladePoint(b, mid, i + 1, v), pv = bladePoint(b, mid, i, v + 0.05f);
    P = xf.pt(p); N = norm(xf.dir(cross(pv - p, pu - p))); if (dot(N, xf.dir(mid.Nn[i])) < 0) N = -N;
}

// hairs standing off a tube path
static inline void addTubeHairs(Mesh& m, const std::vector<V3>& p, const std::vector<float>& r, int count, float hlen, float hrad, uint32_t mat, Rng& rg,
                                float u0 = 0, float u1 = 1, float lean = 0.3f, float droop = 0.0f) {
    int n = (int)p.size(); Frames f = ptFrames(p);
    for (int k = 0; k < count; k++) {
        float u = rg.r(u0, u1) * (n - 1); int i = std::min(n - 2, (int)u); float fr = u - i;
        V3 c = lerp(p[i], p[i + 1], fr); float rr = lerp(r[i], r[i + 1], fr);
        float a = rg.r(0, 2 * PI); V3 out = f.N[i] * std::cos(a) + f.B[i] * std::sin(a);
        V3 d = norm(out + f.T[i] * rg.r(-lean, lean) + rg.dir() * 0.25f);
        float L = hlen * rg.r(0.5f, 1.2f);
        V3 a0 = c + out * rr * 0.9f, a1 = a0 + d * L * 0.5f, a2 = a1 + norm(d + V3(0, -droop, 0) + rg.dir() * 0.2f) * L * 0.5f;
        float rn = rg.f();
        m.seg(a0, hrad, a1, hrad * 0.7f, mat, 0, 0.5f, rn); m.seg(a1, hrad * 0.7f, a2, hrad * 0.15f, mat, 0.5f, 1, rn);
    }
}

// sessile drop: spherical cap sitting on a surface point with normal n (contact angle sets the cap height)
static inline void addDrop(Mesh& m, V3 p, V3 n, float radius, float flat, uint32_t mat, int res = 20) {
    // sphere of radius R sunk so that the footprint radius is `radius`; flat in (0,1]: cap height / R
    float R = radius / std::sqrt(std::max(1e-4f, 1 - (1 - flat) * (1 - flat)));
    V3 c = p - n * (R * (1 - flat));
    M34 fr = frameY(c, n, 1);
    float thMax = std::acos(clampf(1 - flat, -1.f, 1.f)) + 0.12f; // slightly below the surface
    int nu = res, nv = res * 2;
    std::vector<V3> P(nu * nv);
    for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        float th = thMax * i / (nu - 1), ph = 2 * PI * j / nv;
        P[i * nv + j] = fr.pt(V3(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph)) * R);
    }
    addGrid(m, nu, nv, P, nullptr, nullptr, mat, false, true, 0, true);
}
// full sphere
static inline void addSphere(Mesh& m, V3 c, float r, uint32_t mat, int res = 16, float rnd = 0) { addBlob(m, c, V3(r), res, res * 2, mat, rnd); }

// ---------------- soil ----------------
struct SoilItem { V3 p; float s; int kind; V3 axis; float ang; float rnd; V3 tint; };
struct Soil {
    BlasH base; BlasH crumb[8]; BlasH fiber;
    int matBase = 0; // +0 ground, +1 crumbs
    std::vector<SoilItem> items;
    std::function<float(float, float)> height;
};
static inline float soilHeightDefault(float x, float z) {
    return 0.006f * fbm(V3(x * 14, 0.3f, z * 14), 4) + 0.0012f * fbm(V3(x * 90, 1.3f, z * 90), 3);
}
static inline void soilInit(Scene& sc, Soil& so, float halfX, float z0, float z1, int res, V3 colA, V3 colB, uint32_t seed = 7) {
    if (!so.height) so.height = soilHeightDefault;
    so.matBase = (int)sc.mats.size();
    Mat g = mkMat(colA, 0.75f, 0.6f, 1); set4(g.base2, colB); g.scale = 60; g.bump = 1; g.p0[0] = 0.0012f; g.p0[1] = 0.00012f; sc.addMat(g);
    Mat c = g; c.scale = 220; c.p0[0] = 0.0006f; c.p0[1] = 0.00008f; c.p0[2] = 1; sc.addMat(c);
    Mesh m;
    { int nx = res, nz = (int)(res * (z1 - z0) / (2 * halfX)); nz = std::max(nz, 8); std::vector<V3> P(nx * nz);
      for (int i = 0; i < nx; i++) for (int j = 0; j < nz; j++) { float x = (i / (nx - 1.f) * 2 - 1) * halfX, z = z0 + (z1 - z0) * j / (nz - 1.f); P[i * nz + j] = V3(x, so.height(x, z), z); }
      addGrid(m, nx, nz, P, nullptr, nullptr, 0, false, false, 0, true); so.base = sc.addBlas(m); m.clear(); }
    for (int k = 0; k < 8; k++) {
        float sd = seed * 13.0f + k * 7.3f;
        V3 fl(1, 0.62f + 0.3f * hash1(seed * 31 + k), 0.8f + 0.2f * hash1(seed * 37 + k));
        addBlob(m, V3(0), fl, 20, 30, 1, 0, [&](V3 d) { float w = worley(d * 2.6f + V3(sd)); return 0.86f + 0.42f * fbm(d * 1.5f + V3(sd), 3) + 0.30f * (0.6f - w) + 0.10f * fbm(d * 7.0f + V3(sd * 1.7f), 2); });
        so.crumb[k] = sc.addBlas(m); m.clear();
    }
}
// scatter crumbs: density(x,z) in [0,1] thins them; sizes follow a power law between smin and smax
static inline void soilScatter(Soil& so, int n, float x0, float x1, float z0, float z1, float smin, float smax, uint32_t seed, std::function<float(float, float)> density = nullptr, float sizePow = 2.6f) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        float x = r.r(x0, x1), z = r.r(z0, z1);
        float keep = r.f();
        float u = r.f(); float s = smin * std::pow(smax / smin, std::pow(u, sizePow));
        SoilItem it; it.s = s; it.kind = (int)(r.u() % 8); it.axis = r.dir(); it.ang = r.r(0, 2 * PI); it.rnd = r.f();
        float tv = r.r(0.65f, 1.25f); it.tint = V3(tv * r.r(0.92f, 1.08f), tv, tv * r.r(0.9f, 1.05f));
        if (density && keep > density(x, z)) continue;
        it.p = V3(x, so.height(x, z) + s * r.r(-0.15f, 0.45f), z);
        so.items.push_back(it);
    }
}
static inline void soilEmit(Scene& sc, const Soil& so, std::function<bool(SoilItem&)> modify = nullptr) {
    sc.addInst(so.base, M34::identity(), so.matBase);
    for (const SoilItem& it0 : so.items) {
        SoilItem it = it0;
        if (modify && !modify(it)) continue;
        sc.addInst(so.crumb[it.kind], M34::translate(it.p) * M34::rotAxis(it.axis, it.ang) * M34::scale(it.s), so.matBase, it.tint, it.rnd);
    }
}

// ---------------- moss ----------------
struct Moss { BlasH shoot[4]; int mat = 0; };
static inline void mossInit(Scene& sc, Moss& ms, V3 colBase, V3 colTip, uint32_t seed = 3) {
    ms.mat = sc.addMat([&] { Mat m = mkMat(colBase, 0.45f, 0.5f, 4); set4(m.base2, colTip); m.trans = 0.45f; set4(m.p0, 0.15f, 0.95f, 8, 0); set4(m.p2, colTip, 0.0f); return m; }());
    for (int k = 0; k < 4; k++) {
        Mesh m; Rng r(seed * 17 + k);
        int nl = 30 + k * 6; float bend = r.r(-0.3f, 0.3f);
        std::vector<V3> p(8); std::vector<float> rr(8);
        for (int i = 0; i < 8; i++) { float u = i / 7.f; p[i] = V3(bend * u * u * 0.3f, u, 0.1f * std::sin(u * 3 + k)); rr[i] = 0.035f * (1 - 0.5f * u); }
        addTube(m, p, rr, 5, 0, 0);
        for (int i = 0; i < nl; i++) {
            float u = 0.08f + 0.92f * (i + r.f()) / nl; float az = i * 2.39996f + r.r(-0.3f, 0.3f);
            V3 c(bend * u * u * 0.3f, u, 0.1f * std::sin(u * 3 + k));
            Blade b; b.len = 0.26f * (0.7f + 0.5f * r.f()) * (1 - 0.3f * u * u); b.width = 0.045f; b.maxAt = 0.3f; b.q = 0.6f; b.tipQ = 1; b.fold = 0.5f; b.curl = -0.9f * (1 - 0.4f * u); b.nu = 5; b.nv = 1;
            float open = 0.75f - 0.45f * u * u;
            M34 xf = M34::translate(c) * M34::rotY(az) * M34::rotX(open);
            addBlade(m, b, xf, 0, r.f(), 0, [u](float bu, float) { return sat(u * 0.85f + bu * 0.3f); });
        }
        ms.shoot[k] = sc.addBlas(m);
    }
}
static inline void mossAdd(Scene& sc, const Moss& ms, V3 p, V3 n, float h, Rng& r, V3 tint = V3(1)) {
    V3 d = norm(n + r.dir() * 0.35f);
    float tv = r.r(0.75f, 1.25f);
    sc.addInst(ms.shoot[r.u() % 4], frameY(p, d, h * r.r(0.6f, 1.25f), r.r(0, 6.28f)), ms.mat, tint * V3(tv * r.r(0.9f, 1.1f), tv, tv * r.r(0.8f, 1.1f)), r.f());
}

// ---------------- leaf litter ----------------
struct LitterItem { M34 xf; V3 tint; float rnd; int k; V3 p; };
struct Litter { BlasH leaf[8]; int mat = 0; std::vector<LitterItem> items; };
static inline void litterInit(Scene& sc, Litter& lt, uint32_t seed = 5) {
    lt.mat = sc.addMat([] { Mat m = mkMat(V3(0.19f, 0.11f, 0.055f), 0.5f, 0.6f, 2); set4(m.base2, V3(0.36f, 0.17f, 0.05f)); m.trans = 0.2f; set4(m.p0, 8, 0.55f, 0.65f, 26); set4(m.p1, 0.6f, 0.35f, 0.55f, 0.00025f); set4(m.p2, V3(0.06f, 0.04f, 0.03f), 0.0f); m.bump = 1; return m; }());
    for (int k = 0; k < 8; k++) {
        Mesh m; Rng r(seed * 31 + k);
        Blade b; b.len = r.r(0.045f, 0.075f); b.width = b.len * r.r(0.24f, 0.4f); b.maxAt = r.r(0.38f, 0.55f); b.q = r.r(0.65f, 0.95f); b.fold = r.r(-0.15f, 0.3f); b.cup = r.r(-0.7f, 1.0f);
        b.curl = r.r(-0.9f, 1.3f); b.curlPow = r.r(1, 2.5f); b.twist = r.r(-0.6f, 0.6f); b.wave = r.r(0.1f, 0.35f); b.waveFreq = r.r(2, 5); b.wavePhase = r.r(0, 6); b.noise = 0.003f; b.seed = seed + k;
        b.serr = (k % 2) ? 0.14f : 0.05f; b.serrN = r.r(10, 18); b.nu = 44; b.nv = 9; b.pleat = 0.0012f; b.pleatN = 8;
        addBlade(m, b, M34::identity(), 0, 0, r.f() * 0.6f);
        // petiole
        std::vector<V3> p = {V3(0, 0, 0), V3(0.001f, -0.006f, 0.001f), V3(0.002f, -0.013f, 0.0f)}; std::vector<float> rr = {0.0006f, 0.0005f, 0.0005f};
        addTube(m, p, rr, 5, 0, 0);
        lt.leaf[k] = sc.addBlas(m);
    }
}
static inline void litterScatter(Litter& lt, int n, float x0, float x1, float z0, float z1, std::function<float(float, float)> height, uint32_t seed, float lift = 0.004f) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        LitterItem it; it.k = (int)(r.u() % 8); it.rnd = r.f();
        float x = r.r(x0, x1), z = r.r(z0, z1); float s = r.r(0.7f, 1.25f);
        it.p = V3(x, height(x, z) + lift * r.r(0.3f, 1.6f), z);
        bool flip = r.f() < 0.4f;
        it.xf = M34::translate(it.p) * M34::rotY(r.r(0, 2 * PI)) * M34::rotX(-PI / 2 + r.r(-0.22f, 0.22f) + (flip ? PI : 0)) * M34::rotY(r.r(-0.25f, 0.25f)) * M34::scale(s);
        float v = r.r(0.45f, 1.5f); float w = r.f();
        it.tint = V3(v * (1.0f + 0.25f * w), v, v * (1.0f - 0.3f * w));
        lt.items.push_back(it);
    }
}
static inline void litterEmit(Scene& sc, const Litter& lt) { for (const LitterItem& it : lt.items) sc.addInst(lt.leaf[it.k], it.xf, lt.mat, it.tint, it.rnd); }
