// geom.h — mesh building toolkit, BVH builder, scene container (prototypes + instances)
#pragma once
#include "base.h"
#include <atomic>
#include <thread>

struct Vtx { V3 p, n; float u, v, w; };
struct Mesh {
  std::vector<Vtx> v;
  std::vector<u32> idx, mat;
  u32 add(V3 p, V3 n, float u, float vv, float w = 0) { v.push_back({p, n, u, vv, w}); return (u32)v.size() - 1; }
  void tri(u32 a, u32 b, u32 c, u32 m) { idx.push_back(a); idx.push_back(b); idx.push_back(c); mat.push_back(m); }
  void quad(u32 a, u32 b, u32 c, u32 d, u32 m) { tri(a, b, c, m); tri(a, c, d, m); }
  size_t ntri() const { return mat.size(); }
  void append(const Mesh& o, const M34& xf, float wOverride = -1e9f) {
    M34 inv = mInverse(xf);
    u32 base = (u32)v.size();
    for (auto q : o.v) {
      V3 n(inv.m[0] * q.n.x + inv.m[4] * q.n.y + inv.m[8] * q.n.z, inv.m[1] * q.n.x + inv.m[5] * q.n.y + inv.m[9] * q.n.z, inv.m[2] * q.n.x + inv.m[6] * q.n.y + inv.m[10] * q.n.z);
      q.p = xf.pt(q.p); q.n = norm(n);
      if (wOverride > -1e8f) q.w = wOverride;
      v.push_back(q);
    }
    for (u32 i : o.idx) idx.push_back(base + i);
    for (u32 m : o.mat) mat.push_back(m);
  }
};

// Parametric grid patch: P(u,v), u in [u0,u1], v in [v0,v1]; normals from finite differences
typedef std::function<V3(float, float)> SurfFn;
static inline void addGrid(Mesh& M, int nu, int nv, float u0, float u1, float v0, float v1, const SurfFn& P, u32 mat, const M34& xf = M34(), float w = 0, bool flip = false) {
  u32 base = (u32)M.v.size();
  float eu = (u1 - u0) * 0.002f, ev = (v1 - v0) * 0.002f;
  M34 inv = mInverse(xf);
  for (int j = 0; j <= nv; j++)
    for (int i = 0; i <= nu; i++) {
      float u = u0 + (u1 - u0) * i / nu, v = v0 + (v1 - v0) * j / nv;
      V3 p = P(u, v);
      V3 du = P(u + eu, v) - P(u - eu, v), dv = P(u, v + ev) - P(u, v - ev);
      V3 n = cross(dv, du);
      if (len(n) < 1e-18f) n = V3(0, 1, 0);
      n = norm(n);
      if (flip) n = -n;
      V3 nw(inv.m[0] * n.x + inv.m[4] * n.y + inv.m[8] * n.z, inv.m[1] * n.x + inv.m[5] * n.y + inv.m[9] * n.z, inv.m[2] * n.x + inv.m[6] * n.y + inv.m[10] * n.z);
      M.add(xf.pt(p), norm(nw), u, v, w);
    }
  for (int j = 0; j < nv; j++)
    for (int i = 0; i < nu; i++) {
      u32 a = base + j * (nu + 1) + i, b = a + 1, c = a + nu + 2, d = a + nu + 1;
      M.quad(a, b, c, d, mat);
    }
}

