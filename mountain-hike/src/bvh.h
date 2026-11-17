// bvh.h — triangle meshes, binned-SAH BVH builder, two-level scene (instances of meshes)
#pragma once
#include "common.h"

struct TriC { vec3 v0, v1, v2, n0, n1, n2; vec2 t0, t1, t2; uint32_t mat; float aux = 0; };
enum Mat : uint32_t { M_BARK = 0, M_SPRUCE = 1 | 0x100, M_LARCH = 2 | 0x100, M_BARK_LARCH = 3, M_ROCK = 4, M_GRASS = 5, M_LEAF = 6 | 0x100, M_WOOD = 7, M_FERN = 8 | 0x100, M_TWIG = 9 };

struct Mesh {
    std::vector<TriC> t;
    void tri(vec3 a, vec3 b, vec3 c, vec3 na, vec3 nb, vec3 nc, vec2 ta, vec2 tb, vec2 tc, uint32_t m, float aux = 0) { t.push_back({a, b, c, na, nb, nc, ta, tb, tc, m, aux}); }
    void triFlat(vec3 a, vec3 b, vec3 c, uint32_t m, vec2 ta = v2(0, 0), vec2 tb = v2(1, 0), vec2 tc = v2(0, 1)) { vec3 n = normalize(cross(b - a, c - a)); tri(a, b, c, n, n, n, ta, tb, tc, m); }
    // quad a-b-c-d (ccw), uv corners (0,0)(1,0)(1,1)(0,1)
    void quad(vec3 a, vec3 b, vec3 c, vec3 d, uint32_t m, vec3 n = v3(0, 0, 0)) {
        if (simd_length_squared(n) == 0) n = normalize(cross(b - a, d - a));
        tri(a, b, c, n, n, n, v2(0, 0), v2(1, 0), v2(1, 1), m); tri(a, c, d, n, n, n, v2(0, 0), v2(1, 1), v2(0, 1), m);
    }
    // generalized cylinder through points with radii; uv = (around * circumference, length along)
    void tube(const std::vector<vec3>& p, const std::vector<float>& r, int sides, uint32_t m, bool cap = true, float vOff = 0) {
        int n = (int)p.size(); if (n < 2) return; std::vector<vec3> ring((size_t)n * (sides + 1)), nrm((size_t)n * (sides + 1)); std::vector<float> vv(n);
        vec3 prevU = v3(0, 0, 0); float acc = vOff;
        for (int i = 0; i < n; i++) {
            vec3 d = normalize(p[std::min(i + 1, n - 1)] - p[std::max(i - 1, 0)]);
            vec3 u = i == 0 ? normalize(cross(d, fabsf(d.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))) : normalize(prevU - d * dot(prevU, d)); prevU = u; vec3 w = cross(d, u);
            if (i > 0) acc += length(p[i] - p[i - 1]); vv[i] = acc;
            for (int k = 0; k <= sides; k++) { float a = 2 * PI * k / sides; vec3 nn = u * cosf(a) + w * sinf(a); ring[(size_t)i * (sides + 1) + k] = p[i] + nn * r[i]; nrm[(size_t)i * (sides + 1) + k] = nn; }
        }
        for (int i = 0; i + 1 < n; i++) for (int k = 0; k < sides; k++) {
            size_t a = (size_t)i * (sides + 1) + k, b = a + 1, c2 = a + sides + 1, d = c2 + 1; float c0 = 2 * PI * r[i], c1 = 2 * PI * r[i + 1];
            vec2 ta = v2(c0 * k / sides, vv[i]), tb = v2(c0 * (k + 1) / sides, vv[i]), tc = v2(c1 * k / sides, vv[i + 1]), td = v2(c1 * (k + 1) / sides, vv[i + 1]);
            tri(ring[a], ring[b], ring[d], nrm[a], nrm[b], nrm[d], ta, tb, td, m); tri(ring[a], ring[d], ring[c2], nrm[a], nrm[d], nrm[c2], ta, td, tc, m);
        }
        if (cap) { vec3 d = normalize(p[n - 1] - p[n - 2]); for (int k = 0; k < sides; k++) { size_t a = (size_t)(n - 1) * (sides + 1) + k; tri(ring[a], ring[a + 1], p[n - 1], d, d, d, v2(0, 0), v2(0.01f, 0), v2(0, 0.01f), m); } }
    }
    void append(const Mesh& o, const Mat34& x) {
        Mat34 inv = invert(x);
        auto nx = [&](vec3 n) { return normalize(v3(inv.m[0] * n.x + inv.m[4] * n.y + inv.m[8] * n.z, inv.m[1] * n.x + inv.m[5] * n.y + inv.m[9] * n.z, inv.m[2] * n.x + inv.m[6] * n.y + inv.m[10] * n.z)); };
        for (auto q : o.t) { q.v0 = xfPoint(x, q.v0); q.v1 = xfPoint(x, q.v1); q.v2 = xfPoint(x, q.v2); q.n0 = nx(q.n0); q.n1 = nx(q.n1); q.n2 = nx(q.n2); t.push_back(q); }
    }
};

