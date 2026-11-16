// render.metal — self-written path tracer (two-level BVH, procedural materials, sky, water, glass, fog)
#include <metal_stdlib>
using namespace metal;

#define PI 3.14159265f
#define INF 1e30f

struct Node { packed_float3 bmin; uint left; packed_float3 bmax; uint count; };
struct Tri { packed_float3 v0; uint mat; packed_float3 e1; float p1; packed_float3 e2; float p2; };
struct TriAttr { float4 a0, a1, a2, a3; };
struct Inst { float4 r0, r1, r2; float4 posFlex; float4 ex; uint blas; float seed; uint tint; uint flags; };
struct Mat { float4 a, b, c, p, q; };
struct Uni {
  float4 camPos0, camPos1;
  float4 camR0, camU0, camF0, camR1, camU1, camF1;
  float4 lens;     // x tan(hfov/4), y aperture radius, z aspect h/w, w bokeh squeeze
  float4 sunDir;   // w cos(sun radius)
  float4 sunCol;   // rgb (E/pi), w sky gain
  float4 timeP;    // x t open, y shutter, z wind, w wetness
  float4 fogLo, fogHi; // greenhouse box; w: sigma in / sigma out
  float4 water;    // x level, y turbidity, z unused, w view mode
  float4 mapRect;  // x0, z0, 1/w, 1/h
  float4 kr0, kr1;
  float4 ripple[16];
  uint4 counts;    // frame seed, sample base, spp this pass, nRipples
  uint4 dims;      // W, H, yOffset, debug
};
struct Scene {
  const device Node* nodes; const device Tri* tris; const device TriAttr* attrs;
  const device Inst* insts; const device Node* tlas; const device Mat* mats;
  const device Node* dtlas; const device Inst* dinst0; const device Inst* dinst1;
};
struct Ray { float3 o, d; float time; uint rs; };
struct Hit { float t; uint tri; int inst; float2 bc; bool dyn; };
struct LR { float3 o, d; float3 r0, r1, r2; float2 cs; };
struct Surf { float3 alb, tr, n, ng, emis; float rough, f0, spec, glint, haze; int kind; bool back; };

constexpr sampler smp(filter::linear, address::clamp_to_edge);

// ---------------------------------------------------------------- hashing / noise
static inline uint pcg(uint v) { uint s = v * 747796405u + 2891336453u; uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
static inline float h1(uint v) { return float(pcg(v) >> 8) * (1.0f / 16777216.0f); }
static inline float rnd(thread uint& s) { s = pcg(s); return float(s >> 8) * (1.0f / 16777216.0f); }
static inline uint hcell(int3 b) { return uint(b.x) + pcg(uint(b.y) + pcg(uint(b.z))); }
static float vnoise(float3 p) {
  float3 i = floor(p), f = p - i; f = f * f * (3.0f - 2.0f * f);
  int3 b = int3(i);
  float a = mix(mix(h1(hcell(b)), h1(hcell(b + int3(1, 0, 0))), f.x), mix(h1(hcell(b + int3(0, 1, 0))), h1(hcell(b + int3(1, 1, 0))), f.x), f.y);
  float c = mix(mix(h1(hcell(b + int3(0, 0, 1))), h1(hcell(b + int3(1, 0, 1))), f.x), mix(h1(hcell(b + int3(0, 1, 1))), h1(hcell(b + int3(1, 1, 1))), f.x), f.y);
  return mix(a, c, f.z);
}
static float fbm3(float3 p, int oct) {
  float s = 0, a = 0.5f;
  for (int i = 0; i < oct; i++) { s += a * vnoise(p); p = p * 2.03f + float3(17.1f, 3.7f, 9.2f); a *= 0.5f; }
  return s;
}
// returns (distance to nearest feature point, cell hash, distance to 2nd nearest)
static float3 worley2(float2 p) {
  float2 i = floor(p), f = p - i; float d1 = 9, d2 = 9, id = 0;
  for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
    int3 c = int3(int(i.x) + x, int(i.y) + y, 7);
    uint h = pcg(hcell(c));
    float2 o = float2(float(h & 0xffffu), float(h >> 16)) * (1.0f / 65535.0f);
    float d = length(float2(x, y) + o - f);
    if (d < d1) { d2 = d1; d1 = d; id = h1(h); } else if (d < d2) d2 = d;
  }
  return float3(d1, id, d2);
}
static inline float lum(float3 c) { return dot(c, float3(0.2126f, 0.7152f, 0.0722f)); }
static inline float3 octDec(float2 e) {
  float3 n = float3(e.x, e.y, 1.0f - fabs(e.x) - fabs(e.y)); float t = max(-n.z, 0.0f);
  n.x += n.x >= 0 ? -t : t; n.y += n.y >= 0 ? -t : t; return normalize(n);
}

// ---------------------------------------------------------------- instance transform (with wind sway / flapping)
static inline float2 swayK(Inst I, float3 r0, float3 r1, float3 r2, constant Uni& U, float t) {
  float3 P = I.posFlex.xyz; float w = U.timeP.z * I.posFlex.w;
  float g = 0.5f + 0.5f * sin(P.x * 0.7f + P.z * 0.45f - t * 1.6f + 1.3f * I.seed);
  float gust = 0.25f + 0.75f * g * g;
  float fl = sin(t * (5.0f + 6.0f * I.seed) + I.seed * 43.0f);
  float2 wd = float2(-0.86f, 0.5f);
  float2 kw = wd * (w * gust * (0.10f + 0.03f * fl)) + float2(-wd.y, wd.x) * (w * 0.035f * fl);
  float s = 1.0f / max(length(r1), 1e-6f);
  return float2(dot(r0.xz, kw), dot(r2.xz, kw)) * s;
}
static inline LR toLocal(Inst I, const device Inst* in1, uint idx, Ray r, constant Uni& U) {
  float4 r0 = I.r0, r1 = I.r1, r2 = I.r2;
  if (I.flags & 8u) { Inst J = in1[idx]; r0 = mix(r0, J.r0, r.time); r1 = mix(r1, J.r1, r.time); r2 = mix(r2, J.r2, r.time); }
  LR L; L.r0 = r0.xyz; L.r1 = r1.xyz; L.r2 = r2.xyz; L.cs = float2(1, 0);
  L.o = float3(dot(r0.xyz, r.o) + r0.w, dot(r1.xyz, r.o) + r1.w, dot(r2.xyz, r.o) + r2.w);
  L.d = float3(dot(r0.xyz, r.d), dot(r1.xyz, r.d), dot(r2.xyz, r.d));
  float tm = U.timeP.x + r.time * U.timeP.y;
  if (I.flags & 16u) {
    float ang = I.ex.x * sin(6.2831853f * I.ex.y * tm + I.ex.z) + I.ex.w;
    float c = cos(ang), s = sin(ang); L.cs = float2(c, s);
    L.o.xy = float2(c * L.o.x + s * L.o.y, -s * L.o.x + c * L.o.y);
    L.d.xy = float2(c * L.d.x + s * L.d.y, -s * L.d.x + c * L.d.y);
  }
  if (I.posFlex.w > 0) { float2 k = swayK(I, r0.xyz, r1.xyz, r2.xyz, U, tm); L.o.xz -= k * L.o.y; L.d.xz -= k * L.d.y; }
  return L;
}
static inline float boxHit(float3 bmin, float3 bmax, float3 o, float3 inv, float tmax) {
  float3 t0 = (bmin - o) * inv, t1 = (bmax - o) * inv; float3 tn = min(t0, t1), tf = max(t0, t1);
  float a = max(max(tn.x, tn.y), max(tn.z, 0.0f)), b = min(min(tf.x, tf.y), min(tf.z, tmax));
  return a <= b ? a : INF;
}
static inline float3 safeInv(float3 d) {
  return float3(fabs(d.x) > 1e-12f ? 1.0f / d.x : 1e12f, fabs(d.y) > 1e-12f ? 1.0f / d.y : 1e12f, fabs(d.z) > 1e-12f ? 1.0f / d.z : 1e12f);
}

// ---------------------------------------------------------------- alpha coverage for cut-out materials
static float coverage(Mat M, float3 uvw) {
  int type = int(M.c.w + 0.5f);
  float u = uvw.x, v = uvw.y, au = fabs(u);
  if (type == 1) {
    float a = 1;
    int style = int(M.p.z + 0.5f);
    if (M.q.w > 0 && style != 2) { // serration
      float e = 1.0f - M.q.w * (0.5f + 0.5f * sin(v * 150.0f * M.q.w + 1.0f)) * smoothstep(0.02f, 0.15f, v);
      if (au > e) a = 0;
    }
    if (style == 3) { // monstera fenestration
      float s = v * M.p.x - au * M.p.y + (u > 0 ? 0.0f : 0.43f);
      float cell = floor(s), f = s - cell;
      float h = h1(uint(int(cell) + 1000) * 3u + (u > 0 ? 1u : 0u) + uint(uvw.z * 977.0f) * 131u);
      float h2 = h1(uint(h * 65535.0f) + 77u);
      if (v > 0.12f && v < 0.93f) {
        float cx = 0.42f + 0.2f * h2;
        float ex = (au - cx) / 0.2f, ey = (f - 0.5f) / 0.30f;
        if (h < 0.75f && ex * ex + ey * ey < 1.0f) a = 0;
        if (h < 0.55f && au > cx && fabs(f - 0.5f) < 0.24f * (0.35f + 0.65f * (1.0f - au))) a = 0;
      }
    } else if (style == 4) { // banana leaf tears along the lateral veins
      float s = v * M.p.x - au * M.p.y + (u > 0 ? 0.0f : 0.43f);
      float cell = floor(s), f = s - cell;
      float h = h1(uint(int(cell) + 500) * 7u + (u > 0 ? 3u : 0u) + uint(uvw.z * 977.0f) * 131u);
      if (h < 0.22f && au > 0.15f + h * 3.0f && fabs(f - 0.5f) < 0.025f + 0.10f * (au - 0.15f - h * 3.0f)) a = 0;
    }
    if (M.q.x > 0) { // insect holes
      float3 w = worley2(float2(u * 2.2f + uvw.z * 37.0f, v * 5.0f + uvw.z * 11.0f));
      if (w.y < M.q.x && w.x < 0.12f + 0.25f * fract(w.y * 57.0f)) a = 0;
    }
    return a;
  }
  if (type == 12) { // wings
    if (M.p.x < 0.5f) return 1.0f; // butterfly: opaque
    float vein = max(smoothstep(0.42f, 0.5f, fabs(fract(u * 5.0f + v * 1.5f) - 0.5f)), smoothstep(0.42f, 0.5f, fabs(fract(v * 9.0f - u) - 0.5f)));
    return mix(M.q.y, 0.9f, vein);
  }
  return M.q.y; // generic opacity
}

