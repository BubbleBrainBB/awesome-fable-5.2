// flower.h — 波斯菊（Cosmos bipinnatus）：8 枚带褶、有厚度的舌状花瓣 + 斐波那契排列的管状花盘。
// 局部坐标（mm）：原点在花盘中心，+y 为花轴。
#pragma once
#include "model.h"

struct FlowerMats { int petal, petalThin, floret, anther, pollen, bud, recept, stem; };
static FlowerMats gFM;

static void initFlowerMats(World& W) {
    GMat m;
    m = mkMat(M_TRANS, V3(0.56f), 0.42f, 0.035f, 0.55f, T_PETAL, V3(0.63f), 0.10f); m.p0 = 0.72f; gFM.petal = W.addMat(m);       // 双层花瓣：每层的透过率取平方根
    m = mkMat(M_TRANS, V3(0.56f), 0.42f, 0.035f, 0.55f, T_PETAL, V3(0.40f), 0.10f); m.p0 = 1.35f; gFM.petalThin = W.addMat(m);   // 单层（中远景）
    m = mkMat(M_TRANS, V3(0.62f), 0.38f, 0.04f, 0.7f, T_FLORET, V3(0.22f)); m.p0 = 1.5f; gFM.floret = W.addMat(m);
    gFM.anther = W.addMat(mkMat(M_DIFF, V3(0.9f), 0.45f, 0.04f, 0.6f, T_FLORET));
    gFM.pollen = W.addMat(mkMat(M_DIFF, V3(1.0f), 0.6f, 0.03f, 0.4f, T_NONE, V3(0), 0.08f));
    gFM.bud = W.addMat(mkMat(M_DIFF, V3(0.8f), 0.35f, 0.04f, 0.8f, T_FLORET));
    gFM.recept = W.addMat(mkMat(M_DIFF, V3(0.8f), 0.5f, 0.04f, 0.5f));
    gFM.stem = W.addMat(mkMat(M_DIFF, V3(0.8f), 0.4f, 0.04f, 0.7f, T_STEM));
}

struct FlowerInfo {
    float Rd = 5.2f;
    std::vector<V3> top;     // 各小花顶端（花的局部坐标），供蜜蜂落脚、探蜜
    std::vector<V3> axis;
    std::vector<int> kind;   // 0 外圈开放(柱头) 1 花药筒 2 花蕾
    std::vector<float> rho;
    std::vector<V3> petalHinge, petalDir;   // 花瓣基部铰点与伸出方向
    int nPetal = 0;
};

static const V3 POLLEN_COL(0.92f, 0.52f, 0.05f);

struct PetalShape {
    float ang, L, Wmax, phi0, phi1, twist, cup, pleat, baseY, baseR, seedf, curl, notch;
    float rr[65], yy[65];
    void prep() {
        float r = baseR, y = baseY; rr[0] = r; yy[0] = y;
        for (int i = 1; i <= 64; i++) {
            float s = (i - 0.5f) / 64, ph = phi0 + (phi1 - phi0) * std::pow(s, 1.25f);
            r += std::cos(ph) * L / 64; y += std::sin(ph) * L / 64; rr[i] = r; yy[i] = y;
        }
    }
    float smax(float q) const {
        float aq = std::fabs(q);
        // 顶端的齿：波斯菊花瓣端部常有 2~3 个浅缺刻，每片不同
        return 1 - 0.05f * q * q - notch * (0.5f - 0.5f * std::cos(3 * PI * q + seedf)) - 0.16f * sqr(smooth(0.72f, 1.0f, aq))
               - 0.02f * noise3(V3(q * 9 + seedf, seedf * 2.1f, 0.7f));
    }
    V3 pos(float a, float q) const {
        float s = a * smax(q);
        float f = s * 64; int i = std::min((int)f, 63); float t = f - i;
        float r = mix(rr[i], rr[i + 1], t), y = mix(yy[i], yy[i + 1], t);
        float ph = phi0 + (phi1 - phi0) * std::pow(s, 1.25f);
        float Wd = 0.9f + (Wmax - 0.9f) * std::pow(std::sin(std::min(s / 0.68f, 1.f) * PI / 2), 0.95f);
        float xl = q * Wd;
        float env = std::pow(std::sin(PI * std::min(1.f, s * 1.02f)), 0.6f);
        float hp = pleat * env * (0.55f * std::cos(PI * 5.5f * q) + 0.2f * std::cos(PI * 11 * q + 1.0f) + 0.25f * noise3(V3(q * 6 + seedf, s * 1.5f, seedf))) * (0.35f + 0.65f * s);
        float hc = cup * q * q * Wd * (0.3f + 0.7f * s);
        float hn = 0.35f * s * noise3(V3(s * 2.2f + seedf, q * 1.3f, seedf * 3.1f)) + 0.22f * std::fabs(q) * q * q * noise3(V3(s * 7 + seedf * 5, seedf, 0.3f)) * Wd * 0.25f;
        // 边缘不规则的卷曲与波浪
        float aq = std::fabs(q);
        float edge = smooth(0.62f, 1.0f, aq);
        float hcurl = curl * edge * edge * Wd * 0.22f * (0.6f + 0.8f * noise3(V3(s * 3.0f + seedf * 1.3f, q > 0 ? 1.f : -1.f, seedf)))
                      + 0.10f * edge * std::sin(s * 21 + seedf + (q > 0 ? 0 : 2.f)) * (0.4f + s)
                      + 0.5f * curl * sqr(smooth(0.80f, 1.0f, s)) * noise3(V3(q * 2.5f + seedf, seedf * 0.7f, 1.9f));
        float tw = twist * s;
        V3 er(std::cos(ang), 0, std::sin(ang)), et(-std::sin(ang), 0, std::cos(ang)), ey(0, 1, 0);
        V3 nc = er * (-std::sin(ph)) + ey * std::cos(ph);
        V3 lat = et * std::cos(tw) + nc * std::sin(tw);
        V3 nn = nc * std::cos(tw) - et * std::sin(tw);
        return er * r + ey * y + lat * xl + nn * (hp + hc + hn + hcurl);
    }
    float sOf(float a, float q) const { return a * smax(q); }
};

