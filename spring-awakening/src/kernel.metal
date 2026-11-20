// kernel.metal — path tracer megakernel (own two-level BVH, thin lens, procedural materials).
#include <metal_stdlib>
using namespace metal;

struct Node { packed_float3 mn; uint leftFirst; packed_float3 mx; uint count; };
struct Tri { packed_float3 p0; uint mat; packed_float3 e1; float rnd; packed_float3 e2; uint pad; };
struct TriA { packed_float3 n0; float u0; packed_float3 n1; float u1; packed_float3 n2; float u2; float v0, v1, v2, a0, a1, a2, pad0, pad1; };
struct Seg { packed_float3 p0; float r0; packed_float3 p1; float r1; uint mat; float v0, v1, rnd; };
struct Inst { float4 a0, a1, a2; float4 b0, b1, b2; float4 i0, i1, i2; uint root, primBase, flags, matBase; float4 tint; float rnd; float pad0, pad1, pad2; };
struct Mat { float4 base, base2, p0, p1, p2, sh; uint type, pattern; float rough, ior; float trans, bump, coat, scale; };
struct Glint { packed_float3 pos; float radius; packed_float3 col; float pad; };

struct Uni {
    float4 camPos, camR, camU, camF;
    float4 lens;   // tanHalfW, tanHalfH, lensRadius, focusDist
    float4 lens2;  // blades, rotation, roundness, catEye
    float4 lens3;  // rim, shutter t0, shutter t1, eps
    float4 sunDir; // xyz, angular radius
    float4 sunE;   // irradiance rgb, clamp
    float4 skyZen, skyHor, skyGnd;
    float4 skyP;   // horExp, glow, glowExp, envType
    float4 bd0, bd1; // backdrop colours, w: scale / mix
    float4 bdP;
    float4 fog;    // density, falloff, y0, g
    float4 fogCol; // ambient rgb, max dist
    float4 cloud;  // strength, scale, ofs x, ofs z
    float4 misc;   // time, skyGain, gap, sunDisc
    float4 misc2;  // fog samples, fog sun on/off, fog clamp, 0
    uint4 dims;    // w, h, sampleBase, spp
    uint4 counts;  // nInst, nGlints, frameSeed, flags
    uint4 tiles;   // tilesX, tilesY, maxPerTile, 0
};

constant float PI = 3.14159265358979;

// ---------------- rng ----------------
static inline uint pcg(uint v) { uint s = v * 747796405u + 2891336453u; uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
struct RNG { uint s; };
static inline float rnd(thread RNG& r) { r.s = pcg(r.s); return float(r.s >> 8) * (1.0 / 16777216.0); }
static inline float hash11(uint x) { return float(pcg(x) >> 8) * (1.0 / 16777216.0); }
static inline float3 hash33(int3 p) {
    uint h = pcg(uint(p.x) * 73856093u ^ uint(p.y) * 19349663u ^ uint(p.z) * 83492791u);
    uint h2 = pcg(h), h3 = pcg(h2);
    return float3(float(h >> 8), float(h2 >> 8), float(h3 >> 8)) * (1.0 / 16777216.0);
}
static inline float hash31(int3 p) { return float(pcg(uint(p.x) * 73856093u ^ uint(p.y) * 19349663u ^ uint(p.z) * 83492791u) >> 8) * (1.0 / 16777216.0); }

// value noise with analytic gradient, returns (value in [-1,1], grad)
static float4 noised(float3 x) {
    float3 pf = floor(x); int3 p = int3(pf); float3 w = x - pf;
    float3 u = w * w * w * (w * (w * 6.0 - 15.0) + 10.0);
    float3 du = 30.0 * w * w * (w * (w - 2.0) + 1.0);
    float a = hash31(p), b = hash31(p + int3(1, 0, 0)), c = hash31(p + int3(0, 1, 0)), d = hash31(p + int3(1, 1, 0));
    float e = hash31(p + int3(0, 0, 1)), f = hash31(p + int3(1, 0, 1)), g = hash31(p + int3(0, 1, 1)), h = hash31(p + int3(1, 1, 1));
    float k0 = a, k1 = b - a, k2 = c - a, k3 = e - a, k4 = a - b - c + d, k5 = a - c - e + g, k6 = a - b - e + f, k7 = -a + b + c - d + e - f - g + h;
    float v = k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
    float3 gr = du * float3(k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z, k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x, k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);
    return float4(v * 2.0 - 1.0, gr * 2.0);
}
static float4 fbmd(float3 p, int oct) {
    float4 s = float4(0.0); float a = 0.5, f = 1.0;
    for (int i = 0; i < oct; i++) { float4 n = noised(p * f + float3(17.1, 9.2, 5.3) * float(i)); s.x += a * n.x; s.yzw += a * f * n.yzw; a *= 0.5; f *= 2.02; }
    return s;
}
static float fbm(float3 p, int oct) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < oct; i++) {
        s += a * noised(p).x;
        p = float3(0.00 * p.x + 0.80 * p.y + 0.60 * p.z, -0.80 * p.x + 0.36 * p.y - 0.48 * p.z, -0.60 * p.x - 0.48 * p.y + 0.64 * p.z) * 2.02 + float3(17.1, 9.2, 5.3);
        a *= 0.5;
    }
    return s;
}
// worley: returns (F1, F2, cell hash)
static float3 worley(float3 x) {
    float3 pf = floor(x); int3 p = int3(pf); float d1 = 1e9, d2 = 1e9; float id = 0.0;
    for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
        int3 q = p + int3(i, j, k); float3 h = hash33(q); float3 c = float3(q) + h;
        float d = length(c - x);
        if (d < d1) { d2 = d1; d1 = d; id = h.x; } else if (d < d2) d2 = d;
    }
    return float3(d1, d2, id);
}

// ---------------- geometry ----------------
struct Hit { float t; uint inst; uint prim; float2 bc; uint kind; };