// ---------------------------------------------------------------- traversal
#define TSTACK 48
#define BSTACK 30
static Hit traceLevel(Scene S, const device Node* tn, const device Inst* in0, const device Inst* in1, Ray r, float tmax, uint skipMask, constant Uni& U, bool dyn, Hit h) {
  float3 inv = safeInv(r.d);
  uint stack[TSTACK]; float dst[TSTACK]; int sp = 0; uint ni = 0;
  Node root = tn[0];
  if (boxHit(root.bmin, root.bmax, r.o, inv, h.t) >= INF) return h;
  while (true) {
    Node n = tn[ni];
    if (n.count == 0) {
      uint c0 = n.left, c1 = n.left + 1;
      Node a = tn[c0], b = tn[c1];
      float d0 = boxHit(a.bmin, a.bmax, r.o, inv, h.t), d1 = boxHit(b.bmin, b.bmax, r.o, inv, h.t);
      if (d0 > d1) { float td = d0; d0 = d1; d1 = td; uint tc = c0; c0 = c1; c1 = tc; }
      if (d0 < INF) { ni = c0; if (d1 < INF && sp < TSTACK) { stack[sp] = c1; dst[sp] = d1; sp++; } continue; }
    } else {
      for (uint ii = 0; ii < n.count; ii++) {
        uint idx = n.left + ii;
        Inst I = in0[idx];
        if (I.flags & skipMask) continue;
        LR L = toLocal(I, in1, idx, r, U);
        float3 linv = safeInv(L.d);
        uint bs[BSTACK]; int bp = 0; uint bi = I.blas;
        Node br = S.nodes[bi];
        if (boxHit(br.bmin, br.bmax, L.o, linv, h.t) >= INF) continue;
        while (true) {
          Node m = S.nodes[bi];
          if (m.count == 0) {
            uint e0 = m.left, e1 = m.left + 1;
            Node a = S.nodes[e0], b = S.nodes[e1];
            float f0 = boxHit(a.bmin, a.bmax, L.o, linv, h.t), f1 = boxHit(b.bmin, b.bmax, L.o, linv, h.t);
            if (f0 > f1) { float tf = f0; f0 = f1; f1 = tf; uint te = e0; e0 = e1; e1 = te; }
            if (f0 < INF) { bi = e0; if (f1 < INF && bp < BSTACK) bs[bp++] = e1; continue; }
          } else {
            for (uint k = m.left; k < m.left + m.count; k++) {
              Tri T = S.tris[k];
              float3 e1 = T.e1, e2 = T.e2;
              float3 pv = cross(L.d, e2); float det = dot(e1, pv);
              if (det == 0.0f) continue;
              float idet = 1.0f / det; float3 tv = L.o - float3(T.v0);
              float bu = dot(tv, pv) * idet; if (bu < 0.0f || bu > 1.0f) continue;
              float3 qv = cross(tv, e1); float bv = dot(L.d, qv) * idet; if (bv < 0.0f || bu + bv > 1.0f) continue;
              float t = dot(e2, qv) * idet; if (t <= 0.0f || t >= h.t) continue;
              if (T.mat & 0x80000000u) {
                TriAttr A = S.attrs[k];
                float b0 = 1.0f - bu - bv;
                float3 uvw = float3(A.a0.zw * b0 + A.a1.zw * bu + A.a2.zw * bv, A.a3.x * b0 + A.a3.y * bu + A.a3.z * bv);
                float cov = coverage(S.mats[T.mat & 0xffffu], uvw);
                if (cov < 1.0f && cov <= h1(r.rs + k * 7919u + idx * 104729u)) continue;
              }
              h.t = t; h.tri = k; h.inst = int(idx); h.bc = float2(bu, bv); h.dyn = dyn;
            }
          }
          if (bp == 0) break;
          bi = bs[--bp];
        }
      }
    }
    // pop
    bool found = false;
    while (sp > 0) { sp--; if (dst[sp] < h.t) { ni = stack[sp]; found = true; break; } }
    if (!found) break;
  }
  return h;
}
static Hit trace(Scene S, Ray r, float tmax, uint skipMask, constant Uni& U) {
  Hit h; h.t = tmax; h.inst = -1; h.tri = 0; h.bc = float2(0); h.dyn = false;
  h = traceLevel(S, S.tlas, S.insts, S.insts, r, tmax, skipMask, U, false, h);
  h = traceLevel(S, S.dtlas, S.dinst0, S.dinst1, r, tmax, skipMask, U, true, h);
  return h;
}
static float3 shadowLevel(Scene S, const device Node* tn, const device Inst* in0, const device Inst* in1, Ray r, float tmax, constant Uni& U, float3 Tr) {
  float3 inv = safeInv(r.d);
  uint stack[TSTACK]; int sp = 0; uint ni = 0;
  Node root = tn[0];
  if (boxHit(root.bmin, root.bmax, r.o, inv, tmax) >= INF) return Tr;
  while (true) {
    Node n = tn[ni];
    if (n.count == 0) {
      uint c0 = n.left, c1 = n.left + 1;
      Node a = tn[c0], b = tn[c1];
      float d0 = boxHit(a.bmin, a.bmax, r.o, inv, tmax), d1 = boxHit(b.bmin, b.bmax, r.o, inv, tmax);
      if (d0 < INF) { ni = c0; if (d1 < INF && sp < TSTACK) stack[sp++] = c1; continue; }
      if (d1 < INF) { ni = c1; continue; }
    } else {
      for (uint ii = 0; ii < n.count; ii++) {
        uint idx = n.left + ii;
        Inst I = in0[idx];
        if (I.flags & 2u) continue;
        LR L = toLocal(I, in1, idx, r, U);
        float3 linv = safeInv(L.d);
        uint bs[BSTACK]; int bp = 0; uint bi = I.blas;
        Node br = S.nodes[bi];
        if (boxHit(br.bmin, br.bmax, L.o, linv, tmax) >= INF) continue;
        while (true) {
          Node m = S.nodes[bi];
          if (m.count == 0) {
            uint e0 = m.left, e1 = m.left + 1;
            Node a = S.nodes[e0], b = S.nodes[e1];
            float f0 = boxHit(a.bmin, a.bmax, L.o, linv, tmax), f1 = boxHit(b.bmin, b.bmax, L.o, linv, tmax);
            if (f0 < INF) { bi = e0; if (f1 < INF && bp < BSTACK) bs[bp++] = e1; continue; }
            if (f1 < INF) { bi = e1; continue; }
          } else {
            for (uint k = m.left; k < m.left + m.count; k++) {
              Tri T = S.tris[k];
              float3 e1 = T.e1, e2 = T.e2;
              float3 pv = cross(L.d, e2); float det = dot(e1, pv);
              if (det == 0.0f) continue;
              float idet = 1.0f / det; float3 tv = L.o - float3(T.v0);
              float bu = dot(tv, pv) * idet; if (bu < 0.0f || bu > 1.0f) continue;
              float3 qv = cross(tv, e1); float bv = dot(L.d, qv) * idet; if (bv < 0.0f || bu + bv > 1.0f) continue;
              float t = dot(e2, qv) * idet; if (t <= 0.0f || t >= tmax) continue;
              Mat M = S.mats[T.mat & 0xffffu];
              float cov = 1.0f;
              if (T.mat & 0x80000000u) {
                TriAttr A = S.attrs[k];
                float b0 = 1.0f - bu - bv;
                float3 uvw = float3(A.a0.zw * b0 + A.a1.zw * bu + A.a2.zw * bv, A.a3.x * b0 + A.a3.y * bu + A.a3.z * bv);
                cov = coverage(M, uvw);
                if (cov <= 0.0f) continue;
              }
              float st = M.q.z;
              float3 tt = st > 0.0f ? clamp(M.b.rgb * (st * 2.2f) + st * 0.25f, 0.0f, 1.0f) : float3(0.0f);
              Tr *= mix(float3(1.0f), tt, cov);
              if (max(Tr.x, max(Tr.y, Tr.z)) < 0.04f) return float3(0.0f);
            }
          }
          if (bp == 0) break;
          bi = bs[--bp];
        }
      }
    }
    if (sp == 0) break;
    ni = stack[--sp];
  }
  return Tr;
}
static float3 shadow(Scene S, float3 o, float3 d, float tmax, float time, uint rs, constant Uni& U) {
  Ray r; r.o = o; r.d = d; r.time = time; r.rs = rs;
  float3 T = shadowLevel(S, S.tlas, S.insts, S.insts, r, tmax, U, float3(1.0f));
  if (T.x + T.y + T.z <= 0.0f) return T;
  return shadowLevel(S, S.dtlas, S.dinst0, S.dinst1, r, tmax, U, T);
}

// ---------------------------------------------------------------- sky
static float cloudN(float2 q) {
  float s = 0, a = 0.5f;
  for (int i = 0; i < 5; i++) { s += a * vnoise(float3(q, 3.3f + float(i))); q = q * 2.1f + float2(3.1f, 1.7f); a *= 0.5f; }
  return s;
}
static float3 skyRad(float3 d, constant Uni& U, bool disc) {
  float3 sun = U.sunDir.xyz; float mu = dot(d, sun);
  float h = clamp(d.y, 0.0f, 1.0f);
  float3 zen = float3(0.085f, 0.215f, 0.60f), hor = float3(0.50f, 0.62f, 0.78f);
  float3 c = mix(hor, zen, pow(h, 0.45f));
  float m0 = max(mu, 0.0f);
  c += float3(0.60f, 0.38f, 0.16f) * (pow(m0, 5.0f) * (1.0f - h) * 0.8f);
  c += float3(1.0f, 0.78f, 0.5f) * (pow(m0, 60.0f) * 1.2f + pow(m0, 600.0f) * 6.0f);
  if (d.y > 0.015f) {
    float2 q = d.xz / (d.y + 0.10f) * 0.9f + float2(U.timeP.x * 0.006f, 0.003f * U.timeP.x);
    float n = cloudN(q);
    float cover = smoothstep(0.50f, 0.74f, n) * smoothstep(0.015f, 0.14f, d.y);
    float3 cc = mix(float3(0.80f, 0.82f, 0.86f), float3(1.5f, 1.25f, 1.0f), pow(m0, 2.5f)) * (0.75f + 0.5f * smoothstep(0.5f, 0.9f, n));
    c = mix(c, cc, cover * 0.9f);
  } else if (d.y < 0.0f) c = mix(c, float3(0.16f, 0.17f, 0.13f), clamp(-d.y * 5.0f, 0.0f, 1.0f));
  c *= U.sunCol.w;
  if (disc && mu > U.sunDir.w) c += float3(1.0f, 0.92f, 0.8f) * 300.0f;
  return c;
}
static inline float3 skyAmb(constant Uni& U) { return float3(0.36f, 0.44f, 0.56f) * U.sunCol.w; }

