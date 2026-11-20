// scene.h — GPU-mirrored structs, BVH builder, scene container.
#pragma once
#include "vmath.h"
#include <atomic>
#include <thread>
#include <cstring>
#include <cstdio>

// ---- structs mirrored in kernel.metal (keep sizes in sync) ----
struct Node { float mn[3]; uint32_t leftFirst; float mx[3]; uint32_t count; };
struct Tri { float p0[3]; uint32_t mat; float e1[3]; float rnd; float e2[3]; uint32_t pad; };
struct TriA { float n0[3], u0, n1[3], u1, n2[3], u2, v0, v1, v2, a0, a1, a2, pad0, pad1; };
struct Seg { float p0[3], r0, p1[3], r1; uint32_t mat; float v0, v1, rnd; };
struct Inst { float m0[12], m1[12], inv[12]; uint32_t root, primBase, flags, matBase; float tint[4]; float rnd, pad[3]; };
struct Mat {
    float base[4], base2[4], p0[4], p1[4], p2[4], sh[4];
    uint32_t type, pattern; float rough, ior; float trans, bump, coat, scale;
};
struct Glint { float pos[3], radius, col[3], pad; };
static_assert(sizeof(Node) == 32 && sizeof(Tri) == 48 && sizeof(TriA) == 80 && sizeof(Seg) == 48, "size");
static_assert(sizeof(Inst) == 192 && sizeof(Mat) == 128 && sizeof(Glint) == 32, "size");

enum { MT_DIFF = 0, MT_DIEL = 1, MT_THIN = 2, MT_EMIT = 3 };
enum { IF_MOVING = 1, IF_SEG = 2, IF_NOSHADOW = 4, IF_NOCAM = 8, IF_NOFOGSHADOW = 16 };

struct AABB {
    V3 mn{1e30f}, mx{-1e30f};
    void grow(V3 p) { mn = vmin(mn, p); mx = vmax(mx, p); }
    void grow(const AABB& b) { mn = vmin(mn, b.mn); mx = vmax(mx, b.mx); }
    float area() const { V3 e = mx - mn; return e.x < 0 ? 0 : 2 * (e.x * e.y + e.y * e.z + e.z * e.x); }
    V3 cen() const { return (mn + mx) * 0.5f; }
};

// ---- binned SAH BVH ----
struct BvhBuild {
    const AABB* box; uint32_t* idx; Node* nodes; std::atomic<uint32_t> next{1};
    void rec(uint32_t ni, uint32_t first, uint32_t count, int depth) {
        AABB b, cb;
        for (uint32_t i = first; i < first + count; i++) { const AABB& q = box[idx[i]]; b.grow(q); cb.grow(q.cen()); }
        Node& n = nodes[ni];
        n.mn[0] = b.mn.x; n.mn[1] = b.mn.y; n.mn[2] = b.mn.z; n.mx[0] = b.mx.x; n.mx[1] = b.mx.y; n.mx[2] = b.mx.z;
        auto leaf = [&]() { n.leftFirst = first; n.count = count; };
        if (count <= 2) { leaf(); return; }
        const int NB = 16;
        float bestCost = 1e30f; int bestAxis = -1, bestBin = 0;
        V3 ce = cb.mx - cb.mn;
        for (int ax = 0; ax < 3; ax++) {
            if (ce[ax] < 1e-12f) continue;
            AABB bb[NB]; uint32_t bc[NB] = {0};
            float k = NB / ce[ax];
            for (uint32_t i = first; i < first + count; i++) {
                const AABB& q = box[idx[i]];
                int bi = std::min(NB - 1, (int)((q.cen()[ax] - cb.mn[ax]) * k));
                bb[bi].grow(q); bc[bi]++;
            }
            float la[NB], ra[NB]; uint32_t lc[NB], rc[NB];
            AABB acc; uint32_t c = 0;
            for (int i = 0; i < NB - 1; i++) { acc.grow(bb[i]); c += bc[i]; la[i] = acc.area(); lc[i] = c; }
            acc = AABB(); c = 0;
            for (int i = NB - 1; i > 0; i--) { acc.grow(bb[i]); c += bc[i]; ra[i - 1] = acc.area(); rc[i - 1] = c; }
            for (int i = 0; i < NB - 1; i++) {
                if (!lc[i] || !rc[i]) continue;
                float cost = la[i] * lc[i] + ra[i] * rc[i];
                if (cost < bestCost) { bestCost = cost; bestAxis = ax; bestBin = i; }
            }
        }
        uint32_t mid;
        if (bestAxis < 0) {
            if (count <= 8) { leaf(); return; }
            mid = first + count / 2;
        } else {
            float leafCost = b.area() * count;
            if (count <= 4 && bestCost + b.area() * 1.0f >= leafCost) { leaf(); return; }
            float k = NB / ce[bestAxis]; float mn = cb.mn[bestAxis];
            uint32_t* p = std::partition(idx + first, idx + first + count, [&](uint32_t id) {
                int bi = std::min(NB - 1, (int)((box[id].cen()[bestAxis] - mn) * k));
                return bi <= bestBin; });
            mid = (uint32_t)(p - idx);
            if (mid == first || mid == first + count) mid = first + count / 2;
        }
        uint32_t l = next.fetch_add(2);
        n.leftFirst = l; n.count = 0;
        if (depth < 4 && count > 40000) {
            std::thread th([&, l, first, mid, depth]() { rec(l, first, mid - first, depth + 1); });
            rec(l + 1, mid, first + count - mid, depth + 1);
            th.join();
        } else {
            rec(l, first, mid - first, depth + 1);
            rec(l + 1, mid, first + count - mid, depth + 1);
        }
    }
};
// builds nodes (indices relative to 0) and a permutation `order`
static inline void buildBVH(const std::vector<AABB>& boxes, std::vector<Node>& nodes, std::vector<uint32_t>& order) {
    uint32_t n = (uint32_t)boxes.size();
    order.resize(n);
    for (uint32_t i = 0; i < n; i++) order[i] = i;
    nodes.assign(std::max<uint32_t>(2 * n, 2), Node{});
    if (n == 0) { // degenerate, valid empty box that nothing hits
        nodes.resize(1); Node& r = nodes[0];
        r.mn[0] = r.mn[1] = r.mn[2] = 1e30f; r.mx[0] = r.mx[1] = r.mx[2] = 1e30f; r.leftFirst = 0; r.count = 0x80000000u; return;
    }
    BvhBuild b; b.box = boxes.data(); b.idx = order.data(); b.nodes = nodes.data();
    b.rec(0, 0, n, 0);
    nodes.resize(b.next.load());
}