// 一朵波斯菊。lod: 0 主角（全细节、花瓣有厚度、各花瓣是独立部件），1 中景，2 远景。
static void buildCosmos(Builder& B, const std::string& name, uint64_t seed, int lod, const V3& tint, FlowerInfo* info, uint32_t flags, float scale = 1.f, float openK = 1.f) {
    RNG rng(seed, 11);
    World& W = B.W;
    const float Rd = 5.2f * scale;
    if (info) info->Rd = Rd;
    int NP = 8;
    if (info) info->nPetal = NP;
    if (lod > 0) B.beginPart(name, flags);
    // ---- 花瓣 ----
    for (int k = 0; k < NP; k++) {
        PetalShape ps;
        ps.ang = (k + 0.12f * rng.gauss()) * TAU / NP + 0.3f;
        ps.L = (26.5f + 2.5f * rng.gauss()) * scale; ps.Wmax = (8.6f + 0.7f * rng.gauss()) * scale;
        ps.phi0 = (0.30f + 0.10f * rng.gauss()) * openK + (1 - openK) * 1.1f; ps.phi1 = (-0.42f + 0.16f * rng.gauss()) * openK + (1 - openK) * 0.9f;
        ps.twist = 0.22f * rng.gauss(); ps.cup = 0.05f + 0.05f * rng.f(); ps.pleat = (0.15f + 0.06f * rng.f()) * scale;
        ps.baseY = (-1.75f + ((k & 1) ? 0.22f : 0.f) + 0.05f * rng.gauss()) * scale; ps.baseR = 4.7f * scale; ps.seedf = rng.f() * 50;
        ps.curl = (0.5f + 0.9f * rng.f()) * (rng.f() < 0.5f ? 1 : -1); ps.notch = 0.03f + 0.05f * rng.f();
        ps.prep();
        V3 tn = tint * V3(1 + 0.05f * rng.gauss(), 1 + 0.05f * rng.gauss(), 1 + 0.05f * rng.gauss());
        if (lod == 0) {
            B.beginPart(name + "_petal" + std::to_string(k), flags);
            int nu = 220, nv = 120;
            auto nrm = [&](float a, float q) {
                const float e = 2e-3f;
                V3 du = ps.pos(std::min(1.f, a + e), q) - ps.pos(std::max(0.f, a - e), q), dv = ps.pos(a, std::min(1.f, q + e)) - ps.pos(a, std::max(-1.f, q - e));
                return norm(cross(dv, du));     // 朝上（近轴面）
            };
            auto thick = [&](float a, float q) {
                float s = ps.sOf(a, q);
                float rim = std::pow(sat(1 - std::max(std::pow(std::fabs(q), 8.f), std::pow(a, 14.f))), 0.45f);
                return (0.20f - 0.10f * s) * scale * rim + 0.004f;
            };
            // 上表面
            B.grid([&](float a, float qq) { float q = qq * 2 - 1; return PV(ps.pos(a, q), ps.sOf(a, q), q + 4 * k, tn); }, nu, nv, gFM.petal, false, true);
            // 下表面（远轴面，颜色略浅、脉更明显）
            B.grid([&](float a, float qq) { float q = qq * 2 - 1; V3 p = ps.pos(a, q) - nrm(a, q) * thick(a, q); return PV(p, ps.sOf(a, q), q + 4 * k, tn * V3(1.03f, 1.10f, 1.06f)); }, nu, nv, gFM.petal, false, false);
            B.endPart();
            if (info) { info->petalHinge.push_back(V3(std::cos(ps.ang), 0, std::sin(ps.ang)) * ps.baseR + V3(0, ps.baseY, 0)); info->petalDir.push_back(V3(std::cos(ps.ang), 0, std::sin(ps.ang))); }
        } else {
            int nu = lod == 1 ? 36 : 12, nv = lod == 1 ? 16 : 6;
            B.grid([&](float a, float qq) { float q = qq * 2 - 1; return PV(ps.pos(a, q), ps.sOf(a, q), q + 4 * k, tn); }, nu, nv, gFM.petalThin, false, true);
        }
    }
    // ---- 花托、花萼与花梗 ----
    if (lod == 0) B.beginPart(name + "_base", flags);
    {
        B.grid([&](float u, float v) { float r = u * Rd, th = v * TAU; return PV(V3(r * std::cos(th), (-1.9f + 1.3f * (1 - u * u)) * scale - 0.02f, r * std::sin(th)), u, v, V3(0.55f, 0.6f, 0.12f)); },
               lod == 0 ? 24 : 6, lod == 0 ? 64 : 12, gFM.recept, true, true);
        B.grid([&](float u, float v) {
            float th = v * TAU, y = mix(-6.5f, -1.75f, u) * scale, r = mix(1.0f, Rd * 1.02f, std::pow(u, 1.8f)) * (1 + 0.04f * std::cos(8 * th) * u);
            return PV(V3(r * std::cos(th), y, r * std::sin(th)), u, v, V3(0.30f, 0.46f, 0.12f)); }, lod == 0 ? 40 : 6, lod == 0 ? 64 : 10, gFM.stem, true, false);
        if (lod == 0) {
            // 萼片（外层苞片）：8 枚披针形小片，贴着花瓣基部下方
            for (int k = 0; k < 8; k++) {
                float a0 = (k + 0.5f) * TAU / 8 + 0.3f + 0.1f * rng.gauss(); float Ls = 9.0f + 1.5f * rng.f();
                V3 er(std::cos(a0), 0, std::sin(a0)), et(-std::sin(a0), 0, std::cos(a0));
                B.grid([&](float s, float qq) {
                    float q = qq * 2 - 1; float w = 1.5f * std::pow(std::sin(PI * std::min(1.f, 0.15f + 0.85f * s)), 0.7f) * (1 - s * 0.8f);
                    float r = 3.0f + Ls * s * std::cos(0.15f - 0.9f * s), y = -4.6f + Ls * s * std::sin(0.15f - 0.9f * s) * 0.6f - 0.4f * q * q;
                    return PV(er * r + V3(0, y, 0) + et * (q * w), s, qq, V3(0.26f, 0.42f, 0.11f) * (1 - 0.3f * s)); }, 14, 6, gFM.stem, false, false);
            }
        }
        Spine sp{V3(0, -6.0f, 0) * scale, V3(0.4f, -20, 0.3f) * scale, V3(1.8f, -45, 1.5f) * scale, V3(5, -90, 4) * scale, V3(9, -160, 6) * scale, V3(12, -260, 6) * scale};
        B.tube(sp, V3(1, 0, 0.2f), [&](float u, float th, float& ra, float& rb) { ra = rb = (1.05f - 0.15f * u) * scale * (1 + 0.04f * std::cos(5 * th)); }, lod == 0 ? 120 : 16, lod == 0 ? 20 : 6, gFM.stem,
               [](float, float) { return V3(0.28f, 0.45f, 0.12f); });
    }
    if (lod >= 1) {
        B.grid([&](float u, float v) {
            float r = u * Rd, th = v * TAU; float bump = lod == 1 ? 0.35f * scale * (0.5f + 0.5f * std::sin(u * 19 + 5 * th) * std::sin(u * 23 - 8 * th)) : 0;
            return PV(V3(r * std::cos(th), (0.5f - 1.4f * u * u) * scale + bump, r * std::sin(th)), u, v, mix(V3(0.80f, 0.62f, 0.05f), V3(0.95f, 0.70f, 0.06f), u)); },
               lod == 1 ? 14 : 5, lod == 1 ? 28 : 10, gFM.bud, true, true);
        B.endPart();
        return;
    }
    B.endPart();
    // ---- 花盘小花 ----
    B.beginPart(name + "_florets", flags);
    const int NF = 118;
    for (int i = 0; i < NF; i++) {
        float rho = std::sqrt((i + 0.6f) / NF), th = i * 2.39996323f;
        float r = rho * Rd * 0.965f;
        V3 er(std::cos(th), 0, std::sin(th));
        V3 base = er * r + V3(0, -1.9f + 1.3f * (1 - rho * rho), 0);
        V3 ax = norm(V3(0, 1, 0) + er * (0.50f * rho * rho + 0.08f * rho) + V3(rng.gauss(), 0, rng.gauss()) * 0.05f);
        int kind = rho > 0.80f ? 0 : (rho > 0.585f ? 1 : 2);
        if (kind == 1 && rng.f() < 0.12f) kind = 2;
        Xf Lx = Xf::look(base, ax, er) * Xf::rot(V3(0, 0, 1), rng.f() * TAU);   // 局部 z = 小花轴
        V3 yel = V3(0.96f, 0.72f, 0.05f) * (0.92f + 0.12f * rng.f());
        float topZ;
        if (kind == 2) {
            float t = sat(rho / 0.6f);
            float Hb = mix(1.25f, 2.45f, t * t) * (0.94f + 0.12f * rng.f()), rb = mix(0.27f, 0.43f, t) * (0.95f + 0.1f * rng.f());
            V3 col = mix(V3(0.62f, 0.66f, 0.10f), V3(0.95f, 0.70f, 0.05f), smooth(0.05f, 0.5f, rho)) * (0.9f + 0.15f * rng.f());
            float ph = rng.f() * TAU;
            B.grid([&](float u, float v) {
                float thv = v * TAU;
                float rr = u < 0.72f ? rb * (0.50f + 0.50f * smooth(0.f, 0.55f, u)) : rb * std::pow(std::cos((u - 0.72f) / 0.28f * PI / 2), 0.62f);
                float crease = smooth(0.45f, 0.95f, u) * std::pow(std::fabs(std::cos(2.5f * thv + ph)), 5.f);
                rr *= 1 - 0.16f * crease;
                V3 c = col * (1 - 0.25f * crease) * mix(0.8f, 1.0f, smooth(0.2f, 0.8f, u));
                return PV(V3(rr * std::cos(thv), rr * std::sin(thv), u * Hb), u, v, c); }, 20, 22, gFM.bud, true, false, &Lx);
            topZ = Hb;
        } else {
            float Hc = (kind == 0 ? 2.75f : 2.55f) * (0.95f + 0.1f * rng.f()), rt = 0.31f, Ll = 0.62f * (0.9f + 0.2f * rng.f());
            B.grid([&](float u, float v) {
                float thv = v * TAU, rr = mix(0.17f, rt, std::pow(u, 2.2f));
                return PV(V3(rr * std::cos(thv), rr * std::sin(thv), u * Hc), u * 0.5f, v, yel * mix(0.75f, 1.f, u)); }, 10, 24, gFM.floret, true, false, &Lx);
            float psi0 = 0.5f + 0.2f * rng.f(), psi1 = (kind == 0 ? 2.0f : 1.55f) + 0.3f * rng.f();
            for (int k = 0; k < 5; k++) {
                float pk = k * TAU / 5;
                V3 e_r(std::cos(pk), std::sin(pk), 0), e_t(-std::sin(pk), std::cos(pk), 0), ez(0, 0, 1);
                float lk = Ll * (0.9f + 0.2f * rng.f()), p1 = psi1 + 0.25f * rng.gauss();
                B.grid([&](float s, float wv) {
                    float w = wv * 2 - 1;
                    V3 C = e_r * rt + ez * Hc; int n = 8;
                    for (int j = 0; j < n; j++) { float sj = (j + 0.5f) / n * s; float ps = psi0 + (p1 - psi0) * sj; C += (e_r * std::sin(ps) + ez * std::cos(ps)) * (lk * s / n); }
                    float hw = 0.19f * (1 - std::pow(s, 1.5f)) + 0.004f;
                    float ps = psi0 + (p1 - psi0) * s;
                    V3 nr = e_r * (-std::cos(ps)) + ez * std::sin(ps);
                    V3 p = C + e_t * (w * hw) + e_r * (rt * (std::cos(0.628f * w) - 1) * sqr(1 - s)) + nr * (0.05f * (1 - w * w) * s * (1 - s) * 4);
                    return PV(p, 0.5f + 0.5f * s, wv, yel * (1 - 0.12f * (1 - std::fabs(w)) * s)); }, 8, 6, gFM.floret, false, false, &Lx);
            }
            float Ha = kind == 1 ? (1.15f + 0.35f * rng.f()) : (0.35f + 0.3f * rng.f());
            float ra = 0.155f;
            float ph = rng.f() * TAU;
            B.grid([&](float u, float v) {
                float thv = v * TAU, z = Hc - 0.5f + u * (Ha + 0.5f);
                float rr = ra * (1 + 0.09f * std::cos(5 * thv + ph)) * (1 - 0.25f * smooth(0.8f, 1.f, u));
                z += 0.13f * smooth(0.9f, 1.f, u) * std::max(0.f, std::cos(5 * thv + ph));
                V3 c = mix(V3(0.16f, 0.07f, 0.03f), V3(0.30f, 0.15f, 0.04f), smooth(0.75f, 1.f, u));
                return PV(V3(rr * std::cos(thv), rr * std::sin(thv), z), u, v, c); }, 12, 18, gFM.anther, true, false, &Lx);
            V3 tipc = Lx.pt(V3(0, 0, Hc + Ha));
            V3 wax = Lx.dir(V3(0, 0, 1));
            if (kind == 1) {
                B.sphere(tipc - wax * 0.03f, 0.125f, gFM.pollen, POLLEN_COL * 0.9f);
                int ng = 30 + (int)(rng.f() * 40);
                for (int g = 0; g < ng; g++) {
                    V3 d = rng.onSphere(); d.y = std::fabs(d.y);
                    V3 dd = Lx.dir(V3(d.x, d.z, d.y * 0.9f));
                    float rg = 0.014f + 0.006f * rng.f();
                    B.sphere(tipc + dd * (0.11f + 0.07f * rng.f()) + wax * 0.03f, rg, gFM.pollen, POLLEN_COL * (0.85f + 0.3f * rng.f()));
                }
                topZ = Hc + Ha + 0.15f;
            } else {
                float Hs = 0.75f + 0.3f * rng.f();
                Spine st{V3(0, 0, Hc + Ha - 0.2f), V3(0, 0, Hc + Ha + Hs * 0.5f), V3(0, 0, Hc + Ha + Hs)};
                V3 scol = V3(0.92f, 0.62f, 0.05f);
                B.tube(st, V3(1, 0, 0), [&](float, float, float& a, float& b) { a = b = 0.06f; }, 5, 8, gFM.floret, [&](float, float) { return scol; }, &Lx);
                float a0 = rng.f() * PI;
                for (int sgn = -1; sgn <= 1; sgn += 2) {
                    V3 dr(std::cos(a0) * sgn, std::sin(a0) * sgn, 0);
                    float Rc = 0.24f + 0.08f * rng.f(), sweep = 2.2f + 0.8f * rng.f();
                    Spine br; V3 c0(0, 0, Hc + Ha + Hs);
                    for (int j = 0; j <= 6; j++) { float an = sweep * j / 6; br.c.push_back(c0 + dr * (Rc * (1 - std::cos(an))) + V3(0, 0, Rc * std::sin(an))); }
                    B.tube(br, V3(-dr.y, dr.x, 0.1f), [&](float u, float, float& a, float& b) { a = b = 0.058f * (1 - 0.6f * u) + 0.012f; }, 12, 8, gFM.floret,
                           [&](float u, float) { return mix(scol, V3(0.85f, 0.42f, 0.04f), u); }, &Lx);
                }
                int ng = (int)(rng.f() * 14);
                for (int g = 0; g < ng; g++) {
                    V3 d = rng.onSphere();
                    B.sphere(Lx.pt(V3(0, 0, Hc + Ha + Hs + 0.1f) + d * (0.12f + 0.2f * rng.f())), 0.014f + 0.006f * rng.f(), gFM.pollen, POLLEN_COL * (0.85f + 0.3f * rng.f()));
                }
                topZ = Hc + Ha + Hs + 0.25f;
            }
        }
        if (info) { info->top.push_back(Lx.pt(V3(0, 0, topZ))); info->axis.push_back(Lx.dir(V3(0, 0, 1))); info->kind.push_back(kind); info->rho.push_back(rho); }
    }
    B.endPart();
    (void)W;
}