struct GNode { float mn[3]; uint32_t a; float mx[3]; uint32_t b; };
struct GTri { float v0[3]; uint32_t mat; float e1[3]; uint32_t pad1; float e2[3]; uint32_t pad2; };
struct GAttr { float n0[3], n1[3], n2[3]; float uv[6]; float pad; };
struct GInst { float r0[4], r1[4], r2[4]; uint32_t blasRoot, seed, flags, pad; float tint[4]; };

struct AABB { vec3 mn = v3(1e30f, 1e30f, 1e30f), mx = v3(-1e30f, -1e30f, -1e30f);
    void grow(vec3 p) { mn = vmin(mn, p); mx = vmax(mx, p); } void grow(const AABB& b) { mn = vmin(mn, b.mn); mx = vmax(mx, b.mx); }
    float area() const { vec3 e = mx - mn; return e.x * e.y + e.y * e.z + e.z * e.x; } };

// Generic builder: prims given by bounds; outputs nodes (children adjacent) and the prim order.
struct BvhBuilder {
    const std::vector<AABB>& pb; std::vector<vec3> cen; std::vector<uint32_t> idx; std::vector<GNode> nodes; int leafMax;
    BvhBuilder(const std::vector<AABB>& b, int leafMax_) : pb(b), leafMax(leafMax_) {
        idx.resize(b.size()); cen.resize(b.size()); for (size_t i = 0; i < b.size(); i++) { idx[i] = (uint32_t)i; cen[i] = (b[i].mn + b[i].mx) * 0.5f; }
        nodes.reserve(b.size() * 2 + 2); nodes.push_back({}); if (!b.empty()) build(0, 0, (uint32_t)b.size(), 0); else { GNode& n = nodes[0]; n.a = 0; n.b = 0; for (int k = 0; k < 3; k++) { n.mn[k] = 1e30f; n.mx[k] = -1e30f; } n.b = 1; n.a = 0; }
    }
    void build(uint32_t ni, uint32_t lo, uint32_t hi, int depth) {
        AABB bb, cb; for (uint32_t i = lo; i < hi; i++) { bb.grow(pb[idx[i]]); cb.grow(cen[idx[i]]); }
        for (int k = 0; k < 3; k++) { nodes[ni].mn[k] = bb.mn[k]; nodes[ni].mx[k] = bb.mx[k]; }
        uint32_t n = hi - lo;
        if (n <= (uint32_t)leafMax || depth > 26) { nodes[ni].a = lo; nodes[ni].b = n; return; }
        vec3 ext = cb.mx - cb.mn; int ax = ext.x > ext.y ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);
        uint32_t mid = lo + n / 2;
        if (ext[ax] > 1e-6f && depth < 17) {
            const int NB = 12; AABB bbx[NB]; int cnt[NB] = {0}; float k0 = cb.mn[ax], k1 = NB / ext[ax];
            for (uint32_t i = lo; i < hi; i++) { int b = std::min(NB - 1, (int)((cen[idx[i]][ax] - k0) * k1)); cnt[b]++; bbx[b].grow(pb[idx[i]]); }
            float la[NB], ra[NB]; int lc[NB], rc[NB]; AABB acc; int c = 0;
            for (int b = 0; b < NB; b++) { acc.grow(bbx[b]); c += cnt[b]; la[b] = c ? acc.area() : 0; lc[b] = c; }
            acc = AABB(); c = 0; for (int b = NB - 1; b >= 0; b--) { acc.grow(bbx[b]); c += cnt[b]; ra[b] = c ? acc.area() : 0; rc[b] = c; }
            float best = 1e30f; int bs = -1; for (int b = 0; b < NB - 1; b++) { if (!lc[b] || !rc[b + 1]) continue; float cost = la[b] * lc[b] + ra[b + 1] * rc[b + 1]; if (cost < best) { best = cost; bs = b; } }
            if (bs >= 0) { auto it = std::partition(idx.begin() + lo, idx.begin() + hi, [&](uint32_t i) { return std::min(NB - 1, (int)((cen[i][ax] - k0) * k1)) <= bs; }); mid = (uint32_t)(it - idx.begin()); }
        }
        if (mid == lo || mid == hi) { mid = lo + n / 2; std::nth_element(idx.begin() + lo, idx.begin() + mid, idx.begin() + hi, [&](uint32_t a, uint32_t b) { return cen[a][ax] < cen[b][ax]; }); }
        uint32_t l = (uint32_t)nodes.size(); nodes.push_back({}); nodes.push_back({}); nodes[ni].a = l; nodes[ni].b = 0;
        build(l, lo, mid, depth + 1); build(l + 1, mid, hi, depth + 1);
    }
};

