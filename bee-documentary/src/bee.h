// bee.h — 西方蜜蜂工蜂（Apis mellifera）的程序化解剖模型 + 骨架。
// 身体按真实的体节拆成刚体部件（昆虫外骨骼本来就是刚体铰接）：胸、头、触角柄节/鞭节、喙三节、
// 腹部 6 节、4 片翅、6 条足 ×（股节、胫节、基跗节、跗节）、花粉团。每个部件在自己的局部坐标里建模，
// 每帧由 BeeRig::apply 算出各部件的刚体变换。
// 蜂体坐标（mm）：原点=胸部中心，+x 向前（头），+y 向上（背），+z 向右。
#pragma once

struct BeeMats { int cuticle, head, eye, abd, wing, leg, antenna, hairThorax, hairHead, hairAbd, hairDark, hairLeg, hairEye, hairComb, pollenBall, tongue, mandible, ocellus, pollenGrain; };
static BeeMats gBM;

static void initBeeMats(World& W) {
    gBM.cuticle = W.addMat(mkMat(M_DIFF, V3(0.020f, 0.016f, 0.013f), 0.30f, 0.05f, 1, T_CUTICLE));
    gBM.head = W.addMat(mkMat(M_DIFF, V3(0.022f, 0.017f, 0.014f), 0.26f, 0.05f, 1, T_HEAD));
    gBM.eye = W.addMat(mkMat(M_DIFF, V3(0.045f, 0.027f, 0.020f), 0.16f, 0.06f, 1, T_EYE));
    gBM.abd = W.addMat(mkMat(M_DIFF, V3(1), 0.26f, 0.05f, 1, T_ABDOMEN));
    gBM.wing = W.addMat(mkMat(M_FILM, V3(1), 0.17f, 0.05f, 1, T_WING));
    gBM.leg = W.addMat(mkMat(M_DIFF, V3(1), 0.28f, 0.05f, 1, T_LEG));
    gBM.antenna = W.addMat(mkMat(M_DIFF, V3(0.050f, 0.036f, 0.028f), 0.55f, 0.04f, 0.6f, T_ANTENNA, V3(0), 0.25f));
    gBM.hairThorax = W.addMat(mkHair(V3(0.60f, 0.44f, 0.23f), V3(0.74f, 0.62f, 0.44f)));
    gBM.hairHead = W.addMat(mkHair(V3(0.64f, 0.50f, 0.29f), V3(0.76f, 0.66f, 0.50f)));
    gBM.hairAbd = W.addMat(mkHair(V3(0.74f, 0.64f, 0.46f), V3(0.80f, 0.74f, 0.62f)));
    gBM.hairDark = W.addMat(mkHair(V3(0.07f, 0.05f, 0.04f), V3(0.16f, 0.12f, 0.10f)));
    gBM.hairLeg = W.addMat(mkHair(V3(0.50f, 0.36f, 0.19f), V3(0.66f, 0.54f, 0.38f)));
    gBM.hairEye = W.addMat(mkHair(V3(0.40f, 0.30f, 0.19f), V3(0.60f, 0.50f, 0.38f)));
    gBM.hairComb = W.addMat(mkHair(V3(0.80f, 0.52f, 0.16f), V3(0.70f, 0.55f, 0.32f)));
    { GMat m = mkMat(M_DIFF, V3(0.86f, 0.46f, 0.04f), 0.38f, 0.04f, 0.9f, T_POLLENBALL); gBM.pollenBall = W.addMat(m); }
    gBM.tongue = W.addMat(mkMat(M_DIFF, V3(0.085f, 0.038f, 0.018f), 0.30f, 0.045f, 0.8f, T_TONGUE));
    gBM.mandible = W.addMat(mkMat(M_DIFF, V3(0.055f, 0.028f, 0.014f), 0.30f, 0.05f));
    gBM.ocellus = W.addMat(mkMat(M_DIFF, V3(0.01f), 0.10f, 0.06f));
    gBM.pollenGrain = W.addMat(mkMat(M_DIFF, V3(1.0f), 0.6f, 0.03f, 0.4f, T_NONE, V3(0), 0.08f));
}

static V3 ik2(const V3& a, const V3& c, float l1, float l2, const V3& hint) {
    V3 ac = c - a; float d = clampf(len(ac), std::fabs(l1 - l2) * 1.02f + 1e-3f, (l1 + l2) * 0.999f); V3 dir = norm(ac);
    float x = (l1 * l1 - l2 * l2 + d * d) / (2 * d), h = safesqrt(l1 * l1 - x * x);
    V3 perp = norm(hint - dir * dot(hint, dir));
    return a + dir * x + perp * h;
}
// 以 x 轴对准 xdir、z 轴尽量贴近 zhint 的坐标系
static Xf frameX(const V3& o, const V3& xdir, const V3& zhint) {
    Xf r; r.o = o; r.x = norm(xdir); r.y = norm(cross(zhint, r.x)); r.z = cross(r.x, r.y); return r;
}
static V3 nlerp(const V3& a, const V3& b, float t) { return norm(mix(a, b, t)); }

// ---------- 翅：轮廓与翅脉（翅面坐标 mm：x 沿翅展，y 指向前缘） ----------
static void wingOutline(int wing, float x, float& yU, float& yL) {
    if (wing == 0) {
        static const float ux[] = {0, 1, 2.5f, 4, 5.5f, 6.5f, 7.4f, 9.2f}, uy[] = {0.10f, 0.30f, 0.58f, 0.80f, 0.96f, 1.02f, 1.0f, 0.95f};
        static const float lx[] = {0, 1, 2, 3, 4, 5, 6, 7, 7.4f, 9.2f}, ly[] = {-0.10f, -0.36f, -0.78f, -1.32f, -1.75f, -2.0f, -2.1f, -2.05f, -2.0f, -1.9f};
        yU = interpTab(ux, uy, 8, x); yL = interpTab(lx, ly, 10, x);
        if (x > 7.4f) { float t = sat((x - 7.4f) / 1.8f), cap = safesqrt(1 - t * t), yt = -0.35f; yU = yt + (yU - yt) * cap; yL = yt + (yL - yt) * cap; }
    } else {
        static const float ux[] = {0, 1, 2.5f, 4, 5.2f, 6.4f}, uy[] = {0.08f, 0.20f, 0.40f, 0.52f, 0.50f, 0.45f};
        static const float lx[] = {0, 0.8f, 1.6f, 2.6f, 3.6f, 4.6f, 5.2f, 6.4f}, ly[] = {-0.08f, -0.50f, -0.95f, -1.15f, -1.25f, -1.2f, -1.1f, -1.0f};
        yU = interpTab(ux, uy, 6, x); yL = interpTab(lx, ly, 8, x);
        if (x > 5.0f) { float t = sat((x - 5.0f) / 1.4f), cap = safesqrt(1 - t * t), yt = -0.3f; yU = yt + (yU - yt) * cap; yL = yt + (yL - yt) * cap; }
    }
}
static void initVeins(World& W) {
    std::vector<float> v[2];
    auto seg = [&](int w, float ax, float ay, float bx, float by, float hw) { float s[8] = {ax, ay, bx, by, hw, 0, 0, 0}; v[w].insert(v[w].end(), s, s + 8); };
    auto poly = [&](int w, std::initializer_list<float> pts, float hw) {
        const float* p = pts.begin(); int n = (int)pts.size() / 2;
        for (int i = 0; i + 1 < n; i++) seg(w, p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], hw);
    };
    { float px = 0, pyU, pyL; wingOutline(0, 0, pyU, pyL); float py = pyU - 0.045f;
      for (float x = 0.3f; x <= 8.46f; x += 0.3f) { float yU, yL; wingOutline(0, x, yU, yL); float y = yU - 0.045f; seg(0, px, py, x, y, x < 6.4f ? 0.050f : 0.022f); px = x; py = y; } }
    poly(0, {0, 0.02f, 1.5f, 0.24f, 3.0f, 0.50f, 4.4f, 0.70f, 5.6f, 0.84f}, 0.040f);                       // Sc+R
    poly(0, {5.5f, 0.86f, 6.35f, 0.94f}, 0.085f);                                                           // 翅痣
    poly(0, {6.0f, 0.86f, 6.5f, 0.64f, 7.2f, 0.52f, 8.0f, 0.49f, 8.4f, 0.56f, 8.47f, 0.70f}, 0.028f);       // 缘室下缘 Rs
    poly(0, {0, -0.02f, 1.6f, -0.13f, 3.2f, -0.25f}, 0.042f);                                               // M+Cu
    poly(0, {0, -0.07f, 1.2f, -0.36f, 2.4f, -0.74f, 3.3f, -1.03f, 4.4f, -1.45f, 5.0f, -1.70f}, 0.030f);     // A
    poly(0, {3.2f, -0.25f, 3.45f, 0.10f, 3.7f, 0.25f, 3.95f, 0.63f}, 0.030f);                               // 基脉
    poly(0, {3.2f, -0.25f, 3.3f, -1.03f}, 0.026f);                                                          // cu-a
    poly(0, {3.7f, 0.25f, 4.7f, 0.10f, 5.7f, 0.02f, 6.6f, 0.0f, 7.3f, 0.06f}, 0.028f);                      // M
    poly(0, {4.7f, 0.10f, 4.85f, 0.45f, 5.1f, 0.78f}, 0.024f);                                              // 亚缘横脉 1
    poly(0, {5.7f, 0.02f, 5.9f, 0.40f, 6.3f, 0.72f}, 0.024f);                                               // 2
    poly(0, {6.6f, 0.0f, 7.0f, 0.22f, 7.5f, 0.51f}, 0.024f);                                                // 3
    poly(0, {7.3f, 0.06f, 7.5f, 0.51f}, 0.020f);
    poly(0, {3.2f, -0.25f, 4.2f, -0.74f, 5.2f, -1.02f, 6.2f, -1.05f, 6.9f, -0.86f}, 0.028f);                // Cu
    poly(0, {5.2f, -1.02f, 5.35f, -0.5f, 5.55f, 0.03f}, 0.024f);                                            // m-cu 1
    poly(0, {6.9f, -0.86f, 7.1f, -0.4f, 7.3f, 0.06f}, 0.024f);                                              // m-cu 2
    poly(0, {6.2f, -1.05f, 6.6f, -1.35f, 6.95f, -1.58f}, 0.020f);                                           // Cu1 末端
    { float px = 0, pyU, pyL; wingOutline(1, 0, pyU, pyL); float py = pyU - 0.035f;
      for (float x = 0.3f; x <= 3.7f; x += 0.3f) { float yU, yL; wingOutline(1, x, yU, yL); float y = yU - 0.035f; seg(1, px, py, x, y, 0.036f); px = x; py = y; } }
    poly(1, {0, 0.0f, 1.8f, 0.22f, 3.4f, 0.38f, 4.2f, 0.34f, 4.9f, 0.24f}, 0.028f);
    poly(1, {0, -0.03f, 1.2f, -0.17f, 2.2f, -0.30f}, 0.030f);
    poly(1, {2.2f, -0.30f, 3.0f, -0.24f, 3.6f, -0.25f, 4.2f, -0.42f, 4.6f, -0.56f}, 0.024f);
    poly(1, {2.2f, -0.30f, 2.6f, -0.55f, 3.0f, -0.82f}, 0.024f);
    poly(1, {3.4f, 0.38f, 3.6f, -0.25f}, 0.022f);
    poly(1, {0, -0.06f, 0.9f, -0.38f, 1.8f, -0.72f}, 0.024f);
    W.veins = v[0]; W.veins.insert(W.veins.end(), v[1].begin(), v[1].end());
    W.nVeinFore = (int)v[0].size() / 8; W.nVeinHind = (int)v[1].size() / 8;
}

