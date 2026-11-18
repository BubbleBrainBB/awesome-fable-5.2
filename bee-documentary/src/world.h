// world.h — 场景容器：部件（每个部件是一个刚体，有自己的 BLAS）、GPU 侧的数据布局、自写的分箱 SAH BVH。
#pragma once
#include "core.h"
#include "shared.h"
#include <atomic>
#include <thread>
#include <string>
#include <map>

// ---- 与 kernel.metal 一一对应的布局 ----
struct GNode { float lo[3]; uint32_t a; float hi[3]; uint32_t n; };
struct GTri { float p0[3], e1[3], e2[3]; };
struct GTriIdx { uint32_t i0, i1, i2, mat; };
struct GVert { float n[3]; float u; float col[3]; float v; };
struct GCyl { float p0[3]; float r0; float p1[3]; float r1; };
struct GCylAttr { float col[3]; uint32_t mat; float rnd, v0, v1, pad; };
struct GSph { float c[3]; float r; };
struct GSphAttr { float col[3]; uint32_t mat; };
struct GInst { uint32_t root, type, keyStart, numKeys, flags, part, pad0, pad1; };
struct GKey { float r0[4], r1[4], r2[4]; };
struct GMat { float kd[3]; uint32_t type; float kt[3]; uint32_t tex; float rough, f0, specW, sheen; float hairC[3]; float p0; };

// 刚体/仿射变换
struct Xf {
    V3 o, x, y, z;
    Xf() : o(0), x(1, 0, 0), y(0, 1, 0), z(0, 0, 1) {}
    Xf(const V3& o_, const V3& x_, const V3& y_, const V3& z_) : o(o_), x(x_), y(y_), z(z_) {}
    V3 pt(const V3& p) const { return o + x * p.x + y * p.y + z * p.z; }
    V3 dir(const V3& d) const { return x * d.x + y * d.y + z * d.z; }
    Xf operator*(const Xf& b) const { return Xf(pt(b.o), dir(b.x), dir(b.y), dir(b.z)); }
    static Xf rot(const V3& axis, float ang) { Xf r; V3 k = norm(axis); r.x = rotAxis(V3(1, 0, 0), k, ang); r.y = rotAxis(V3(0, 1, 0), k, ang); r.z = rotAxis(V3(0, 0, 1), k, ang); return r; }
    static Xf trans(const V3& t) { Xf r; r.o = t; return r; }
    static Xf scale(float s) { Xf r; r.x = V3(s, 0, 0); r.y = V3(0, s, 0); r.z = V3(0, 0, s); return r; }
    static Xf scale3(float a, float b, float c) { Xf r; r.x = V3(a, 0, 0); r.y = V3(0, b, 0); r.z = V3(0, 0, c); return r; }
    // 以 z 轴对准 dir 的坐标系
    static Xf look(const V3& o, const V3& zdir, const V3& up) { Xf r; r.o = o; r.z = norm(zdir); r.x = norm(cross(up, r.z)); r.y = cross(r.z, r.x); return r; }
    // 绕过 pivot 点的轴旋转
    static Xf rotAbout(const V3& pivot, const V3& axis, float ang) { Xf r = rot(axis, ang); r.o = pivot - r.dir(pivot); return r; }
    // 刚体（正交）变换的逆
    Xf inv() const {
        V3 ix(x.x, y.x, z.x), iy(x.y, y.y, z.y), iz(x.z, y.z, z.z);
        float sx = 1 / dot(x, x), sy = 1 / dot(y, y), sz = 1 / dot(z, z);
        Xf r; r.x = V3(ix.x * sx, iy.x * sy, iz.x * sz); r.y = V3(ix.y * sx, iy.y * sy, iz.y * sz); r.z = V3(ix.z * sx, iy.z * sy, iz.z * sz);
        // 上式按“列正交”求逆：inv = diag(1/|c|^2) * M^T
        r.x = V3(x.x * sx, y.x * sy, z.x * sz); r.y = V3(x.y * sx, y.y * sy, z.y * sz); r.z = V3(x.z * sx, y.z * sy, z.z * sz);
        r.o = V3(0) - r.dir(o);
        return r;
    }
};
static inline Xf lerpXf(const Xf& a, const Xf& b, float t) { return Xf(mix(a.o, b.o, t), mix(a.x, b.x, t), mix(a.y, b.y, t), mix(a.z, b.z, t)); }