static inline bool hitBox(float3 o, float3 inv, float3 mn, float3 mx, float tmax, thread float& tn) {
    float3 t0 = (mn - o) * inv, t1 = (mx - o) * inv;
    float3 a = min(t0, t1), b = max(t0, t1);
    tn = max(max(a.x, a.y), max(a.z, 0.0));
    float tf = min(min(b.x, b.y), min(b.z, tmax));
    return tn <= tf;
}
static inline bool hitTri(float3 o, float3 d, float3 p0, float3 e1, float3 e2, float tmax, thread float& t, thread float2& bc) {
    float3 pv = cross(d, e2); float det = dot(e1, pv);
    if (fabs(det) < 1e-30) return false;
    float id = 1.0 / det; float3 tv = o - p0; float u = dot(tv, pv) * id;
    if (u < 0.0 || u > 1.0) return false;
    float3 qv = cross(tv, e1); float v = dot(d, qv) * id;
    if (v < 0.0 || u + v > 1.0) return false;
    float tt = dot(e2, qv) * id;
    if (tt <= 0.0 || tt >= tmax) return false;
    t = tt; bc = float2(u, v); return true;
}
// ray vs tapered cylinder segment (no caps); bc.x = s along the axis
static inline bool hitSeg(float3 o, float3 d, float3 p0, float r0, float3 p1, float r1, float tmax, thread float& t, thread float2& bc) {
    float3 ax = p1 - p0; float al = dot(ax, ax);
    if (al < 1e-30) return false;
    float3 oc = o - p0;
    float dd = dot(d, d), da = dot(d, ax), oa = dot(oc, ax), od = dot(oc, d), oo = dot(oc, oc);
    float r = max(r0, r1);
    float A = dd - da * da / al, B = od - oa * da / al, C = oo - oa * oa / al - r * r;
    if (A < 1e-30) return false;
    float disc = B * B - A * C;
    if (disc < 0.0) return false;
    float sq = sqrt(disc);
    float tt = (-B - sq) / A;
    float s = (oa + tt * da) / al;
    // refine with the local radius
    float rl = mix(r0, r1, clamp(s, 0.0, 1.0));
    C = oo - oa * oa / al - rl * rl; disc = B * B - A * C;
    if (disc < 0.0) return false;
    tt = (-B - sqrt(disc)) / A; s = (oa + tt * da) / al;
    if (s < 0.0 || s > 1.0 || tt <= 0.0 || tt >= tmax) return false;
    t = tt; bc = float2(s, 0.0); return true;
}

struct Scn {
    const device Node* tlas; const device Inst* insts; const device Node* nodes; const device Tri* tris;
    const device TriA* triAs; const device Seg* segs; const device Mat* mats; const device Glint* glints;
    const device uint* tileIdx; uint nInst;
};

static inline void instRay(const device Inst& I, float tm, float3 o, float3 d, thread float3& oo, thread float3& od) {
    if (I.flags & 1u) {
        float4 r0 = mix(I.a0, I.b0, tm), r1 = mix(I.a1, I.b1, tm), r2 = mix(I.a2, I.b2, tm);
        float3 X = float3(r0.x, r1.x, r2.x), Y = float3(r0.y, r1.y, r2.y), Z = float3(r0.z, r1.z, r2.z), T = float3(r0.w, r1.w, r2.w);
        float3 c0 = cross(Y, Z), c1 = cross(Z, X), c2 = cross(X, Y);
        float id = 1.0 / dot(X, c0);
        float3 q = o - T;
        oo = float3(dot(c0, q), dot(c1, q), dot(c2, q)) * id;
        od = float3(dot(c0, d), dot(c1, d), dot(c2, d)) * id;
    } else {
        oo = float3(dot(I.i0.xyz, o) + I.i0.w, dot(I.i1.xyz, o) + I.i1.w, dot(I.i2.xyz, o) + I.i2.w);
        od = float3(dot(I.i0.xyz, d), dot(I.i1.xyz, d), dot(I.i2.xyz, d));
    }
}

static void traceClosest(thread Scn& S, float3 o, float3 d, float tm, float tmax, thread Hit& h) {
    h.t = tmax; h.inst = 0xffffffffu;
    if (S.nInst == 0u) return;
    if (!(all(isfinite(o)) && all(isfinite(d)))) return;
    float3 inv = 1.0 / d;
    uint tstack[24]; int tsp = 0; uint ni = 0u;
    uint bstack[40]; float btn[40];
    float tn;
    if (!hitBox(o, inv, S.tlas[0].mn, S.tlas[0].mx, h.t, tn)) return;
    while (true) {
        const device Node& n = S.tlas[ni];
        if (n.count > 0u) {
            for (uint k = 0u; k < n.count; k++) {
                uint ii = n.leftFirst + k;
                const device Inst& I = S.insts[ii];
                float3 oo, od; instRay(I, tm, o, d, oo, od);
                float3 oinv = 1.0 / od;
                uint root = I.root; bool isSeg = (I.flags & 2u) != 0u;
                int bsp = 0; uint bi = 0u; float btnn;
                if (!hitBox(oo, oinv, S.nodes[root].mn, S.nodes[root].mx, h.t, btnn)) continue;
                while (true) {
                    const device Node& b = S.nodes[root + bi];
                    if (b.count > 0u) {
                        for (uint j = 0u; j < b.count; j++) {
                            uint pi = I.primBase + b.leftFirst + j; float t; float2 bc;
                            if (isSeg) {
                                const device Seg& sg = S.segs[pi];
                                if (hitSeg(oo, od, sg.p0, sg.r0, sg.p1, sg.r1, h.t, t, bc)) { h.t = t; h.inst = ii; h.prim = pi; h.bc = bc; h.kind = 1u; }
                            } else {
                                const device Tri& tr = S.tris[pi];
                                if (hitTri(oo, od, tr.p0, tr.e1, tr.e2, h.t, t, bc)) { h.t = t; h.inst = ii; h.prim = pi; h.bc = bc; h.kind = 0u; }
                            }
                        }
                    } else {
                        uint l = b.leftFirst; float tl, tr2;
                        bool hl = hitBox(oo, oinv, S.nodes[root + l].mn, S.nodes[root + l].mx, h.t, tl);
                        bool hr = hitBox(oo, oinv, S.nodes[root + l + 1u].mn, S.nodes[root + l + 1u].mx, h.t, tr2);
                        if (hl && hr) {
                            if (tl < tr2) { bstack[bsp] = l + 1u; btn[bsp] = tr2; bsp++; bi = l; } else { bstack[bsp] = l; btn[bsp] = tl; bsp++; bi = l + 1u; }
                            if (bsp >= 39) bsp = 39;
                            continue;
                        } else if (hl) { bi = l; continue; } else if (hr) { bi = l + 1u; continue; }
                    }
                    bool got = false;
                    while (bsp > 0) { bsp--; if (btn[bsp] < h.t) { bi = bstack[bsp]; got = true; break; } }
                    if (!got) break;
                }
            }
        } else {
            uint l = n.leftFirst; float tl, tr2;
            bool hl = hitBox(o, inv, S.tlas[l].mn, S.tlas[l].mx, h.t, tl);
            bool hr = hitBox(o, inv, S.tlas[l + 1u].mn, S.tlas[l + 1u].mx, h.t, tr2);
            if (hl && hr) { if (tl < tr2) { tstack[tsp++] = l + 1u; ni = l; } else { tstack[tsp++] = l; ni = l + 1u; } if (tsp >= 23) tsp = 23; continue; }
            else if (hl) { ni = l; continue; } else if (hr) { ni = l + 1u; continue; }
        }
        if (tsp == 0) break;
        ni = tstack[--tsp];
    }
}