// ---------------------------------------------------------------- water surface normal
static float3 waterN(float3 p, float t, constant Uni& U) {
  float2 g = float2(0.0f);
  float w = 0.6f + 0.4f * U.timeP.z;
  float2 q = p.xz;
  float amp = vnoise(float3(q * 0.9f - float2(t * 0.25f, 0.0f), 1.0f));
  amp = 0.25f + amp * amp * 1.6f;
  for (int i = 0; i < 5; i++) {
    float fi = float(i);
    float2 dir = normalize(float2(-0.86f + 0.5f * sin(fi * 2.1f), 0.5f + 0.6f * cos(fi * 1.3f)));
    float k = 26.0f + 17.0f * fi, om = sqrt(9.8f * k) * 0.55f;
    float ph = dot(dir, q) * k - om * t + fi * 1.7f + 1.5f * vnoise(float3(q * 2.3f, fi));
    g += dir * (cos(ph) * 0.0105f * w * amp / (1.0f + 0.5f * fi));
  }
  for (uint i = 0; i < U.counts.w; i++) {
    float4 R = U.ripple[i]; float age = t - R.z;
    if (age <= 0.0f || age > 5.0f) continue;
    float2 dv = q - R.xy; float rr = length(dv) + 1e-4f;
    float c = 0.24f, k = 150.0f;
    float front = c * age;
    float env = smoothstep(front + 0.03f, front - 0.02f, rr) * exp(-(front - rr) * 9.0f) * exp(-age * 1.1f) / (1.0f + 6.0f * rr);
    g += dv / rr * (cos((rr - front) * k) * R.w * env);
  }
  return normalize(float3(-g.x, 1.0f, -g.y));
}
static float caustic(float2 q, float t) {
  float2 p = q * 7.0f;
  float a = 0.0f;
  for (int i = 0; i < 3; i++) {
    float fi = float(i);
    p += float2(sin(p.y * 0.9f + t * 0.7f + fi), cos(p.x * 0.8f - t * 0.6f + fi * 2.0f)) * 0.7f;
    a += 1.0f / (0.25f + fabs(sin(p.x + fi) + cos(p.y * 1.1f - fi)));
  }
  return clamp(a * 0.22f, 0.3f, 3.0f);
}

// ---------------------------------------------------------------- materials
static float leafVeins(float u, float v, float nv, float slope, int style) {
  float au = fabs(u);
  if (style == 1) {
    float f = fabs(fract(au * nv) - 0.5f);
    return max(smoothstep(0.38f, 0.5f, f) * 0.7f, smoothstep(0.05f, 0.012f, au));
  }
  if (style == 2) {
    float f = fabs(fract(u * nv) - 0.5f);
    return smoothstep(0.5f - 0.035f / max(v, 0.1f), 0.5f, f) * (1.0f - v * 0.45f);
  }
  float s = v * nv - au * slope + (u > 0 ? 0.0f : 0.43f);
  float f = fabs(fract(s) - 0.5f);
  float lat = smoothstep(0.43f, 0.5f, f) * (1.0f - au * au * au);
  float mid = smoothstep(0.075f * (1.15f - v), 0.02f * (1.15f - v), au);
  return max(lat * 0.8f, mid);
}
static float3 stonePalette(float s) {
  float h = fract(s * 7.31f), g = fract(s * 3.7f);
  float3 c = mix(float3(0.20f, 0.195f, 0.19f), float3(0.34f, 0.30f, 0.25f), g);
  if (h < 0.10f) c = float3(0.48f, 0.46f, 0.43f);
  else if (h < 0.24f) c = float3(0.27f, 0.17f, 0.11f);
  else if (h < 0.38f) c = float3(0.10f, 0.105f, 0.115f);
  else if (h < 0.50f) c = float3(0.36f, 0.29f, 0.19f);
  else if (h < 0.58f) c = float3(0.22f, 0.20f, 0.15f);
  else if (h < 0.64f) c = float3(0.30f, 0.21f, 0.19f);
  return c * (0.8f + 0.4f * fract(s * 51.7f));
}