struct AABB {
    V3 lo, hi;
    AABB() : lo(1e30f), hi(-1e30f) {}
    void grow(const V3& p) { lo = vmin(lo, p); hi = vmax(hi, p); }
    void grow(const AABB& b) { lo = vmin(lo, b.lo); hi = vmax(hi, b.hi); }
    float area() const { V3 e = hi - lo; return e.x < 0 ? 0 : 2 * (e.x * e.y + e.y * e.z + e.z * e.x); }
    bool valid() const { return hi.x >= lo.x; }
};

struct Part {
    std::string name;
    uint32_t t0 = 0, t1 = 0, c0 = 0, c1 = 0, s0 = 0, s1 = 0;
    int rootT = -1, rootC = -1, rootS = -1;
    uint32_t flags = L_FG;
    bool hairy = false;
};

struct World {
    std::vector<GVert> verts; std::vector<GTri> tris; std::vector<GTriIdx> tidx;
    std::vector<GCyl> cyls; std::vector<GCylAttr> cattr; std::vector<GSph> sphs; std::vector<GSphAttr> sattr;
    std::vector<GNode> nodes; std::vector<GMat> mats;
    std::vector<Part> parts;
    std::map<std::string, int> partIndex;
    std::vector<float> veins; int nVeinFore = 0, nVeinHind = 0;   // 每段 8 个 float：ax,ay,bx,by, hw,0,0,0

    int addMat(const GMat& m) { mats.push_back(m); return (int)mats.size() - 1; }
    int part(const std::string& n) const { auto it = partIndex.find(n); if (it == partIndex.end()) { fprintf(stderr, "no part %s\n", n.c_str()); exit(1); } return it->second; }
    bool hasPart(const std::string& n) const { return partIndex.count(n) > 0; }

    // ---- BVH ----
    struct BuildCtx { std::vector<AABB> bb; std::vector<V3> cen; std::vector<uint32_t> idx; uint32_t base; };
    std::atomic<uint32_t> nodeCount{0};