// transmittance along a shadow ray (opaque -> 0)
static float3 traceShadowM(thread Scn& S, float3 o, float3 d, float tm, float tmax, uint mask) {
    float3 T = float3(1.0);
    if (S.nInst == 0u) return T;
    if (!(all(isfinite(o)) && all(isfinite(d)))) return float3(0.0);
    float3 inv = 1.0 / d;
    uint tstack[24]; int tsp = 0; uint ni = 0u;
    uint bstack[40];
    float tn;
    if (!hitBox(o, inv, S.tlas[0].mn, S.tlas[0].mx, tmax, tn)) return T;
    while (true) {
        const device Node& n = S.tlas[ni];
        if (n.count > 0u) {
            for (uint k = 0u; k < n.count; k++) {
                uint ii = n.leftFirst + k;
                const device Inst& I = S.insts[ii];
                if (I.flags & mask) continue;
                float3 oo, od; instRay(I, tm, o, d, oo, od);
                float3 oinv = 1.0 / od;
                uint root = I.root; bool isSeg = (I.flags & 2u) != 0u;
                int bsp = 0; uint bi = 0u; float btnn;
                if (!hitBox(oo, oinv, S.nodes[root].mn, S.nodes[root].mx, tmax, btnn)) continue;
                while (true) {
                    const device Node& b = S.nodes[root + bi];
                    if (b.count > 0u) {
                        for (uint j = 0u; j < b.count; j++) {
                            uint pi = I.primBase + b.leftFirst + j; float t; float2 bc; uint mi;
                            bool hit;
                            if (isSeg) { const device Seg& sg = S.segs[pi]; hit = hitSeg(oo, od, sg.p0, sg.r0, sg.p1, sg.r1, tmax, t, bc); mi = sg.mat; }
                            else { const device Tri& tr = S.tris[pi]; hit = hitTri(oo, od, tr.p0, tr.e1, tr.e2, tmax, t, bc); mi = tr.mat; }
                            if (hit) {
                                float3 sh = S.mats[I.matBase + mi].sh.xyz;
                                T *= sh;
                                if (max(T.x, max(T.y, T.z)) < 0.003) return float3(0.0);
                            }
                        }
                    } else {
                        uint l = b.leftFirst; float tl, tr2;
                        bool hl = hitBox(oo, oinv, S.nodes[root + l].mn, S.nodes[root + l].mx, tmax, tl);
                        bool hr = hitBox(oo, oinv, S.nodes[root + l + 1u].mn, S.nodes[root + l + 1u].mx, tmax, tr2);
                        if (hl && hr) { bstack[bsp++] = l + 1u; bi = l; if (bsp >= 39) bsp = 39; continue; }
                        else if (hl) { bi = l; continue; } else if (hr) { bi = l + 1u; continue; }
                    }
                    if (bsp == 0) break;
                    bi = bstack[--bsp];
                }
            }
        } else {
            uint l = n.leftFirst; float tl, tr2;
            bool hl = hitBox(o, inv, S.tlas[l].mn, S.tlas[l].mx, tmax, tl);
            bool hr = hitBox(o, inv, S.tlas[l + 1u].mn, S.tlas[l + 1u].mx, tmax, tr2);
            if (hl && hr) { tstack[tsp++] = l + 1u; ni = l; if (tsp >= 23) tsp = 23; continue; }
            else if (hl) { ni = l; continue; } else if (hr) { ni = l + 1u; continue; }
        }
        if (tsp == 0) break;
        ni = tstack[--tsp];
    }
    return T;
}

static inline float3 traceShadow(thread Scn& S, float3 o, float3 d, float tm, float tmax) { return traceShadowM(S, o, d, tm, tmax, 4u); }

