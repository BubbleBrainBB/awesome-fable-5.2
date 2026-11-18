// model.h — 程序化建模工具：参数曲面网格化、沿样条的管体、毛发生长。几何都写进“当前部件”的局部坐标。
#pragma once
#include "world.h"

struct PV { V3 p; float u, v; V3 col; PV() : u(0), v(0), col(1) {} PV(const V3& p_, float u_, float v_, const V3& c = V3(1)) : p(p_), u(u_), v(v_), col(c) {} };
typedef std::function<PV(float, float)> SurfFn;

// Catmull-Rom 样条
struct Spine {
    std::vector<V3> c;
    Spine() {}
    Spine(std::initializer_list<V3> l) : c(l) {}
    V3 at(float t) const {
        int n = (int)c.size() - 1;
        float f = clampf(t, 0, 1) * n; int i = std::min((int)f, n - 1); float s = f - i;
        const V3& p1 = c[i]; const V3& p2 = c[i + 1];
        V3 p0 = i > 0 ? c[i - 1] : p1 * 2 - p2, p3 = i + 2 <= n ? c[i + 2] : p2 * 2 - p1;
        float s2 = s * s, s3 = s2 * s;
        return (p1 * 2 + (p2 - p0) * s + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * s2 + (p1 * 3 - p0 - p2 * 3 + p3) * s3) * 0.5f;
    }
    V3 tan(float t) const { float e = 1e-3f; return norm(at(std::min(1.f, t + e)) - at(std::max(0.f, t - e))); }
    float length() const { float L = 0; V3 a = at(0); for (int i = 1; i <= 64; i++) { V3 b = at(i / 64.f); L += len(b - a); a = b; } return L; }
};

struct HairP {
    float length = 0.5f, r0 = 0.004f; int seg = 5;
    V3 bend = V3(0); float bendK = 0;     // 向 bend 方向逐渐弯曲
    float jitter = 0.1f;                  // 每段随机抖动
    int mat = 0; V3 col = V3(1); float rnd = 0;
    int barbs = 0; float barbLen = 0.04f; // 分叉小枝（蜜蜂的羽状毛）
    float taper = 0.8f;
    float curl = 0;                       // 末端卷曲
};

static float gHairWidthK = 1.0f;          // 远景镜头里把毛加粗（同时减少根数），保持覆盖率

struct Builder {
    World& W; RNG rng;
    int cur = -1;
    Builder(World& w, uint64_t seed) : W(w), rng(seed, 77) {}

    int beginPart(const std::string& name, uint32_t flags = L_FG) {
        Part P; P.name = name; P.flags = flags;
        P.t0 = (uint32_t)W.tris.size(); P.c0 = (uint32_t)W.cyls.size(); P.s0 = (uint32_t)W.sphs.size();
        W.parts.push_back(P); cur = (int)W.parts.size() - 1; W.partIndex[name] = cur;
        return cur;
    }
    void endPart() {
        Part& P = W.parts[cur];
        P.t1 = (uint32_t)W.tris.size(); P.c1 = (uint32_t)W.cyls.size(); P.s1 = (uint32_t)W.sphs.size();
        P.hairy = P.c1 > P.c0 && W.mats[W.cattr[P.c0].mat].type == M_HAIR;
        P.rootT = W.buildBLAS(PT_TRI, P.t0, P.t1); P.rootC = W.buildBLAS(PT_CYL, P.c0, P.c1); P.rootS = W.buildBLAS(PT_SPH, P.s0, P.s1);
        cur = -1;
    }

    uint32_t vert(const V3& p, const V3& n, float u, float v, const V3& col) {
        GVert g; g.n[0] = n.x; g.n[1] = n.y; g.n[2] = n.z; g.u = u; g.v = v; g.col[0] = col.x; g.col[1] = col.y; g.col[2] = col.z;
        W.verts.push_back(g); vpos.push_back(p); return (uint32_t)W.verts.size() - 1;
    }
    std::vector<V3> vpos;   // 顶点位置（只在建模时用；GPU 上位置存在三角形里）
    void tri(uint32_t a, uint32_t b, uint32_t c, int mat) {
        V3 p0 = vpos[a], e1 = vpos[b] - p0, e2 = vpos[c] - p0;
        if (len2(cross(e1, e2)) < 1e-20f) return;
        GTri t; t.p0[0] = p0.x; t.p0[1] = p0.y; t.p0[2] = p0.z; t.e1[0] = e1.x; t.e1[1] = e1.y; t.e1[2] = e1.z; t.e2[0] = e2.x; t.e2[1] = e2.y; t.e2[2] = e2.z;
        W.tris.push_back(t); W.tidx.push_back({a, b, c, (uint32_t)mat});
    }
    void sphere(const V3& c, float r, int mat, const V3& col = V3(1)) {
        W.sphs.push_back({{c.x, c.y, c.z}, r}); W.sattr.push_back({{col.x, col.y, col.z}, (uint32_t)mat});
    }
    void cyl(const V3& a, const V3& b, float r0, float r1, int mat, const V3& col, float v0, float v1, float rnd) {
        W.cyls.push_back({{a.x, a.y, a.z}, r0, {b.x, b.y, b.z}, r1}); W.cattr.push_back({{col.x, col.y, col.z}, (uint32_t)mat, rnd, v0, v1, 0});
    }