// ---------- leaves ----------
typedef std::function<float(float)> Fn1;
// width profile helpers (v in 0..1 -> 0..1)
static inline Fn1 profEll(float k = 1.0f, float e = 0.7f, float base = 0.0f) {
  return [=](float v) { v = clampf(v, 0, 1); float s = powf(fmaxf(sinf(PI * powf(v, k)), 0.0f), e); return fmaxf(s, base * (1 - v) * (1 - v * v)); };
}
static inline Fn1 profStrap(float taper = 3.0f, float base = 0.8f) {
  return [=](float v) { v = clampf(v, 0, 1); return (base + (1 - base) * sinf(PI * fminf(v * 2.5f, 0.5f))) * powf(1.0f - powf(v, taper), 0.8f); };
}
struct LeafP {
  float len = 0.1f, wid = 0.04f;
  int nu = 4, nv = 8;
  Fn1 prof = profEll();
  float pitch = 0.3f;     // initial angle above horizontal (rad)
  float droop = 0.6f;     // total bend downward along the leaf (rad)
  float droopPow = 1.5f;
  float fold = 0.25f;     // V-fold about midrib (rad)
  float curl = 0.0f;      // cross-section curl (+: edges down)
  float wave = 0.0f;      // edge undulation amplitude (fraction of width)
  float waveN = 3.0f;
  float twist = 0.0f;     // total twist along the leaf (rad)
  float side = 0.0f;      // sideways bend (rad total)
  float w = 0;            // per-leaf random id stored in uv.w
  float phase = 0;
  float belly = 0;        // pushes the centre down (used for the underside of thick succulent leaves)
};
static const int LEAF_NS = 48;
// midrib frames of a leaf (shared with animation code that needs to walk along a blade)
static inline void leafSpine(const LeafP& L, V3* pos, V3* T, V3* S, V3* N) {
  const int NS = LEAF_NS;
  V3 p(0, 0, 0);
  for (int i = 0; i <= NS; i++) {
    float v = (float)i / NS;
    float a = L.pitch - L.droop * powf(v, L.droopPow);
    float sb = L.side * v * v;
    V3 t(sinf(sb) * cosf(a), sinf(a), cosf(sb) * cosf(a));
    V3 s = norm(cross(V3(0, 1, 0), t));
    if (fabsf(t.y) > 0.999f) s = V3(1, 0, 0);
    V3 n = cross(t, s);
    float tw = L.twist * v;
    V3 s2 = s * cosf(tw) + n * sinf(tw), n2 = n * cosf(tw) - s * sinf(tw);
    pos[i] = p; T[i] = t; S[i] = s2; N[i] = n2;
    p += t * (L.len / NS);
  }
}
static inline void addLeaf(Mesh& M, const LeafP& L, const M34& xf, u32 mat) {
  const int NS = LEAF_NS;
  V3 pos[NS + 1], T[NS + 1], S[NS + 1], N[NS + 1];
  leafSpine(L, pos, T, S, N);
  auto P = [&](float u, float v) {
    float vc = clampf(v, 0, 1);
    float fi = vc * NS; int i = std::min((int)fi, NS - 1); float f = fi - i;
    V3 c = lerp(pos[i], pos[i + 1], f), s = lerp(S[i], S[i + 1], f), n = lerp(N[i], N[i + 1], f);
    if (v > 1) c += T[NS] * ((v - 1) * L.len);
    if (v < 0) c += T[0] * (v * L.len);
    float w = L.wid * 0.5f * L.prof(vc);
    float au = fabsf(u);
    float h = au * sinf(L.fold) - L.curl * u * u + L.wave * sinf(L.waveN * 2 * PI * vc + L.phase + (u > 0 ? 0.0f : 1.7f)) * au * sqrtf(au) - L.belly * (1 - u * u) * (1 - 0.75f * vc);
    return c + s * (u * w * cosf(L.fold)) + n * (h * w);
  };
  addGrid(M, L.nu, L.nv, -1, 1, 0, 1, P, mat, xf, L.w);
}