// ---------------- environment ----------------
static float cloudShadow(constant Uni& U, float3 P) {
    if (U.cloud.x <= 0.0) return 1.0;
    float2 q = (P.xz + P.y * U.sunDir.xz / max(U.sunDir.y, 0.05)) * U.cloud.y + U.cloud.zw;
    float n = fbm(float3(q.x, 0.37, q.y), 3) + 0.5;
    return mix(1.0, smoothstep(0.35, 0.7, n), U.cloud.x);
}
static float3 skyColor(constant Uni& U, float3 d, bool spec) {
    float y = d.y;
    float3 c;
    if (y >= 0.0) c = mix(U.skyHor.xyz, U.skyZen.xyz, pow(y, U.skyP.x));
    else c = mix(U.skyHor.xyz, U.skyGnd.xyz, smoothstep(0.0, 0.12, -y));
    float cs = max(dot(d, U.sunDir.xyz), 0.0);
    c += U.sunE.xyz * U.skyP.y * (pow(cs, U.skyP.z) + 0.15 * pow(cs, U.skyP.z * 0.125));
    int et = int(U.skyP.w);
    if (et >= 1) {
        // blotchy out-of-focus backdrop (distant foliage / snow / gaps), mixed in below an elevation
        float3 q = d * U.bd0.w;
        float n1 = fbm(q + float3(3.1, 0.0, 1.7), 3);
        float n2 = fbm(q * 2.3 + float3(11.0, 4.0, 2.0), 3);
        float m = smoothstep(U.bdP.x, U.bdP.y, y + 0.25 * n1);   // 0 = backdrop, 1 = sky
        float3 bd = mix(U.bd0.xyz, U.bd1.xyz, smoothstep(-0.25, 0.3, n2));
        bd *= 0.75 + 0.5 * smoothstep(-0.3, 0.3, n1);
        if (et == 2) { // woods: ground (bd colours) below the horizon line bdP.x, trunks (skyGnd colour) fading out towards bdP.y
            float az = atan2(d.z, d.x);
            float t1 = noised(float3(az * U.bdP.z, 0.5, 1.0)).x, t2 = noised(float3(az * U.bdP.z * 3.3 + 7.0, 1.5, 2.0)).x;
            float tr = clamp(smoothstep(0.18, 0.42, t1) + 0.7 * smoothstep(0.3, 0.5, t2), 0.0, 1.0);
            float band = smoothstep(U.bdP.y + 0.25 * n1, U.bdP.x, y);
            float gap = smoothstep(0.0, 0.35, n2 + 0.4 * n1 - U.misc.z);
            c = mix(c, U.skyGnd.xyz * (1.6 + 0.8 * n1), band * U.bdP.w * (1.0 - 0.9 * gap));   // distant forest wall with sky gaps
            c = mix(c, U.skyGnd.xyz * (0.7 + 0.6 * n2), tr * band * U.bdP.w);
            float gm = smoothstep(U.bdP.x + 0.02 + 0.04 * n1, U.bdP.x - 0.02 + 0.04 * n1, y);
            c = mix(c, bd, gm);
        } else c = mix(bd, c, mix(m, 1.0, 1.0 - U.bd1.w));
    }
    if (spec && U.misc.w > 0.0) c += U.sunE.xyz * U.misc.w * smoothstep(0.9975, 0.9996, cs); // soft sun disc, only for mirror/refraction chains
    return c * U.misc.y;
}

// ---------------- materials ----------------
struct Surf {
    float3 N; float3 Ng; float3 diff; float3 tcol; float trans; float rough; float F0; float coat;
    uint type; float ior; float3 sigma; float3 emit; float wrap; float sheen;
};

#include "patterns.metal"

static inline float3 toWorld(float3 n, float3 v) {
    float3 a = fabs(n.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 x = normalize(cross(a, n)), z = cross(n, x);
    return x * v.x + n * v.y + z * v.z;
}
static inline float ggxD(float nh, float a) { float a2 = a * a; float d = nh * nh * (a2 - 1.0) + 1.0; return a2 / (PI * d * d); }
static inline float smithG1(float nv, float a) { float a2 = a * a; return 2.0 * nv / (nv + sqrt(a2 + (1.0 - a2) * nv * nv)); }
static inline float schlick(float c, float F0) { float m = clamp(1.0 - c, 0.0, 1.0); float m2 = m * m; return F0 + (1.0 - F0) * m2 * m2 * m; }
static inline float fresnelD(float ci, float eta) { // eta = n_t / n_i, ci = cos incident
    float s2 = (1.0 - ci * ci) / (eta * eta);
    if (s2 >= 1.0) return 1.0;
    float ct = sqrt(1.0 - s2);
    float rs = (ci - eta * ct) / (ci + eta * ct), rp = (eta * ci - ct) / (eta * ci + ct);
    return 0.5 * (rs * rs + rp * rp);
}
static inline float lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }

// lens aperture shape test, l in units of the aperture radius; field = image position (normalised).
// soft > 0 gives an analytic soft edge (used by the deterministic bokeh lights).
static inline float apertureW(constant Uni& U, float2 l, float2 field, float soft) {
    float r = length(l);
    float e = 1.0;
    float nb = U.lens2.x;
    float lim = 1.0;
    if (nb >= 3.0) {
        float seg = 2.0 * PI / nb;
        float th = atan2(l.y, l.x) + U.lens2.y;
        th = th - seg * floor(th / seg) - seg * 0.5;
        float rp = cos(seg * 0.5) / cos(th);
        lim = mix(rp, 1.0, U.lens2.z);
    }
    float rn = r / lim;
    float2 c = l + field * U.lens2.w;
    float rc = length(c);
    if (soft > 0.0) e = smoothstep(1.0, 1.0 - soft, rn) * smoothstep(1.0, 1.0 - soft, rc);
    else e = (rn <= 1.0 && rc <= 1.0) ? 1.0 : 0.0;
    if (e <= 0.0) return 0.0;
    float r2 = min(rn, 1.0); r2 *= r2;
    return e * (1.0 + U.lens3.x * (r2 * r2 - 0.3333));
}

struct Path { float3 o; float3 d; float3 thr; float3 sigma; int bounce; int flags; }; // flags: 1 specular chain from camera, 2 had diffuse