static Surf evalSurf(Scene S, Hit h, Ray r, constant Uni& U, texture2d<float> gmap, texture2d<float> labels) {
  Surf s;
  Inst I; if (h.dyn) I = S.dinst0[h.inst]; else I = S.insts[h.inst];
  LR L; if (h.dyn) L = toLocal(I, S.dinst1, uint(h.inst), r, U); else L = toLocal(I, S.insts, uint(h.inst), r, U);
  Tri T = S.tris[h.tri]; TriAttr A = S.attrs[h.tri];
  Mat M = S.mats[T.mat & 0xffffu];
  float b0 = 1.0f - h.bc.x - h.bc.y;
  float3 nl = normalize(octDec(A.a0.xy) * b0 + octDec(A.a1.xy) * h.bc.x + octDec(A.a2.xy) * h.bc.y);
  float2 uv = A.a0.zw * b0 + A.a1.zw * h.bc.x + A.a2.zw * h.bc.y;
  float lw = A.a3.x * b0 + A.a3.y * h.bc.x + A.a3.z * h.bc.y;
  float3 pl = L.o + L.d * h.t;
  float3 pw = r.o + r.d * h.t;
  float3 e1 = T.e1, e2 = T.e2;
  float3 ngl = cross(e1, e2);
  float2 d1 = A.a1.zw - A.a0.zw, d2 = A.a2.zw - A.a0.zw;
  float dt = d1.x * d2.y - d1.y * d2.x;
  float3 tu = fabs(dt) > 1e-12f ? (e1 * d2.y - e2 * d1.y) / dt : e1;
  float3 tvv = fabs(dt) > 1e-12f ? (e2 * d1.x - e1 * d2.x) / dt : e2;
  // undo flap rotation for directions, then to world
  float c = L.cs.x, sn = L.cs.y;
  nl.xy = float2(c * nl.x - sn * nl.y, sn * nl.x + c * nl.y);
  ngl.xy = float2(c * ngl.x - sn * ngl.y, sn * ngl.x + c * ngl.y);
  tu.xy = float2(c * tu.x - sn * tu.y, sn * tu.x + c * tu.y);
  tvv.xy = float2(c * tvv.x - sn * tvv.y, sn * tvv.x + c * tvv.y);
  float3 n = normalize(L.r0 * nl.x + L.r1 * nl.y + L.r2 * nl.z);
  float3 ng = normalize(L.r0 * ngl.x + L.r1 * ngl.y + L.r2 * ngl.z);
  float3 Tw = normalize(L.r0 * tu.x + L.r1 * tu.y + L.r2 * tu.z);
  float3 Bw = normalize(L.r0 * tvv.x + L.r1 * tvv.y + L.r2 * tvv.z);
  if (dot(n, ng) < 0.0f) ng = -ng;
  bool back = dot(ng, r.d) > 0.0f;
  float3 tint = float3(float(I.tint & 255u), float((I.tint >> 8) & 255u), float((I.tint >> 16) & 255u)) * (1.0f / 128.0f);
  float age = float(I.tint >> 24) * (1.0f / 255.0f);
  float seed = I.seed;
  int type = int(M.c.w + 0.5f);
  float wet = U.timeP.w * (all(pw > U.fogLo.xyz) && all(pw < U.fogHi.xyz) ? 0.5f : 1.0f);
  s.alb = M.a.rgb; s.tr = float3(0.0f); s.rough = M.a.w; s.f0 = 0.04f; s.spec = 1.0f; s.kind = 0; s.emis = float3(0.0f); s.glint = 0.0f; s.haze = 0.0f;
  float2 bump = float2(0.0f); float3 bump3 = float3(0.0f);
  float u = uv.x, v = uv.y, au = fabs(u);

  if (type == 1) { // ---- LEAF
    int style = int(M.p.z + 0.5f);
    float edge = style == 2 ? v : max(au, v * v);
    float vein = leafVeins(u, v, M.p.x, M.p.y, style);
    float vu = leafVeins(u + 0.02f, v, M.p.x, M.p.y, style), vv = leafVeins(u, v + 0.006f, M.p.x, M.p.y, style);
    float n1 = fbm3(float3(u * 2.3f + lw * 31.0f, v * 4.0f + seed * 57.0f, lw * 7.0f + seed * 3.0f), 3);
    float n2 = vnoise(float3(u * 13.0f, v * 31.0f, lw * 91.0f + seed * 5.0f));
    float3 col = mix(M.a.rgb, M.b.rgb, clamp(n1 * 1.5f - 0.35f + (lw - 0.5f) * 0.6f + age * 0.7f, 0.0f, 1.0f));
    col *= 0.80f + 0.40f * n2;
    col = mix(col, M.c.rgb, vein * (back ? 0.9f : 0.6f));
    if (M.p.w > 0.0f) {
      float vg = smoothstep(0.50f, 0.58f, fbm3(float3(u * 2.5f, v * 5.0f, lw * 13.0f), 2) + (1.0f - au) * 0.12f);
      col = mix(col, float3(0.80f, 0.84f, 0.55f), vg * M.p.w);
    }
    float br = smoothstep(1.0f - M.q.y * (0.25f + 1.5f * n1), 1.03f, edge + 0.3f * (n2 - 0.5f)) * step(0.001f, M.q.y);
    br = max(br, smoothstep(0.75f, 0.95f, age + 0.5f * n1) * 0.9f);
    col = mix(col, float3(0.33f, 0.21f, 0.08f) * (0.55f + 0.9f * n2), br);
    if (M.q.x > 0.0f) {
      float3 w = worley2(float2(u * 2.2f + lw * 37.0f, v * 5.0f + lw * 11.0f));
      if (w.y < M.q.x) col = mix(col, float3(0.25f, 0.16f, 0.06f), smoothstep(0.30f, 0.14f, w.x - 0.25f * fract(w.y * 57.0f)) * 0.8f);
    }
    col *= tint;
    s.tr = pow(col, float3(0.75f)) * float3(1.15f, 1.25f, 0.55f) * (M.b.w * (1.0f - vein * 0.55f) * (1.0f - br * 0.5f));
    if (back) { col = mix(col, float3(lum(col)) * float3(0.9f, 1.0f, 0.8f) + 0.02f, 0.35f) * 1.15f; s.rough = min(1.0f, s.rough * 1.6f); s.spec = 0.5f; }
    s.alb = col;
    s.rough = clamp(s.rough * (0.8f + 0.5f * n2) * (1.0f - 0.35f * wet), 0.08f, 1.0f);
    bump = float2(vu - vein, vv - vein) * float2(1.6f, 0.5f) * (back ? -1.0f : 1.0f);
    // dew beads
    if (wet > 0.0f) {
      float3 w = worley2(float2(u * 6.0f + lw * 13.0f, v * 16.0f + seed * 31.0f));
      if (w.y < 0.25f * wet) s.glint = smoothstep(0.14f, 0.05f, w.x) * (0.08f + 0.45f * fract(w.y * 91.0f));
    }
  } else if (type == 2) { // ---- GRASS BLADE
    float nb = h1(uint(lw * 8191.0f) + uint(seed * 65535.0f) * 31u);
    float st = vnoise(float3(u * 5.0f + lw * 40.0f, v * 3.0f, seed * 9.0f));
    float3 col = mix(M.a.rgb, M.b.rgb, clamp(nb * 1.25f - 0.15f + age, 0.0f, 1.0f));
    col *= (0.78f + 0.44f * st) * (0.9f + 0.2f * sin(u * 9.0f + lw * 50.0f));
    col = mix(M.c.rgb, col, smoothstep(0.0f, 0.28f, v + 0.1f * st));
    float dry = smoothstep(0.82f - 0.9f * step(0.82f, nb) * fract(nb * 17.0f), 1.0f, v + 0.1f * st) * step(0.55f, fract(nb * 5.3f));
    dry = max(dry, step(0.93f, fract(nb * 9.7f)) * 0.9f);
    col = mix(col, float3(0.42f, 0.34f, 0.15f) * (0.7f + 0.6f * st), dry);
    col *= tint;
    s.alb = col;
    s.tr = pow(col, float3(0.75f)) * float3(1.2f, 1.3f, 0.5f) * (M.b.w * (1.0f - 0.6f * dry) * (1.0f - 0.35f * smoothstep(0.12f, 0.0f, au)));
    s.rough = clamp(M.a.w * (0.85f + 0.4f * st) * (1.0f - 0.3f * wet), 0.1f, 1.0f);
    bump = float2(-sign(u) * 0.35f * (1.0f - v * 0.7f) + sin(u * 22.0f) * 0.12f, 0.0f);
    if (wet > 0.0f) {
      float cy = floor(v * 14.0f + nb * 7.0f); float hh = h1(uint(cy) * 131u + uint(nb * 65535.0f));
      float2 dc = float2(u - (hh - 0.5f) * 1.0f, (fract(v * 14.0f + nb * 7.0f) - 0.5f) * 2.4f);
      if (hh < 0.16f * wet && v > 0.25f) s.glint = smoothstep(0.5f, 0.15f, length(dc)) * (0.08f + 0.5f * fract(hh * 77.0f));
    }
  } else if (type == 3) { // ---- PETAL
    float st = fbm3(float3(u * 9.0f + lw * 17.0f, v * 1.7f, seed * 13.0f + lw), 3);
    float3 col = mix(M.a.rgb, M.b.rgb, smoothstep(0.0f, 0.45f, v + (st - 0.5f) * 0.2f));
    col = mix(col, M.c.rgb, smoothstep(0.55f, 1.0f, v) * M.p.z);
    col *= 0.82f + 0.36f * st;
    col *= 1.0f - 0.25f * smoothstep(0.42f, 0.5f, fabs(fract(u * 7.0f + st) - 0.5f)) * M.p.y;
    if (M.p.x > 0.0f) { // spots (foxglove throat)
      float3 w = worley2(float2(u * 7.0f + lw * 9.0f, v * 13.0f));
      float sp = smoothstep(0.30f, 0.18f, w.x) * step(v, 0.75f) * step(au, 0.7f) * M.p.x;
      col = mix(col, float3(0.95f, 0.9f, 0.92f), smoothstep(0.48f, 0.30f, w.x) * step(v, 0.75f) * step(au, 0.7f) * M.p.x * 0.8f);
      col = mix(col, float3(0.25f, 0.02f, 0.10f), sp);
    }
    col *= tint;
    s.alb = col; s.tr = pow(col, float3(0.8f)) * M.b.w; s.rough = M.a.w; s.spec = 0.6f;
    bump = float2(sin(u * 40.0f + st * 6.0f) * 0.10f, 0.0f);
  } else if (type == 4) { // ---- BARK / woody stem
    float3 q = pl * M.p.x;
    float n1 = fbm3(float3(q.x, q.y * M.p.y, q.z), 4);
    float ridge = fabs(fract(n1 * 6.0f + u * 3.0f) - 0.5f) * 2.0f;
    float3 col = mix(M.a.rgb, M.b.rgb, smoothstep(0.2f, 0.8f, ridge) * 0.7f + n1 * 0.4f);
    if (M.p.z > 0.5f) { // birch: white with dark lenticels
      float ln = smoothstep(0.62f, 0.72f, vnoise(float3(u * 5.0f, pl.y * 55.0f, seed)) * vnoise(float3(u * 2.0f, pl.y * 9.0f, 3.0f)) * 2.2f);
      col = mix(M.a.rgb * (0.8f + 0.3f * n1), M.b.rgb, ln);
    }
    float moss = smoothstep(0.55f, 0.75f, fbm3(pw * 9.0f, 3) + 0.25f * n.y - 0.2f * smoothstep(0.0f, 1.2f, pw.y)) * M.p.w;
    col = mix(col, float3(0.13f, 0.20f, 0.04f), moss);
    s.alb = col * tint; s.rough = 0.9f; s.spec = 0.3f;
    bump3 = float3(vnoise(q * 3.0f + 1.3f), vnoise(q * 3.0f + 5.1f), vnoise(q * 3.0f + 9.7f)) - 0.5f + (ridge - 0.5f) * Tw * 0.8f;
  } else if (type == 5) { // ---- STONE
    float3 base = M.p.z > 0.5f ? M.a.rgb : stonePalette(seed);
    float3 q = pl * M.p.x + seed * 31.0f;
    float n1 = fbm3(q, 4), n2 = vnoise(q * 9.0f), n3 = vnoise(q * 31.0f);
    float3 col = base * (0.62f + 0.75f * n1) * (0.86f + 0.28f * n3);
    col = mix(col, base * 1.45f, smoothstep(0.72f, 0.80f, n2) * 0.45f);
    col = mix(col, col * 0.5f, smoothstep(0.035f, 0.01f, fabs(fbm3(q * 0.7f + 9.0f, 3) - 0.42f)) * 0.5f * fract(seed * 91.0f));
    float wl = U.water.x;
    float wt = max(M.p.y, smoothstep(wl + 0.035f, wl + 0.005f, pw.y));
    float moss = smoothstep(0.50f, 0.70f, fbm3(pw * 14.0f, 3) + 0.30f * n.y - 0.12f) * M.p.w;
    col = mix(col, float3(0.055f, 0.085f, 0.022f) * (0.6f + 0.9f * n3), moss);
    if (M.q.x > 0.0f) { // lichen spots on dry sunny stone
      float3 lw3 = worley2(pl.xz * 21.0f + pl.y * 11.0f + seed * 20.0f);
      float li = smoothstep(0.36f, 0.16f, lw3.x + 0.25f * (n3 - 0.5f) + 0.2f * (n1 - 0.5f)) * step(lw3.y, 0.40f) * M.q.x;
      col = mix(col, lw3.y < 0.12f ? float3(0.40f, 0.33f, 0.12f) : float3(0.36f, 0.39f, 0.31f), li * 0.5f);
    }
    col *= mix(1.0f, 0.5f, wt);
    s.alb = col * tint; s.rough = mix(0.85f, 0.18f, wt) * (1.0f - 0.2f * wet); s.spec = mix(0.5f, 1.0f, wt);
    bump3 = (float3(vnoise(q * 6.0f + 1.3f), vnoise(q * 6.0f + 5.1f), vnoise(q * 6.0f + 9.7f)) - 0.5f) * 0.9f;
    if (moss > 0.3f) bump3 += (float3(vnoise(pw * 300.0f), vnoise(pw * 300.0f + 7.0f), vnoise(pw * 300.0f + 13.0f)) - 0.5f) * 1.5f;
  } else if (type == 6) { // ---- GROUND
    float2 mp = (pw.xz - U.mapRect.xy) * U.mapRect.zw;
    float4 g = gmap.sample(smp, mp, level(0.0f));
    float nA = fbm3(pw * 11.0f, 4), nB = vnoise(pw * 85.0f), nC = vnoise(pw * 330.0f), nD = fbm3(pw * 2.1f + 7.0f, 3);
    float3 cd = float3(0.30f, 0.28f, 0.25f) * (0.65f + 0.6f * nA);
    float3 cl = mix(float3(0.075f, 0.058f, 0.035f), float3(0.13f, 0.125f, 0.05f), nA) * (0.7f + 0.6f * nB);
    cl = mix(cl, float3(0.32f, 0.27f, 0.14f), smoothstep(0.72f, 0.8f, nC) * 0.5f);
    float3 cs = mix(float3(0.060f, 0.042f, 0.028f), float3(0.115f, 0.082f, 0.055f), nA) * (0.6f + 0.8f * nB);
    cs = mix(cs, float3(0.20f, 0.15f, 0.10f), smoothstep(0.70f, 0.78f, nC) * 0.6f);
    cs = mix(cs, cs * 1.5f, smoothstep(0.45f, 0.7f, nD) * 0.5f);
    float3 cg = float3(0.36f, 0.33f, 0.28f) * (0.5f + 0.7f * nB) * (0.8f + 0.4f * nC);
    float3 ca = float3(0.56f, 0.47f, 0.35f) * (0.72f + 0.5f * nB) * (0.85f + 0.3f * nC);
    float3 col = cd;
    col = mix(col, cl, g.r); col = mix(col, cs, g.g); col = mix(col, cg, g.b); col = mix(col, ca, g.a);
    float wl = U.water.x;
    float wt = smoothstep(wl + 0.07f, wl + 0.01f, pw.y + 0.03f * (nA - 0.5f));
    wt = max(wt, g.g * 0.35f * wet);
    col *= mix(1.0f, 0.45f, wt);
    if (pw.y < wl) col = mix(col, float3(0.07f, 0.085f, 0.035f) * (0.6f + 0.8f * nA), 0.75f);
    s.alb = col; s.rough = mix(0.95f, 0.25f, wt); s.spec = mix(0.25f, 1.0f, wt);
    float bs = mix(0.5f, 1.6f, max(g.g, max(g.b, g.a)));
    bump3 = (float3(vnoise(pw * 60.0f + 1.3f), 0.5f, vnoise(pw * 60.0f + 9.7f)) - 0.5f) * bs + (float3(vnoise(pw * 240.0f), 0.5f, vnoise(pw * 240.0f + 3.0f)) - 0.5f) * bs * 0.7f;
  } else if (type == 7) { // ---- WOOD (grain along v)
    float pid = floor(lw * 100.0f + 0.5f) + seed * 17.0f;
    float g1 = fbm3(float3(u * 55.0f * M.p.x + pid * 3.0f, v * 2.2f, pid), 4);
    float g2 = vnoise(float3(u * 260.0f * M.p.x, v * 6.0f, pid * 1.7f));
    float ring = fabs(fract(g1 * 9.0f) - 0.5f) * 2.0f;
    float3 col = mix(M.a.rgb, M.b.rgb, ring * 0.6f + g2 * 0.4f) * (0.75f + 0.5f * g1);
    float crack = smoothstep(0.80f, 0.86f, vnoise(float3(u * 30.0f * M.p.x + pid, v * 1.2f, 2.0f)) * (0.7f + 0.3f * g2));
    col *= 1.0f - 0.6f * crack;
    float alg = smoothstep(0.5f, 0.75f, fbm3(pw * 7.0f, 3) + 0.25f - smoothstep(0.0f, 0.9f, pw.y) * 0.5f) * M.p.w;
    col = mix(col, float3(0.16f, 0.22f, 0.07f), alg * 0.7f);
    s.alb = col * tint; s.rough = mix(M.a.w, 0.95f, 0.5f * g2); s.spec = 0.5f;
    bump = float2((ring - 0.5f) * 0.5f + crack * 1.2f * (g2 - 0.5f), 0.0f);
  } else if (type == 8) { // ---- BRICK (u,v in metres on the face)
    float bw = M.p.x, bh = M.p.y;
    float row = floor(v / bh); float uo = u + (fmod(row, 2.0f) < 0.5f ? 0.0f : bw * 0.5f) + seed * 0.37f;
    float col_i = floor(uo / bw);
    float2 f = float2(uo / bw - col_i, v / bh - row);
    float bid = h1(uint(int(col_i) + 4000) * 131u + uint(int(row) + 4000) * 7919u + uint(seed * 255.0f));
    float2 mw = float2(0.012f / bw, 0.011f / bh) * (1.0f + 0.5f * vnoise(float3(u * 70.0f, v * 70.0f, 1.0f)));
    float mort = 1.0f - smoothstep(mw.x * 0.5f, mw.x, min(f.x, 1.0f - f.x)) * smoothstep(mw.y * 0.5f, mw.y, min(f.y, 1.0f - f.y));
    float n1 = fbm3(float3(u * 18.0f, v * 18.0f, bid * 9.0f), 4), n2 = vnoise(float3(u * 160.0f, v * 160.0f, bid * 3.0f));
    float3 bc = mix(M.a.rgb, M.b.rgb, bid);
    if (bid > 0.86f) bc = M.c.rgb;
    bc *= (0.65f + 0.6f * n1) * (0.85f + 0.3f * n2);
    bc = mix(bc, float3(0.62f, 0.60f, 0.56f), smoothstep(0.62f, 0.8f, vnoise(float3(u * 9.0f, v * 9.0f, 7.0f)) * (0.6f + 0.4f * n2)) * 0.35f);
    float3 mc = float3(0.44f, 0.42f, 0.38f) * (0.75f + 0.4f * n2);
    float3 col = mix(bc, mc, mort);
    float alg = smoothstep(0.45f, 0.75f, fbm3(pw * 6.0f, 3) + 0.38f - smoothstep(0.0f, 0.55f, pw.y) * 0.55f + 0.2f * mort) * M.p.w;
    col = mix(col, float3(0.085f, 0.11f, 0.045f) * (0.6f + 0.8f * n2), alg * 0.8f);
    s.alb = col * tint; s.rough = 0.92f; s.spec = 0.3f;
    float2 em = float2(smoothstep(mw.x * 1.6f, mw.x * 0.6f, f.x) - smoothstep(mw.x * 1.6f, mw.x * 0.6f, 1.0f - f.x), smoothstep(mw.y * 1.6f, mw.y * 0.6f, f.y) - smoothstep(mw.y * 1.6f, mw.y * 0.6f, 1.0f - f.y));
    bump = em * 0.8f + float2(vnoise(float3(u * 300.0f, v * 300.0f, 1.0f)) - 0.5f, vnoise(float3(u * 300.0f, v * 300.0f, 5.0f)) - 0.5f) * 0.5f;
    s.alb *= 1.0f - 0.35f * mort;
  } else if (type == 9) { // ---- GLASS
    s.kind = 2;
    float cnd = fbm3(pw * float3(3.0f, 1.3f, 3.0f), 4);
    float low = smoothstep(2.4f, 0.4f, pw.y);
    float hz = clamp(smoothstep(0.30f, 0.62f, cnd + 0.35f * low) * M.p.x, 0.0f, 1.0f);
    hz *= mix(0.8f, 0.3f, smoothstep(22.5f, 24.0f, pw.x));
    float run = smoothstep(0.80f, 0.86f, vnoise(float3(pw.x * 60.0f + pw.z * 60.0f, pw.y * 1.5f, 2.0f)));
    hz *= 1.0f - 0.8f * run;
    s.haze = hz;
    float dirt = fbm3(pw * float3(9.0f, 2.0f, 9.0f) + 5.0f, 3);
    s.alb = mix(float3(0.80f, 0.84f, 0.82f), float3(0.45f, 0.52f, 0.36f), smoothstep(0.5f, 0.75f, dirt + 0.3f * smoothstep(0.6f, 0.0f, pw.y)));
    s.rough = 0.05f; s.f0 = 0.04f;
  } else if (type == 10) { // ---- WATER
    s.kind = 1; n = waterN(pw, U.timeP.x + r.time * U.timeP.y, U); ng = float3(0, 1, 0); s.rough = 0.02f; s.f0 = 0.02f;
  } else if (type == 11) { // ---- INSECT body
    int st = int(M.p.x + 0.5f);
    float3 col = M.a.rgb;
    if (st == 0) { // ladybird elytra: u around (0..1), v along
      float2 q = float2((u - 0.5f) * 2.0f, v);
      float sp = 0.0f;
      sp = max(sp, smoothstep(0.17f, 0.13f, length(float2(fabs(q.x) - 0.42f, (q.y - 0.36f) * 1.3f))));
      sp = max(sp, smoothstep(0.15f, 0.11f, length(float2(fabs(q.x) - 0.52f, (q.y - 0.62f) * 1.3f))));
      sp = max(sp, smoothstep(0.13f, 0.09f, length(float2(fabs(q.x) - 0.22f, (q.y - 0.78f) * 1.3f))));
      sp = max(sp, smoothstep(0.14f, 0.10f, length(float2(q.x, (q.y - 0.22f) * 1.2f))));
      col = mix(M.a.rgb, M.b.rgb, sp);
      col = mix(col, M.b.rgb, smoothstep(0.03f, 0.012f, fabs(q.x)) * 0.8f);
    } else if (st == 1) { // bee: bands along v, fuzzy
      float band = smoothstep(0.42f, 0.5f, fabs(fract(v * M.p.y + 0.15f) - 0.5f) + 0.12f * (vnoise(float3(u * 40.0f, v * 40.0f, 1.0f)) - 0.5f));
      col = mix(M.a.rgb, M.b.rgb, band);
      col = mix(col, M.c.rgb, smoothstep(0.82f, 0.9f, v));
      col *= 0.6f + 0.8f * vnoise(float3(u * 90.0f, v * 90.0f, 3.0f));
    } else if (st == 2) { // dragonfly: metallic segments
      float seg = smoothstep(0.40f, 0.5f, fabs(fract(v * M.p.y) - 0.5f));
      col = mix(M.a.rgb, M.b.rgb, seg);
    }
    s.alb = col * tint; s.rough = M.a.w; s.f0 = M.p.z > 0.0f ? M.p.z : 0.05f; s.spec = 1.0f;
  } else if (type == 12) { // ---- WING
    if (M.p.x < 0.5f) { // butterfly: v from root (0) to tip (1), u across chord -1..1
      float rr = v, th = u;
      float veins = smoothstep(0.40f, 0.5f, fabs(fract(th * 3.5f + 0.5f) - 0.5f)) * smoothstep(0.1f, 0.3f, rr);
      float cross = smoothstep(0.44f, 0.5f, fabs(fract(rr * 2.3f + 0.2f * sin(th * 9.0f)) - 0.5f)) * 0.0f;
      float margin = smoothstep(0.80f, 0.88f, rr + 0.04f * sin(th * 30.0f));
      float3 col = mix(M.a.rgb, M.b.rgb, smoothstep(0.0f, 0.8f, rr) * 0.6f);
      col *= 0.85f + 0.3f * vnoise(float3(u * 30.0f, v * 30.0f, seed));
      col = mix(col, M.c.rgb, max(max(veins, cross) * 0.9f, margin));
      float dots = smoothstep(0.30f, 0.18f, length(float2(fract(th * 6.0f) - 0.5f, (rr - 0.92f) * 9.0f))) * margin;
      col = mix(col, float3(0.92f), dots);
      s.alb = col * tint; s.tr = s.alb * 0.35f; s.rough = 0.6f; s.spec = 0.35f;
    } else { // clear membranous wing
      s.alb = float3(0.55f, 0.6f, 0.62f); s.tr = float3(0.6f); s.rough = 0.15f; s.f0 = 0.08f; s.spec = 2.0f;
    }
  } else if (type == 13) { // ---- KOI (u around 0..1 with 0.25 = back, v head..tail 0..1)
    float side = fabs(fract(u + 0.25f) - 0.5f) * 2.0f; // 1 at back, 0 at belly
    float kind = fract(seed * 7.13f);
    float n1 = fbm3(float3(v * 3.2f + seed * 40.0f, fract(u) * 2.4f, seed * 9.0f), 3);
    float n2 = fbm3(float3(v * 5.0f + seed * 11.0f, fract(u) * 3.0f, seed * 23.0f), 3);
    float3 white = float3(0.86f, 0.84f, 0.78f), orange = float3(0.90f, 0.22f, 0.03f), black = float3(0.03f, 0.03f, 0.035f), gold = float3(0.85f, 0.55f, 0.10f);
    float3 col;
    if (kind < 0.40f) col = mix(white, orange, smoothstep(0.46f, 0.52f, n1 + 0.15f * side));
    else if (kind < 0.62f) { col = mix(white, orange, smoothstep(0.48f, 0.54f, n1 + 0.1f * side)); col = mix(col, black, smoothstep(0.60f, 0.64f, n2)); }
    else if (kind < 0.80f) col = gold * (0.8f + 0.4f * n1);
    else if (kind < 0.90f) col = mix(orange, black, smoothstep(0.5f, 0.56f, n2) * 0.8f);
    else col = mix(black * 3.0f, orange, smoothstep(0.55f, 0.6f, n1));
    col = mix(col, white * 0.9f, smoothstep(0.35f, 0.0f, side) * 0.6f);
    float3 sc = worley2(float2(fract(u) * 46.0f, v * 60.0f));
    col *= 0.85f + 0.25f * smoothstep(0.0f, 0.5f, sc.x);
    if (M.p.x > 0.5f) { col = mix(col, float3(0.9f, 0.85f, 0.8f), 0.35f); s.tr = col * 0.4f; col *= 0.9f + 0.2f * sin(u * 60.0f); } // fins
    s.alb = col; s.rough = 0.25f; s.spec = 1.0f; s.f0 = 0.05f;
    bump = float2(sc.x - 0.4f, 0.0f) * 0.4f;
  } else if (type == 14) { // ---- FUR
    float n1 = fbm3(pl * M.p.x + seed, 3);
    float stripe = smoothstep(0.45f, 0.6f, fbm3(pl * float3(M.p.y, M.p.y * 0.25f, M.p.y) + 3.0f, 3));
    float3 col = mix(M.a.rgb, M.b.rgb, stripe) * (0.7f + 0.6f * n1);
    col = mix(col, M.c.rgb, smoothstep(M.p.z, M.p.z - 0.03f, lw) * step(0.001f, M.p.z));
    col = mix(col * 0.6f, col, smoothstep(0.0f, 0.5f, v));
    s.alb = col * tint; s.tr = col * M.b.w; s.rough = 0.55f; s.spec = 0.6f;
  } else if (type == 15) { // ---- CLOTH
    float tw = sin((u * 900.0f + v * 900.0f) * M.p.x) * 0.5f + 0.5f;
    float n1 = fbm3(pl * 14.0f + seed, 4), n2 = vnoise(pl * 220.0f);
    float3 col = mix(M.a.rgb, M.b.rgb, tw * 0.5f + 0.3f * n2) * (0.7f + 0.55f * n1);
    s.alb = col * tint; s.rough = 0.95f; s.spec = 0.15f;
    bump3 = (float3(vnoise(pl * 30.0f + 1.0f), vnoise(pl * 30.0f + 4.0f), vnoise(pl * 30.0f + 8.0f)) - 0.5f) * 0.7f;
  } else if (type == 16) { // ---- CACTUS / SUCCULENT skin: u around (0..1), v along
    float ribs = M.p.x;
    float f = ribs > 0.0f ? fabs(fract(u * ribs) - 0.5f) * 2.0f : 1.0f; // 0 at crest
    float n1 = fbm3(pl * 22.0f + seed * 7.0f, 3), n2 = vnoise(pl * 260.0f);
    float3 col = mix(M.a.rgb, M.b.rgb, clamp(n1 * 1.2f - 0.1f + lw * 0.4f, 0.0f, 1.0f)) * (0.8f + 0.4f * n2);
    if (ribs > 0.0f) {
      col *= 0.72f + 0.28f * smoothstep(1.0f, 0.2f, f);
      float ar = smoothstep(0.16f, 0.07f, f) * smoothstep(0.30f, 0.14f, fabs(fract(v * M.p.y) - 0.5f));
      col = mix(col, float3(0.78f, 0.72f, 0.58f), ar);
    } else {
      col = mix(col, M.c.rgb, smoothstep(0.80f, 1.0f, v) * M.p.z + smoothstep(0.85f, 1.0f, au) * M.p.z * 0.6f);
      col = mix(col, float3(0.75f, 0.82f, 0.80f), M.p.w * (0.35f + 0.3f * n1)); // glaucous bloom
    }
    s.alb = col * tint; s.rough = M.a.w; s.tr = col * M.b.w; s.spec = 0.8f;
  } else if (type == 17) { // ---- LABEL
    float3 col = M.a.rgb * (0.8f + 0.4f * vnoise(pl * 300.0f));
    uint rows = uint(M.p.x);
    uint row = min(uint(seed * float(rows)), rows - 1u);
    float2 tq = float2((u - 0.06f) / 0.88f, (v - 0.30f) / 0.40f);
    if (tq.x > 0.0f && tq.x < 1.0f && tq.y > 0.0f && tq.y < 1.0f) {
      float tx = labels.sample(smp, float2(tq.x, (float(row) + 0.07f + (1.0f - tq.y) * 0.86f) / float(rows)), level(0.0f)).r;
      col = mix(col, M.b.rgb, smoothstep(0.35f, 0.6f, tx));
    }
    s.alb = col; s.rough = 0.35f; s.f0 = 0.05f;
  } else if (type == 18) { // ---- DEW / water drop
    s.kind = 3; s.rough = 0.0f; s.f0 = 0.02f; s.alb = float3(1.0f);
  } else if (type == 19) { // ---- painted metal
    float n1 = fbm3(pl * 9.0f + seed * 3.0f, 4), n2 = vnoise(pl * 55.0f);
    float chip = smoothstep(0.60f, 0.66f, n1 + 0.10f * n2) * M.p.x;
    float3 col = mix(M.a.rgb * (0.94f + 0.08f * n2), M.b.rgb * (0.7f + 0.5f * n2), chip);
    float alg = smoothstep(0.5f, 0.8f, fbm3(pw * 5.0f, 3) + 0.2f) * M.p.w;
    col = mix(col, float3(0.20f, 0.26f, 0.10f), alg * 0.5f);
    s.alb = col; s.rough = mix(M.a.w, 0.9f, chip); s.f0 = 0.05f;
    bump3 = (float3(n2, vnoise(pl * 120.0f + 3.0f), vnoise(pl * 120.0f + 7.0f)) - 0.5f) * 0.25f * (0.3f + chip);
  } else if (type == 21) { // ---- PAVING (world xz)
    float2 q = pw.xz;
    float sw = M.p.x, sh = M.p.y;
    float row = floor(q.y / sh); float xo = q.x + h1(uint(int(row) + 999) * 31u) * sw;
    float ci = floor(xo / sw);
    float2 f = float2(xo / sw - ci, q.y / sh - row);
    float bid = h1(uint(int(ci) + 2000) * 131u + uint(int(row) + 2000) * 7919u);
    float2 jw = float2(0.012f / sw, 0.012f / sh);
    float jd = min(min(f.x, 1.0f - f.x) / jw.x, min(f.y, 1.0f - f.y) / jw.y);
    float joint = 1.0f - smoothstep(0.5f, 1.0f, jd);
    float n1 = fbm3(pw * 7.0f + bid * 13.0f, 4), n2 = vnoise(pw * 130.0f), n3 = fbm3(pw * 1.3f, 3);
    float3 col = mix(M.a.rgb, M.b.rgb, bid) * (0.7f + 0.5f * n1) * (0.88f + 0.24f * n2);
    float moss = max(joint * smoothstep(0.25f, 0.6f, n3 + 0.1f), smoothstep(0.62f, 0.75f, n1 + 0.3f * smoothstep(3.0f, 1.0f, jd))) * M.p.w;
    col = mix(col, mix(float3(0.10f, 0.08f, 0.06f), float3(0.12f, 0.20f, 0.04f), smoothstep(0.3f, 0.6f, n3)), max(joint * 0.85f, moss * 0.8f));
    float pud = smoothstep(0.52f, 0.60f, n3 + 0.05f * n1) * M.p.z * (1.0f - joint);
    col *= mix(1.0f, 0.55f, pud);
    s.alb = col; s.rough = mix(0.8f, 0.04f, pud); s.spec = mix(0.5f, 1.0f, pud);
    bump3 = (float3(n2, 0.5f, vnoise(pw * 130.0f + 5.0f)) - 0.5f) * 0.5f * (1.0f - pud) + float3((f.x < 0.5f ? 1.0f : -1.0f) * step(jd, 1.0f), 0.0f, (f.y < 0.5f ? 1.0f : -1.0f) * step(jd, 1.0f)) * 0.3f * joint;
  } else { // ---- PLAIN (type 0): two-colour fbm blend in local space
    float n1 = fbm3(pl * M.p.y + seed * 5.0f, 4), n2 = vnoise(pl * M.p.y * 12.0f);
    float3 col = mix(M.a.rgb, M.b.rgb, clamp((n1 - 0.5f) * 2.0f * M.p.x + 0.5f * M.p.x, 0.0f, 1.0f)) * (0.88f + 0.24f * n2);
    float grime = smoothstep(0.5f, 0.8f, fbm3(pw * 8.0f, 3) + 0.3f - smoothstep(0.0f, 0.3f, pw.y) * 0.4f) * M.p.w;
    col = mix(col, M.c.rgb, grime);
    s.alb = col * tint; s.tr = s.alb * M.b.w; s.rough = M.a.w; s.spec = M.p.z > 0.0f ? M.p.z : 0.6f;
    bump3 = (float3(n2, vnoise(pl * M.p.y * 12.0f + 3.0f), vnoise(pl * M.p.y * 12.0f + 7.0f)) - 0.5f) * 0.3f;
  }
  if (s.kind == 0) {
    float3 nn = n - Tw * bump.x - Bw * bump.y;
    nn -= (bump3 - n * dot(bump3, n));
    n = normalize(nn);
    if (back) { n = -n; ng = -ng; }
    if (dot(n, r.d) > -0.02f) n = normalize(n - r.d * (dot(n, r.d) + 0.02f));
  } else if (s.kind != 1) {
    if (back) { n = -n; ng = -ng; }
  }
  s.n = n; s.ng = ng; s.back = back;
  return s;
}