static const float ABD_UB_C[7] = {0.0f, 0.135f, 0.33f, 0.53f, 0.71f, 0.86f, 1.0f};  // 腹部背板分界（与 kernel 里一致）

struct LegDef { V3 coxa; float Lf, Lt, Lb, Ltar; V3 ideal; float rf, rt; };
static const LegDef LEGS[3] = {
    {V3(1.20f, -1.38f, 0.80f), 1.9f, 1.75f, 1.25f, 1.15f, V3(3.5f, -3.5f, 2.7f), 0.20f, 0.155f},
    {V3(0.15f, -1.62f, 1.00f), 2.2f, 2.20f, 1.60f, 1.25f, V3(0.3f, -3.5f, 5.0f), 0.215f, 0.165f},
    {V3(-1.10f, -1.50f, 1.05f), 2.5f, 3.10f, 2.00f, 1.40f, V3(-5.2f, -3.5f, 3.6f), 0.24f, 0.20f}};

struct BeeState {
    Xf X;                                      // 蜂体 -> 世界
    float headPitch = 0.55f, headYaw = 0, headRoll = 0;
    float antYaw[2] = {0, 0}, antPitch[2] = {0, 0}, antElbow[2] = {0, 0};
    float probExt = 0, lick = 0.3f; V3 probTarget = V3(0);   // 喙：伸出程度、舌伸出量、目标点（世界）
    float abdCurl = 0, abdYaw = 0, breath = 0;                // breath: 0..1 伸缩
    float wingFlight = 0, wingPhase = 0, wingAmp = 1, wingOpen = 0;
    V3 foot[6], footN[6];                      // 落脚点与该处的“地面”法线（世界），下标 = 足对*2 + 侧(0 左 -z, 1 右 +z)
    float pellet = 1;                          // 花粉团大小
};

struct BeeRig {
    // 部件编号
    int pThorax, pHead, pScape[2], pFlag[2], pProb1, pProb2, pGlossa, pAbd[6], pWing[2][2], pLeg[6][4], pPellet[2];
    std::vector<int> all;
    V3 neck = V3(2.12f, 0.12f, 0), neckH = V3(-0.50f, 0.38f, 0);
    V3 sock[2], mouth = V3(-0.05f, -1.50f, 0);
    float Lscape = 1.18f, L1 = 1.25f, L2 = 2.9f, Lg = 1.9f;
    V3 hip[6];
    Spine abSp;
    V3 corbC;                                  // 花粉篮中心（后足胫节局部）