// tube along polyline; rad(t) with t in 0..1 along length
static inline void addTube(Mesh& M, const std::vector<V3>& pts, const Fn1& rad, int sides, u32 mat, const M34& xf = M34(), float w = 0, bool cap = true) {
  int n = (int)pts.size();
  if (n < 2) return;
  std::vector<float> acc(n, 0);
  for (int i = 1; i < n; i++) acc[i] = acc[i - 1] + len(pts[i] - pts[i - 1]);
  float L = fmaxf(acc[n - 1], 1e-9f);
  u32 base = (u32)M.v.size();
  V3 t0 = norm(pts[1] - pts[0]);
  V3 X = norm(cross(fabsf(t0.y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0), t0));
  for (int i = 0; i < n; i++) {
    V3 t = norm((i == 0) ? pts[1] - pts[0] : (i == n - 1 ? pts[n - 1] - pts[n - 2] : pts[i + 1] - pts[i - 1]));
    X = norm(X - t * dot(X, t));
    V3 Y = cross(t, X);
    float r = rad(acc[i] / L);
    for (int k = 0; k <= sides; k++) {
      float a = 2 * PI * k / sides;
      V3 d = X * cosf(a) + Y * sinf(a);
      M.add(xf.pt(pts[i] + d * r), norm(xf.dir(d)), (float)k / sides, acc[i] / L, w);
    }
  }
  for (int i = 0; i < n - 1; i++)
    for (int k = 0; k < sides; k++) {
      u32 a = base + i * (sides + 1) + k;
      M.quad(a, a + 1, a + sides + 2, a + sides + 1, mat);
    }
  if (cap) {
    V3 te = norm(pts[n - 1] - pts[n - 2]);
    u32 c = M.add(xf.pt(pts[n - 1] + te * (rad(1) * 0.6f)), norm(xf.dir(te)), 0.5f, 1, w);
    u32 r0 = base + (n - 1) * (sides + 1);
    for (int k = 0; k < sides; k++) M.tri(r0 + k, r0 + k + 1, c, mat);
  }
}
static inline std::vector<V3> curvePts(V3 a, V3 b, V3 c, V3 d, int n) { // cubic bezier
  std::vector<V3> r;
  for (int i = 0; i <= n; i++) {
    float t = (float)i / n, s = 1 - t;
    r.push_back(a * (s * s * s) + b * (3 * s * s * t) + c * (3 * s * t * t) + d * (t * t * t));
  }
  return r;
}
// surface of revolution around Y; prof = list of (r,y); optional rib(theta, t) radius multiplier
static inline void addRevolve(Mesh& M, const std::vector<V2>& prof, int sides, int rings, u32 mat, const M34& xf = M34(), const std::function<float(float, float)>& rib = nullptr, float w = 0) {
  int n = (int)prof.size();
  auto cr = [&](float t) {
    float f = clampf(t, 0, 1) * (n - 1); int i = std::min((int)f, n - 2); f -= i;
    V2 p0 = prof[std::max(i - 1, 0)], p1 = prof[i], p2 = prof[i + 1], p3 = prof[std::min(i + 2, n - 1)];
    float f2 = f * f, f3 = f2 * f;
    auto c = [&](float a, float b, float cc, float d) { return 0.5f * ((2 * b) + (-a + cc) * f + (2 * a - 5 * b + 4 * cc - d) * f2 + (-a + 3 * b - 3 * cc + d) * f3); };
    return V2(fmaxf(c(p0.x, p1.x, p2.x, p3.x), 0.0f), c(p0.y, p1.y, p2.y, p3.y));
  };
  auto P = [&](float u, float v) {
    V2 q = cr(v); float th = 2 * PI * u;
    float r = q.x * (rib ? rib(th, clampf(v, 0, 1)) : 1.0f);
    return V3(r * cosf(th), q.y, r * sinf(th));
  };
  addGrid(M, sides, rings, 0, 1, 0, 1, P, mat, xf, w, false);
}
// noisy ellipsoid
static inline void addBlob(Mesh& M, V3 c, V3 rad, int nu, int nv, float amp, float freq, float seed, u32 mat, const M34& xf = M34(), float w = 0, float flatBottom = -2.0f) {
  auto P = [&](float u, float v) {
    float th = 2 * PI * u, ph = PI * clampf(v, 0.0005f, 0.9995f);
    V3 d(sinf(ph) * cosf(th), cosf(ph), sinf(ph) * sinf(th));
    float k = 1.0f + amp * (fbm(d * freq + V3(seed * 13.1f, seed * 7.7f, seed * 3.3f), 3) - 0.45f) * 2.0f;
    V3 q = d * k;
    if (q.y < flatBottom) q.y = flatBottom;
    return c + rad * q;
  };
  addGrid(M, nu, nv, 0, 1, 0, 1, P, mat, xf, w, true);
}
// axis aligned box in local space with uv in metres
static inline void addBox(Mesh& M, V3 lo, V3 hi, u32 mat, const M34& xf = M34(), float w = 0) {
  static const int F[6][4] = {{0, 4, 6, 2}, {5, 1, 3, 7}, {2, 6, 7, 3}, {1, 5, 4, 0}, {4, 5, 7, 6}, {1, 0, 2, 3}};
  static const V3 Nn[6] = {V3(-1, 0, 0), V3(1, 0, 0), V3(0, 1, 0), V3(0, -1, 0), V3(0, 0, 1), V3(0, 0, -1)};
  V3 c[8];
  for (int i = 0; i < 8; i++) c[i] = V3((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z);
  V3 sz = hi - lo;
  for (int f = 0; f < 6; f++) {
    float su = (f < 2) ? sz.z : sz.x, sv = (f == 2 || f == 3) ? sz.z : sz.y;
    static const float UV[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    u32 id[4];
    for (int k = 0; k < 4; k++) {
      V3 p = c[F[f][k]];
      float uu, vv;
      if (f < 2) { uu = p.z - lo.z; vv = p.y - lo.y; }
      else if (f < 4) { uu = p.x - lo.x; vv = p.z - lo.z; }
      else { uu = p.x - lo.x; vv = p.y - lo.y; }
      (void)su; (void)sv; (void)UV;
      id[k] = M.add(xf.pt(p), norm(xf.dir(Nn[f])), uu, vv, w + f * 0.01f);
    }
    M.quad(id[0], id[1], id[2], id[3], mat);
  }
}

// maps t in [-1,1] towards a box profile; stays linear near 0 so that sphere poles remain closed
static inline float squarish(float t, float p) {
  float a = fminf(fabsf(t), 1.0f);
  float e = p + (1.0f - p) * expf(-a * 12.0f);
  return (t < 0 ? -1.0f : 1.0f) * powf(a, e);
}
// ---------- BVH ----------
struct AABB {
  V3 lo = V3(1e30f, 1e30f, 1e30f), hi = V3(-1e30f, -1e30f, -1e30f);
  void grow(V3 p) { lo = vmin(lo, p); hi = vmax(hi, p); }
  void grow(const AABB& b) { lo = vmin(lo, b.lo); hi = vmax(hi, b.hi); }
  float area() const { V3 d = hi - lo; return d.x < 0 ? 0 : 2 * (d.x * d.y + d.y * d.z + d.z * d.x); }
};
struct BNode { V3 bmin; u32 left; V3 bmax; u32 count; };
struct BVHBuild {
  const AABB* bx; std::vector<V3> cen; u32* ord; BNode* nodes; std::atomic<u32> nn{1};
  int leafMax = 4, maxDepth = 40; bool par = false;
  void rec(u32 ni, u32 first, u32 count, int depth) {
    AABB b, cb;
    for (u32 i = first; i < first + count; i++) { b.grow(bx[ord[i]]); cb.grow(cen[ord[i]]); }
    BNode& N = nodes[ni];
    N.bmin = b.lo; N.bmax = b.hi;
    if ((int)count <= leafMax || depth >= maxDepth) { N.left = first; N.count = count; return; }
    int bestAxis = -1, bestSplit = 0; float bestCost = 1e30f;
    const int NB = 16;
    V3 ext = cb.hi - cb.lo;
    if (depth < maxDepth - 14)
      for (int ax = 0; ax < 3; ax++) {
        if (ext[ax] < 1e-9f) continue;
        AABB bb[NB]; int bc[NB] = {0};
        float k = NB / ext[ax];
        for (u32 i = first; i < first + count; i++) {
          int bi = std::min(NB - 1, (int)((cen[ord[i]][ax] - cb.lo[ax]) * k));
          bb[bi].grow(bx[ord[i]]); bc[bi]++;
        }
        float ra[NB]; AABB acc; int cnt = 0;
        for (int i = NB - 1; i > 0; i--) { acc.grow(bb[i]); cnt += bc[i]; ra[i] = acc.area() * cnt; }
        acc = AABB(); cnt = 0;
        for (int i = 0; i < NB - 1; i++) {
          acc.grow(bb[i]); cnt += bc[i];
          if (cnt == 0 || cnt == (int)count) continue;
          float c = acc.area() * cnt + ra[i + 1];
          if (c < bestCost) { bestCost = c; bestAxis = ax; bestSplit = i; }
        }
      }
    u32 mid;
    if (bestAxis >= 0) {
      float k = NB / ext[bestAxis], lo = cb.lo[bestAxis]; int ax = bestAxis, sp = bestSplit;
      u32* m = std::partition(ord + first, ord + first + count, [&](u32 id) { return std::min(NB - 1, (int)((cen[id][ax] - lo) * k)) <= sp; });
      mid = (u32)(m - ord);
    } else mid = first;
    if (mid == first || mid == first + count) {
      int ax = ext.x > ext.y ? (ext.x > ext.z ? 0 : 2) : (ext.y > ext.z ? 1 : 2);
      mid = first + count / 2;
      std::nth_element(ord + first, ord + mid, ord + first + count, [&](u32 a, u32 c) { return cen[a][ax] < cen[c][ax]; });
    }
    u32 c = nn.fetch_add(2);
    N.left = c; N.count = 0;
    if (par && depth < 4 && count > 20000) {
      std::thread th([=] { rec(c, first, mid - first, depth + 1); });
      rec(c + 1, mid, first + count - mid, depth + 1);
      th.join();
    } else {
      rec(c, first, mid - first, depth + 1);
      rec(c + 1, mid, first + count - mid, depth + 1);
    }
  }
};
static inline void buildBVH(const std::vector<AABB>& boxes, std::vector<BNode>& nodes, std::vector<u32>& order, int leafMax, int maxDepth, bool par) {
  u32 n = (u32)boxes.size();
  order.resize(n);
  for (u32 i = 0; i < n; i++) order[i] = i;
  nodes.resize(std::max<u32>(2 * n, 2));
  BVHBuild B;
  B.bx = boxes.data(); B.cen.resize(n);
  for (u32 i = 0; i < n; i++) B.cen[i] = (boxes[i].lo + boxes[i].hi) * 0.5f;
  B.ord = order.data(); B.nodes = nodes.data(); B.leafMax = leafMax; B.maxDepth = maxDepth; B.par = par;
  if (n == 0) { nodes[0] = {V3(1e30f, 1e30f, 1e30f), 0, V3(1e30f, 1e30f, 1e30f), 0}; nodes[1] = nodes[0]; nodes.resize(2); return; }
  B.rec(0, 0, n, 0);
  nodes.resize(B.nn.load());
}

// ---------- GPU-side layouts ----------
struct GTri { float v0[3]; u32 mat; float e1[3]; float p1; float e2[3]; float p2; };
struct GAttr { float a[16]; };
struct GInst { float r[12]; float pos[3]; float flex; float ex[4]; u32 blas; float seed; u32 tint; u32 flags; };
struct GMat { float a[4], b[4], c[4], p[4], q[4]; };
enum { F_NOPRIMARY = 1, F_NOSHADOW = 2, F_NOBOUNCE = 4, F_DYN = 8, F_FLAP = 16 };
static const u32 MAT_ALPHA = 0x80000000u;

static inline void octEnc(V3 n, float* o) {
  float s = fabsf(n.x) + fabsf(n.y) + fabsf(n.z);
  float px = n.x / s, py = n.y / s;
  if (n.z < 0) { float qx = (1 - fabsf(py)) * (px >= 0 ? 1.0f : -1.0f), qy = (1 - fabsf(px)) * (py >= 0 ? 1.0f : -1.0f); px = qx; py = qy; }
  o[0] = px; o[1] = py;
}
static inline u32 packTint(V3 c, float age = 0) {
  auto q = [](float x) { return (u32)clampf(x * 127.5f + 0.5f, 0, 255); }; // 1.0 -> 128, allows up to 2x
  return q(c.x) | (q(c.y) << 8) | (q(c.z) << 16) | ((u32)clampf(age * 255 + 0.5f, 0, 255) << 24);
}
static const u32 TINT1 = 0x00808080u;

struct Proto { u32 root; AABB box; u32 ntri; };
struct InstCPU { int proto; M34 xf; float flex; float seed; u32 tint; u32 flags; float ex[4]; };
struct Scene {
  std::vector<BNode> nodes;
  std::vector<GTri> tris;
  std::vector<GAttr> attrs;
  std::vector<Proto> protos;
  std::vector<GMat> mats;
  std::vector<InstCPU> inst;       // static instances (CPU form)
  std::vector<GInst> ginst;        // static instances in TLAS order
  std::vector<BNode> tlas;         // static TLAS (root 0), separate buffer
  size_t instTris = 0;

  int addMat(int type, V3 a, float rough, V3 b, float trans, V3 c, float p0 = 0, float p1 = 0, float p2 = 0, float p3 = 0, float q0 = 0, float q1 = 0, float shadowT = 0, float q3 = 0) {
    GMat m = {{a.x, a.y, a.z, rough}, {b.x, b.y, b.z, trans}, {c.x, c.y, c.z, (float)type}, {p0, p1, p2, p3}, {q0, q1, shadowT, q3}};
    mats.push_back(m);
    return (int)mats.size() - 1;
  }
  int addProto(const Mesh& m) {
    u32 nt = (u32)m.ntri();
    std::vector<AABB> bx(nt);
    AABB all;
    for (u32 t = 0; t < nt; t++) {
      for (int k = 0; k < 3; k++) bx[t].grow(m.v[m.idx[t * 3 + k]].p);
      all.grow(bx[t]);
    }
    std::vector<BNode> bn; std::vector<u32> ord;
    buildBVH(bx, bn, ord, 4, 26, nt > 200000);
    u32 nbase = (u32)nodes.size(), tbase = (u32)tris.size();
    for (auto n : bn) {
      if (n.count == 0) n.left += nbase; else n.left += tbase;
      nodes.push_back(n);
    }
    for (u32 i = 0; i < nt; i++) {
      u32 t = ord[i];
      const Vtx& a = m.v[m.idx[t * 3]], &b = m.v[m.idx[t * 3 + 1]], &c = m.v[m.idx[t * 3 + 2]];
      GTri g; V3 e1 = b.p - a.p, e2 = c.p - a.p;
      g.v0[0] = a.p.x; g.v0[1] = a.p.y; g.v0[2] = a.p.z; g.mat = m.mat[t];
      g.e1[0] = e1.x; g.e1[1] = e1.y; g.e1[2] = e1.z; g.p1 = 0;
      g.e2[0] = e2.x; g.e2[1] = e2.y; g.e2[2] = e2.z; g.p2 = 0;
      tris.push_back(g);
      GAttr at;
      const Vtx* vs[3] = {&a, &b, &c};
      for (int k = 0; k < 3; k++) { octEnc(vs[k]->n, &at.a[k * 4]); at.a[k * 4 + 2] = vs[k]->u; at.a[k * 4 + 3] = vs[k]->v; at.a[12 + k] = vs[k]->w; }
      at.a[15] = 0;
      attrs.push_back(at);
    }
    protos.push_back({nbase, all, nt});
    return (int)protos.size() - 1;
  }
  void put(int proto, const M34& xf, float flex = 0, float seed = 0, u32 tint = TINT1, u32 flags = 0, float e0 = 0, float e1 = 0, float e2 = 0, float e3 = 0) {
    inst.push_back({proto, xf, flex, seed, tint, flags, {e0, e1, e2, e3}});
    instTris += protos[proto].ntri;
  }
  GInst toG(const InstCPU& I) const {
    GInst g; M34 inv = mInverse(I.xf);
    memcpy(g.r, inv.m, sizeof g.r);
    V3 o = I.xf.origin();
    g.pos[0] = o.x; g.pos[1] = o.y; g.pos[2] = o.z; g.flex = I.flex;
    memcpy(g.ex, I.ex, sizeof g.ex);
    g.blas = protos[I.proto].root; g.seed = I.seed; g.tint = I.tint; g.flags = I.flags;
    return g;
  }
  AABB instBox(const InstCPU& I, float extra = 0) const {
    const AABB& b = protos[I.proto].box; AABB r;
    for (int i = 0; i < 8; i++) r.grow(I.xf.pt(V3((i & 1) ? b.hi.x : b.lo.x, (i & 2) ? b.hi.y : b.lo.y, (i & 4) ? b.hi.z : b.lo.z)));
    float sc = len(I.xf.dir(V3(0, 1, 0)));
    float pad = I.flex * 0.24f * (b.hi.y - fminf(b.lo.y, 0.0f)) * sc + extra;
    r.lo -= V3(pad, pad, pad); r.hi += V3(pad, pad, pad);
    return r;
  }
  void finalize() {
    std::vector<AABB> bx(inst.size());
    for (size_t i = 0; i < inst.size(); i++) bx[i] = instBox(inst[i]);
    std::vector<u32> ord;
    buildBVH(bx, tlas, ord, 1, 44, true);
    ginst.resize(inst.size());
    for (size_t i = 0; i < inst.size(); i++) ginst[i] = toG(inst[ord[i]]);
  }
};