// ---------------------------------------------------------------- shading helpers
static float3 sunBRDF(Surf s, float3 V, float3 L, constant Uni& U) {
  float nl = dot(s.n, L);
  float3 c = float3(0.0f);
  if (nl > 0.0f) {
    c = s.alb * nl;
    float3 H = normalize(V + L);
    float nh = max(dot(s.n, H), 0.0f), nv = max(dot(s.n, V), 1e-3f), vh = max(dot(V, H), 0.0f);
    float a = max(s.rough * s.rough, 0.003f), a2 = a * a; float d = nh * nh * (a2 - 1.0f) + 1.0f; float D = a2 / (PI * d * d);
    float k = a * 0.5f; float G = 1.0f / ((nv * (1.0f - k) + k) * (nl * (1.0f - k) + k));
    float F = s.f0 + (1.0f - s.f0) * pow(1.0f - vh, 5.0f);
    c += float3(min(s.spec * D * F * 0.25f * G * nl * PI, 40.0f));
  } else c = s.tr * (-nl);
  return c * U.sunCol.rgb;
}
static inline float3 cosDir(float3 n, float u1, float u2) {
  float r = sqrt(u1), ph = 6.2831853f * u2;
  float3 t = normalize(fabs(n.y) < 0.99f ? cross(n, float3(0, 1, 0)) : cross(n, float3(1, 0, 0)));
  float3 b = cross(n, t);
  return normalize(t * (r * cos(ph)) + b * (r * sin(ph)) + n * sqrt(max(1.0f - u1, 0.0f)));
}
static inline float3 coneDir(float3 d, float cosMax, float u1, float u2) {
  float ct = 1.0f - u1 * (1.0f - cosMax), st = sqrt(max(1.0f - ct * ct, 0.0f)), ph = 6.2831853f * u2;
  float3 t = normalize(fabs(d.y) < 0.99f ? cross(d, float3(0, 1, 0)) : cross(d, float3(1, 0, 0)));
  float3 b = cross(d, t);
  return normalize(t * (st * cos(ph)) + b * (st * sin(ph)) + d * ct);
}
struct FogSeg { float lenIn; };
static inline float fogTau(float3 o, float3 d, float t, constant Uni& U) {
  float3 inv = safeInv(d);
  float3 t0 = (U.fogLo.xyz - o) * inv, t1 = (U.fogHi.xyz - o) * inv; float3 tn = min(t0, t1), tf = max(t0, t1);
  float a = max(max(tn.x, tn.y), max(tn.z, 0.0f)), b = min(min(tf.x, tf.y), min(tf.z, t));
  float li = max(b - a, 0.0f);
  return U.fogLo.w * li + U.fogHi.w * (t - li);
}