// ---- mesh under construction ----
struct Mesh {
    std::vector<Tri> t; std::vector<TriA> a; std::vector<Seg> s;
    void tri(V3 p0, V3 p1, V3 p2, V3 n0, V3 n1, V3 n2, V2 t0, V2 t1, V2 t2, float a0, float a1, float a2, uint32_t mat, float rnd = 0) {
        Tri T; V3 e1 = p1 - p0, e2 = p2 - p0;
        T.p0[0] = p0.x; T.p0[1] = p0.y; T.p0[2] = p0.z; T.e1[0] = e1.x; T.e1[1] = e1.y; T.e1[2] = e1.z; T.e2[0] = e2.x; T.e2[1] = e2.y; T.e2[2] = e2.z;
        T.mat = mat; T.rnd = rnd; T.pad = 0;
        TriA A;
        A.n0[0] = n0.x; A.n0[1] = n0.y; A.n0[2] = n0.z; A.n1[0] = n1.x; A.n1[1] = n1.y; A.n1[2] = n1.z; A.n2[0] = n2.x; A.n2[1] = n2.y; A.n2[2] = n2.z;
        A.u0 = t0.x; A.u1 = t1.x; A.u2 = t2.x; A.v0 = t0.y; A.v1 = t1.y; A.v2 = t2.y; A.a0 = a0; A.a1 = a1; A.a2 = a2; A.pad0 = A.pad1 = 0;
        t.push_back(T); a.push_back(A);
    }
    void seg(V3 p0, float r0, V3 p1, float r1, uint32_t mat, float v0, float v1, float rnd) {
        Seg S; S.p0[0] = p0.x; S.p0[1] = p0.y; S.p0[2] = p0.z; S.r0 = r0; S.p1[0] = p1.x; S.p1[1] = p1.y; S.p1[2] = p1.z; S.r1 = r1;
        S.mat = mat; S.v0 = v0; S.v1 = v1; S.rnd = rnd; s.push_back(S);
    }
    void clear() { t.clear(); a.clear(); s.clear(); }
    void append(const Mesh& o, const M34& xf, int matOfs = 0) {
        M34 inv = xf.inverse();
        for (size_t i = 0; i < o.t.size(); i++) {
            const Tri& T = o.t[i]; const TriA& A = o.a[i];
            V3 p0(T.p0[0], T.p0[1], T.p0[2]), e1(T.e1[0], T.e1[1], T.e1[2]), e2(T.e2[0], T.e2[1], T.e2[2]);
            auto nt = [&](const float* n) { V3 v(n[0], n[1], n[2]); return norm(V3(inv.m[0] * v.x + inv.m[4] * v.y + inv.m[8] * v.z, inv.m[1] * v.x + inv.m[5] * v.y + inv.m[9] * v.z, inv.m[2] * v.x + inv.m[6] * v.y + inv.m[10] * v.z)); };
            tri(xf.pt(p0), xf.pt(p0 + e1), xf.pt(p0 + e2), nt(A.n0), nt(A.n1), nt(A.n2), {A.u0, A.v0}, {A.u1, A.v1}, {A.u2, A.v2}, A.a0, A.a1, A.a2, T.mat + matOfs, T.rnd);
        }
        float sc = len(xf.X());
        for (const Seg& S : o.s) seg(xf.pt({S.p0[0], S.p0[1], S.p0[2]}), S.r0 * sc, xf.pt({S.p1[0], S.p1[1], S.p1[2]}), S.r1 * sc, S.mat + matOfs, S.v0, S.v1, S.rnd);
    }
};