struct Scene {
    std::vector<GNode> blasNodes, tlasNodes; std::vector<GTri> tris; std::vector<GAttr> attrs;
    struct Blas { uint32_t root; AABB bb; size_t ntri; };
    std::vector<Blas> blas; std::vector<GInst> insts; std::vector<AABB> ibounds;

    int addMesh(const Mesh& m) {
        std::vector<AABB> pb(m.t.size()); AABB all;
        for (size_t i = 0; i < m.t.size(); i++) { pb[i].grow(m.t[i].v0); pb[i].grow(m.t[i].v1); pb[i].grow(m.t[i].v2); all.grow(pb[i]); }
        BvhBuilder b(pb, 3); uint32_t nodeBase = (uint32_t)blasNodes.size(), triBase = (uint32_t)tris.size();
        for (auto n : b.nodes) { if (n.b > 0) n.a += triBase; else n.a += nodeBase; blasNodes.push_back(n); }
        for (uint32_t i : b.idx) { const TriC& q = m.t[i]; GTri g; vec3 e1 = q.v1 - q.v0, e2 = q.v2 - q.v0;
            for (int k = 0; k < 3; k++) { g.v0[k] = q.v0[k]; g.e1[k] = e1[k]; g.e2[k] = e2[k]; } g.mat = q.mat; g.pad1 = g.pad2 = 0; tris.push_back(g);
            GAttr a; for (int k = 0; k < 3; k++) { a.n0[k] = q.n0[k]; a.n1[k] = q.n1[k]; a.n2[k] = q.n2[k]; }
            a.uv[0] = q.t0.x; a.uv[1] = q.t0.y; a.uv[2] = q.t1.x; a.uv[3] = q.t1.y; a.uv[4] = q.t2.x; a.uv[5] = q.t2.y; a.pad = q.aux; attrs.push_back(a); }
        blas.push_back({nodeBase, all, m.t.size()}); return (int)blas.size() - 1;
    }
    // o2w: object->world. sway > 0 enables wind shear (object y up), tint multiplies albedo
    void addInstance(int bi, const Mat34& o2w, vec3 tint = v3(1, 1, 1), float sway = 0, uint32_t seed = 0) {
        Mat34 w2o = invert(o2w); GInst g; memcpy(g.r0, w2o.m, 48); g.blasRoot = blas[bi].root; g.seed = sh_hu(seed + 0x1234567u); g.flags = 0; g.pad = 0;
        g.tint[0] = tint.x; g.tint[1] = tint.y; g.tint[2] = tint.z; g.tint[3] = sway; insts.push_back(g);
        AABB wb; const AABB& ob = blas[bi].bb; float pad = sway * 0.6f * std::max(0.f, ob.mx.y);
        for (int k = 0; k < 8; k++) { vec3 c = v3((k & 1) ? ob.mx.x + pad : ob.mn.x - pad, (k & 2) ? ob.mx.y : ob.mn.y, (k & 4) ? ob.mx.z + pad : ob.mn.z - pad); wb.grow(xfPoint(o2w, c)); }
        ibounds.push_back(wb);
    }
    void buildTLAS() {
        BvhBuilder b(ibounds, 2); tlasNodes = b.nodes; std::vector<GInst> ni(insts.size()); for (size_t i = 0; i < b.idx.size(); i++) ni[i] = insts[b.idx[i]]; insts.swap(ni);
        std::vector<AABB> nb(ibounds.size()); for (size_t i = 0; i < b.idx.size(); i++) nb[i] = ibounds[b.idx[i]]; ibounds.swap(nb);
    }
};