static inline bool inGH(float3 p, constant Uni& U) {
  if (!(all(p > U.fogLo.xyz) && all(p < U.fogHi.xyz))) return false;
  float zr = 0.5f * (U.fogLo.z + U.fogHi.z), hs = 0.5f * (U.fogHi.z - U.fogLo.z);
  return p.y < U.water.z + (U.fogHi.y - U.water.z) * (1.0f - fabs(p.z - zr) / hs);
}
// ---------------------------------------------------------------- path integrator
static float3 radiance(Scene S, Ray ray, constant Uni& U, texture2d<float> gmap, texture2d<float> labels, thread uint& rs, float4 xi, float xl) {
  float3 L = float3(0.0f), thr = float3(1.0f);
  float3 sun = U.sunDir.xyz;
  bool inWater = false;
  uint skip = 1u;
  float tabs = U.timeP.x + ray.time * U.timeP.y;
  for (int depth = 0; depth < 7; depth++) {
    ray.rs = rs + uint(depth) * 7717u;
    Hit h = trace(S, ray, 200.0f, skip, U);
    bool miss = h.inst < 0;
    float tseg = miss ? 45.0f : h.t;
    if (inWater) { // absorption in pond water
      float3 sig = float3(1.9f, 0.75f, 1.25f) * U.water.y;
      float3 a = exp(-sig * tseg);
      L += thr * (float3(1.0f) - a) * float3(0.018f, 0.032f, 0.012f) * U.sunCol.rgb;
      thr *= a;
    } else { // outdoor haze (analytic) + greenhouse mist (single scattering with shadow rays -> light shafts)
      float3 inv = safeInv(ray.d);
      float3 t0 = (U.fogLo.xyz - ray.o) * inv, t1 = (U.fogHi.xyz - ray.o) * inv; float3 tn = min(t0, t1), tf = max(t0, t1);
      float fa = max(max(tn.x, tn.y), max(tn.z, 0.0f)), fb = min(min(tf.x, tf.y), min(tf.z, tseg));
      float li = max(fb - fa, 0.0f);
      float mu = dot(ray.d, sun);
      float tauO = U.fogHi.w * (tseg - li) * exp(-max(ray.o.y + ray.d.y * tseg * 0.3f, 0.0f) * 0.06f);
      float pho = (1.0f - 0.25f) / (4.0f * PI * pow(1.25f - mu, 1.5f)) * 0.7f + 0.3f / (4.0f * PI);
      float eo = exp(-tauO);
      L += thr * (U.sunCol.rgb * (PI * pho * 0.75f) + skyAmb(U) * 0.55f) * (1.0f - eo);
      thr *= eo;
      if (li > 0.0f) {
        float fin = 0.0f;
        for (int k = 0; k < 4; k++) fin += inGH(ray.o + ray.d * (fa + (float(k) + 0.5f) * 0.25f * li), U) ? 0.25f : 0.0f;
        float ts = fa + rnd(rs) * li; float3 ps = ray.o + ray.d * ts;
        if (inGH(ps, U)) {
          float3 sd = coneDir(sun, U.sunDir.w, rnd(rs), rnd(rs));
          float3 sh = shadow(S, ps, sd, 120.0f, ray.time, rs, U);
          float g = 0.45f;
          float ph = (1.0f - g * g) / (4.0f * PI * pow(1.0f + g * g - 2.0f * g * mu, 1.5f)) * 0.7f + 0.3f / (4.0f * PI);
          float3 Lin = U.sunCol.rgb * (PI * ph) * sh + skyAmb(U) * 0.9f;
          L += thr * Lin * (U.fogLo.w * li * exp(-U.fogLo.w * (ts - fa) * fin));
        }
        thr *= exp(-U.fogLo.w * li * fin);
      }
    }
    if (miss) { L += thr * skyRad(ray.d, U, true); break; }
    Surf s = evalSurf(S, h, ray, U, gmap, labels);
    float3 P = ray.o + ray.d * h.t;
    float eps = 4e-5f + 1.5e-5f * h.t;
    float3 V = -ray.d;
    if (s.kind == 1) { // water surface
      skip = 0u;
      if (inWater) { ray.o = P + float3(0, 1, 0) * eps; inWater = false; continue; }
      float cosi = max(dot(V, s.n), 0.0f);
      float F = 0.02f + 0.98f * pow(1.0f - cosi, 5.0f);
      if (depth == 0) { // first hit: evaluate reflection AND refraction (no stochastic pick -> much less noise on the pond)
        float3 rd = reflect(ray.d, s.n); if (rd.y < 0.01f) rd.y = 0.01f; rd = normalize(rd);
        Ray rr; rr.o = P + float3(0, 1, 0) * eps; rr.d = rd; rr.time = ray.time; rr.rs = rs + 313u;
        Hit hr = trace(S, rr, 80.0f, 0u, U);
        float3 Lr;
        if (hr.inst < 0) Lr = skyRad(rd, U, true);
        else {
          Surf sr = evalSurf(S, hr, rr, U, gmap, labels);
          if (sr.kind == 0) {
            float3 sd = coneDir(sun, U.sunDir.w, rnd(rs), rnd(rs));
            float nl = dot(sr.ng, sd); float er = 4e-5f + 1.5e-5f * hr.t;
            float3 shr = float3(0.0f);
            if (nl > 0.0f || lum(sr.tr) > 0.0f) shr = shadow(S, rr.o + rr.d * hr.t + sr.ng * (nl > 0.0f ? er : -er), sd, 150.0f, ray.time, rs, U);
            Lr = sunBRDF(sr, -rd, sd, U) * shr + (sr.alb + sr.tr * 0.5f) * skyAmb(U) * 0.55f;
          } else Lr = skyRad(rd, U, false) * 0.7f;
        }
        L += thr * F * min(Lr, float3(24.0f));
        thr *= (1.0f - F);
        float3 td = refract(ray.d, s.n, 1.0f / 1.33f); if (td.y > -0.05f) td = normalize(float3(ray.d.x, -0.3f, ray.d.z));
        ray.d = td; ray.o = P - float3(0, 1, 0) * eps; inWater = true;
        continue;
      }
      float pick = rnd(rs);
      if (pick < F) { float3 rd = reflect(ray.d, s.n); if (rd.y < 0.01f) rd.y = 0.01f; ray.d = normalize(rd); ray.o = P + float3(0, 1, 0) * eps; }
      else { float3 rd = refract(ray.d, s.n, 1.0f / 1.33f); if (rd.y > -0.05f) rd = normalize(float3(ray.d.x, -0.3f, ray.d.z)); ray.d = rd; ray.o = P - float3(0, 1, 0) * eps; inWater = true; }
      continue;
    }
    if (s.kind == 2) { // greenhouse glass (thin, partly fogged)
      skip = 0u;
      float cosi = max(dot(V, s.n), 0.0f);
      float F = 0.04f + 0.96f * pow(1.0f - cosi, 5.0f);
      float r1 = depth == 0 ? xl : rnd(rs);
      float pR = F * (1.0f - s.haze * 0.7f);
      if (r1 < pR) { ray.d = reflect(ray.d, s.n); ray.o = P + s.ng * eps; continue; }
      if ((r1 - pR) / max(1.0f - pR, 1e-3f) < s.haze * 0.55f) { // condensation acts as a thin diffuse layer
        float3 sd = coneDir(sun, U.sunDir.w, rnd(rs), rnd(rs));
        float nl = dot(s.n, sd);
        float3 sh = shadow(S, P + s.ng * (nl > 0 ? eps : -eps), sd, 120.0f, ray.time, rs, U);
        L += thr * s.alb * (0.45f * fabs(nl) * U.sunCol.rgb * sh + skyAmb(U) * 0.75f);
        break;
      }
      float3 j = float3(rnd(rs), rnd(rs), rnd(rs)) - 0.5f;
      ray.d = normalize(ray.d + j * (0.02f + 0.5f * s.haze * s.haze));
      thr *= float3(0.90f, 0.94f, 0.91f);
      ray.o = P - s.ng * eps; continue;
    }
    if (s.kind == 3) { // water drop
      skip = 0u;
      float3 nn = s.n; float eta = s.back ? 1.33f : 1.0f / 1.33f;
      float cosi = max(dot(V, nn), 0.0f);
      float F = 0.02f + 0.98f * pow(1.0f - cosi, 5.0f);
      float3 rd = refract(ray.d, nn, eta);
      if (dot(rd, rd) < 0.5f || (!s.back && rnd(rs) < F)) { ray.d = reflect(ray.d, nn); ray.o = P + nn * eps; }
      else { ray.d = normalize(rd); ray.o = P - nn * eps; }
      continue;
    }
    // ---- opaque / translucent surface: direct sun + one bounce
    float cau = 1.0f;
    if (inWater) cau = caustic(P.xz, tabs) * smoothstep(-0.01f, -0.10f, P.y - U.water.x) + 1.0f * smoothstep(-0.10f, -0.01f, P.y - U.water.x);
    float3 sd = coneDir(sun, U.sunDir.w, xi.x, xi.y);
    float nlg = dot(s.ng, sd);
    float3 so = P + s.ng * (nlg > 0.0f ? eps : -eps);
    float3 sh = float3(0.0f);
    if (nlg > 0.0f || lum(s.tr) > 0.0f) sh = shadow(S, so, sd, 150.0f, ray.time, rs, U);
    float3 direct = sunBRDF(s, V, sd, U);
    if (s.glint > 0.0f) {
      float tw = pow(max(dot(ray.d, sun), 0.0f), 3.0f);
      direct += U.sunCol.rgb * s.glint * (0.6f + 3.0f * tw) * smoothstep(0.04f, 0.3f, h.t) / (1.0f + h.t * h.t * 0.6f);
    }
    L += thr * (s.emis + direct * sh * cau);
    // far surfaces: take the one-bounce estimate only part of the time, otherwise a smooth ambient term (saves two traversals where detail is sub-pixel anyway)
    float pB = clamp(1.3f - h.t / 6.0f, 0.3f, 1.0f);
    if (rnd(rs) > pB) { L += thr * (s.alb + s.tr * 0.5f) * skyAmb(U) * (0.42f + 0.3f * max(s.n.y, 0.0f)); break; }
    // bounce
    float nv = max(dot(s.n, V), 1e-3f);
    float Fv = s.f0 + (1.0f - s.f0) * pow(1.0f - nv, 5.0f);
    float pS = s.spec > 0.0f ? clamp(Fv * s.spec * (1.6f - s.rough) * 1.5f, 0.04f, 0.6f) : 0.0f;
    float lt = lum(s.tr), la = lum(s.alb);
    float pT = (1.0f - pS) * lt / (lt + la + 1e-4f);
    float pD = 1.0f - pS - pT;
    float r1 = rnd(rs);
    float3 bd; float3 w; float bmax = 3.5f; bool specB = false;
    if (r1 < pS) {
      float a = max(s.rough * s.rough, 0.002f);
      float u1 = xi.z, u2 = xi.w;
      float ct = sqrt((1.0f - u1) / (1.0f + (a * a - 1.0f) * u1)), st = sqrt(max(1.0f - ct * ct, 0.0f)), ph = 6.2831853f * u2;
      float3 t = normalize(fabs(s.n.y) < 0.99f ? cross(s.n, float3(0, 1, 0)) : cross(s.n, float3(1, 0, 0)));
      float3 b = cross(s.n, t);
      float3 H = normalize(t * (st * cos(ph)) + b * (st * sin(ph)) + s.n * ct);
      bd = reflect(ray.d, H);
      if (dot(bd, s.ng) <= 0.0f) { break; }
      float vh = max(dot(V, H), 1e-3f), nh = max(dot(s.n, H), 1e-3f), nl = max(dot(s.n, bd), 1e-3f);
      float k = a * 0.5f; float G = (nv / (nv * (1.0f - k) + k)) * (nl / (nl * (1.0f - k) + k));
      float F = s.f0 + (1.0f - s.f0) * pow(1.0f - vh, 5.0f);
      w = float3(min(s.spec * F * G * vh / (nv * nh) / pS, 3.0f));
      bmax = 40.0f; specB = true;
    } else if (r1 < pS + pT) {
      bd = cosDir(-s.n, xi.z, xi.w); w = s.tr / max(pT, 1e-3f);
    } else {
      bd = cosDir(s.n, xi.z, xi.w); w = s.alb / max(pD, 1e-3f);
      if (dot(bd, s.ng) <= 0.0f) break;
    }
    Ray br; br.o = P + s.ng * (dot(bd, s.ng) > 0.0f ? eps : -eps); br.d = bd; br.time = ray.time; br.rs = rs + 991u;
    Hit h2 = trace(S, br, bmax, 4u, U);
    float3 Li;
    if (h2.inst < 0) {
      Li = (bd.y > -0.05f || specB) ? skyRad(bd, U, false) : skyAmb(U) * 0.25f;
      if (inWater) Li *= 0.5f;
    } else {
      Surf s2 = evalSurf(S, h2, br, U, gmap, labels);
      float3 P2 = br.o + br.d * h2.t;
      if (s2.kind == 1) {
        float3 rd = reflect(bd, s2.n); rd.y = fabs(rd.y);
        float F2 = 0.02f + 0.98f * pow(1.0f - max(dot(-bd, s2.n), 0.0f), 5.0f);
        Li = skyRad(rd, U, false) * F2 + float3(0.02f, 0.035f, 0.02f) * U.sunCol.rgb * (1.0f - F2);
      } else if (s2.kind != 0) {
        Li = skyRad(bd, U, false) * 0.7f;
      } else {
        float3 sd2 = coneDir(sun, U.sunDir.w, rnd(rs), rnd(rs));
        float nl2 = dot(s2.ng, sd2);
        float e2 = 4e-5f + 1.5e-5f * h2.t;
        float3 sh2 = float3(0.0f);
        if (nl2 > 0.0f || lum(s2.tr) > 0.0f) sh2 = shadow(S, P2 + s2.ng * (nl2 > 0.0f ? e2 : -e2), sd2, 150.0f, ray.time, rs, U);
        Surf sdif = s2; sdif.spec = 0.0f;
        Li = sunBRDF(sdif, -bd, sd2, U) * sh2 + (s2.alb + s2.tr * 0.5f) * skyAmb(U) * (0.22f + 0.22f * max(s2.n.y, 0.0f));
      }
    }
    L += thr * w * min(Li, float3(30.0f));
    break;
  }
  return L;
}