struct BlasH { int tri = -1, seg = -1; };
struct BlasRec { uint32_t root, primBase, type; AABB box; };

struct Camera {
    V3 pos{0, 0.1f, 0.3f}, target{0, 0, 0}, up{0, 1, 0};
    float focal = 0.05f;        // metres
    float fstop = 4.0f;
    float focusDist = -1;       // <0: distance to target
    float sensorW = 0.036f;
    float blades = 7, bladeRot = 0.3f, roundness = 0.6f, catEye = 0.5f, rim = 0.25f;
    float roll = 0;
    float shutter = 0.5f;       // fraction of frame interval (informational; shots give t0/t1 themselves)
};
struct Env {
    V3 sunDir{0.3f, 0.6f, 0.4f}; V3 sunE{3.0f, 2.8f, 2.5f}; float sunAngR = 0.02f;
    V3 zen{0.25f, 0.45f, 0.9f}, hor{0.8f, 0.85f, 0.9f}, gnd{0.1f, 0.09f, 0.07f};
    float horExp = 0.5f, glow = 0.0f, glowExp = 64, type = 0;
    V3 bd0{0, 0, 0}, bd1{0, 0, 0}; float bdScale = 3, bdMix = 0; // backdrop blotches
    float bdP0 = 0, bdP1 = 0, bdP2 = 0, bdP3 = 0;
    float fogDen = 0, fogFall = 0, fogY0 = 0, fogG = 0.6f; V3 fogAmb{0.5f, 0.55f, 0.6f}; float fogMax = 100;
    float cloudStr = 0, cloudScale = 1; V2 cloudOfs{0, 0};
    float clampL = 12.0f; float eps = 1e-5f; float time = 0;
    float skyGain = 1.0f; float bdGap = 0.0f; float sunDisc = 0.0f; int fogN = 1; float fogSun = 1.0f; float fogClamp = 4.0f; int maxDiff = 5;
};

struct Scene {
    std::vector<Node> nodes; std::vector<Tri> tris; std::vector<TriA> triAs; std::vector<Seg> segs;
    std::vector<BlasRec> blas; std::vector<Inst> insts; std::vector<Mat> mats; std::vector<Glint> glints;
    std::vector<Node> tlas;
    size_t sNodes = 0, sTris = 0, sSegs = 0, sBlas = 0, sInsts = 0, sMats = 0;
    bool staticDirty = true;
    void freezeStatic() { sNodes = nodes.size(); sTris = tris.size(); sSegs = segs.size(); sBlas = blas.size(); sInsts = insts.size(); sMats = mats.size(); staticDirty = true; }
    void resetDynamic() { nodes.resize(sNodes); tris.resize(sTris); triAs.resize(sTris); segs.resize(sSegs); blas.resize(sBlas); insts.resize(sInsts); mats.resize(sMats); glints.clear(); }

    int addMat(const Mat& m) { mats.push_back(m); return (int)mats.size() - 1; }