    // 参数曲面 -> 三角网格（差分求平滑法线）。flip 控制法线朝向。
    void grid(const SurfFn& f, int nu, int nv, int mat, bool wrapV = false, bool flip = false, const Xf* xf = nullptr) {
        std::vector<PV> P((nu + 1) * (nv + 1));
        for (int i = 0; i <= nu; i++) for (int j = 0; j <= nv; j++) {
            PV q = f((float)i / nu, (float)j / nv);
            if (xf) q.p = xf->pt(q.p);
            P[i * (nv + 1) + j] = q;
        }
        auto at = [&](int i, int j) -> const V3& { return P[i * (nv + 1) + j].p; };
        std::vector<V3> N((nu + 1) * (nv + 1));
        std::vector<char> bad((nu + 1) * (nv + 1), 0);
        for (int i = 0; i <= nu; i++) for (int j = 0; j <= nv; j++) {
            int i0 = std::max(i - 1, 0), i1 = std::min(i + 1, nu);
            int j0 = j - 1, j1 = j + 1;
            if (wrapV) { if (j0 < 0) j0 = nv - 1; if (j1 > nv) j1 = 1; } else { j0 = std::max(j0, 0); j1 = std::min(j1, nv); }
            V3 n = cross(at(i1, j) - at(i0, j), at(i, j1) - at(i, j0));
            float l = len(n);
            if (l < 1e-12f) bad[i * (nv + 1) + j] = 1; else N[i * (nv + 1) + j] = (flip ? -n : n) / l;
        }
        for (int pass = 0; pass < 4; pass++)
            for (int i = 0; i <= nu; i++) for (int j = 0; j <= nv; j++) if (bad[i * (nv + 1) + j]) {
                for (int di : {-1, 1}) { int k = i + di; if (k >= 0 && k <= nu && !bad[k * (nv + 1) + j]) { N[i * (nv + 1) + j] = N[k * (nv + 1) + j]; bad[i * (nv + 1) + j] = 0; break; } }
            }
        uint32_t base = (uint32_t)W.verts.size();
        for (size_t k = 0; k < P.size(); k++) vert(P[k].p, N[k], P[k].u, P[k].v, P[k].col);
        for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
            uint32_t a = base + i * (nv + 1) + j, b = a + 1, c = a + (nv + 1), d = c + 1;
            tri(a, c, d, mat); tri(a, d, b, mat);
        }
    }

    // 沿样条的椭圆截面管体。rad(u, theta, ra, rb)：在 ref 方向与副法线方向的半径。
    void tube(const Spine& sp, const V3& ref, const std::function<void(float, float, float&, float&)>& rad, int nu, int nv, int mat,
              const std::function<V3(float, float)>& colf = nullptr, const Xf* xf = nullptr) {
        SurfFn f = [&](float u, float v) {
            V3 c = sp.at(u), t = sp.tan(u);
            V3 n = norm(ref - t * dot(ref, t)), b = cross(t, n);
            float th = v * TAU, ra, rb; rad(u, th, ra, rb);
            V3 p = c + n * (ra * std::cos(th)) + b * (rb * std::sin(th));
            return PV(p, u, v, colf ? colf(u, v) : V3(1));
        };
        grid(f, nu, nv, mat, true, true, xf);
    }

    // 一根毛：若干段逐渐变细的带状段，可带羽状小枝
    size_t hairCount = 0;
    void hair(const V3& root, const V3& dir0, const HairP& hp) {
        hairCount++;
        V3 p = root, d = norm(dir0);
        float segL = hp.length / hp.seg;
        V3 t1, t2;
        float wk = gHairWidthK;
        for (int i = 0; i < hp.seg; i++) {
            float ta = (float)i / hp.seg, tb = (float)(i + 1) / hp.seg;
            float ra = hp.r0 * (1 - hp.taper * std::pow(ta, 1.6f)) * wk, rb = hp.r0 * (1 - hp.taper * std::pow(tb, 1.6f)) * wk;
            V3 q = p + d * segL;
            cyl(p, q, ra, rb, hp.mat, hp.col, ta, tb, hp.rnd);
            if (hp.barbs > 0 && i >= 1) {
                basis(d, t1, t2);
                for (int k = 0; k < hp.barbs; k++) {
                    float s = rng.f(), ang = rng.f() * TAU;
                    V3 bp = p + d * (segL * s);
                    V3 side = t1 * std::cos(ang) + t2 * std::sin(ang);
                    V3 bd = norm(d * 0.75f + side * 0.66f);
                    float bl = hp.barbLen * (0.6f + 0.8f * rng.f()) * (1 - 0.4f * ta);
                    cyl(bp, bp + bd * bl, ra * 0.55f, ra * 0.25f, hp.mat, hp.col, ta, tb, hp.rnd);
                }
            }
            p = q;
            d = norm(d + hp.bend * hp.bendK + rng.onSphere() * hp.jitter + (hp.curl != 0 ? cross(d, hp.bend) * (hp.curl * tb) : V3(0)));
        }
    }

    // 在参数曲面上按面积均匀撒点长毛。
    int grow(const SurfFn& f, bool flip, int count,
             const std::function<float(float, float, const V3&, const V3&)>& dens,
             const std::function<bool(float, float, const V3&, const V3&, HairP&, V3&)>& setup,
             float u0 = 0, float u1 = 1, float v0 = 0, float v1 = 1) {
        const float e = 2e-3f;
        auto frame = [&](float u, float v, V3& p, V3& n) {
            p = f(u, v).p;
            V3 du = f(std::min(1.f, u + e), v).p - f(std::max(0.f, u - e), v).p;
            V3 dv = f(u, v + e).p - f(u, v - e).p;
            n = cross(du, dv); float J = len(n) / (4 * e * e); n = norm(flip ? -n : n);
            return J;
        };
        float maxJ = 0; V3 p, n;
        for (int i = 0; i < 3000; i++) maxJ = std::max(maxJ, frame(rng.range(u0, u1), rng.range(v0, v1), p, n));
        maxJ *= 1.1f;
        int made = 0, tries = 0;
        while (made < count && tries < count * 60) {
            tries++;
            float u = rng.range(u0, u1), v = rng.range(v0, v1);
            float J = frame(u, v, p, n);
            if (rng.f() * maxJ > J) continue;
            made++;
            if (rng.f() > dens(u, v, p, n)) continue;
            HairP hp; V3 d = n;
            if (!setup(u, v, p, n, hp, d)) continue;
            hair(p, d, hp);
        }
        return made;
    }
};