    // ============ 建模 ============
    // headK / legK：头、足的毛量与毛长系数（<1 时更疏更短，露出体壁和分节）
    void build(Builder& B, uint32_t flags, float hairK, float pollenK, float headK = 1.0f, float legK = 1.0f) {
        World& W = B.W; RNG& rng = B.rng;
        initVeins(W);
        auto reg = [&](int id) { all.push_back(id); return id; };
        struct HairRange { size_t a, b; float w; };
        auto dust = [&](std::vector<HairRange>& hosts, int nClusters, int loose, float lowBias) {
            // 沾在毛上的花粉粒：成团 + 零散
            if (hosts.empty()) return;
            float wsum = 0; for (auto& h : hosts) wsum += h.w * (h.b - h.a);
            if (wsum <= 0) return;
            auto pickSeg = [&]() -> size_t {
                float pick = rng.f() * wsum;
                for (auto& h : hosts) { float ww = h.w * (h.b - h.a); if (pick < ww) return std::min(h.b - 1, h.a + (size_t)(pick / h.w)); pick -= ww; }
                return hosts.back().b - 1;
            };
            auto place = [&](size_t ci) {
                const GCyl c = W.cyls[ci];
                V3 p0(c.p0[0], c.p0[1], c.p0[2]), p1(c.p1[0], c.p1[1], c.p1[2]);
                V3 ax = norm(p1 - p0), t1, t2; basis(ax, t1, t2);
                float ang = rng.f() * TAU, rg = 0.0120f + 0.0065f * rng.f();
                V3 base = mix(p0, p1, rng.f()) + (t1 * std::cos(ang) + t2 * std::sin(ang)) * (c.r0 + rg * 0.85f);
                V3 col = POLLEN_COL * V3(0.80f + 0.25f * rng.f(), 0.75f + 0.3f * rng.f(), 0.8f);
                B.sphere(base, rg, gBM.pollenGrain, col);
            };
            size_t lo = hosts.front().a, hi = hosts.back().b;
            for (int k = 0; k < nClusters; k++) {
                size_t ci = pickSeg(); const GCyl c = W.cyls[ci];
                V3 ctr(c.p0[0], c.p0[1], c.p0[2]);
                float low = sat(0.5f - ctr.y * 0.35f);
                if (rng.f() > (1 - lowBias) + lowBias * low) continue;
                float rad = 0.22f + 0.45f * rng.f(); int want = 15 + (int)(rng.f() * rng.f() * 150);
                std::vector<uint32_t> nearv;
                for (size_t i = lo; i < hi; i++) { const GCyl& q = W.cyls[i]; if (W.cattr[i].v0 >= 0.12f && sqr(q.p0[0] - ctr.x) + sqr(q.p0[1] - ctr.y) + sqr(q.p0[2] - ctr.z) < rad * rad) nearv.push_back((uint32_t)i); }
                if (nearv.empty()) continue;
                for (int g = 0; g < want; g++) {
                    uint32_t qi = nearv[(size_t)(rng.f() * nearv.size()) % nearv.size()]; const GCyl& q = W.cyls[qi];
                    float d2 = sqr(q.p0[0] - ctr.x) + sqr(q.p0[1] - ctr.y) + sqr(q.p0[2] - ctr.z);
                    if (rng.f() > std::exp(-d2 / (0.35f * rad * rad))) continue;
                    place(qi);
                }
            }
            for (int i = 0; i < loose; i++) { size_t ci = pickSeg(); if (W.cattr[ci].v0 < 0.12f) continue; place(ci); }
        };
        V3 back(-1, 0, 0), upw(0, 1, 0);

        // ================= 胸 =================
        pThorax = reg(B.beginPart("bee_thorax", flags));
        SurfFn thoraxL = [](float u, float v) {
            float th = v * TAU, x = mix(-2.1f, 2.0f, u);
            float k = std::pow(std::max(0.f, 1 - std::pow(std::fabs(2 * u - 1), 2.6f)), 0.46f);
            float ry = 1.86f * k, rz = 1.80f * k, cy = 0.05f + 0.20f * std::sin(PI * u);
            float c = std::cos(th), s = std::sin(th);
            float r = 1 + 0.10f * std::exp(-sqr((u - 0.14f) / 0.06f)) * sqr(std::max(0.f, c))      // 小盾片
                      - 0.025f * std::exp(-sqr((u - 0.24f) / 0.02f)) * std::max(0.f, c);           // 盾间沟
            return PV(V3(x, cy + ry * c * (c < 0 ? 0.93f : 1) * r, rz * s * r), u, v);
        };
        B.grid(thoraxL, 90, 100, gBM.cuticle, true, true);
        { Spine sp{V3(1.5f, 0.0f, 0), V3(2.15f, 0.08f, 0)}; B.tube(sp, V3(0, 1, 0), [](float, float, float& a, float& b) { a = b = 0.60f; }, 4, 14, gBM.cuticle); }   // 颈
        { Spine sp{V3(-1.9f, -0.2f, 0), V3(-2.55f, -0.15f, 0)}; B.tube(sp, V3(0, 1, 0), [](float, float, float& a, float& b) { a = b = 0.42f; }, 4, 12, gBM.cuticle); } // 腹柄
        V3 legCol(0.085f, 0.048f, 0.028f), tarCol(0.21f, 0.095f, 0.035f);
        for (int e = 0; e < 2; e++) {
            float sg = e == 0 ? -1.f : 1.f;
            // 翅基片
            B.grid([=](float u, float v) {
                float th = u * PI, ph = v * TAU;
                return PV(V3(0.62f, 1.22f, sg * 1.30f) + V3(0.30f * std::sin(th) * std::cos(ph), 0.13f * std::cos(th), 0.22f * std::sin(th) * std::sin(ph)), u, v, V3(0.11f, 0.055f, 0.025f)); },
                12, 18, gBM.leg, true, false);
            // 基节（固定在胸上）
            for (int li = 0; li < 3; li++) {
                const LegDef& D = LEGS[li];
                V3 c0(D.coxa.x, D.coxa.y, D.coxa.z * sg);
                V3 c1 = c0 + norm(V3(li == 0 ? 0.25f : (li == 2 ? -0.25f : 0), -1, sg * 0.45f)) * 0.75f;
                hip[li * 2 + e] = c1;
                Spine sp{c0 + V3(0, 0.3f, -sg * 0.1f), c1};
                B.tube(sp, V3(1, 0, 0), [](float u, float, float& a, float& b) { a = b = mix(0.36f, 0.22f, u) * (0.7f + 0.3f * endRound(u, 0.2f)); }, 10, 14, gBM.leg, [=](float, float) { return legCol; });
            }
        }
        {
            size_t c0 = W.cyls.size();
            B.grow(thoraxL, true, (int)(30000 * hairK),
                   [](float u, float v, const V3& p, const V3&) {
                       float c = std::cos(v * TAU); float d = 1; if (u < 0.04f || u > 0.97f) d = 0.3f;
                       float bald = smooth(0.72f, 0.97f, c) * smooth(0.30f, 0.42f, u) * smooth(0.80f, 0.66f, u);   // 背板中央磨秃
                       d *= 1 - 0.60f * bald;
                       d *= 0.72f + 0.45f * noise3(p * 1.7f);
                       return d; },
                   [&](float u, float v, const V3& p, const V3& n, HairP& hp, V3& d) {
                       float c = std::cos(v * TAU);
                       float dors = smooth(0.5f, 0.95f, c) * smooth(0.25f, 0.4f, u) * smooth(0.8f, 0.65f, u);
                       hp.length = (0.50f + 0.34f * rng.f() + (rng.f() < 0.12f ? 0.30f * rng.f() : 0)) * (1 - 0.45f * dors) * (c < -0.3f ? 1.15f : 1.0f) * (0.85f + 0.3f * noise3(p * 1.3f + V3(7)));
                       hp.r0 = 0.0044f + 0.001f * rng.f(); hp.seg = 6; hp.mat = rng.f() < 0.07f ? gBM.hairLeg : gBM.hairThorax; hp.rnd = sat(0.5f + 0.35f * noise3(p * 1.1f + V3(3, 8, 1)) + 0.5f * (rng.f() - 0.5f));
                       hp.jitter = 0.18f; hp.bend = back * 0.6f - upw * 0.25f + rng.onSphere() * 0.5f; hp.bendK = 0.10f + 0.14f * rng.f();
                       hp.barbs = 2; hp.barbLen = 0.050f; hp.taper = 0.72f;
                       V3 tuft(noise3(p * 2.3f), noise3(p * 2.3f + V3(31, 7, 3)), noise3(p * 2.3f + V3(5, 19, 47)));
                       d = norm(n + back * (0.30f + 0.3f * rng.f()) + tuft * 0.55f + rng.onSphere() * 0.30f);
                       return true; });
            std::vector<HairRange> hosts{{c0, W.cyls.size(), 1.0f}};
            dust(hosts, (int)(34 * pollenK), (int)(140 * pollenK * hairK), 0.75f);
        }
        B.endPart();

        // ================= 头 =================（头部局部：x = 面法线，y = 头顶方向，z = 右）
        pHead = reg(B.beginPart("bee_head", flags));
        SurfFn headL = [](float u, float v) {
            float th = u * PI, ph = v * TAU;
            float st = std::sin(th), ct = std::cos(th), sp = std::sin(ph), cp = std::cos(ph);
            float wm = 1 - 0.40f * smooth(0.15f, -0.95f, ct);                     // 向上颚收窄：正面看是倒圆角三角形
            float rf = sp > 0 ? 0.95f * (1 - 0.12f * smooth(-0.2f, -1.0f, ct)) : 0.55f;
            // 唇基：下半张脸中央隆起的梯形板；两侧是凹下去的额唇基沟
            float mid = smooth(0.55f, 0.15f, std::fabs(cp));
            float cl = smooth(0.3f, 0.75f, sp) * mid * smooth(0.02f, -0.22f, ct) * smooth(-0.95f, -0.72f, ct);
            float groove = smooth(0.3f, 0.75f, sp) * std::exp(-sqr((std::fabs(cp) - 0.50f) / 0.07f)) * smooth(0.05f, -0.2f, ct) * smooth(-0.95f, -0.6f, ct);
            // 触角窝之间的额隆线
            float frons = smooth(0.4f, 0.8f, sp) * std::exp(-sqr(cp / 0.10f)) * smooth(0.0f, 0.2f, ct) * smooth(0.55f, 0.3f, ct);
            float bump = 1 + 0.085f * cl - 0.05f * groove + 0.03f * frons;
            return PV(V3(rf * st * sp * bump, 1.68f * ct * (ct > 0 ? 0.94f : 1.0f), 1.86f * st * cp * wm), u, v);
        };
        B.grid(headL, 90, 120, gBM.head, true, false);
        // 上唇：唇基下方的小横板
        B.grid([](float u, float v) { float th = u * PI, ph = v * TAU;
                 return PV(V3(0.62f, -1.50f, 0) + V3(0.16f * std::sin(th) * std::cos(ph), 0.20f * std::cos(th), 0.42f * std::sin(th) * std::sin(ph)), u, v, V3(0.7f, 0.6f, 0.5f)); }, 10, 16, gBM.mandible, true, false);
        struct EyeDef { V3 c, o, l, m; float Ro, Rl, Rm; };
        EyeDef eyes[2];
        std::vector<HairRange> headHosts;
        for (int e = 0; e < 2; e++) {
            float sg = e == 0 ? -1.f : 1.f;
            EyeDef& E = eyes[e];
            E.c = V3(0.16f, 0.26f, sg * 1.40f);
            E.o = norm(V3(0.30f, 0.06f, sg));
            E.l = norm(V3(-0.10f, 1, sg * 0.07f) - E.o * dot(V3(-0.10f, 1, sg * 0.07f), E.o));
            E.m = cross(E.o, E.l);
            E.Ro = 0.58f; E.Rl = 1.30f; E.Rm = 0.72f;
            EyeDef Ec = E;
            SurfFn eyeL = [Ec](float u, float v) {
                float th = u * PI * 0.62f, ph = v * TAU;
                float st = std::sin(th), ct = std::cos(th), rho = 2 * std::sin(th / 2);
                // 肾形：下端略窄，后缘略凹
                float cph = std::cos(ph), sph = std::sin(ph);
                float kid = 1 - 0.10f * smooth(0.0f, -1.0f, cph) + 0.05f * sph * (1 - cph * cph);
                V3 p = Ec.c + Ec.o * (Ec.Ro * ct) + Ec.l * (Ec.Rl * st * cph) + Ec.m * (Ec.Rm * st * sph * kid);
                return PV(p, Ec.Rl * rho * cph, Ec.Rm * rho * sph, V3(1));
            };
            B.grid(eyeL, 80, 140, gBM.eye, true, false);
            size_t c0 = W.cyls.size();
            B.grow(eyeL, false, (int)(1300 * hairK), [](float u, float, const V3&, const V3&) { return u < 0.93f ? 1.f : 0.f; },
                   [&](float, float, const V3& p, const V3& n, HairP& hp, V3& d) {
                       hp.length = 0.10f + 0.13f * rng.f(); hp.r0 = 0.0028f; hp.seg = 2; hp.mat = gBM.hairEye; hp.rnd = rng.f(); hp.jitter = 0.06f; hp.taper = 0.6f;
                       d = norm(n + rng.onSphere() * 0.22f); return true; });
            headHosts.push_back({c0, W.cyls.size(), 0.15f});
        }
        auto inEye = [&](const V3& ph, float grow) {
            for (int e = 0; e < 2; e++) {
                const EyeDef& E = eyes[e]; V3 d = ph - E.c;
                float a = dot(d, E.o) / (E.Ro * grow), b = dot(d, E.l) / (E.Rl * grow), c = dot(d, E.m) / (E.Rm * grow);
                if (a * a + b * b + c * c < 1) return true;
            }
            return false;
        };
        for (int k = 0; k < 3; k++) {   // 单眼
            V3 p = k == 0 ? V3(0.52f, 1.42f, 0) : V3(0.30f, 1.55f, k == 1 ? -0.27f : 0.27f);
            B.sphere(p, 0.125f, gBM.ocellus, V3(1));
        }
        for (int e = 0; e < 2; e++) {
            float sg = e == 0 ? -1.f : 1.f;
            sock[e] = V3(0.90f, 0.12f, sg * 0.34f);
            // 触角窝：一圈略隆起的环
            B.grid([=](float u, float v) { float th = u * PI, ph = v * TAU; return PV(sock[e] + V3(0.07f * std::cos(th), 0.17f * std::sin(th) * std::cos(ph), 0.17f * std::sin(th) * std::sin(ph)), u, v); }, 8, 14, gBM.head, true, false);
            // 上颚：铲状，端部略内弯
            Spine sp{V3(0.42f, -1.42f, sg * 0.66f), V3(0.66f, -1.80f, sg * 0.42f), V3(0.74f, -2.12f, sg * 0.10f)};
            B.grid(mkTube(sp, V3(1, 0, 0), [](float u, float, float& a, float& b) { float k = endRound(u, 0.12f); a = 0.10f * k * (1 + 0.6f * u); b = (0.26f - 0.06f * u) * k; },
                          [](float u, float) { return mix(V3(0.5f, 0.4f, 0.35f), V3(2.2f, 1.5f, 1.0f), u * u); }), 22, 14, gBM.mandible, true, true);
        }
        {
            size_t c0 = W.cyls.size();
            V3 hup(0, 1, 0);
            B.grow(headL, false, (int)(11000 * hairK * headK),
                   [&](float u, float v, const V3& p, const V3&) {
                       if (inEye(p, 1.03f)) return 0.f;
                       if (inEye(p, 1.22f)) return 0.35f;
                       float sp = std::sin(v * TAU), ct = std::cos(u * PI);
                       if (sp > 0.45f && ct < -0.12f && std::fabs(std::cos(v * TAU)) < 0.50f) return 0.07f;   // 唇基几乎是光的
                       if (sp > 0.3f && ct < -0.55f) return 0.15f;
                       if (sp < -0.3f) return 0.5f;
                       if (sp > 0.25f && ct < 0.55f) return 0.30f;
                       return 1.f; },
                   [&](float u, float v, const V3& p, const V3& n, HairP& hp, V3& d) {
                       float ct = std::cos(u * PI), sp = std::sin(v * TAU);
                       bool vertex = ct > 0.6f; bool face = sp > 0.3f;
                       hp.length = vertex ? 0.34f + 0.32f * rng.f() : (face ? 0.06f + 0.11f * rng.f() : 0.20f + 0.26f * rng.f());
                       if (face) hp.r0 = 0.0030f;
                       hp.length *= 0.55f + 0.45f * headK;
                       if (inEye(p, 1.22f)) hp.length *= 0.55f;
                       hp.r0 = 0.0040f + 0.001f * rng.f(); hp.seg = 5; hp.mat = gBM.hairHead; hp.rnd = rng.f();
                       hp.jitter = 0.15f; hp.bend = face ? V3(0) - hup * 0.5f : V3(-0.5f, 0, 0); hp.bendK = 0.12f;
                       hp.barbs = 2; hp.barbLen = 0.04f; hp.taper = 0.72f;
                       V3 tuft(noise3(p * 3.1f), noise3(p * 3.1f + V3(31, 7, 3)), noise3(p * 3.1f + V3(5, 19, 47)));
                       d = norm(n + (face ? V3(0) - hup * 0.25f : hup * 0.15f) + tuft * 0.45f + rng.onSphere() * 0.28f);
                       return true; });
            headHosts.push_back({c0, W.cyls.size(), 1.2f});
            dust(headHosts, (int)(26 * pollenK), (int)(110 * pollenK * hairK), 0.6f);
        }
        B.endPart();

        // ================= 触角 =================（柄节 / 鞭节 各自的局部坐标：x 沿轴）
        for (int e = 0; e < 2; e++) {
            pScape[e] = reg(B.beginPart(std::string("bee_scape") + (e ? "R" : "L"), flags));
            V3 scapeCol(0.9f, 0.8f, 0.7f);
            Spine sc{V3(-0.08f, 0, 0), V3(0.6f, 0.02f, 0), V3(Lscape, 0, 0)};
            B.grid(mkTube(sc, V3(0, 0, 1), [](float u, float, float& a, float& b) { a = b = (0.068f + 0.018f * u) * endRound(u, 0.05f); }, [=](float, float) { return scapeCol; }), 24, 12, gBM.antenna, true, true);
            B.sphere(V3(0, 0, 0), 0.12f, gBM.cuticle);
            B.sphere(V3(Lscape, 0, 0), 0.095f, gBM.antenna, V3(1));
            for (int i = 0; i < (int)(120 * hairK); i++) {
                float u = 0.1f + 0.85f * rng.f(), a = rng.f() * TAU; V3 n(0, std::cos(a), std::sin(a));
                HairP hp; hp.length = 0.05f + 0.06f * rng.f(); hp.r0 = 0.003f; hp.seg = 2; hp.mat = gBM.hairHead; hp.rnd = rng.f(); hp.jitter = 0.05f;
                B.hair(V3(u * Lscape, 0, 0) + n * 0.09f, norm(n + V3(0.6f, 0, 0)), hp);
            }
            B.endPart();
            pFlag[e] = reg(B.beginPart(std::string("bee_flag") + (e ? "R" : "L"), flags));
            Spine fl; float FL = 3.05f;
            for (int j = 0; j <= 6; j++) { float t = j / 6.f; V3 dd = norm(V3(1, 0, 0) + V3(0, -0.85f - 0.25f * e, e ? 0.12f : -0.08f) * (t * 1.2f)); fl.c.push_back(j == 0 ? V3(0) : fl.c.back() + dd * (FL / 6)); }
            SurfFn flag = mkTube(fl, V3(0, 0, 1), [](float u, float, float& a, float& b) {
                float seg = std::pow(std::fabs(std::cos(PI * (u * 10.6f + 0.05f))), 10.f);
                a = b = (0.072f + 0.014f * smooth(0.f, 0.35f, u) - 0.012f * smooth(0.6f, 1.f, u)) * (1 - 0.20f * seg) * std::pow(sat((1 - u) / 0.035f), 0.5f) + 0.002f; });
            B.grid(flag, 150, 16, gBM.antenna, true, true);
            // 鞭节上极短的感觉毛：让轮廓有一层绒
            for (int i = 0; i < (int)(1600 * hairK); i++) {
                float u = 0.04f + 0.95f * rng.f(), v = rng.f(); PV q = flag(u, v); V3 c = fl.at(u), n = norm(q.p - c), t = fl.tan(u);
                HairP hp; hp.length = 0.012f + 0.014f * rng.f(); hp.r0 = 0.0022f; hp.seg = 1; hp.mat = gBM.hairDark; hp.rnd = rng.f(); hp.jitter = 0; hp.taper = 0.7f;
                B.hair(q.p, norm(n + t * 0.9f), hp);
            }
            B.endPart();
        }

        // ================= 喙 =================（三节：轴节茎节 / 外颚叶与下唇须合成的鞘 / 中唇舌）
        pProb1 = reg(B.beginPart("bee_prob1", flags));
        { Spine sp{V3(-0.1f, 0, 0), V3(L1, 0, 0)};
          B.grid(mkTube(sp, V3(0, 0, 1), [](float u, float th, float& a, float& b) { float k = endRound(u, 0.1f); a = 0.30f * k * (1 - 0.15f * u); b = 0.19f * k; (void)th; },
                        [](float u, float) { return mix(V3(0.55f, 0.45f, 0.4f), V3(0.9f, 0.75f, 0.6f), u); }), 16, 14, gBM.tongue, true, true); }
        B.endPart();
        pProb2 = reg(B.beginPart("bee_prob2", flags));
        { Spine sp{V3(-0.1f, 0, 0), V3(L2 * 0.5f, 0.03f, 0), V3(L2, 0, 0)};
          B.grid(mkTube(sp, V3(0, 0, 1), [](float u, float th, float& a, float& b) {
                     float k = std::pow(sat(u / 0.06f), 0.5f);
                     float w = mix(0.26f, 0.085f, std::pow(u, 0.8f)) * k;
                     float groove = 1 - 0.22f * std::pow(std::fabs(std::sin(th)), 6.f) * smooth(0.1f, 0.5f, u);
                     a = w * groove; b = w * 0.62f; },
                 [](float u, float v) { float seam = std::pow(std::fabs(std::cos(v * TAU)), 12.f); return mix(V3(0.8f, 0.7f, 0.6f), V3(1.25f, 1.0f, 0.8f), u) * (1 - 0.35f * seam); }), 60, 16, gBM.tongue, true, true);
          // 下唇须：贴在鞘两侧的两根细须，端部略分开
          for (int s = -1; s <= 1; s += 2) {
              Spine lp{V3(L2 * 0.35f, 0, 0.13f * s), V3(L2 * 0.7f, -0.02f, 0.12f * s), V3(L2 * 1.02f, -0.06f, 0.16f * s)};
              B.grid(mkTube(lp, V3(0, 1, 0), [](float u, float, float& a, float& b) { a = b = 0.040f * (1 - 0.5f * u) + 0.006f; }, [](float, float) { return V3(0.9f, 0.7f, 0.55f); }), 14, 8, gBM.tongue, true, true);
          }
        }
        B.endPart();
        pGlossa = reg(B.beginPart("bee_glossa", flags));
        { Spine gl{V3(0, 0, 0), V3(Lg * 0.5f, 0.03f, 0), V3(Lg, 0, 0)};
          B.grid(mkTube(gl, V3(0, 0, 1), [](float u, float, float& a, float& b) { a = b = 0.050f * (1 - 0.45f * u) * endRound(u, 0.03f); }, [](float u, float) { return mix(V3(1.3f, 0.9f, 0.6f), V3(1.7f, 1.1f, 0.7f), u); }), 30, 8, gBM.tongue, true, true);
          B.sphere(V3(Lg, 0, 0), 0.040f, gBM.tongue, V3(1.6f, 1.1f, 0.7f));     // 舌端的小匙（中舌瓣）
          for (int i = 0; i < (int)(420 * std::max(hairK, 0.5f)); i++) {            // 舌上成环的细毛：蘸蜜用
              float u = 0.25f + 0.75f * rng.f(); V3 c = gl.at(u); V3 d = norm(rng.onSphere() + gl.tan(u) * 0.9f);
              HairP hp; hp.length = 0.07f + 0.06f * rng.f(); hp.r0 = 0.0035f; hp.seg = 2; hp.mat = gBM.hairComb; hp.rnd = rng.f(); hp.jitter = 0.05f;
              B.hair(c, d, hp);
          } }
        B.endPart();

        // ================= 腹 =================
        abSp = Spine{V3(-2.00f, -0.22f, 0), V3(-3.30f, -0.05f, 0), V3(-5.0f, -0.25f, 0), V3(-6.75f, -0.95f, 0), V3(-8.2f, -2.0f, 0)};
        Spine ab = abSp;
        auto abProf = [](float u) {
            static const float xs[] = {0, 0.05f, 0.14f, 0.28f, 0.44f, 0.60f, 0.75f, 0.88f, 0.96f, 1.0f};
            static const float ys[] = {0.28f, 1.10f, 1.72f, 1.98f, 2.02f, 1.82f, 1.36f, 0.82f, 0.36f, 0.03f};
            return interpTab(xs, ys, 10, u);
        };
        for (int i = 0; i < 6; i++) {
            pAbd[i] = reg(B.beginPart("bee_abd" + std::to_string(i), flags));
            SurfFn terg = [=](float uu, float v) {
                float u0 = std::max(0.f, ABD_UB_C[i] - 0.05f), u1 = ABD_UB_C[i + 1];
                float u = mix(u0, u1, uu);
                float w = (u - ABD_UB_C[i]) / (ABD_UB_C[i + 1] - ABD_UB_C[i]);
                float lip = w < 0 ? -0.03f + w * 0.25f : mix(-0.022f, 0.036f, w);
                if (i < 5) lip -= 0.045f * sqr(smooth(0.965f, 1.0f, uu));
                float rz = abProf(u) * (1 + lip), ry = rz * 0.90f;
                V3 c = ab.at(u), t = ab.tan(u);
                V3 n = norm(V3(0, 1, 0) - t * t.y), b = cross(t, n);
                float th = v * TAU, cs = std::cos(th), sn = std::sin(th);
                if (cs < 0) ry *= 0.78f;
                return PV(c + n * (ry * cs) + b * (rz * sn), u, v, V3((float)i, 0, 0));
            };
            B.grid(terg, 50, 150, gBM.abd, true, true);
            V3 backA = ab.tan(mix(ABD_UB_C[i], ABD_UB_C[i + 1], 0.5f));
            float u0g = std::max(0.f, ABD_UB_C[i] - 0.05f);
            float uu0 = (ABD_UB_C[i] - u0g) / (ABD_UB_C[i + 1] - u0g);
            size_t c0 = W.cyls.size();
            int nBand = i == 0 ? 6000 : (i <= 4 ? 13000 : 2200);
            B.grow(terg, true, (int)(nBand * hairK),
                   [=](float uu, float v, const V3&, const V3&) {
                       float w = (uu - uu0) / (1 - uu0); if (w < 0.02f) return 0.f;
                       float vent = smooth(0.14f, 0.22f, std::fabs(v - 0.5f));
                       float band = i == 0 ? 1.f : (i <= 4 ? smooth(0.40f, 0.27f, w) : 0.4f);
                       return band * (0.35f + 0.65f * vent); },
                   [&](float uu, float v, const V3& p, const V3& n, HairP& hp, V3& d) {
                       float w = (uu - uu0) / (1 - uu0);
                       if (i == 0) { hp.length = 0.55f + 0.4f * rng.f(); hp.seg = 5; hp.barbs = 2; d = norm(n + backA * 0.5f + rng.onSphere() * 0.35f); hp.mat = gBM.hairThorax; }
                       else { hp.length = (0.13f + 0.10f * rng.f()) * (1 - 0.3f * w); hp.seg = 3; hp.barbs = 1; d = norm(n * 0.30f + backA * 1.0f + rng.onSphere() * 0.22f); hp.mat = gBM.hairAbd; }
                       hp.r0 = 0.0038f + 0.001f * rng.f(); hp.rnd = rng.f(); hp.jitter = 0.10f; hp.bend = backA - n * 0.4f; hp.bendK = 0.10f; hp.barbLen = 0.03f; hp.taper = 0.7f;
                       return true; }, uu0, 1.f);
            std::vector<HairRange> hosts{{c0, W.cyls.size(), 1.0f}};
            B.grow(terg, true, (int)(700 * hairK),
                   [=](float uu, float, const V3&, const V3&) { float w = (uu - uu0) / (1 - uu0); return w > 0.02f ? 1.f : 0.f; },
                   [&](float, float v, const V3&, const V3& n, HairP& hp, V3& d) {
                       bool vent = std::fabs(v - 0.5f) < 0.16f;
                       hp.length = vent ? 0.5f + 0.4f * rng.f() : (0.14f + 0.16f * rng.f()) * (i >= 4 ? 1.6f : 1);
                       hp.seg = 4; hp.r0 = 0.0036f; hp.rnd = rng.f(); hp.jitter = 0.08f; hp.taper = 0.8f;
                       hp.mat = vent ? gBM.hairAbd : (rng.f() < (i >= 3 ? 0.55f : 0.35f) ? gBM.hairDark : gBM.hairLeg);
                       hp.bend = backA; hp.bendK = 0.15f;
                       d = norm(n * (vent ? 1.0f : 0.6f) + backA * 0.8f + rng.onSphere() * 0.25f);
                       return true; }, uu0, 1.f);
            if (i >= 1 && i <= 3) dust(hosts, (int)(5 * pollenK), (int)(12 * pollenK * hairK), 0.8f);
            B.endPart();
        }

        // ================= 翅 =================（翅的局部坐标：x 沿翅展，y 指向前缘，z 为翅面法向）
        for (int e = 0; e < 2; e++) for (int wing = 0; wing < 2; wing++) {
            pWing[e][wing] = reg(B.beginPart(std::string("bee_wing") + (e ? "R" : "L") + (wing ? "h" : "f"), flags));
            float Lw = wing == 0 ? 9.2f : 6.4f;
            // 每片翅的破损不同：老工蜂的翅缘常有缺刻
            float seedw = (float)(e * 2 + wing) * 7.3f + 1.1f;
            B.grid([=](float a, float bq) {
                float x = Lw * (1 - std::pow(1 - a, 1.7f));
                float yU, yL; wingOutline(wing, x, yU, yL);
                float tear = 0.10f * smooth(0.35f, 0.75f, noise3(V3(x * 1.3f, seedw, 0.5f))) * smooth(0.5f, 0.85f, x / Lw);
                yL += tear * (yU - yL);
                float y = mix(yL, yU, bq);
                float camber = 0.10f * (1 - sqr(2 * bq - 1)) * std::sin(PI * std::min(1.f, x / Lw * 1.1f));
                float bend = -0.012f * x * x * (wing == 0 ? 1 : 0.6f);
                float corr = 0.035f * noise3(V3(x * 0.9f, y * 1.3f, seedw)) + 0.02f * std::sin(y * 6 + x * 0.8f) * smooth(0, 3, x);
                return PV(V3(x, y, camber + bend + corr), x, y, V3((float)wing, (float)e, 0));
            }, wing == 0 ? 170 : 120, wing == 0 ? 64 : 46, gBM.wing, false, false);
            B.endPart();
        }

        // ================= 足 =================（各节局部：x 沿节轴，y 在腿平面内，z = 腿平面法线 = 外侧）
        for (int li = 0; li < 3; li++) for (int e = 0; e < 2; e++) {
            const LegDef& D = LEGS[li];
            std::string nm = std::string("bee_leg") + "FMH"[li] + (e ? "R" : "L");
            int idx = li * 2 + e;
            auto legHair = [&](const SurfFn& f, int kind, float L) {
                size_t c0 = W.cyls.size();
                int cnt = kind == 3 ? 400 : 3200;
                B.grow(f, true, (int)(cnt * hairK * (0.35f + 0.65f * legK)),
                       [=](float u, float v, const V3&, const V3&) {
                           // 关节两端也要有毛盖住（否则正对镜头的膝部是一圈毛中间一个黑点）
                           if (kind == 0) { if (u < 0.04f || u > 0.995f) return 0.f; }
                           else if (kind == 1 || kind == 4) { if (u < 0.008f || u > 0.97f) return 0.f; }
                           else if (u < 0.04f || u > 0.97f) return 0.f;
                           if (kind == 4) { float s = std::sin(v * TAU); return (s > 0.25f && u > 0.2f) ? 0.02f : 1.f; }   // 花粉篮面光滑
                           if (kind == 5) { float s = std::sin(v * TAU); return s < -0.2f ? 0.f : 1.f; }                  // 后足基跗节内侧留给花粉梳
                           return 1.f; },
                       [&](float u, float v, const V3&, const V3& n, HairP& hp, V3& d) {
                           V3 ax(1, 0, 0);
                           float cs = std::cos(v * TAU);
                           hp.rnd = rng.f(); hp.jitter = 0.08f; hp.taper = 0.8f; hp.seg = 4; hp.mat = gBM.hairLeg; hp.r0 = 0.0038f;
                           if (kind == 0) { hp.length = 0.28f + 0.45f * rng.f() * (cs < 0 ? 1.3f : 0.6f); d = norm(n + ax * 0.5f + rng.onSphere() * 0.3f); hp.mat = rng.f() < 0.6f ? gBM.hairHead : gBM.hairLeg; hp.barbs = 1; hp.barbLen = 0.03f; }
                           else if (kind == 1) { hp.length = 0.20f + 0.34f * rng.f(); d = norm(n * 0.8f + ax * 0.8f + rng.onSphere() * 0.25f); hp.mat = rng.f() < 0.5f ? gBM.hairHead : gBM.hairLeg; }
                           else if (kind == 4) {
                               bool edge = std::fabs(cs) > 0.75f;      // 花粉篮两条棱上的长弯毛：向外、再向篮面合拢
                               hp.length = edge ? 0.7f + 0.55f * rng.f() : 0.18f + 0.2f * rng.f(); hp.seg = edge ? 6 : 3; hp.r0 = edge ? 0.0048f : 0.0036f;
                               d = norm(n + ax * 0.35f + rng.onSphere() * 0.2f);
                               if (edge) { hp.bend = V3(0, cs > 0 ? -0.9f : 0.9f, 0.9f); hp.bendK = 0.30f; }
                           }
                           else if (kind == 2 || kind == 5) { hp.length = 0.12f + 0.16f * rng.f(); hp.seg = 3; hp.r0 = 0.0044f; d = norm(n * 0.7f + ax * 1.0f + rng.onSphere() * 0.15f); hp.mat = rng.f() < 0.7f ? gBM.hairLeg : gBM.hairDark; hp.taper = 0.6f; }
                           else { hp.length = 0.07f + 0.08f * rng.f(); hp.seg = 2; d = norm(n * 0.6f + ax + rng.onSphere() * 0.2f); }
                           if (kind != 4) { hp.length *= 0.45f + 0.55f * legK; if (legK < 1 && hp.mat == gBM.hairHead && rng.f() < 0.5f) hp.mat = gBM.hairLeg; }
                           return true; });
                // 较粗的硬刚毛
                if (kind != 3) B.grow(f, true, (int)(560 * hairK),
                       [=](float u, float v, const V3&, const V3&) { if (u < 0.08f || u > 0.96f) return 0.f; if ((kind == 4 && std::sin(v * TAU) > 0.25f && u > 0.2f) || (kind == 5 && std::sin(v * TAU) < -0.2f)) return 0.f; return 1.f; },
                       [&](float, float, const V3&, const V3& n, HairP& hp, V3& d) {
                           V3 ax(1, 0, 0);
                           hp.rnd = rng.f(); hp.jitter = 0.04f; hp.taper = 0.9f; hp.seg = 4; hp.r0 = 0.0060f + 0.002f * rng.f();
                           hp.length = (kind == 2 || kind == 5 ? 0.20f : 0.30f) + 0.28f * rng.f(); hp.mat = rng.f() < 0.75f ? gBM.hairLeg : gBM.hairDark;
                           hp.bend = ax; hp.bendK = 0.12f;
                           d = norm(n * 1.0f + ax * (0.55f + 0.4f * rng.f()) + rng.onSphere() * 0.15f);
                           return true; });
                std::vector<HairRange> hosts{{c0, W.cyls.size(), 1.0f}};
                float amt = kind == 2 || kind == 5 ? 2.5f : (kind == 3 ? 0.3f : 1.0f);
                dust(hosts, (int)(5 * amt * pollenK), (int)(22 * amt * pollenK * hairK), 0.2f);
                (void)L;
            };
            // ---- 股节（含转节）----
            pLeg[idx][0] = reg(B.beginPart(nm + "_femur", flags));
            {
                Spine sp{V3(0, 0, 0), V3(D.Lf, 0, 0)};
                SurfFn f = mkTube(sp, V3(0, 1, 0), [=](float u, float, float& a, float& b) {
                    float k = (0.55f + 0.45f * endRound(u, 0.10f)) * std::pow(sat((1 - u) / 0.035f), 0.5f) * std::pow(sat(u / 0.02f), 0.5f) + 0.002f;   // 两端收口（否则管口是个黑洞）
                    float tro = 1 + 0.25f * std::exp(-sqr((u - 0.05f) / 0.05f)) - 0.18f * std::exp(-sqr((u - 0.13f) / 0.03f));   // 转节的膨大与缩缢
                    a = D.rf * (0.8f + 0.5f * std::sin(PI * std::pow(u, 0.7f))) * k * tro; b = D.rf * 0.85f * k * tro; }, [=](float u, float) { return legCol * (1 + 0.5f * smooth(0.9f, 1.0f, u)); });
                B.grid(f, 40, 20, gBM.leg, true, true);
                B.sphere(V3(D.Lf - 0.03f, 0, 0), D.rf * 0.40f, gBM.leg, V3(0.26f, 0.17f, 0.09f));     // 膝关节：小、颜色接近毛色
                legHair(f, 0, D.Lf);
                // 膝部的一小簇毛：从关节球面向外长，盖住正对镜头时露出的深色端面（用独立的随机数流，不影响其他毛的布局）
                { RNG kr(9000 + idx, 3);
                  for (int i = 0; i < (int)(170 * std::max(hairK, 0.3f) * P("kneeTuft", 1.0f)); i++) {
                      V3 d = kr.onSphere(); if (d.x < -0.2f) d.x = -d.x;
                      HairP hp; hp.length = 0.14f + 0.20f * kr.f(); hp.r0 = 0.0038f; hp.seg = 3; hp.mat = kr.f() < 0.6f ? gBM.hairHead : gBM.hairLeg; hp.rnd = kr.f(); hp.jitter = 0.10f; hp.taper = 0.8f;
                      B.hair(V3(D.Lf - 0.03f, 0, 0) + d * (D.rf * 0.40f), norm(d + V3(0.5f, 0, 0)), hp);
                  } }
            }
            B.endPart();
            // ---- 胫节 ----
            pLeg[idx][1] = reg(B.beginPart(nm + "_tibia", flags));
            {
                Spine sp{V3(0, 0, 0), V3(D.Lt, 0, 0)};
                SurfFn f = li == 2
                    ? mkTube(sp, V3(0, 1, 0), [=](float u, float th, float& a, float& b) { float k = endRound(u, 0.05f); a = mix(0.26f, 0.62f, std::pow(u, 0.8f)) * k; b = mix(0.20f, 0.17f, u) * k * (1 - 0.40f * std::max(0.f, std::sin(th)) * smooth(0.15f, 0.5f, u)); }, [=](float u, float v) { float s = std::sin(v * TAU); return legCol * (1 + 0.6f * std::max(0.f, s) * smooth(0.15f, 0.5f, u)); })
                    : mkTube(sp, V3(0, 1, 0), [=](float u, float, float& a, float& b) { float k = endRound(u, 0.07f); a = D.rt * (0.85f + 0.45f * u) * k; b = D.rt * 0.9f * k; }, [=](float, float) { return legCol; });
                B.grid(f, 50, 24, gBM.leg, true, true);
                if (li == 1) {   // 中足胫节端部的距
                    Spine spur{V3(D.Lt * 0.97f, -0.10f, 0), V3(D.Lt + 0.25f, -0.20f, 0.02f), V3(D.Lt + 0.48f, -0.24f, 0.03f)};
                    B.grid(mkTube(spur, V3(0, 0, 1), [](float u, float, float& a, float& b) { a = b = 0.035f * (1 - 0.85f * u) + 0.004f; }, [=](float, float) { return tarCol; }), 10, 8, gBM.leg, true, true);
                }
                if (li == 0) {   // 前足的净角器（胫节端的小片 + 基跗节的缺口，这里只做小片）
                    Spine spur{V3(D.Lt * 0.97f, -0.08f, 0), V3(D.Lt + 0.18f, -0.14f, 0), V3(D.Lt + 0.30f, -0.12f, 0)};
                    B.grid(mkTube(spur, V3(0, 0, 1), [](float u, float, float& a, float& b) { a = 0.05f * (1 - 0.7f * u) + 0.004f; b = 0.02f; }, [=](float, float) { return tarCol; }), 8, 8, gBM.leg, true, true);
                }
                legHair(f, li == 2 ? 4 : 1, D.Lt);
                if (li == 2) corbC = V3(D.Lt * 0.60f, 0, 0);
            }
            B.endPart();
            // ---- 基跗节 ----
            pLeg[idx][2] = reg(B.beginPart(nm + "_basitarsus", flags));
            {
                Spine sp{V3(0, 0, 0), V3(D.Lb, 0, 0)};
                SurfFn f = li == 2
                    ? mkTube(sp, V3(0, 1, 0), [=](float u, float, float& a, float& b) { float k = endRound(u, 0.06f); a = mix(0.52f, 0.38f, u) * k; b = 0.15f * k; }, [=](float u, float) { return mix(legCol, tarCol, u * 0.6f); })
                    : mkTube(sp, V3(0, 1, 0), [=](float u, float, float& a, float& b) { float k = endRound(u, 0.08f); a = mix(0.17f, 0.12f, u) * k * (li == 1 ? 1.15f : 1); b = 0.11f * k; }, [=](float u, float) { return mix(legCol, tarCol, u * 0.7f); });
                B.grid(f, 36, 20, gBM.leg, true, true);
                legHair(f, li == 2 ? 5 : 2, D.Lb);
                if (li == 2) {
                    // 花粉梳：基跗节内侧面（-z）上 10 排整齐的金色硬毛
                    for (int row = 0; row < 10; row++) {
                        float x = D.Lb * (0.10f + 0.085f * row);
                        float hw = mix(0.50f, 0.37f, x / D.Lb) * 0.88f;
                        int n = (int)(46 * std::max(hairK, 0.4f));
                        for (int j = 0; j < n; j++) {
                            float y = hw * (2.f * (j + rng.f()) / n - 1);
                            HairP hp; hp.length = 0.19f + 0.04f * rng.f(); hp.r0 = 0.0058f; hp.seg = 2; hp.mat = gBM.hairComb; hp.rnd = rng.f(); hp.jitter = 0.015f; hp.taper = 0.5f;
                            B.hair(V3(x + 0.01f * rng.gauss(), y, -0.14f * safesqrt(1 - sqr(y / (hw / 0.88f)))), norm(V3(0.85f, 0, -0.55f)), hp);
                        }
                    }
                }
            }
            B.endPart();
            // ---- 跗节 2-5 + 爪 ----（局部：x 沿跗节，y = 落脚面法线）
            pLeg[idx][3] = reg(B.beginPart(nm + "_tarsus", flags));
            {
                float segL[4] = {0.24f, 0.21f, 0.19f, 0.48f}; float tot = 0; for (float s : segL) tot += s;
                V3 p(0, 0, 0); std::vector<SurfFn> fs;
                for (int k = 0; k < 4; k++) {
                    float L = segL[k] * D.Ltar / tot;
                    V3 q = p + V3(L, k == 3 ? -0.05f : -0.012f * k, 0);
                    Spine sp{p, q};
                    SurfFn f = mkTube(sp, V3(0, 1, 0), [=](float u, float, float& a, float& b) { float kk = endRound(u, 0.15f); float r = k < 3 ? mix(0.060f, 0.105f, u) : mix(0.058f, 0.075f, u); a = r * 0.8f * kk; b = r * kk; }, [=](float, float) { return tarCol; });
                    B.grid(f, 10, 12, gBM.leg, true, true); fs.push_back(f);
                    p = q;
                }
                for (int c = -1; c <= 1; c += 2) {    // 一对二叉的爪
                    Spine sp{p, p + V3(0.13f, 0.03f, 0.06f * c), p + V3(0.22f, -0.10f, 0.10f * c)};
                    B.grid(mkTube(sp, V3(0, 1, 0), [](float u, float, float& a, float& b) { a = b = 0.028f * (1 - 0.85f * u) + 0.003f; }, [](float, float) { return V3(0.02f, 0.012f, 0.01f); }), 10, 8, gBM.leg, true, true);
                    Spine sp2{p + V3(0.09f, 0.02f, 0.045f * c), p + V3(0.15f, -0.04f, 0.05f * c)};
                    B.grid(mkTube(sp2, V3(0, 1, 0), [](float u, float, float& a, float& b) { a = b = 0.016f * (1 - 0.85f * u) + 0.002f; }, [](float, float) { return V3(0.02f, 0.012f, 0.01f); }), 5, 6, gBM.leg, true, true);
                }
                B.sphere(p + V3(0.05f, -0.02f, 0), 0.05f, gBM.leg, tarCol * 0.6f);   // 中垫
                for (auto& f : fs) legHair(f, 3, D.Ltar);
            }
            B.endPart();
        }

        // ================= 花粉团 =================（后足胫节局部坐标，贴在花粉篮外侧 +z）
        for (int e = 0; e < 2; e++) {
            pPellet[e] = reg(B.beginPart(std::string("bee_pellet") + (e ? "R" : "L"), flags));
            V3 cc = corbC + V3(0, 0, 0.50f);
            float sd = 3.3f + 5.1f * e;
            SurfFn pb = [=](float u, float v) {
                float th = u * PI, ph = v * TAU; V3 d(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
                float r = 1 + 0.20f * fbm3(d * 1.9f + V3(sd), 3) + 0.07f * noise3(d * 7 + V3(sd)) + 0.035f * (1 - cell3(d * 9 + V3(sd))) + 0.015f * noise3(d * 30);
                // d.y -> 沿胫节轴(x)，d.x -> 腿平面内(y)，d.z -> 外侧(z)
                return PV(cc + V3(1.15f * d.y * r, 0.84f * d.x * r + 0.12f * d.y * d.y, 0.72f * d.z * r), u, v); };
            B.grid(pb, 90, 120, gBM.pollenBall, true, false);
            for (int i = 0; i < 160; i++) {
                V3 d = rng.onSphere(); if (d.z < -0.3f) continue;
                float th = std::acos(clampf(d.y, -1, 1)), ph = std::atan2(d.z, d.x); if (ph < 0) ph += TAU;
                V3 p = pb(th / PI, ph / TAU).p;
                B.sphere(p + d * 0.006f, 0.013f + 0.007f * rng.f(), gBM.pollenGrain, POLLEN_COL * (0.85f + 0.3f * rng.f()));
            }
            B.endPart();
        }
    }

    // ============ 摆姿态：BeeState -> 每个部件的世界变换 ============
    struct LegSolve { V3 H, K, A, Bp, F, planeN, nF; };
    // 一条足的求解。要点是“连续”：落脚点平滑移动时，各关节也必须平滑移动，不能在两个解之间跳。
    // 做法：足端（基跗节+跗节）按一个仰角搭在落脚面上；落脚点离髋越近，足端立得越陡，
    // 保证踝关节始终在髋的外侧一定距离；膝关节朝“上 + 外”固定的一侧弯；腿平面法线的手性每条腿固定。
    LegSolve solveLeg(int idx, const V3& Fb, const V3& nFb) const {
        int li = idx / 2, e = idx % 2; float sg = e == 0 ? -1.f : 1.f;
        const LegDef& D = LEGS[li];
        LegSolve s; s.H = hip[idx]; s.nF = norm(nFb);
        V3 rel = Fb - s.H;
        V3 out = rel - s.nF * dot(rel, s.nF);
        float dh = len(out);
        V3 side0 = norm(V3(li == 0 ? 0.5f : (li == 2 ? -0.6f : 0.0f), 0, sg));
        // 落脚点几乎在髋正下方时，水平方向平滑地过渡到这条腿的“默认外侧”
        V3 tdir = norm(out + side0 * (0.6f * std::exp(-dh * dh / 0.5f) + 1e-3f));
        float Lfoot = D.Lb + 0.92f * D.Ltar;
        float beta0 = li == 2 ? 0.50f : 0.62f;
        float a = Lfoot * std::cos(beta0), b = Lfoot * std::sin(beta0);
        // 踝关节相对落脚点的水平方向 u：落脚点远时踝在“髋—脚”连线上（腿伸开）；
        // 落脚点靠近髋时 u 平滑地转向这条腿的外侧——腿折成 Z 形，膝和踝在外、脚收在身下
        float w = smooth(a + 2.4f, a + 0.3f, dh);
        V3 fa(li == 0 ? 1.f : -1.f, 0, 0);                    // 转动时经过的方向：前足绕前方，中后足绕后方
        V3 u = norm((V3(0) - tdir) * (1 - w) + side0 * w + fa * (0.45f * std::sin(PI * w)));
        u = norm(u - s.nF * dot(u, s.nF));
        V3 Ap = Fb + u * a + s.nF * b;
        float L2 = D.Lf + D.Lt;
        V3 ha = Ap - s.H; float reach = len(ha);
        float mx = L2 * 0.97f, mn = L2 * 0.30f;
        if (reach > mx) { Ap = s.H + ha * (mx / reach); }
        else if (reach < mn) { Ap = s.H + norm(ha + side0 * 1e-3f) * mn; }
        ha = Ap - s.H;
        V3 dir = norm(ha);
        V3 hint = norm(V3(li == 0 ? P("frontHintX", 0.30f) : (li == 2 ? -0.35f : 0), 1.0f, sg * (li == 0 ? P("frontHintZ", 0.45f) : 0.45f)));
        V3 perp = hint - dir * dot(hint, dir);
        // 提示方向与腿几乎平行时，掺入第二提示（保持连续）
        V3 hint2 = norm(V3(li == 2 ? -1.0f : (li == 0 ? 1.0f : 0.2f), 0.3f, sg * 0.3f));
        V3 perp2 = hint2 - dir * dot(hint2, dir);
        float w2 = smooth(0.35f, 0.08f, len(perp));
        perp = norm(perp + perp2 * w2);
        float d = clampf(len(ha), std::fabs(D.Lf - D.Lt) * 1.05f + 1e-3f, L2 * 0.999f);
        float x = (D.Lf * D.Lf - D.Lt * D.Lt + d * d) / (2 * d), h = safesqrt(D.Lf * D.Lf - x * x);
        s.K = s.H + dir * x + perp * h;
        s.A = s.H + dir * d;
        // 足端：从踝关节指向落脚点，各节保持自己的长度；中间在落脚面法线方向略微拱起
        V3 af = Fb - s.A; float afl = std::max(len(af), 1e-3f);
        V3 afd = af / afl;
        V3 bow = s.nF - afd * dot(s.nF, afd);
        V3 bd = norm(afd + bow * 0.18f);
        s.Bp = s.A + bd * D.Lb;
        V3 fd = norm(Fb - s.Bp + afd * 1e-3f);
        s.F = s.Bp + fd * D.Ltar;
        s.planeN = norm(cross(perp, dir)) * legSign[idx];
        return s;
    }
    float legSign[6] = {1, 1, 1, 1, 1, 1};
    // 建好模型后调用一次：按标准站姿确定每条腿平面法线的手性（朝外）
    void calibrate() {
        for (int idx = 0; idx < 6; idx++) {
            int li = idx / 2; float sg = idx % 2 == 0 ? -1.f : 1.f;
            legSign[idx] = 1;
            LegSolve s = solveLeg(idx, V3(LEGS[li].ideal.x, LEGS[li].ideal.y, LEGS[li].ideal.z * sg), V3(0, 1, 0));
            if (dot(s.planeN, V3(li == 2 ? -0.3f : 0.3f, 0, sg)) < 0) legSign[idx] = -1;
        }
    }
    void wingFrame(const BeeState& S, int e, int wing, Xf& out) const {
        float sg = e == 0 ? -1.f : 1.f;
        V3 hinge = wing == 0 ? V3(0.60f, 1.30f, sg * 1.42f) : V3(-0.30f, 1.12f, sg * 1.40f);
        // 静止：收拢搭在腹部上方
        float spread = ((wing == 0 ? 24.f : 15.f) + 30.f * S.wingOpen) * PI / 180 + (e == 0 ? 0.06f : -0.03f);
        float lift = ((wing == 0 ? 5.f : 1.f) + 12.f * S.wingOpen) * PI / 180 + (e == 0 ? 0.03f : 0.0f);
        V3 Sr = norm(V3(-std::cos(spread) * std::cos(lift), std::sin(lift), sg * std::sin(spread) * std::cos(lift)));
        V3 outw(0, 0, sg);
        V3 Cr = norm(outw - Sr * dot(outw, Sr));
        V3 Nr = cross(Sr, Cr) * sg;
        float roll = (-16.f + (wing ? 4 : 0)) * PI / 180;
        Cr = norm(Cr * std::cos(roll) + Nr * std::sin(roll));
        // 飞行：在拍动平面内扑动，行程两端翻转攻角
        const float bs = 38.f * PI / 180;                       // 拍动平面相对体背的倾角
        V3 nsp(std::sin(bs), std::cos(bs), 0), ef(std::cos(bs), -std::sin(bs), 0);
        float ph = S.wingPhase + (e == 0 ? 0.0f : 0.04f);
        float phi = (-10.f + 50.f * S.wingAmp * std::sin(ph)) * PI / 180;
        V3 Sf = V3(0, 0, sg) * std::cos(phi) + ef * std::sin(phi);
        Sf = norm(Sf + nsp * (0.10f * std::cos(2 * ph)));       // 8 字形的轻微偏离
        V3 v0 = V3(0, 0, -sg) * std::sin(phi) + ef * std::cos(phi);
        float psi = 52.f * PI / 180 * std::tanh(2.6f * std::cos(ph)) / std::tanh(2.6f);
        V3 Cf = norm(v0 * std::sin(psi) + nsp * std::cos(psi));
        Cf = norm(Cf - Sf * dot(Cf, Sf));
        V3 hingeF = wing == 0 ? hinge : V3(0.60f, 1.30f, sg * 1.42f) + (V3(0) - Cf) * 0.95f - Sf * 0.15f;   // 飞行时后翅钩在前翅后缘
        float w = S.wingFlight;
        V3 Sx = nlerp(Sr, Sf, w), Cx = nlerp(Cr, Cf, w);
        Cx = norm(Cx - Sx * dot(Cx, Sx));
        out = Xf(mix(hinge, hingeF, w), Sx, Cx, cross(Sx, Cx));
    }
    // out[partId] = 世界变换
    void apply(const BeeState& S, std::vector<Xf>& out) const {
        const Xf& X = S.X;
        out[pThorax] = X;
        // 头
        Xf R = Xf::rot(V3(0, 1, 0), S.headYaw) * Xf::rot(V3(0, 0, 1), -S.headPitch) * Xf::rot(V3(1, 0, 0), S.headRoll);
        Xf Hx = R; Hx.o = neck - R.dir(neckH);
        Xf HW = X * Hx;
        out[pHead] = HW;
        // 触角
        for (int e = 0; e < 2; e++) {
            float sg = e == 0 ? -1.f : 1.f;
            V3 sdir = norm(V3(0.62f, 0.72f + S.antPitch[e] + (e ? 0.08f : 0), sg * (0.50f + S.antYaw[e] + (e ? 0.06f : 0))));
            V3 fdir = norm(V3(1.0f, -0.42f + S.antElbow[e] - (e ? 0.10f : 0), sg * (0.48f + 0.9f * S.antYaw[e] + (e ? 0.14f : 0))));
            V3 side = norm(cross(sdir, fdir));
            Xf sc = frameX(sock[e], sdir, side);
            out[pScape[e]] = HW * sc;
            // 鞭节局部的 -y 是自然下弯的方向：让它落在柄节-鞭节平面内
            Xf fl = frameX(sock[e] + sdir * Lscape, fdir, side);
            if (dot(fl.y, sdir) < 0) { fl.y = V3(0) - fl.y; fl.z = V3(0) - fl.z; }
            out[pFlag[e]] = HW * fl;
        }
        // 喙：折叠（贴在头下方）↔ 伸出（对准目标）
        {
            V3 M = mouth;
            V3 Tw = S.probTarget; V3 d = Tw - HW.o; V3 Th(dot(d, HW.x), dot(d, HW.y), dot(d, HW.z));   // 目标点（头部局部）
            // 两节用两段反向运动学对准小花口；够不着的那一段由舌伸出去补
            V3 toT = Th - M; float dT = len(toT);
            V3 Tc = M + toT * (std::min(dT, (L1 + L2) * 0.985f) / std::max(dT, 1e-4f));
            V3 Ke = ik2(M, Tc, L1, L2, V3(-1.0f, 0.25f, 0));
            V3 d1f = norm(V3(-0.82f, -0.57f, 0)), d2f = norm(V3(-0.93f, -0.36f, 0));    // 折叠时两节都指向后下方，收成一小束
            V3 d1e = norm(Ke - M), d2e = norm(Tc - Ke);
            float e1 = smooth(0.0f, 0.7f, S.probExt), e2 = smooth(0.15f, 1.0f, S.probExt);
            V3 d1 = nlerp(d1f, d1e, e1), d2 = nlerp(d2f, d2e, e2);
            float k1 = mix(0.55f, 1.0f, e1), k2 = mix(0.36f, 1.0f, e2);     // 折叠时沿轴缩短（真实的喙是对折收进头后的凹窝）
            Xf p1 = frameX(M, d1, V3(0, 0, 1));
            V3 K = M + d1 * (L1 * k1);
            Xf p2 = frameX(K, d2, V3(0, 0, 1));
            out[pProb1] = HW * p1 * Xf::scale3(k1, 1, 1); out[pProb2] = HW * p2 * Xf::scale3(k2, 1, 1);
            float need = std::max(0.f, dT - (L1 + L2) * 0.985f);
            float ext = mix(-Lg * 0.92f, -Lg + 0.25f + need + S.lick * 0.55f, e2);        // 舌从鞘里伸出多少
            Xf g = frameX(K + d2 * (L2 * k2 + ext * k2), d2, V3(0, 0, 1)) * Xf::scale3(k2, 1, 1);
            out[pGlossa] = HW * g;
        }
        // 腹：各节绕前缘枢轴逐节弯曲 + 套叠伸缩（呼吸）
        {
            Xf A = Xf::rotAbout(V3(-2.2f, -0.2f, 0), V3(0, 0, 1), S.abdCurl * 0.5f) * Xf::rotAbout(V3(-2.2f, -0.2f, 0), V3(0, 1, 0), S.abdYaw * 0.5f);
            for (int i = 0; i < 6; i++) {
                V3 piv = abSp.at(ABD_UB_C[i]), tg = abSp.tan(mix(ABD_UB_C[i], ABD_UB_C[i + 1], 0.5f));
                float segL = (ABD_UB_C[i + 1] - ABD_UB_C[i]) * 6.6f;
                if (i > 0) A = A * Xf::rotAbout(piv, V3(0, 0, 1), S.abdCurl * 0.22f) * Xf::rotAbout(piv, V3(0, 1, 0), S.abdYaw * 0.2f) * Xf::trans(tg * (-(S.breath - 0.5f) * 0.085f * segL));
                // 呼吸时背腹方向略微鼓缩
                Xf Sc = Xf::trans(piv) * Xf::scale3(1, 1 + 0.025f * (S.breath - 0.5f), 1 + 0.012f * (S.breath - 0.5f)) * Xf::trans(V3(0) - piv);
                out[pAbd[i]] = X * A * Sc;
            }
        }
        // 翅
        for (int e = 0; e < 2; e++) for (int w = 0; w < 2; w++) { Xf wf; wingFrame(S, e, w, wf); out[pWing[e][w]] = X * wf; }
        // 足
        Xf Xi = X.inv();
        for (int idx = 0; idx < 6; idx++) {
            int li = idx / 2;
            V3 Fb = Xi.pt(S.foot[idx]), nb = Xi.dir(S.footN[idx]);
            LegSolve s = solveLeg(idx, Fb, nb);
            Xf fem = frameX(s.H, s.K - s.H, s.planeN), tib = frameX(s.K, s.A - s.K, s.planeN), bas = frameX(s.A, s.Bp - s.A, s.planeN);
            // 跗节：y 轴贴近落脚面法线
            V3 tx = norm(s.F - s.Bp); V3 tzr = cross(tx, s.nF); if (len(tzr) < 0.2f) tzr = tzr + cross(tx, s.planeN) * 0.3f; V3 tz = norm(tzr); Xf tar; tar.o = s.Bp; tar.x = tx; tar.z = tz; tar.y = cross(tz, tx);
            out[pLeg[idx][0]] = X * fem; out[pLeg[idx][1]] = X * tib; out[pLeg[idx][2]] = X * bas; out[pLeg[idx][3]] = X * tar;
            if (li == 2) {
                float k = std::max(S.pellet, 0.02f);
                // 花粉团绕花粉篮面中心缩放（越采越大）
                V3 c = corbC + V3(0, 0, 0.14f);
                out[pPellet[idx % 2]] = X * tib * Xf::trans(c) * Xf::scale3(k, k, k) * Xf::trans(V3(0) - c);
            }
        }
    }
    // 右侧（+z）的足是左侧的镜像：腿平面法线取“外侧”，所以两侧共用同一套局部几何
};