kernel void render(uint2 gid [[thread_position_in_grid]],
                   constant Uni& U [[buffer(0)]],
                   const device Node* tlas [[buffer(1)]],
                   const device Inst* insts [[buffer(2)]],
                   const device Node* nodes [[buffer(3)]],
                   const device Tri* tris [[buffer(4)]],
                   const device TriA* triAs [[buffer(5)]],
                   const device Seg* segs [[buffer(6)]],
                   const device Mat* mats [[buffer(7)]],
                   const device Glint* glints [[buffer(8)]],
                   const device uint* tileIdx [[buffer(9)]],
                   device float4* accum [[buffer(10)]],
                   device float4* accumFog [[buffer(11)]]) {
    uint W = U.dims.x, H = U.dims.y;
    if (gid.x >= W || gid.y >= H) return;
    uint pix = gid.y * W + gid.x;
    Scn S; S.tlas = tlas; S.insts = insts; S.nodes = nodes; S.tris = tris; S.triAs = triAs; S.segs = segs; S.mats = mats; S.glints = glints; S.tileIdx = tileIdx; S.nInst = U.counts.x;
    float3 sum = float3(0.0); float3 sumFog = float3(0.0);
    uint spp = U.dims.w;
    float eps = U.lens3.w;
    float3 sunDir = U.sunDir.xyz;
    uint pseed = pcg(pix * 9781u + U.counts.z * 6271u + 17u);
    float3 cp = float3(hash11(pseed), hash11(pseed + 1u), hash11(pseed + 2u));
    float2 cp2 = float2(hash11(pseed + 3u), hash11(pseed + 4u));

    for (uint si = 0u; si < spp; si++) {
        uint sidx = U.dims.z + si;
        RNG rng; rng.s = pcg(pseed ^ (sidx * 0x9E3779B9u + 0x85ebca6bu));
        // low-discrepancy (R5) for pixel, lens, time
        float fs = float(sidx) + 1.0;
        float2 pj = fract(cp.xy + fs * float2(0.8566748838545029, 0.7338918566271260));
        float2 lj = fract(float2(cp.z, cp2.x) + fs * float2(0.6287067210378087, 0.5385972572236101));
        float tj = fract(cp2.y + fs * 0.4613960965440431);
        float tm = tj;
        // tent filter, radius 1.15 px
        float2 tf = float2(pj.x < 0.5 ? sqrt(2.0 * pj.x) - 1.0 : 1.0 - sqrt(2.0 - 2.0 * pj.x), pj.y < 0.5 ? sqrt(2.0 * pj.y) - 1.0 : 1.0 - sqrt(2.0 - 2.0 * pj.y));
        float2 sp = (float2(gid) + 0.5 + tf * 1.15);
        float2 st = float2((sp.x / float(W) * 2.0 - 1.0) * U.lens.x, (1.0 - sp.y / float(H) * 2.0) * U.lens.y);
        float2 field = st / length(U.lens.xy);
        // lens sample
        float2 l = float2(0.0); float lw = 1.0;
        if (U.lens.z > 0.0) {
            float2 q = lj;
            for (int tr = 0; tr < 6; tr++) {
                float2 a = q * 2.0 - 1.0; float2 dsk;
                if (a.x == 0.0 && a.y == 0.0) dsk = float2(0.0);
                else if (fabs(a.x) > fabs(a.y)) { float ph = (PI / 4.0) * (a.y / a.x); dsk = a.x * float2(cos(ph), sin(ph)); }
                else { float ph = (PI / 2.0) - (PI / 4.0) * (a.x / a.y); dsk = a.y * float2(cos(ph), sin(ph)); }
                lw = apertureW(U, dsk, field, 0.0);
                if (lw > 0.0) { l = dsk; break; }
                q = float2(rnd(rng), rnd(rng));
            }
            if (lw <= 0.0) { l = float2(0.0); lw = 1.0; }
        }
        float3 F = U.camPos.xyz + (U.camF.xyz + U.camR.xyz * st.x + U.camU.xyz * st.y) * U.lens.w;
        float3 ro = U.camPos.xyz + (U.camR.xyz * l.x + U.camU.xyz * l.y) * U.lens.z;
        float3 rd = normalize(F - ro);
        float3 L = float3(0.0); float3 Lg = float3(0.0);

        // ---- deterministic glints (bokeh lights) ----
        if (U.counts.y > 0u) {
            uint tx = min(uint(sp.x * float(U.tiles.x) / float(W)), U.tiles.x - 1u), ty = min(uint(clamp(sp.y, 0.0, float(H) - 1.0) * float(U.tiles.y) / float(H)), U.tiles.y - 1u);
            uint base = (ty * U.tiles.x + tx) * (U.tiles.z + 1u);
            uint cnt = tileIdx[base];
            float pt = 2.0 * U.lens.x / float(W);
            for (uint k = 0u; k < cnt; k++) {
                const device Glint& g = glints[tileIdx[base + 1u + k]];
                float3 gp = float3(g.pos);
                float3 rel = gp - U.camPos.xyz;
                float Z = dot(rel, U.camF.xyz);
                if (Z <= 1e-6) continue;
                float2 sL = float2(dot(rel, U.camR.xyz), dot(rel, U.camU.xyz)) / Z;
                float fd = U.lens.w;
                float c = U.lens.z * fabs(fd - Z) / (Z * fd);       // CoC radius, tangent units
                float cmin = 1.3 * pt;
                float ce = max(c, cmin);
                float sgn = (Z > fd) ? -1.0 : 1.0;
                float2 ln = sgn * (sL - st) / ce;                    // lens coordinate (unit radius)
                float w = apertureW(U, ln, field, clamp(g.pad + 2.5 * pt / ce, 0.03, 0.9));
                if (c < cmin) { float r2 = dot(ln, ln); w = r2 < 1.0 ? 2.0 * (1.0 - r2) : 0.0; ln *= c / cmin; }
                if (w <= 0.0) continue;
                float3 lo = U.camPos.xyz + (U.camR.xyz * ln.x + U.camU.xyz * ln.y) * U.lens.z;
                float3 dv = gp - lo; float dl = length(dv);
                float3 Tg = traceShadow(S, lo, dv / dl, tm, dl - eps * 4.0);
                Lg += float3(g.col) * Tg * (w / (Z * Z * PI * ce * ce));
            }
        }

        Path stk[4]; int nsp = 0;
        stk[0].o = ro; stk[0].d = rd; stk[0].thr = float3(lw); stk[0].sigma = float3(0.0); stk[0].bounce = 0; stk[0].flags = 1; nsp = 1;
        int splits = 2;
        while (nsp > 0) {
            Path P = stk[--nsp];
            float3 o = P.o, d = P.d, thr = P.thr, sigma = P.sigma; int flags = P.flags;
            for (int bounce = P.bounce; bounce < 12; bounce++) {
                Hit h; traceClosest(S, o, d, tm, 1e30, h);
                bool hit = h.inst != 0xffffffffu;
                // medium absorption
                if (hit && (sigma.x + sigma.y + sigma.z) > 0.0) thr *= exp(-sigma * h.t);
                // fog on the camera segment
                if (bounce == 0 && U.fog.x > 0.0) {
                    float tmaxF = min(hit ? h.t : U.fogCol.w, U.fogCol.w);
                    // optical depth along ray for exponential height fog
                    float ky = U.fog.y, y0 = o.y - U.fog.z, dy = d.y;
                    float den0 = U.fog.x * exp(-ky * y0);
                    float a = ky * dy;
                    float od = fabs(a) > 1e-5 ? den0 * (1.0 - exp(-a * tmaxF)) / a : den0 * tmaxF;
                    float Tf = exp(-od);
                    // in-scattering: a few stratified samples along the segment; near-field grass is ignored (flag 16)
                    float g = U.fog.w; float cs = dot(d, sunDir);
                    float ph = (1.0 - g * g) / (4.0 * PI * pow(max(1.0 + g * g - 2.0 * g * cs, 1e-4), 1.5));
                    int NF = int(U.misc2.x);
                    float3 ins = float3(0.0);
                    float j0 = rnd(rng);
                    for (int fi = 0; fi < NF; fi++) {
                        float ts = (float(fi) + fract(j0 + 0.618 * float(fi))) / float(NF) * tmaxF;
                        float3 ps = o + d * ts;
                        float dens = U.fog.x * exp(-ky * (ps.y - U.fog.z));
                        float ods = fabs(a) > 1e-5 ? den0 * (1.0 - exp(-a * ts)) / a : den0 * ts;
                        float3 Ts = float3(0.0);
                        if (U.misc2.y > 0.0) Ts = traceShadowM(S, ps, sunDir, tm, 1e30, 20u) * cloudShadow(U, ps);
                        ins += (U.sunE.xyz * Ts * ph * U.misc2.y + U.fogCol.xyz * (1.0 / (4.0 * PI))) * dens * exp(-ods);
                    }
                    ins *= tmaxF / float(NF);
                    float li = lum(ins); if (li > U.misc2.z) ins *= U.misc2.z / li;
                    if (all(isfinite(ins))) sumFog += thr * ins;      // kept in its own buffer: the host smooths it before adding
                    thr *= Tf;
                }
                if (!hit) {
                    L += thr * skyColor(U, d, (flags & 1) != 0);
                    break;
                }
                // ---- surface ----
                const device Inst& I = insts[h.inst];
                float3 oo, od2; instRay(I, tm, o, d, oo, od2);
                float3 Pw = o + d * h.t;
                float3 Po = oo + od2 * h.t;
                float3 No, Ngo; float2 uv; float attr; float prnd; uint mi; float3 dpdu = float3(0.0), dpdv = float3(0.0);
                if (h.kind == 0u) {
                    const device Tri& tr = tris[h.prim]; const device TriA& ta = triAs[h.prim];
                    float w0 = 1.0 - h.bc.x - h.bc.y;
                    No = normalize(float3(ta.n0) * w0 + float3(ta.n1) * h.bc.x + float3(ta.n2) * h.bc.y);
                    Ngo = normalize(cross(float3(tr.e1), float3(tr.e2)));
                    if (!all(isfinite(No))) No = Ngo;
                    uv = float2(ta.u0, ta.v0) * w0 + float2(ta.u1, ta.v1) * h.bc.x + float2(ta.u2, ta.v2) * h.bc.y;
                    attr = ta.a0 * w0 + ta.a1 * h.bc.x + ta.a2 * h.bc.y;
                    prnd = tr.rnd; mi = tr.mat;
                    float2 d1 = float2(ta.u1 - ta.u0, ta.v1 - ta.v0), d2 = float2(ta.u2 - ta.u0, ta.v2 - ta.v0);
                    float dt = d1.x * d2.y - d1.y * d2.x;
                    if (fabs(dt) > 1e-20) { dpdu = (float3(tr.e1) * d2.y - float3(tr.e2) * d1.y) / dt; dpdv = (float3(tr.e2) * d1.x - float3(tr.e1) * d2.x) / dt; }
                } else {
                    const device Seg& sg = segs[h.prim];
                    float3 axp = mix(float3(sg.p0), float3(sg.p1), h.bc.x);
                    float3 ax = normalize(float3(sg.p1) - float3(sg.p0));
                    No = Po - axp; No = normalize(No - ax * dot(No, ax)); Ngo = No;
                    if (!all(isfinite(No))) { No = -normalize(od2); Ngo = No; }
                    uv = float2(mix(sg.v0, sg.v1, h.bc.x), 0.0); attr = uv.x; prnd = sg.rnd; mi = sg.mat;
                    dpdu = ax; dpdv = cross(No, ax);
                }
                const device Mat& M = mats[I.matBase + mi];
                Surf sf; sf.type = M.type; sf.ior = M.ior; sf.sigma = float3(0.0); sf.emit = float3(0.0); sf.wrap = 0.0; sf.sheen = 0.0;
                sf.diff = M.base.xyz; sf.tcol = M.base2.xyz; sf.trans = M.trans; sf.rough = M.rough; sf.F0 = 0.04; sf.coat = M.coat;
                float3 g3 = float3(0.0); float2 guv = float2(0.0);
                bool frontFace = dot(Ngo, od2) < 0.0;
                evalPattern(M, Po, Pw, No, uv, attr, prnd, I.rnd, I.tint.xyz, frontFace, U.misc.x, sf, g3, guv);
                // bump
                float3 Nb = No;
                if (M.bump != 0.0) {
                    float3 tu = dpdu, tv = dpdv;
                    Nb = No - (g3 - No * dot(g3, No)) * M.bump;
                    float lu = length(tu), lv = length(tv);
                    if (lu > 1e-12 && lv > 1e-12) Nb -= (tu / (lu * lu) * guv.x + tv / (lv * lv) * guv.y) * M.bump;
                    Nb = normalize(Nb);
                    if (!all(isfinite(Nb))) Nb = No;
                }
                // to world (normal transform by inverse transpose)
                float3 N, Ng;
                {
                    float4 i0 = I.i0, i1 = I.i1, i2 = I.i2;
                    if (I.flags & 1u) {
                        float4 r0 = mix(I.a0, I.b0, tm), r1 = mix(I.a1, I.b1, tm), r2 = mix(I.a2, I.b2, tm);
                        float3 X = float3(r0.x, r1.x, r2.x), Y = float3(r0.y, r1.y, r2.y), Z = float3(r0.z, r1.z, r2.z);
                        float3 c0 = cross(Y, Z), c1 = cross(Z, X), c2 = cross(X, Y);
                        i0.xyz = c0; i1.xyz = c1; i2.xyz = c2;
                    }
                    N = normalize(float3(i0.x * Nb.x + i1.x * Nb.y + i2.x * Nb.z, i0.y * Nb.x + i1.y * Nb.y + i2.y * Nb.z, i0.z * Nb.x + i1.z * Nb.y + i2.z * Nb.z));
                    Ng = normalize(float3(i0.x * Ngo.x + i1.x * Ngo.y + i2.x * Ngo.z, i0.y * Ngo.x + i1.y * Ngo.y + i2.y * Ngo.z, i0.z * Ngo.x + i1.z * Ngo.y + i2.z * Ngo.z));
                }
                bool entering = dot(Ng, d) < 0.0;
                float3 Ngf = entering ? Ng : -Ng;      // geometric normal facing the viewer
                if (dot(N, Ngf) < 0.0) N = -N;
                if (dot(N, d) > -1e-3) N = normalize(N - d * (dot(N, d) + 1e-3)); // keep the shading normal visible
                float3 V = -d;
                L += thr * sf.emit;

                if (sf.type == 1u) {
                    // ---------- dielectric ----------
                    float eta = entering ? sf.ior : 1.0 / sf.ior;
                    float3 Nd = N;
                    if (sf.rough > 0.001) { // rough: perturb micro-normal
                        float a = sf.rough; float u1 = rnd(rng), u2 = rnd(rng);
                        float ct = sqrt((1.0 - u1) / (1.0 + (a * a - 1.0) * u1)); float sn = sqrt(max(0.0, 1.0 - ct * ct));
                        Nd = toWorld(N, float3(sn * cos(2.0 * PI * u2), ct, sn * sin(2.0 * PI * u2)));
                        if (dot(Nd, V) < 0.0) Nd = N;
                    }
                    float ci = clamp(dot(Nd, V), 0.0, 1.0);
                    float Fr = fresnelD(ci, eta);
                    if (entering && sf.coat > 0.0) { // sun highlight on the outer surface (roughness floor keeps it stable through DOF)
                        float a = max(sf.rough, 0.05); float nl = dot(N, sunDir);
                        if (nl > 0.0) {
                            float3 hv = normalize(V + sunDir); float nh = max(dot(N, hv), 0.0); float nv = max(dot(N, V), 0.03);
                            float Fh = fresnelD(max(dot(V, hv), 0.0), eta);
                            float3 Ts = traceShadow(S, Pw + Ngf * eps, sunDir, tm, 1e30);
                            L += thr * U.sunE.xyz * Ts * (ggxD(nh, a) * smithG1(nv, a) * smithG1(nl, a) * Fh / (4.0 * nv)) * sf.coat;
                        }
                    }
                    float3 rdir = reflect(d, Nd);
                    if (dot(rdir, Ngf) < 0.0) rdir = reflect(d, Ngf);
                    float3 tdir = refract(d, Nd, 1.0 / eta);
                    bool tir = (Fr >= 0.9999) || (dot(tdir, tdir) < 0.5);
                    float3 sigIn = entering ? sf.sigma : float3(0.0);
                    if (tir) { o = Pw + Ngf * eps; d = rdir; }
                    else if (splits > 0 && (flags & 1) != 0 && nsp < 3 && Fr > 0.005) {
                        splits--;
                        // push reflection, continue with refraction
                        stk[nsp].o = Pw + Ngf * eps; stk[nsp].d = rdir; stk[nsp].thr = thr * Fr; stk[nsp].sigma = sigma; stk[nsp].bounce = bounce + 1; stk[nsp].flags = flags; nsp++;
                        thr *= (1.0 - Fr); o = Pw - Ngf * eps; d = normalize(tdir); sigma = sigIn;
                    } else {
                        if (rnd(rng) < Fr) { o = Pw + Ngf * eps; d = rdir; }
                        else { o = Pw - Ngf * eps; d = normalize(tdir); sigma = sigIn; }
                    }
                    continue;
                }
                if (sf.type == 2u) {
                    // ---------- thin dielectric film (wings, water film): straight-through or mirror ----------
                    float ci = clamp(dot(N, V), 0.0, 1.0);
                    float Fr = fresnelD(ci, sf.ior); Fr = 2.0 * Fr / (1.0 + Fr);
                    Fr = clamp(Fr * sf.coat, 0.0, 1.0);
                    // sun highlight through NEE on a slightly rough film
                    {
                        float a = max(sf.rough, 0.06);
                        float3 hv = normalize(V + sunDir); float nh = max(dot(N, hv), 0.0), nl = dot(N, sunDir);
                        if (nl > 0.0) {
                            float3 Ts = traceShadow(S, Pw + Ngf * eps, sunDir, tm, 1e30);
                            float sp = ggxD(nh, a) * 0.25 / max(ci, 0.05) * schlick(max(dot(V, hv), 0.0), 0.05) * sf.coat;
                            L += thr * U.sunE.xyz * Ts * sp;
                        }
                    }
                    float pd = sf.trans; // diffuse (opaque-ish pigment) share, tcol = film tint
                    float u = rnd(rng);
                    if (u < Fr) { o = Pw + Ngf * eps; d = reflect(d, N); if (dot(d, Ngf) < 0.0) d = reflect(-V, Ngf); continue; }
                    if (pd <= 0.0 || rnd(rng) > pd) { thr *= sf.tcol; o = Pw - Ngf * eps; continue; }
                    // otherwise fall through to diffuse shading of the pigment
                    sf.trans = 0.5; sf.coat = 0.0;
                }
                // ---------- diffuse / coated / translucent ----------
                flags &= ~1;
                float a = max(sf.rough, 0.035);
                float nv = max(dot(N, V), 1e-3);
                float Fv = schlick(nv, sf.F0) * sf.coat;
                // direct sun
                {
                    float2 u2 = float2(rnd(rng), rnd(rng));
                    float ang = U.sunDir.w * sqrt(u2.x), phi = 2.0 * PI * u2.y;
                    float3 ld = normalize(toWorld(sunDir, float3(sin(ang) * cos(phi), cos(ang), sin(ang) * sin(phi))));
                    float nl = dot(N, ld); float ngl = dot(Ngf, ld);
                    float3 f = float3(0.0);
                    if (ngl > 0.0) {
                        float nlw = max((nl + sf.wrap) / (1.0 + sf.wrap), 0.0);
                        f = sf.diff * (1.0 - sf.trans) * (1.0 - Fv) * nlw / PI;
                        if (nl > 0.0 && sf.coat > 0.0) {
                            float3 hv = normalize(V + ld); float nh = max(dot(N, hv), 0.0);
                            float Fh = schlick(max(dot(V, hv), 0.0), sf.F0) * sf.coat;
                            f += float3(ggxD(nh, a) * smithG1(nv, a) * smithG1(nl, a) * Fh / (4.0 * nv));
                        }
                        if (sf.sheen > 0.0) f += sf.diff * sf.sheen * pow(1.0 - nv, 4.0) * max(nl, 0.0) / PI;
                    } else if (sf.trans > 0.0) {
                        float nlw = max((-nl + sf.wrap) / (1.0 + sf.wrap), 0.0);
                        f = sf.tcol * sf.trans * (1.0 - Fv) * nlw / PI;
                    }
                    if (f.x + f.y + f.z > 0.0) {
                        float3 so = Pw + (ngl > 0.0 ? Ngf : -Ngf) * eps;
                        float3 Ts = traceShadow(S, so, ld, tm, 1e30) * cloudShadow(U, Pw);
                        L += thr * f * U.sunE.xyz * Ts;
                    }
                }
                // sample next direction
                float ps = sf.coat > 0.0 ? clamp(Fv / (Fv + lum(sf.diff) * (1.0 - Fv) + 1e-4), 0.05, 0.9) : 0.0;
                if (flags & 2) ps = min(ps, 0.5);
                float u = rnd(rng);
                if (u < ps) {
                    float u1 = rnd(rng), u2 = rnd(rng);
                    float ct = sqrt((1.0 - u1) / (1.0 + (a * a - 1.0) * u1)); float sn = sqrt(max(0.0, 1.0 - ct * ct));
                    float3 hv = toWorld(N, float3(sn * cos(2.0 * PI * u2), ct, sn * sin(2.0 * PI * u2)));
                    float3 nd = reflect(d, hv);
                    if (dot(nd, Ngf) <= 0.0) break;
                    float nl = max(dot(N, nd), 1e-4), vh = max(dot(V, hv), 1e-4), nh = max(dot(N, hv), 1e-4);
                    float Fh = schlick(vh, sf.F0) * sf.coat;
                    float wgt = Fh * smithG1(nv, a) * smithG1(nl, a) * vh / (nv * nh);
                    thr *= wgt / ps;
                    o = Pw + Ngf * eps; d = nd;
                    if (sf.rough < 0.08 && bounce == 0) flags |= 1; // sharp mirror-ish bounce keeps the camera chain "specular"
                } else {
                    float u1 = rnd(rng), u2 = rnd(rng);
                    float r = sqrt(u1), ph = 2.0 * PI * u2;
                    float3 lv = float3(r * cos(ph), sqrt(max(0.0, 1.0 - u1)), r * sin(ph));
                    float pt = sf.trans;
                    float lr = lum(sf.diff) * (1.0 - pt), lt = lum(sf.tcol) * pt;
                    float ptr = lt / max(lr + lt, 1e-5);
                    if (rnd(rng) < ptr) {
                        float3 nd = toWorld(-N, lv);
                        if (dot(nd, Ngf) > 0.0) nd = toWorld(-Ngf, lv);
                        thr *= sf.tcol * pt * (1.0 - Fv) / ((1.0 - ps) * ptr);
                        o = Pw - Ngf * eps; d = nd;
                    } else {
                        float3 nd = toWorld(N, lv);
                        if (dot(nd, Ngf) <= 0.0) break;
                        thr *= sf.diff * (1.0 - pt) * (1.0 - Fv) / ((1.0 - ps) * max(1.0 - ptr, 1e-5));
                        o = Pw + Ngf * eps; d = nd;
                    }
                    flags |= 2;
                }
                float mx = max(thr.x, max(thr.y, thr.z));
                if (mx < 1e-4) break;
                if (bounce >= 3) { float q = min(mx, 0.95); if (rnd(rng) > q) break; thr /= q; }
                if ((flags & 2) && bounce >= int(U.misc2.w)) break;
            }
        }
        // clamp fireflies (indirect + everything): per-sample luminance clamp
        float ll = lum(L); float cl = U.sunE.w;
        if (ll > cl) L *= cl / ll;
        L += min(Lg * lw, float3(200.0));
        if (all(isfinite(L))) sum += L;
    }
    float4 prev = (U.dims.z == 0u) ? float4(0.0) : accum[pix];
    accum[pix] = prev + float4(sum, float(spp));
    float4 prevF = (U.dims.z == 0u) ? float4(0.0) : accumFog[pix];
    accumFog[pix] = prevF + float4(sumFog, float(spp));
}