static float interpTab(const float* xs, const float* ys, int n, float x) {
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    int i = 0; while (i < n - 2 && x > xs[i + 1]) i++;
    float t = (x - xs[i]) / (xs[i + 1] - xs[i]);
    float y0 = ys[std::max(i - 1, 0)], y1 = ys[i], y2 = ys[i + 1], y3 = ys[std::min(i + 2, n - 1)];
    float m1 = (y2 - y0) * 0.5f, m2 = (y3 - y1) * 0.5f;
    if (i == 0) m1 = y2 - y1;
    if (i == n - 2) m2 = y2 - y1;
    float t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * y1 + (t3 - 2 * t2 + t) * m1 + (-2 * t3 + 3 * t2) * y2 + (t3 - t2) * m2;
}
static SurfFn mkTube(const Spine& sp, const V3& ref, const std::function<void(float, float, float&, float&)>& rad, const std::function<V3(float, float)>& colf = nullptr) {
    return [=](float u, float v) {
        V3 c = sp.at(u), t = sp.tan(u);
        V3 n = norm(ref - t * dot(ref, t)), b = cross(t, n);
        float th = v * TAU, ra, rb; rad(u, th, ra, rb);
        return PV(c + n * (ra * std::cos(th)) + b * (rb * std::sin(th)), u, v, colf ? colf(u, v) : V3(1));
    };
}
static SurfFn xfSurf(const SurfFn& f, const Xf& X) { return [=](float u, float v) { PV q = f(u, v); q.p = X.pt(q.p); return q; }; }
static float endRound(float u, float k = 0.06f) { return std::pow(sat(std::min(u, 1 - u) / k), 0.5f) * 0.999f + 0.001f; }

static GMat mkMat(int type, V3 kd, float rough, float f0, float specW = 1, int tex = T_NONE, V3 kt = V3(0), float sheen = 0) {
    GMat m; memset(&m, 0, sizeof(m));
    m.type = type; m.kd[0] = kd.x; m.kd[1] = kd.y; m.kd[2] = kd.z; m.kt[0] = kt.x; m.kt[1] = kt.y; m.kt[2] = kt.z; m.tex = tex;
    m.rough = rough; m.f0 = f0; m.specW = specW; m.sheen = sheen; m.hairC[0] = m.hairC[1] = m.hairC[2] = 1;
    return m;
}
// 毛发材质：C = 毛色（多次散射后的颜色），fwd = 每穿过一根毛的前向透过率
static GMat mkHair(V3 C, V3 fwd) {
    GMat m = mkMat(M_HAIR, V3(1), 0.36f, 0.05f);
    m.hairC[0] = C.x; m.hairC[1] = C.y; m.hairC[2] = C.z; m.kt[0] = fwd.x; m.kt[1] = fwd.y; m.kt[2] = fwd.z;
    return m;
}