// ---------------------------------------------------------------- kernel
kernel void render(texture2d<float, access::read_write> acc [[texture(0)]],
                   texture2d<float> gmap [[texture(1)]],
                   texture2d<float> labels [[texture(2)]],
                   constant Uni& U [[buffer(0)]],
                   const device Node* nodes [[buffer(1)]],
                   const device Tri* tris [[buffer(2)]],
                   const device TriAttr* attrs [[buffer(3)]],
                   const device Inst* insts [[buffer(4)]],
                   const device Node* tlas [[buffer(5)]],
                   const device Mat* mats [[buffer(6)]],
                   const device Node* dtlas [[buffer(7)]],
                   const device Inst* dinst0 [[buffer(8)]],
                   const device Inst* dinst1 [[buffer(9)]],
                   uint2 gid [[thread_position_in_grid]]) {
  uint2 px = uint2(gid.x, gid.y + U.dims.z);
  if (px.x >= U.dims.x || px.y >= U.dims.y) return;
  Scene S; S.nodes = nodes; S.tris = tris; S.attrs = attrs; S.insts = insts; S.tlas = tlas; S.mats = mats; S.dtlas = dtlas; S.dinst0 = dinst0; S.dinst1 = dinst1;
  uint pseed = pcg(px.x + pcg(px.y + pcg(U.counts.x)));
  float4 o0 = float4(h1(pseed), h1(pseed + 1u), h1(pseed + 2u), h1(pseed + 3u));
  float4 o1 = float4(h1(pseed + 4u), h1(pseed + 5u), h1(pseed + 6u), h1(pseed + 7u));
  float3 sum = float3(0.0f);
  for (uint si = 0; si < U.counts.z; si++) {
    uint sidx = U.counts.y + si;
    uint rs = pcg(pseed + sidx * 9781u + 17u);
    float4 k0 = fract(o0 + float(sidx) * U.kr0), k1 = fract(o1 + float(sidx) * U.kr1);
    Ray r; r.time = k1.x; r.rs = rs;
    float a = r.time;
    float3 cp = mix(U.camPos0.xyz, U.camPos1.xyz, a);
    float3 cr = normalize(mix(U.camR0.xyz, U.camR1.xyz, a)), cu = normalize(mix(U.camU0.xyz, U.camU1.xyz, a)), cf = normalize(mix(U.camF0.xyz, U.camF1.xyz, a));
    float2 ndc = (float2(px) + k0.xy) / float2(U.dims.xy) * 2.0f - 1.0f;
    ndc.y = -ndc.y * U.lens.z;
    float3 L;
    if (U.water.w > 0.5f) { // orthographic map view
      r.o = cp + cr * (ndc.x * U.lens.x) + cu * (ndc.y * U.lens.x); r.d = cf;
    } else {
      float rr = length(ndc);
      float th = 2.0f * atan(rr * U.lens.x);
      float3 dir = rr > 1e-6f ? normalize(cf * cos(th) + (cr * ndc.x + cu * ndc.y) * (sin(th) / rr)) : cf;
      float fd = mix(U.camPos0.w, U.camPos1.w, a);
      float3 pf = cp + dir * fd;
      float lr = sqrt(k0.z) * U.lens.y, la = 6.2831853f * k0.w;
      r.o = cp + cr * (lr * cos(la) * U.lens.w) + cu * (lr * sin(la));
      r.d = normalize(pf - r.o);
    }
    float4 xi = float4(k1.y, k1.z, 0.0f, 0.0f);
    xi.z = rnd(rs); xi.w = rnd(rs);
    L = radiance(S, r, U, gmap, labels, rs, xi, k1.w);
    if (!(L.x == L.x) || !(L.y == L.y) || !(L.z == L.z)) L = float3(0.0f);
    float m = max(L.x, max(L.y, L.z));
    if (m > 60.0f) L *= 60.0f / m;
    sum += L;
  }
  float4 prev = acc.read(px);
  acc.write(prev + float4(sum, float(U.counts.z)), px);
}