    void buildRec(BuildCtx& c, uint32_t ni, uint32_t first, uint32_t count, int depth, int leafMax) {
        AABB nb, cb;
        for (uint32_t i = first; i < first + count; i++) { nb.grow(c.bb[c.idx[i]]); cb.grow(c.cen[c.idx[i]]); }
        GNode& nd = nodes[ni]; nd.lo[0] = nb.lo.x; nd.lo[1] = nb.lo.y; nd.lo[2] = nb.lo.z; nd.hi[0] = nb.hi.x; nd.hi[1] = nb.hi.y; nd.hi[2] = nb.hi.z;
        if (count <= 2) { nd.a = c.base + first; nd.n = count; return; }
        const int NB = 16;
        float bestCost = 1e30f; int bestAxis = -1, bestBin = 0;
        V3 ext = cb.hi - cb.lo;
        for (int ax = 0; ax < 3; ax++) {
            if (ext[ax] < 1e-9f) continue;
            AABB bins[NB]; uint32_t cnt[NB] = {0};
            float sc = NB / ext[ax];
            for (uint32_t i = first; i < first + count; i++) {
                uint32_t p = c.idx[i];
                int b = std::min(NB - 1, (int)((c.cen[p][ax] - cb.lo[ax]) * sc));
                bins[b].grow(c.bb[p]); cnt[b]++;
            }
            float la[NB], ra[NB]; uint32_t lc[NB], rc[NB];
            AABB acc; uint32_t n = 0;
            for (int i = 0; i < NB - 1; i++) { acc.grow(bins[i]); n += cnt[i]; la[i] = acc.area(); lc[i] = n; }
            acc = AABB(); n = 0;
            for (int i = NB - 1; i > 0; i--) { acc.grow(bins[i]); n += cnt[i]; ra[i - 1] = acc.area(); rc[i - 1] = n; }
            for (int i = 0; i < NB - 1; i++) {
                if (!lc[i] || !rc[i]) continue;
                float cost = la[i] * lc[i] + ra[i] * rc[i];
                if (cost < bestCost) { bestCost = cost; bestAxis = ax; bestBin = i; }
            }
        }
        uint32_t mid;
        if (bestAxis < 0 || (count <= (uint32_t)leafMax && bestCost >= nb.area() * count)) {
            if (count <= (uint32_t)leafMax) { nd.a = c.base + first; nd.n = count; return; }
            mid = first + count / 2;
        } else {
            float sc = NB / ext[bestAxis], lo = cb.lo[bestAxis];
            auto it = std::partition(c.idx.begin() + first, c.idx.begin() + first + count, [&](uint32_t p) {
                return std::min(NB - 1, (int)((c.cen[p][bestAxis] - lo) * sc)) <= bestBin; });
            mid = (uint32_t)(it - c.idx.begin());
            if (mid == first || mid == first + count) mid = first + count / 2;
        }
        uint32_t ch = nodeCount.fetch_add(2);
        nd.a = ch; nd.n = 0;
        if (count > 60000 && depth < 4) {
            std::thread th([&, ch, first, mid, depth] { buildRec(c, ch, first, mid - first, depth + 1, leafMax); });
            buildRec(c, ch + 1, mid, first + count - mid, depth + 1, leafMax);
            th.join();
        } else {
            buildRec(c, ch, first, mid - first, depth + 1, leafMax);
            buildRec(c, ch + 1, mid, first + count - mid, depth + 1, leafMax);
        }
    }
    template <class Arr> static void permute(Arr& arr, uint32_t base, const std::vector<uint32_t>& idx) {
        std::vector<typename Arr::value_type> tmp(idx.size());
        for (size_t i = 0; i < idx.size(); i++) tmp[i] = arr[base + idx[i]];
        std::copy(tmp.begin(), tmp.end(), arr.begin() + base);
    }
    int buildBLAS(int type, uint32_t a, uint32_t b) {
        uint32_t N = b - a; if (N == 0) return -1;
        BuildCtx c; c.bb.resize(N); c.cen.resize(N); c.idx.resize(N); c.base = a;
        for (uint32_t i = 0; i < N; i++) {
            AABB& bb = c.bb[i];
            if (type == PT_TRI) {
                const GTri& t = tris[a + i]; V3 p0(t.p0[0], t.p0[1], t.p0[2]), e1(t.e1[0], t.e1[1], t.e1[2]), e2(t.e2[0], t.e2[1], t.e2[2]);
                bb.grow(p0); bb.grow(p0 + e1); bb.grow(p0 + e2);
            } else if (type == PT_CYL) {
                const GCyl& g = cyls[a + i]; V3 p0(g.p0[0], g.p0[1], g.p0[2]), p1(g.p1[0], g.p1[1], g.p1[2]);
                bb.grow(p0 - V3(g.r0)); bb.grow(p0 + V3(g.r0)); bb.grow(p1 - V3(g.r1)); bb.grow(p1 + V3(g.r1));
            } else {
                const GSph& g = sphs[a + i]; V3 cc(g.c[0], g.c[1], g.c[2]); bb.grow(cc - V3(g.r)); bb.grow(cc + V3(g.r));
            }
            c.cen[i] = (bb.lo + bb.hi) * 0.5f; c.idx[i] = i;
        }
        uint32_t root = nodeCount.fetch_add(1);
        if (nodes.size() < (size_t)root + 2 * N + 2) nodes.resize(root + 2 * N + 2);
        buildRec(c, root, 0, N, 0, 4);
        if (type == PT_TRI) { permute(tris, a, c.idx); permute(tidx, a, c.idx); }
        else if (type == PT_CYL) { permute(cyls, a, c.idx); permute(cattr, a, c.idx); }
        else { permute(sphs, a, c.idx); permute(sattr, a, c.idx); }
        return (int)root;
    }
    AABB nodeBox(int root) const { AABB b; const GNode& n = nodes[root]; b.lo = V3(n.lo[0], n.lo[1], n.lo[2]); b.hi = V3(n.hi[0], n.hi[1], n.hi[2]); return b; }
};

// 每帧：各部件的变换关键帧（快门开→关，1 个或多个），由此生成实例与 TLAS
struct FramePose {
    std::vector<std::vector<Xf>> keys;     // [part][key]
    std::vector<char> visible;
    void init(size_t n) { keys.assign(n, std::vector<Xf>(1)); visible.assign(n, 1); }
};