    BlasH addBlas(Mesh& m) {
        BlasH h;
        if (!m.t.empty()) {
            size_t n = m.t.size(); std::vector<AABB> bx(n);
            for (size_t i = 0; i < n; i++) { const Tri& T = m.t[i]; V3 p0(T.p0[0], T.p0[1], T.p0[2]), e1(T.e1[0], T.e1[1], T.e1[2]), e2(T.e2[0], T.e2[1], T.e2[2]); bx[i].grow(p0); bx[i].grow(p0 + e1); bx[i].grow(p0 + e2); }
            std::vector<Node> nd; std::vector<uint32_t> ord; buildBVH(bx, nd, ord);
            BlasRec r; r.root = (uint32_t)nodes.size(); r.primBase = (uint32_t)tris.size(); r.type = 0;
            r.box.mn = V3(nd[0].mn[0], nd[0].mn[1], nd[0].mn[2]); r.box.mx = V3(nd[0].mx[0], nd[0].mx[1], nd[0].mx[2]);
            nodes.insert(nodes.end(), nd.begin(), nd.end());
            size_t b = tris.size(); tris.resize(b + n); triAs.resize(b + n);
            for (size_t i = 0; i < n; i++) { tris[b + i] = m.t[ord[i]]; triAs[b + i] = m.a[ord[i]]; }
            h.tri = (int)blas.size(); blas.push_back(r);
        }
        if (!m.s.empty()) {
            size_t n = m.s.size(); std::vector<AABB> bx(n);
            for (size_t i = 0; i < n; i++) { const Seg& S = m.s[i]; float r = std::max(S.r0, S.r1); V3 a(S.p0[0], S.p0[1], S.p0[2]), c(S.p1[0], S.p1[1], S.p1[2]); bx[i].grow(a - V3(r)); bx[i].grow(a + V3(r)); bx[i].grow(c - V3(r)); bx[i].grow(c + V3(r)); }
            std::vector<Node> nd; std::vector<uint32_t> ord; buildBVH(bx, nd, ord);
            BlasRec r; r.root = (uint32_t)nodes.size(); r.primBase = (uint32_t)segs.size(); r.type = 1;
            r.box.mn = V3(nd[0].mn[0], nd[0].mn[1], nd[0].mn[2]); r.box.mx = V3(nd[0].mx[0], nd[0].mx[1], nd[0].mx[2]);
            nodes.insert(nodes.end(), nd.begin(), nd.end());
            size_t b = segs.size(); segs.resize(b + n);
            for (size_t i = 0; i < n; i++) segs[b + i] = m.s[ord[i]];
            h.seg = (int)blas.size(); blas.push_back(r);
        }
        return h;
    }
    void addInst1(int bi, const M34& xf, int matBase, V3 tint, float rnd, const M34* xf1, uint32_t flags) {
        if (bi < 0) return;
        const BlasRec& b = blas[bi];
        Inst I; memset(&I, 0, sizeof(I));
        memcpy(I.m0, xf.m, 48); memcpy(I.m1, (xf1 ? *xf1 : xf).m, 48);
        M34 inv = xf.inverse(); memcpy(I.inv, inv.m, 48);
        I.root = b.root; I.primBase = b.primBase; I.flags = flags | (xf1 ? IF_MOVING : 0) | (b.type ? IF_SEG : 0); I.matBase = matBase;
        I.tint[0] = tint.x; I.tint[1] = tint.y; I.tint[2] = tint.z; I.tint[3] = (float)bi; I.rnd = rnd;
        insts.push_back(I);
    }
    void addInst(BlasH h, const M34& xf, int matBase = 0, V3 tint = V3(1), float rnd = 0, const M34* xf1 = nullptr, uint32_t flags = 0) {
        addInst1(h.tri, xf, matBase, tint, rnd, xf1, flags); addInst1(h.seg, xf, matBase, tint, rnd, xf1, flags);
    }
    void addGlint(V3 p, V3 col, float soft = 0.06f) { Glint g; g.pos[0] = p.x; g.pos[1] = p.y; g.pos[2] = p.z; g.radius = 0; g.col[0] = col.x; g.col[1] = col.y; g.col[2] = col.z; g.pad = soft; glints.push_back(g); }

    static void xfBox(const AABB& b, const float* m, AABB& out) {
        for (int i = 0; i < 8; i++) {
            V3 p((i & 1) ? b.mx.x : b.mn.x, (i & 2) ? b.mx.y : b.mn.y, (i & 4) ? b.mx.z : b.mn.z);
            out.grow(V3(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]));
        }
    }
    void buildTLAS() {
        size_t n = insts.size(); std::vector<AABB> bx(n);
        for (size_t i = 0; i < n; i++) {
            const Inst& I = insts[i]; const BlasRec& b = blas[(int)I.tint[3]];
            xfBox(b.box, I.m0, bx[i]); if (I.flags & IF_MOVING) xfBox(b.box, I.m1, bx[i]);
        }
        std::vector<uint32_t> ord; buildBVH(bx, tlas, ord);
        std::vector<Inst> tmp(n); for (size_t i = 0; i < n; i++) tmp[i] = insts[ord[i]];
        insts.swap(tmp);
    }
};

static inline Mat mkMat(V3 base, float rough = 0.5f, float coat = 1.0f, uint32_t pattern = 0) {
    Mat m; memset(&m, 0, sizeof(m));
    m.base[0] = base.x; m.base[1] = base.y; m.base[2] = base.z; m.base[3] = 1;
    m.base2[0] = base.x; m.base2[1] = base.y; m.base2[2] = base.z;
    m.type = MT_DIFF; m.pattern = pattern; m.rough = rough; m.ior = 1.5f; m.coat = coat; m.scale = 1; m.bump = 0;
    return m;
}
static inline void set4(float* d, V3 v, float w = 0) { d[0] = v.x; d[1] = v.y; d[2] = v.z; d[3] = w; }
static inline void set4(float* d, float a, float b, float c, float e) { d[0] = a; d[1] = b; d[2] = c; d[3] = e; }