struct TLASBuild {
    std::vector<GInst> inst; std::vector<GKey> keys; std::vector<GNode> nodes;
    static GKey toKey(const Xf& x) { GKey k; k.r0[0] = x.x.x; k.r0[1] = x.y.x; k.r0[2] = x.z.x; k.r0[3] = x.o.x; k.r1[0] = x.x.y; k.r1[1] = x.y.y; k.r1[2] = x.z.y; k.r1[3] = x.o.y; k.r2[0] = x.x.z; k.r2[1] = x.y.z; k.r2[2] = x.z.z; k.r2[3] = x.o.z; return k; }
    void rec(std::vector<AABB>& bb, std::vector<uint32_t>& idx, uint32_t ni, uint32_t first, uint32_t count) {
        AABB nb; for (uint32_t i = first; i < first + count; i++) nb.grow(bb[idx[i]]);
        nodes[ni].lo[0] = nb.lo.x; nodes[ni].lo[1] = nb.lo.y; nodes[ni].lo[2] = nb.lo.z; nodes[ni].hi[0] = nb.hi.x; nodes[ni].hi[1] = nb.hi.y; nodes[ni].hi[2] = nb.hi.z;
        if (count <= 2) { nodes[ni].a = first; nodes[ni].n = count; return; }
        // 按面积启发式在三个轴上找最优的排序切分（实例数很少，直接全搜）
        float best = 1e30f; int bax = 0; uint32_t bmid = first + count / 2;
        std::vector<uint32_t> tmp(idx.begin() + first, idx.begin() + first + count), bestOrder;
        for (int ax = 0; ax < 3; ax++) {
            std::sort(tmp.begin(), tmp.end(), [&](uint32_t a, uint32_t b) { return bb[a].lo[ax] + bb[a].hi[ax] < bb[b].lo[ax] + bb[b].hi[ax]; });
            std::vector<float> la(count), ra(count);
            AABB acc; for (uint32_t i = 0; i < count; i++) { acc.grow(bb[tmp[i]]); la[i] = acc.area(); }
            acc = AABB(); for (uint32_t i = count; i-- > 0;) { acc.grow(bb[tmp[i]]); ra[i] = acc.area(); }
            for (uint32_t i = 1; i < count; i++) { float cst = la[i - 1] * i + ra[i] * (count - i); if (cst < best) { best = cst; bax = ax; bmid = first + i; bestOrder = tmp; } }
        }
        (void)bax;
        if (!bestOrder.empty()) std::copy(bestOrder.begin(), bestOrder.end(), idx.begin() + first);
        uint32_t ch = (uint32_t)nodes.size(); nodes.push_back(GNode()); nodes.push_back(GNode());
        nodes[ni].a = ch; nodes[ni].n = 0;
        rec(bb, idx, ch, first, bmid - first);
        rec(bb, idx, ch + 1, bmid, first + count - bmid);
    }
    void build(const World& W, const FramePose& fp) {
        std::vector<GInst> raw; std::vector<AABB> bb; keys.clear();
        for (size_t p = 0; p < W.parts.size(); p++) {
            if (!fp.visible[p]) continue;
            const Part& P = W.parts[p];
            uint32_t ks = (uint32_t)keys.size(), nk = (uint32_t)fp.keys[p].size();
            for (const Xf& x : fp.keys[p]) keys.push_back(toKey(x));
            for (int ty = 0; ty < 3; ty++) {
                int root = ty == 0 ? P.rootT : (ty == 1 ? P.rootC : P.rootS);
                if (root < 0) continue;
                GInst in; in.root = (uint32_t)root; in.type = ty; in.keyStart = ks; in.numKeys = nk; in.flags = P.flags | ((ty == PT_CYL && P.hairy) ? 256u : 0u); in.part = (uint32_t)p; in.pad0 = in.pad1 = 0;
                AABB lb = W.nodeBox(root), wb;
                for (const Xf& x : fp.keys[p]) for (int c = 0; c < 8; c++) wb.grow(x.pt(V3(c & 1 ? lb.hi.x : lb.lo.x, c & 2 ? lb.hi.y : lb.lo.y, c & 4 ? lb.hi.z : lb.lo.z)));
                raw.push_back(in); bb.push_back(wb);
            }
        }
        std::vector<uint32_t> idx(raw.size()); for (size_t i = 0; i < idx.size(); i++) idx[i] = (uint32_t)i;
        nodes.clear(); nodes.reserve(raw.size() * 2 + 2); nodes.push_back(GNode());
        if (raw.empty()) { nodes[0].lo[0] = nodes[0].lo[1] = nodes[0].lo[2] = 1e30f; nodes[0].hi[0] = nodes[0].hi[1] = nodes[0].hi[2] = 1e30f; nodes[0].a = 0; nodes[0].n = 0; nodes.push_back(nodes[0]); nodes.push_back(nodes[0]); nodes[0].a = 1; inst.assign(1, GInst()); return; }
        rec(bb, idx, 0, 0, (uint32_t)raw.size());
        inst.resize(raw.size());
        for (size_t i = 0; i < raw.size(); i++) inst[i] = raw[idx[i]];
    }
};
