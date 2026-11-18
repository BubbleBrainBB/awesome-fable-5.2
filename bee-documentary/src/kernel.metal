// kernel.metal — 自写的 Metal compute 光线追踪内核（宿主在前面拼接 shared.h）。
// 两级 BVH（部件 BLAS + 每帧 TLAS，实例矩阵按快门时间插值）、三种图元（三角形 / 毛发带 / 球）、
// 薄透镜景深、太阳直接光（带半透明阴影）、一次间接反弹、毛发/花瓣/翅膜/花粉的着色。
#include <metal_stdlib>
using namespace metal;

struct Node { packed_float3 lo; uint a; packed_float3 hi; uint n; };
struct Tri { packed_float3 p0; packed_float3 e1; packed_float3 e2; };
struct TriIdx { uint i0, i1, i2, mat; };
struct Vert { packed_float3 n; float u; packed_float3 col; float v; };
struct Cyl { packed_float3 p0; float r0; packed_float3 p1; float r1; };
struct CylAttr { packed_float3 col; uint mat; float rnd, v0, v1, pad; };
struct Sph { packed_float3 c; float r; };
struct SphAttr { packed_float3 col; uint mat; };
struct Inst { uint root, type, keyStart, numKeys, flags, part, pad0, pad1; };
struct Key { float4 r0, r1, r2; };
struct MatG { packed_float3 kd; uint type; packed_float3 kt; uint tex; float rough, f0, specW, sheen; packed_float3 hairC; float p0; };

struct Scn {
    const device Node* tlas; const device Inst* inst; const device Key* keys;
    const device Node* nodes; const device Tri* tris; const device TriIdx* tidx; const device Vert* verts;
    const device Cyl* cyls; const device CylAttr* cattr; const device Sph* sphs; const device SphAttr* sattr;
    const device MatG* mats; const device float4* veins; const device float4* U;
};

constant float PI = 3.14159265358979f;
constant float TAU = 6.28318530717959f;

// ---------------- 随机数与噪声 ----------------
static inline uint hashU(uint x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline float hashF(uint x) { return float(hashU(x) >> 8) * (1.0f / 16777216.0f); }
static inline uint hash3i(int x, int y, int z) { return hashU(uint(x) * 73856093u ^ hashU(uint(y) * 19349663u ^ hashU(uint(z) * 83492791u + 0x9e3779b9u))); }
struct RNG { uint s; };
static inline float rnd(thread RNG& r) { r.s = r.s * 747796405u + 2891336453u; uint w = ((r.s >> ((r.s >> 28u) + 4u)) ^ r.s) * 277803737u; w = (w >> 22u) ^ w; return float(w >> 8) * (1.0f / 16777216.0f); }

static inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float gradn(uint h, float x, float y, float z) {
    uint k = h & 15u;
    float u = k < 8u ? x : y;
    float v = k < 4u ? y : ((k == 12u || k == 14u) ? x : z);
    return ((k & 1u) != 0u ? -u : u) + ((k & 2u) != 0u ? -v : v);
}
static float noise3(float3 p) {
    float3 f = floor(p); int X = int(f.x), Y = int(f.y), Z = int(f.z);
    float x = p.x - f.x, y = p.y - f.y, z = p.z - f.z;
    float u = fade(x), v = fade(y), w = fade(z);
    float n000 = gradn(hash3i(X, Y, Z), x, y, z), n100 = gradn(hash3i(X + 1, Y, Z), x - 1, y, z);
    float n010 = gradn(hash3i(X, Y + 1, Z), x, y - 1, z), n110 = gradn(hash3i(X + 1, Y + 1, Z), x - 1, y - 1, z);
    float n001 = gradn(hash3i(X, Y, Z + 1), x, y, z - 1), n101 = gradn(hash3i(X + 1, Y, Z + 1), x - 1, y, z - 1);
    float n011 = gradn(hash3i(X, Y + 1, Z + 1), x, y - 1, z - 1), n111 = gradn(hash3i(X + 1, Y + 1, Z + 1), x - 1, y - 1, z - 1);
    return mix(mix(mix(n000, n100, u), mix(n010, n110, u), v), mix(mix(n001, n101, u), mix(n011, n111, u), v), w);
}
static float fbm3(float3 p, int oct) {
    float a = 0.5f, s = 0;
    for (int i = 0; i < oct; i++) { s += a * noise3(p); p = p * 2.03f + float3(17.1f, 3.7f, 9.2f); a *= 0.5f; }
    return s;
}
static float cell3(float3 p) {
    int X = int(floor(p.x)), Y = int(floor(p.y)), Z = int(floor(p.z));
    float best = 9;
    for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
        uint h = hash3i(X + i, Y + j, Z + k);
        float3 q = float3(float(X + i) + hashF(h), float(Y + j) + hashF(h + 1u), float(Z + k) + hashF(h + 2u));
        float3 dd = q - p; float d = dot(dd, dd);
        best = min(best, d);
    }
    return sqrt(best);
}
static inline float sqr(float x) { return x * x; }
static inline float lum(float3 c) { return dot(c, float3(0.2126f, 0.7152f, 0.0722f)); }
static inline float3 bumpNoise(float3 n, float3 p, float freq, float amp, int oct) {
    float e = 0.35f / freq;
    float h0 = fbm3(p * freq, oct);
    float3 g = float3(fbm3((p + float3(e, 0, 0)) * freq, oct) - h0, fbm3((p + float3(0, e, 0)) * freq, oct) - h0, fbm3((p + float3(0, 0, e)) * freq, oct) - h0) / e;
    g = g - n * dot(g, n);
    return normalize(n - g * (amp / freq));
}
static inline float3 bumpCell(float3 n, float3 p, float freq, float amp) {
    float e = 0.08f / freq;
    float h0 = cell3(p * freq);
    float3 g = float3(cell3((p + float3(e, 0, 0)) * freq) - h0, cell3((p + float3(0, e, 0)) * freq) - h0, cell3((p + float3(0, 0, e)) * freq) - h0) / e;
    g = g - n * dot(g, n);
    return normalize(n - g * (amp / freq));
}
static inline void basis(float3 n, thread float3& t, thread float3& b) {
    float s = n.z >= 0 ? 1.0f : -1.0f;
    float a = -1.0f / (s + n.z), c = n.x * n.y * a;
    t = float3(1 + s * n.x * n.x * a, s * c, -s * n.x);
    b = float3(c, s + n.y * n.y * a, -n.y);
}

// ---------------- 实例变换 ----------------
struct Xf { float3 c0, c1, c2, o; };
static inline Xf instXf(const Scn S, Inst in, float time) {
    Key k = S.keys[in.keyStart];
    if (in.numKeys > 1u) {
        float f = clamp(time, 0.0f, 0.9999f) * float(in.numKeys - 1u); uint i = min(uint(f), in.numKeys - 2u); float w = f - float(i);
        Key a = S.keys[in.keyStart + i], b = S.keys[in.keyStart + i + 1u];
        k.r0 = mix(a.r0, b.r0, w); k.r1 = mix(a.r1, b.r1, w); k.r2 = mix(a.r2, b.r2, w);
    }
    Xf x; x.c0 = float3(k.r0.x, k.r1.x, k.r2.x); x.c1 = float3(k.r0.y, k.r1.y, k.r2.y); x.c2 = float3(k.r0.z, k.r1.z, k.r2.z); x.o = float3(k.r0.w, k.r1.w, k.r2.w);
    return x;
}
static inline float3 xfDir(Xf x, float3 d) { return x.c0 * d.x + x.c1 * d.y + x.c2 * d.z; }
static inline float3 xfPt(Xf x, float3 p) { return x.o + xfDir(x, p); }
static inline float3 xfInvDir(Xf x, float3 v) {
    float3 r0 = cross(x.c1, x.c2), r1 = cross(x.c2, x.c0), r2 = cross(x.c0, x.c1);
    float id = 1.0f / dot(x.c0, r0);
    return float3(dot(r0, v), dot(r1, v), dot(r2, v)) * id;
}

// ---------------- 求交 ----------------
struct Hit { float t; uint inst; uint prim; float u; float v; };

static inline bool hitBox(float3 lo, float3 hi, float3 o, float3 inv, float tmin, float tmax, thread float& tn) {
    float3 t0 = (lo - o) * inv, t1 = (hi - o) * inv;
    float3 a = min(t0, t1), b = max(t0, t1);
    float ta = max(max(a.x, a.y), max(a.z, tmin)), tb = min(min(b.x, b.y), min(b.z, tmax));
    tn = ta;
    return tb >= ta;
}
static inline bool hitTri(Tri g, float3 o, float3 d, float tmin, float tmax, thread float& t, thread float& u, thread float& v) {
    float3 e1 = g.e1, e2 = g.e2;
    float3 pv = cross(d, e2); float det = dot(e1, pv);
    if (fabs(det) < 1e-24f) return false;
    float inv = 1.0f / det; float3 tv = o - float3(g.p0);
    u = dot(tv, pv) * inv; if (u < 0 || u > 1) return false;
    float3 qv = cross(tv, e1); v = dot(d, qv) * inv; if (v < 0 || u + v > 1) return false;
    t = dot(e2, qv) * inv;
    return t > tmin && t < tmax;
}
// 毛发段：朝向光线的带状体。u = 沿段参数，v = 横向偏移（-1..1）
static inline bool hitCyl(Cyl g, float3 o, float3 d, float tmin, float tmax, thread float& t, thread float& u, thread float& v) {
    float3 p0 = g.p0, ax = float3(g.p1) - p0;
    float a = dot(d, d);
    float t0 = dot(p0 - o, d) / a;
    float3 w0 = o + d * t0 - p0;                 // 先把原点挪到毛发附近，避免大数相减
    float b = dot(d, ax), c = dot(ax, ax), dd = dot(d, w0), e = dot(ax, w0);
    float den = a * c - b * b;
    float tc = den > 1e-12f * a * c ? clamp((a * e - b * dd) / den, 0.0f, 1.0f) : 0.5f;
    float sc = (tc * b - dd) / a;
    float3 w = w0 + d * sc - ax * tc;
    float r = mix(g.r0, g.r1, tc);
    float d2 = dot(w, w);
    if (d2 > r * r) return false;
    t = t0 + sc - sqrt(max(r * r - d2, 0.0f) / a) * 0.7f;
    if (!(t > tmin && t < tmax)) return false;
    u = tc;
    float3 side = cross(d, ax);
    v = clamp(sqrt(d2) / r, 0.0f, 1.0f) * (dot(side, w) >= 0 ? 1.0f : -1.0f);
    return true;
}
static inline bool hitSph(Sph g, float3 o, float3 d, float tmin, float tmax, thread float& t) {
    float a = dot(d, d);
    float tc = dot(float3(g.c) - o, d) / a;
    float3 oc = o + d * tc - float3(g.c);
    float c2 = g.r * g.r - dot(oc, oc); if (c2 < 0) return false;
    float sq = sqrt(c2 / a);
    t = tc - sq;
    if (t <= tmin) t = tc + sq;
    return t > tmin && t < tmax;
}

static bool blasClosest(const Scn S, uint root, uint type, float3 o, float3 d, float tmin, thread float& tmax, thread Hit& h, uint skipPrim) {
    float3 inv = 1.0f / d;
    uint stack[40]; int sp = 0; float tn;
    Node rn = S.nodes[root];
    if (!hitBox(rn.lo, rn.hi, o, inv, tmin, tmax, tn)) return false;
    stack[sp++] = root; bool found = false;
    while (sp > 0) {
        Node n = S.nodes[stack[--sp]];
        if (n.n > 0u) {
            for (uint i = n.a; i < n.a + n.n; i++) {
                if (i == skipPrim) continue;
                float t, u = 0, v = 0; bool hit;
                if (type == PT_TRI) hit = hitTri(S.tris[i], o, d, tmin, tmax, t, u, v);
                else if (type == PT_CYL) hit = hitCyl(S.cyls[i], o, d, tmin, tmax, t, u, v);
                else hit = hitSph(S.sphs[i], o, d, tmin, tmax, t);
                if (hit) { tmax = t; h.t = t; h.prim = i; h.u = u; h.v = v; found = true; }
            }
        } else {
            Node ca = S.nodes[n.a], cb = S.nodes[n.a + 1u];
            float ta, tb;
            bool ha = hitBox(ca.lo, ca.hi, o, inv, tmin, tmax, ta);
            bool hb = hitBox(cb.lo, cb.hi, o, inv, tmin, tmax, tb);
            if (ha && hb) {
                if (sp < 38) { if (ta < tb) { stack[sp++] = n.a + 1u; stack[sp++] = n.a; } else { stack[sp++] = n.a; stack[sp++] = n.a + 1u; } }
            } else if (ha) stack[sp++] = n.a;
            else if (hb) stack[sp++] = n.a + 1u;
        }
    }
    return found;
}

// 最近交点。mask：只与 flags&mask 非零的实例求交；noHair：跳过毛发实例
static bool traceClosest(const Scn S, float3 o, float3 d, float tmin, float tmax, float time, uint mask, bool noHair, uint skipInst, uint skipPrim, thread Hit& h) {
    float dl = dot(d, d); if (!(dl > 0.25f && dl < 4.0f) || !(dot(o, o) < 1e12f)) return false;
    float3 inv = 1.0f / d;
    uint stack[24]; int sp = 0; float tn;
    Node rn = S.tlas[0];
    if (!hitBox(rn.lo, rn.hi, o, inv, tmin, tmax, tn)) return false;
    stack[sp++] = 0u; bool found = false;
    while (sp > 0) {
        Node n = S.tlas[stack[--sp]];
        if (n.n > 0u) {
            for (uint i = n.a; i < n.a + n.n; i++) {
                Inst in = S.inst[i];
                if ((in.flags & mask) == 0u) continue;
                if (noHair && (in.flags & 256u) != 0u) continue;
                Xf x = instXf(S, in, time);
                float3 lo = xfInvDir(x, o - x.o), ld = xfInvDir(x, d);
                if (blasClosest(S, in.root, in.type, lo, ld, tmin, tmax, h, i == skipInst ? skipPrim : 0xffffffffu)) { h.inst = i; found = true; }
            }
        } else {
            Node ca = S.tlas[n.a], cb = S.tlas[n.a + 1u];
            float ta, tb;
            bool ha = hitBox(ca.lo, ca.hi, o, inv, tmin, tmax, ta);
            bool hb = hitBox(cb.lo, cb.hi, o, inv, tmin, tmax, tb);
            if (ha && hb) { if (sp < 22) { if (ta < tb) { stack[sp++] = n.a + 1u; stack[sp++] = n.a; } else { stack[sp++] = n.a; stack[sp++] = n.a + 1u; } } }
            else if (ha) stack[sp++] = n.a;
            else if (hb) stack[sp++] = n.a + 1u;
        }
    }
    return found;
}

// 透过率（阴影）遍历：rgb = 透过率，w = 穿过的毛发根数
static bool blasTrans(const Scn S, uint root, uint type, float3 o, float3 d, float tmin, float tmax, thread float4& T, uint skipPrim) {
    float3 inv = 1.0f / d;
    uint stack[40]; int sp = 0; float tn;
    Node rn = S.nodes[root];
    if (!hitBox(rn.lo, rn.hi, o, inv, tmin, tmax, tn)) return false;
    stack[sp++] = root;
    while (sp > 0) {
        Node n = S.nodes[stack[--sp]];
        if (n.n > 0u) {
            for (uint i = n.a; i < n.a + n.n; i++) {
                if (i == skipPrim) continue;
                float t, u = 0, v = 0;
                if (type == PT_TRI) {
                    if (!hitTri(S.tris[i], o, d, tmin, tmax, t, u, v)) continue;
                    TriIdx ti = S.tidx[i]; MatG m = S.mats[ti.mat];
                    if (m.type == M_TRANS) { float3 c = float3(S.verts[ti.i0].col); T.xyz *= float3(m.kt) * pow(c, float3(m.p0)); }
                    else if (m.type == M_FILM) T.xyz *= 0.86f;
                    else return true;
                } else if (type == PT_CYL) {
                    if (!hitCyl(S.cyls[i], o, d, tmin, tmax, t, u, v)) continue;
                    MatG m = S.mats[S.cattr[i].mat];
                    if (m.type == M_HAIR) { T.xyz *= float3(m.kt); T.w += 1.0f; }
                    else return true;
                } else {
                    if (hitSph(S.sphs[i], o, d, tmin, tmax, t)) return true;
                }
                if (max(T.x, max(T.y, T.z)) < 0.02f) return true;
            }
        } else {
            Node ca = S.nodes[n.a], cb = S.nodes[n.a + 1u];
            float ta, tb;
            bool ha = hitBox(ca.lo, ca.hi, o, inv, tmin, tmax, ta);
            bool hb = hitBox(cb.lo, cb.hi, o, inv, tmin, tmax, tb);
            if (ha && sp < 39) stack[sp++] = n.a;
            if (hb && sp < 39) stack[sp++] = n.a + 1u;
        }
    }
    return false;
}
static float4 traceTrans(const Scn S, float3 o, float3 d, float tmin, float tmax, float time, uint skipInst, uint skipPrim) {
    float dl = dot(d, d); if (!(dl > 0.25f && dl < 4.0f) || !(dot(o, o) < 1e12f)) return float4(0);
    float3 inv = 1.0f / d;
    uint stack[24]; int sp = 0; float tn;
    float4 T = float4(1, 1, 1, 0);
    Node rn = S.tlas[0];
    if (!hitBox(rn.lo, rn.hi, o, inv, tmin, tmax, tn)) return T;
    stack[sp++] = 0u;
    while (sp > 0) {
        Node n = S.tlas[stack[--sp]];
        if (n.n > 0u) {
            for (uint i = n.a; i < n.a + n.n; i++) {
                Inst in = S.inst[i];
                if ((in.flags & 512u) != 0u) continue;      // 不投影的实例（靠近镜头的前景虚影）
                Xf x = instXf(S, in, time);
                float3 lo = xfInvDir(x, o - x.o), ld = xfInvDir(x, d);
                if (blasTrans(S, in.root, in.type, lo, ld, tmin, tmax, T, i == skipInst ? skipPrim : 0xffffffffu)) return float4(0, 0, 0, T.w);
            }
        } else {
            Node ca = S.tlas[n.a], cb = S.tlas[n.a + 1u];
            float ta, tb;
            if (hitBox(ca.lo, ca.hi, o, inv, tmin, tmax, ta) && sp < 23) stack[sp++] = n.a;
            if (hitBox(cb.lo, cb.hi, o, inv, tmin, tmax, tb) && sp < 23) stack[sp++] = n.a + 1u;
        }
    }
    return T;
}

// ---------------- 环境光 ----------------
static float3 envRadiance(const Scn S, float3 d) {
    float3 zen = S.U[U_SKY_ZEN].xyz, hor = S.U[U_SKY_HOR].xyz, gnd = S.U[U_GROUND].xyz;
    float3 sd = S.U[U_SUN_DIR].xyz;
    float y = d.y;
    float3 sky = mix(hor, zen, sqrt(saturate(y)));
    float cs = saturate(dot(d, sd));
    sky += S.U[U_SUN_E].xyz * (0.035f * pow(cs, 6.0f) + 0.25f * pow(cs, 90.0f));
    // 四周的植被：地平线附近参差的暗绿色
    float nz = noise3(d * 2.6f + float3(3.1f, 0, 7.7f)) + 0.5f * noise3(d * 6.3f);
    float hgt = 0.10f + 0.22f * nz;
    float m = smoothstep(hgt - 0.06f, hgt + 0.06f, y);
    float3 veg = gnd * (0.75f + 0.5f * noise3(d * 4.0f + float3(11, 5, 2))) * (1.0f + 1.4f * pow(cs, 3.0f));
    float3 c = mix(veg, sky, m);
    float4 fl = S.U[U_FILL];
    float cf = saturate(dot(d, fl.xyz));
    c += fl.w * cf * cf * float3(1.0f, 0.97f, 0.92f);
    return c;
}
static inline float3 ambientGuess(const Scn S, float3 n) {
    float3 sky = 0.5f * (S.U[U_SKY_ZEN].xyz + S.U[U_SKY_HOR].xyz);
    return mix(S.U[U_GROUND].xyz * 0.9f, sky, 0.5f + 0.5f * n.y) * 0.55f;
}

// ---------------- 表面与程序纹理 ----------------
struct Surf {
    float3 P, Pl, Ng, N, dpdu, dpdv, kd, kt, col, hairC, filmTint, T;
    float alpha, f0, specW, sheen, u, v, rndv, filmD, hoff, sss;
    uint type, tex;
};

static float veinDist(const Scn S, int wing, float x, float y, thread float& wOut) {
    uint n0 = wing == 0 ? 0u : uint(S.U[U_COUNTS].z), n1 = wing == 0 ? uint(S.U[U_COUNTS].z) : uint(S.U[U_COUNTS].z + S.U[U_COUNTS].w);
    float best = 1e9f; wOut = 0.03f;
    for (uint i = n0; i < n1; i++) {
        float4 s = S.veins[2u * i]; float hw = S.veins[2u * i + 1u].x;
        float dx = s.z - s.x, dy = s.w - s.y, px = x - s.x, py = y - s.y;
        float t = saturate((px * dx + py * dy) / (dx * dx + dy * dy + 1e-12f));
        float ex = px - dx * t, ey = py - dy * t;
        float d = sqrt(ex * ex + ey * ey) - hw;
        if (d < best) { best = d; wOut = hw; }
    }
    return best;
}
constant float ABD_UB[7] = {0.0f, 0.135f, 0.33f, 0.53f, 0.71f, 0.86f, 1.0f};

// fp：命中处一个像素对应的物方尺寸（mm），用来压掉小于像素的凹凸（否则闪烁）。detail=false 时只算颜色。
static void applyTex(const Scn S, MatG m, thread Surf& s, float fp, bool detail) {
    float u = s.u, v = s.v; float3 col = s.col; float3 p = s.Pl;
    switch (m.tex) {
    case T_PETAL: {
        float kf = floor(v / 4 + 0.5f); float q = v - 4 * kf;
        float3 c = col;
        float ring = smoothstep(0.26f, 0.07f, u), claw = smoothstep(0.055f, 0.0f, u);
        c = mix(c, c * float3(0.74f, 0.30f, 0.52f), ring * 0.85f);
        c = mix(c, float3(0.92f, 0.86f, 0.45f), claw);
        float vv = cos(PI * 5.5f * q);
        float vein = smoothstep(-0.955f, -1.0f, vv);
        float vein2 = smoothstep(0.985f, 1.0f, fabs(cos(PI * 11 * q + 0.5f)));
        float streak = noise3(float3(q * 55, u * 2.5f, kf * 7.3f)) + 0.8f * noise3(float3(q * 150, u * 4, kf * 3.1f)) + (detail ? 0.5f * noise3(float3(q * 420, u * 9, kf * 1.7f)) : 0.0f);
        float sh = 1 - 0.16f * vein * (0.4f + 0.6f * (1 - u)) - 0.05f * vein2 + 0.075f * streak + 0.06f * fbm3(p * 0.9f, 3);
        // 边缘略褪色、偶有褐色小斑
        float edge = smoothstep(0.78f, 1.0f, fabs(q)) + smoothstep(0.9f, 1.0f, u);
        c = mix(c, c * float3(1.06f, 1.02f, 1.04f) + 0.03f, 0.5f * saturate(edge));
        float spot = smoothstep(0.62f, 0.75f, noise3(p * 0.9f + float3(40, 9, kf * 13)));
        c = mix(c, c * float3(0.72f, 0.55f, 0.50f), 0.5f * spot);
        c = c * sh;
        s.kd = c * float3(m.kd);
        s.kt = pow(c, float3(m.p0)) * float3(m.kt);
        if (detail) {
            float k1 = saturate(0.030f / fp - 0.3f), k2 = saturate(0.018f / fp - 0.3f);
            if (k1 > 0) s.N = bumpNoise(s.N, p, 34, 0.30f * k1, 2);
            if (k2 > 0) s.N = bumpCell(s.N, p, 55, 0.20f * k2);
        }
        break;
    }
    case T_FLORET: {
        float n = fbm3(p * 9, 2);
        float3 c = col * (1 + 0.12f * n);
        s.kd = c * float3(m.kd); s.kt = c * c * float3(m.kt);
        if (detail && fp < 0.03f) s.N = bumpNoise(s.N, p, 22, 0.22f, 2);
        break;
    }
    case T_STEM: {
        float n = fbm3(p * float3(6, 0.6f, 6), 3);
        float3 c = col * (1 + 0.15f * n);
        s.kd = c * float3(m.kd);
        if (detail && fp < 0.05f) s.N = bumpNoise(s.N, p, 12, 0.15f, 2);
        break;
    }
    case T_HEAD:
    case T_CUTICLE: {
        s.kd = float3(m.kd) * col;
        if (detail) {
            float k1 = saturate(0.04f / fp - 0.3f);
            if (k1 > 0) { s.N = bumpCell(s.N, p, 26, 0.22f * k1); s.N = bumpNoise(s.N, p, 60, 0.12f * k1, 1); }
        }
        break;
    }
    case T_EYE: {
        const float sp = 0.0225f, sy = sp * 1.7320508f;     // 小眼面间距 22.5 µm 的六角晶格
        float ax = round(u / sp) * sp, ay = round(v / sy) * sy;
        float bx = (floor(u / sp) + 0.5f) * sp, by = (floor(v / sy) + 0.5f) * sy;
        float dax = u - ax, day = v - ay, dbx = u - bx, dby = v - by;
        float dx = dax, dy = day;
        if (dbx * dbx + dby * dby < dax * dax + day * day) { dx = dbx; dy = dby; }
        float r = sqrt(dx * dx + dy * dy) / (sp * 0.5f);
        float k = saturate(sp / fp * 0.45f - 0.35f);          // 小眼面小于约 2.5 像素时淡出
        float3 tu = normalize(s.dpdu), tv = normalize(s.dpdv);
        tu = tu - s.N * dot(tu, s.N); tv = tv - s.N * dot(tv, s.N);
        float border = smoothstep(0.78f, 1.05f, r);
        if (detail) s.N = normalize(s.N + (tu * dx + tv * dy) * (0.62f * k / (sp * 0.5f)));
        float sh = mix(1.0f, (1 - 0.85f * border) * (1.25f - 0.6f * r * r), k);
        // 伪瞳孔：正对视线的小眼面更暗（看进了感杆束），由宿主放在 col.yz 里的眼面局部坐标不好得，这里用大尺度噪声代替明暗不匀
        s.kd = float3(m.kd) * sh * (1 + 0.25f * fbm3(p * 3, 2));
        s.specW = m.specW * (1 - 0.55f * border * k);
        break;
    }
    case T_ABDOMEN: {
        int i = int(col.x + 0.5f);
        float w = saturate((u - ABD_UB[i]) / (ABD_UB[i + 1] - ABD_UB[i]));
        float vent = 1 - smoothstep(0.12f, 0.20f, fabs(v - 0.5f));
        float3 amber = float3(0.46f, 0.185f, 0.035f), dark = float3(0.022f, 0.016f, 0.012f), brown = float3(0.12f, 0.055f, 0.022f);
        float edge = 0.60f + 0.05f * noise3(float3(v * 38, float(i) * 3.7f, 1.5f)) + 0.03f * noise3(float3(v * 120, float(i) * 1.7f, 4.5f));
        float band = smoothstep(edge - 0.07f, edge + 0.05f, w);
        float3 base;
        if (i == 0) base = mix(dark, amber, smoothstep(0.35f, 0.75f, w) * 0.85f);
        else if (i <= 2) base = amber * (i == 1 ? 1.0f : 0.9f);
        else if (i == 3) base = mix(amber * 0.55f, dark, smoothstep(0.10f, 0.42f, w));
        else base = dark;
        float3 c = mix(base, dark, i == 0 ? band * 0.6f : band);
        c = mix(c, brown, vent * 0.75f);
        c = c * (1 + 0.18f * fbm3(p * 4, 3));
        s.kd = c;
        if (detail) {
            float k1 = saturate(0.03f / fp - 0.3f);
            if (k1 > 0) { s.N = bumpNoise(s.N, p, 55, 0.16f * k1, 2); s.N = bumpCell(s.N, p, 20, 0.10f * k1); }
        }
        s.alpha = sqr(m.rough + 0.08f * band);
        // 半透明的琥珀色体壁：逆光时略透
        s.sss = (1 - band) * (i <= 3 ? 0.35f : 0.0f);
        break;
    }
    case T_WING: {
        int wing = col.x > 0.5f ? 1 : 0; float wv;
        float d = veinDist(S, wing, u, v, wv);
        if (d < 0) {
            s.type = M_DIFF; s.kd = float3(0.030f, 0.020f, 0.014f) * (1 + 0.3f * noise3(float3(u * 9, v * 9, 2))); s.alpha = sqr(0.50f); s.f0 = 0.04f; s.specW = 0.5f;
            if (detail) {
                const float e = 0.004f; float w2;
                float gx = (veinDist(S, wing, u + e, v, w2) - d) / e, gy = (veinDist(S, wing, u, v + e, w2) - d) / e;
                float k = saturate(1 + d / max(wv, 1e-4f)) * 0.5f;
                s.N = normalize(s.N + (normalize(s.dpdu) * gx + normalize(s.dpdv) * gy) * k);
            }
            break;
        }
        s.kd = float3(0.02f); s.kt = float3(0);
        float n1 = fbm3(float3(u * 0.55f, v * 0.8f, float(wing) * 7.0f + col.y * 3.0f), 3);
        s.filmD = 470 - 20 * u + 230 * n1 + 25 * noise3(float3(u * 2.5f, v * 2.5f, 3.3f));
        float nearV = exp(-d / 0.05f);
        // 翅面微毛造成的细密暗点 + 靠近翅脉处略深 + 外缘略有烟色
        float hairs = smoothstep(0.55f, 0.9f, 1 - cell3(float3(u * 28, v * 28, float(wing) * 3.0f)) * 1.6f) * saturate(0.012f / fp);
        s.filmTint = float3(0.90f, 0.875f, 0.82f) * (1 - 0.30f * nearV) * (1 - 0.06f * (0.5f + 0.5f * noise3(float3(u * 14, v * 14, 7)))) * (1 - 0.35f * hairs);
        s.alpha = sqr(m.rough);
        if (detail) s.N = bumpNoise(s.N, float3(u, v, float(wing) * 4.0f), 2.2f, 0.035f, 2);
        break;
    }
    case T_LEG: {
        s.kd = col * (1 + 0.25f * fbm3(p * 6, 2));
        if (detail && fp < 0.03f) s.N = bumpNoise(s.N, p, 40, 0.14f, 2);
        break;
    }
    case T_ANTENNA: {
        s.kd = float3(m.kd) * col * (1 + 0.2f * fbm3(p * 20, 2));
        if (detail && fp < 0.012f) s.N = bumpNoise(s.N, p, 90, 0.30f, 1);
        break;
    }
    case T_TONGUE: {
        s.kd = float3(m.kd) * col * (1 + 0.2f * fbm3(p * 14, 2));
        s.sss = 0.10f;
        break;
    }
    case T_POLLENBALL: {
        float n = fbm3(p * 5, 3);
        float cl = cell3(p * 30);
        float blot = fbm3(p * 1.6f + float3(5, 1, 9), 3);
        s.kd = float3(m.kd) * float3(1, 1 + 0.10f * n + 0.25f * blot, 1 + 0.25f * n) * (0.55f + 0.60f * smoothstep(0.08f, 0.55f, cl)) * (1 + 0.5f * blot);
        if (detail && fp < 0.05f) s.N = bumpCell(s.N, p, 30, 0.8f);
        s.sss = 0.35f;
        break;
    }
    case T_LEAF: {
        float n = noise3(p * 0.05f) * 0.5f + noise3(p * 0.4f) * 0.25f;
        float3 c = col * (1 + 0.35f * n);
        s.kd = c * float3(m.kd); s.kt = float3(c.x * 1.1f, c.y, c.z * 0.6f) * float3(m.kt);
        break;
    }
    default:
        s.kd = col * float3(m.kd); s.kt = col * float3(m.kt);
        break;
    }
}

// 命中点的着色信息。V = 指向观察者（光线来向）
static void setupSurf(const Scn S, Hit h, float3 o, float3 d, float time, float fp, bool detail, thread Surf& s) {
    Inst in = S.inst[h.inst];
    Xf x = instXf(S, in, time);
    s.P = o + d * h.t; s.Pl = xfInvDir(x, s.P - x.o);
    s.sss = 0; s.filmD = 0; s.filmTint = float3(1); s.hoff = 0; s.T = float3(0, 1, 0); s.dpdu = float3(1, 0, 0); s.dpdv = float3(0, 0, 1);
    uint mi;
    if (in.type == PT_TRI) {
        Tri g = S.tris[h.prim]; TriIdx ti = S.tidx[h.prim];
        Vert a = S.verts[ti.i0], b = S.verts[ti.i1], c = S.verts[ti.i2];
        float w = 1 - h.u - h.v;
        float3 nl = float3(a.n) * w + float3(b.n) * h.u + float3(c.n) * h.v;
        s.u = a.u * w + b.u * h.u + c.u * h.v; s.v = a.v * w + b.v * h.u + c.v * h.v;
        s.col = float3(a.col) * w + float3(b.col) * h.u + float3(c.col) * h.v;
        float3 e1 = g.e1, e2 = g.e2;
        float du1 = b.u - a.u, du2 = c.u - a.u, dv1 = b.v - a.v, dv2 = c.v - a.v;
        float det = du1 * dv2 - du2 * dv1;
        if (fabs(det) > 1e-20f) { s.dpdu = xfDir(x, (e1 * dv2 - e2 * dv1) / det); s.dpdv = xfDir(x, (e2 * du1 - e1 * du2) / det); }
        s.Ng = normalize(xfDir(x, cross(e1, e2)));
        float3 nw = xfDir(x, nl); float nwl = dot(nw, nw);
        s.N = (nwl > 1e-12f && nwl < 1e12f) ? nw * rsqrt(nwl) : s.Ng;
        mi = ti.mat; s.rndv = hashF(h.prim);
    } else if (in.type == PT_CYL) {
        Cyl g = S.cyls[h.prim]; CylAttr ca = S.cattr[h.prim];
        float3 ax = normalize(xfDir(x, float3(g.p1) - float3(g.p0)));
        s.T = ax;
        float3 side = normalize(cross(d, ax));
        float3 nf = normalize(cross(ax, side));        // 朝向光线来向的法线
        if (dot(nf, d) > 0) nf = -nf;
        s.N = normalize(nf * sqrt(max(1 - h.v * h.v, 0.0f)) + side * h.v);
        s.Ng = s.N; s.u = mix(ca.v0, ca.v1, h.u); s.v = 0; s.col = ca.col; s.rndv = ca.rnd; s.hoff = h.v;
        mi = ca.mat;
    } else {
        Sph g = S.sphs[h.prim]; SphAttr sa = S.sattr[h.prim];
        s.N = normalize(xfDir(x, s.Pl - float3(g.c))); s.Ng = s.N; s.u = 0; s.v = 0; s.col = sa.col; s.rndv = hashF(h.prim * 7u + 3u);
        mi = sa.mat;
    }
    MatG m = S.mats[mi];
    s.type = m.type; s.tex = m.tex; s.alpha = sqr(m.rough); s.f0 = m.f0; s.specW = m.specW; s.sheen = m.sheen;
    s.kd = float3(m.kd) * s.col; s.kt = float3(m.kt) * s.col; s.hairC = float3(m.hairC);
    if (m.type == M_SSS) s.sss = 0.8f;
    if (m.type == M_HAIR) {
        // 每根毛的色素浓度略有差别，根深梢浅
        float k = (0.55f + 0.9f * s.rndv) * (1.15f - 0.3f * s.u);
        s.hairC = pow(float3(m.hairC), float3(k)) * s.col;
        return;
    }
    applyTex(S, m, s, fp, detail);
}

// ---------------- BSDF 小工具 ----------------
static inline float ggxD(float nh, float a) { float a2 = a * a, d = nh * nh * (a2 - 1) + 1; return a2 / (PI * d * d); }
static inline float ggxG1(float nx, float a) { float a2 = a * a; return 2 * nx / (nx + sqrt(a2 + (1 - a2) * nx * nx)); }
static inline float schlick(float f0, float c) { float m = saturate(1 - c); float m2 = m * m; return f0 + (1 - f0) * m2 * m2 * m; }

static float3 filmR(float c, float d) {
    const float n = 1.56f;
    float sinT = sqrt(max(0.0f, 1 - c * c)) / n, cosT = sqrt(1 - sinT * sinT);
    float rs = (c - n * cosT) / (c + n * cosT), rp = (n * c - cosT) / (n * c + cosT);
    float3 R = float3(0);
    for (int k = -1; k <= 1; k++) {
        float3 lam = float3(610, 545, 462) + float(k) * 28.0f;
        float3 cd = cos(4 * PI * n * d * cosT / lam);
        float r2 = rs * rs; R += 0.5f * (2 * r2 * (1 - cd)) / (1 + r2 * r2 - 2 * r2 * cd);
        r2 = rp * rp; R += 0.5f * (2 * r2 * (1 - cd)) / (1 + r2 * r2 - 2 * r2 * cd);
    }
    return R / 3.0f;
}

// 毛发单次散射（R / TT / TRT 三瓣，Karis 2016 的解析近似）
static inline float hairG(float B, float th) { return exp(-0.5f * th * th / (B * B)) / (sqrt(TAU) * B); }
static inline float hairF(float c) { const float n = 1.55f; float f0 = sqr((1 - n) / (1 + n)); float m = saturate(1 - c); float m2 = m * m; return f0 + (1 - f0) * m2 * m2 * m; }
static float3 hairSingle(float3 T, float3 V, float3 L, float3 C, float rough, thread float3& ttOut) {
    float sL = clamp(dot(T, L), -1.0f, 1.0f), sV = clamp(dot(T, V), -1.0f, 1.0f);
    float VoL = dot(V, L);
    float cosThetaD = cos(0.5f * fabs(asin(sV) - asin(sL)));
    float3 Lp = L - sL * T, Vp = V - sV * T;
    float cosPhi = dot(Lp, Vp) * rsqrt(dot(Lp, Lp) * dot(Vp, Vp) + 1e-4f);
    float cosHalfPhi = sqrt(saturate(0.5f + 0.5f * cosPhi));
    float nPrime = 1.19f / cosThetaD + 0.36f * cosThetaD;
    float r2 = rough * rough;
    float B0 = r2, B1 = r2 * 0.5f, B2 = r2 * 2.0f;
    const float shift = 0.035f;
    float3 S = float3(0);
    {   // R
        float sa = sin(-shift * 2), ca = cos(-shift * 2);
        float sh = 2 * sa * (ca * cosHalfPhi * sqrt(max(1 - sV * sV, 0.0f)) + sa * sV);
        float Mp = hairG(B0 * 1.41421356f * max(cosHalfPhi, 0.05f), sL + sV - sh);
        float Np = 0.25f * cosHalfPhi;
        float Fp = hairF(sqrt(saturate(0.5f + 0.5f * VoL)));
        S += float3(Mp * Np * Fp);
    }
    {   // TT
        float Mp = hairG(B1, sL + sV - shift);
        float a = 1.0f / nPrime;
        float h = cosHalfPhi * (1 + a * (0.6f - 0.8f * cosPhi));
        float f = hairF(cosThetaD * sqrt(saturate(1 - h * h)));
        float Fp = sqr(1 - f);
        float3 Tp = pow(C, float3(0.5f * sqrt(max(1 - sqr(h * a), 0.0f)) / cosThetaD));
        float Np = exp(-3.65f * cosPhi - 3.98f);
        ttOut = Mp * Np * Fp * Tp;
        S += ttOut;
    }
    {   // TRT
        float Mp = hairG(B2, sL + sV - shift * 4);
        float f = hairF(cosThetaD * 0.5f);
        float Fp = sqr(1 - f) * f;
        float3 Tp = pow(C, float3(0.8f / cosThetaD));
        float Np = exp(17.0f * cosPhi - 16.78f);
        S += Mp * Np * Fp * Tp;
    }
    return S;
}

static inline float3 cosHemi(float3 n, float u1, float u2) {
    float3 t, b; basis(n, t, b);
    float r = sqrt(u1), ph = TAU * u2;
    return normalize(t * (r * cos(ph)) + b * (r * sin(ph)) + n * sqrt(max(1 - u1, 0.0f)));
}
static inline float3 sunSample(const Scn S, thread RNG& rng) {
    float4 sd = S.U[U_SUN_DIR];
    float3 t, b; basis(sd.xyz, t, b);
    float cz = mix(sd.w, 1.0f, rnd(rng)), sz = sqrt(max(1 - cz * cz, 0.0f)), ph = TAU * rnd(rng);
    return normalize(sd.xyz * cz + t * (sz * cos(ph)) + b * (sz * sin(ph)));
}

// 命中点受太阳直接照射的出射辐亮度（毛发的全局散射用穿过的毛发根数近似，见 README）
static float3 sunDirect(const Scn S, Surf s, float3 V, float time, uint hInst, uint hPrim, thread RNG& rng, bool full) {
    float3 L = sunSample(S, rng);
    float3 E = S.U[U_SUN_E].xyz;
    if (s.type == M_HAIR) {
        float4 Tr = traceTrans(S, s.P, L, 2e-3f, 1e9f, time, hInst, hPrim);
        if (max(Tr.x, max(Tr.y, Tr.z)) <= 0) return float3(0);
        float sL = dot(s.T, L);
        float kaj = sqrt(max(1 - sL * sL, 0.0f));
        float boost = S.U[U_MISC].w;
        float3 C = s.hairC;
        float3 tt = float3(0);
        float3 single = full ? hairSingle(s.T, V, L, C, 0.36f, tt) : float3(0);
        float direct = Tr.w < 0.5f ? 1.0f : 0.0f;
        // 多次散射的漫射近似：毛色越浅，毛丛内部越亮
        float3 diff = C * kaj * (boost / (PI * PI));
        // 被前面的毛挡住直射时：前向散射过来的光 ≈ 透过率 × (变宽了的 TT 瓣 + 漫射)
        float VoL = dot(V, L);
        float3 fwd = sqrt(C) * (0.20f * boost) * sqr(saturate(-VoL)) * kaj;
        return E * Tr.xyz * (direct * single + diff + (1 - direct) * (fwd + tt * 0.6f));
    }
    float3 N = s.N;
    float nv = dot(N, V);
    if (s.type == M_TRANS || s.type == M_FILM) { if (nv < 0) { N = -N; nv = -nv; } }
    else if (nv < 0) { N = normalize(N - V * (nv - 0.02f)); nv = 0.02f; }
    float nl = dot(N, L);
    float3 Ng = dot(s.Ng, N) < 0 ? -s.Ng : s.Ng;
    bool back = nl < 0;
    if (back && s.type != M_TRANS && s.sss <= 0) return float3(0);
    float3 off = Ng * (dot(Ng, L) > 0 ? 3e-3f : -3e-3f);
    float4 Tr = traceTrans(S, s.P + off, L, 1e-4f, 1e9f, time, 0xffffffffu, 0xffffffffu);
    if (max(Tr.x, max(Tr.y, Tr.z)) <= 0) return float3(0);
    float3 Lo = float3(0);
    if (!back) {
        float3 H = normalize(V + L);
        float nh = saturate(dot(N, H)), vh = saturate(dot(V, H));
        float a = max(s.alpha, 0.012f);
        float F = schlick(s.f0, vh) * s.specW;
        float spec = F * ggxD(nh, a) * ggxG1(max(nv, 1e-3f), a) * ggxG1(nl, a) / (4 * max(nv, 1e-3f));
        float3 dif = s.kd * ((1 - F) / PI) * nl;
        if (s.sheen > 0) { float mm = saturate(1 - vh); dif += float3(s.sheen * mm * mm * mm * mm * mm) * nl; }
        if (s.sss > 0) dif = s.kd * ((1 - F) / PI) * mix(nl, saturate((nl + 0.5f) / 1.5f), s.sss);
        Lo = dif + float3(spec);
    } else if (s.type == M_TRANS) {
        Lo = s.kt * (-nl / PI);
    } else {
        // 次表面的简化：背光面按“包裹”的余弦透一点光
        Lo = s.kd * s.kd * (s.sss * saturate((nl + 0.5f) / 1.5f) / PI);
    }
    return E * Tr.xyz * Lo;
}

// 间接光：从命中点再追一条光线；命中则在那里只算太阳直接光 + 估计的环境光
static float3 bounceLight(const Scn S, float3 o, float3 d, float time, thread RNG& rng, bool noHair, uint skipInst, uint skipPrim) {
    Hit h; h.t = 1e9f;
    if (!traceClosest(S, o, d, 2e-3f, 150.0f, time, 0xffu & ~uint(L_NEAR), noHair, skipInst, skipPrim, h)) return envRadiance(S, d);
    Surf s; setupSurf(S, h, o, d, time, 1.0f, false, s);
    float3 V = -d;
    float3 Lo = sunDirect(S, s, V, time, h.inst, h.prim, rng, false);
    float3 N = dot(s.N, V) < 0 ? -s.N : s.N;
    float3 alb = s.type == M_HAIR ? s.hairC * 0.6f : (s.type == M_FILM ? float3(0.1f) : s.kd);
    Lo += alb * ambientGuess(S, N);
    return Lo;
}

// 一个相机采样：返回 rgb 辐亮度（已乘覆盖），alpha = 覆盖率；coc = 首次命中处弥散圆直径（像素）
static float4 samplePath(const Scn S, float3 o, float3 d, float time, uint mask, thread RNG& rng, float fpk, thread float& coc, float3 camPos, float3 camF, float focus) {
    float3 thr = float3(1); float3 L = float3(0);
    uint skipI = 0xffffffffu, skipP = 0xffffffffu;
    coc = 1e9f;
    for (int depth = 0; depth < 6; depth++) {
        Hit h; h.t = 1e9f;
        if (!traceClosest(S, o, d, 1e-3f, 1e9f, time, mask, false, skipI, skipP, h)) break;
        float z = dot(o + d * h.t - camPos, camF);
        float fp = fpk * max(z, 1.0f);
        if (depth == 0) coc = fabs(1 - focus / max(z, 1.0f)) * S.U[U_LENS].w * 2.0f / (fpk * focus);
        int dbg = int(S.U[U_MISC].z);
        if (dbg == 1) { L = float3(fract(h.t * 0.1f), float(h.inst % 7u) / 7.0f, 0.5f); thr = float3(0); break; }
        Surf s; setupSurf(S, h, o, d, time, fp, dbg != 2, s);
        float3 V = -d;
        if (dbg == 2 || dbg == 3) { L = s.kd * (0.3f + 0.7f * fabs(dot(s.N, V))); thr = float3(0); break; }
        if (dbg == 4) { L = sunDirect(S, s, V, time, h.inst, h.prim, rng, true); thr = float3(0); break; }
        if (dbg == 5) { float3 wq = cosHemi(dot(s.N, V) < 0 ? -s.N : s.N, rnd(rng), rnd(rng)); Hit hq; hq.t = 1e9f; bool hh = traceClosest(S, s.P + wq * 0.01f, wq, 2e-3f, 1e9f, time, 0xffu, false, 0xffffffffu, 0xffffffffu, hq); L = hh ? float3(0.1f) : envRadiance(S, wq); thr = float3(0); break; }
        if (dbg == 6) { float3 wq = cosHemi(dot(s.N, V) < 0 ? -s.N : s.N, rnd(rng), rnd(rng)); L = bounceLight(S, s.P + wq * 0.01f, wq, time, rng, false, 0xffffffffu, 0xffffffffu); thr = float3(0); break; }
        if (s.type == M_FILM) {
            // 翅膜：薄膜干涉反射（太阳高光 + 环境反射）后，光线带着透射率继续直穿
            float3 N = dot(s.N, V) < 0 ? -s.N : s.N;
            float nv = saturate(dot(N, V));
            float3 F = filmR(nv, s.filmD); F = mix(float3(lum(F)), F, 0.45f) * 0.55f;
            float3 Ls = sunSample(S, rng);
            float nl = dot(N, Ls);
            if (nl > 0) {
                float3 H = normalize(V + Ls);
                float a = max(s.alpha, 0.16f);
                float sp = 0.35f * ggxD(saturate(dot(N, H)), a) * ggxG1(max(nv, 1e-3f), a) * ggxG1(nl, a) / (4 * max(nv, 1e-3f));
                float4 Tr = traceTrans(S, s.P + N * 3e-3f, Ls, 1e-4f, 1e9f, time, 0xffffffffu, 0xffffffffu);
                L += thr * F * sp * S.U[U_SUN_E].xyz * Tr.xyz;
            }
            float3 R = reflect(d, N);
            L += thr * F * envRadiance(S, R) * 0.6f;
            // 翅面微毛与细皱造成的一层很淡的漫射（否则翅在慢动作里几乎看不见）
            L += thr * float3(0.050f, 0.048f, 0.042f) * (S.U[U_SUN_E].xyz * (fabs(nl) / PI) + ambientGuess(S, N));
            thr *= (1 - F) * s.filmTint;
            o = s.P; skipI = h.inst; skipP = h.prim;
            continue;
        }
        // 太阳直接光
        L += thr * sunDirect(S, s, V, time, h.inst, h.prim, rng, true);
        // 一次间接：背景层（低分辨率、完全虚化）只用估计的环境光；前景每个采样以 1/2 的概率追一条（权重 2）
        if ((mask & L_FG) == 0u) {
            float3 Na = dot(s.N, V) < 0 ? -s.N : s.N;
            float3 alb = s.type == M_HAIR ? s.hairC * 0.6f : s.kd;
            L += thr * alb * ambientGuess(S, Na) * 1.3f;
            if (s.type == M_TRANS) L += thr * s.kt * ambientGuess(S, -Na) * 1.3f;
            thr = float3(0); break;
        }
        if (rnd(rng) < 0.5f) { thr = float3(0); break; }
        thr *= 2.0f;
        if (s.type == M_HAIR) {
            float3 w = float3(0); { float zz = 1 - 2 * rnd(rng), rr = sqrt(max(1 - zz * zz, 0.0f)), ph = TAU * rnd(rng); w = float3(rr * cos(ph), rr * sin(ph), zz); }
            float sL = dot(s.T, w); float kaj = sqrt(max(1 - sL * sL, 0.0f));
            float3 f = s.hairC * kaj * (S.U[U_MISC].w / (PI * PI));
            // 先找不含毛发的最近表面，再算到那里为止穿过了多少毛
            Hit hb; hb.t = 1e9f;
            bool hitS = traceClosest(S, s.P, w, 2e-3f, 150.0f, time, 0xffu & ~uint(L_NEAR), true, 0xffffffffu, 0xffffffffu, hb);
            float4 Tr = traceTrans(S, s.P, w, 2e-3f, hitS ? hb.t * 0.999f : 150.0f, time, h.inst, h.prim);
            float3 Li;
            if (!hitS) Li = envRadiance(S, w);
            else {
                Surf sb; setupSurf(S, hb, s.P, w, time, 1.0f, false, sb);
                float3 Vb = -w;
                // 毛的环境光打到身体/花上：不再追阴影光线，用“半遮挡”的太阳 + 估计的环境光
                float3 Nb = dot(sb.N, Vb) < 0 ? -sb.N : sb.N;
                float3 alb = sb.type == M_FILM ? float3(0.1f) : sb.kd;
                float nls = dot(Nb, S.U[U_SUN_DIR].xyz);
                Li = alb * (ambientGuess(S, Nb) + S.U[U_SUN_E].xyz * (0.45f / PI) * max(nls, 0.0f));
                if (sb.type == M_TRANS && nls < 0) Li += sb.kt * S.U[U_SUN_E].xyz * (0.45f / PI) * (-nls);
            }
            // 透过率里已穿过的毛发，同时也在向这根毛散射光：用较高的等效透过率
            L += thr * f * (4 * PI) * Li * pow(max(Tr.xyz, float3(0)), float3(0.7f));
        } else {
            float3 N = s.N; float nv = dot(N, V);
            if (s.type == M_TRANS) { if (nv < 0) { N = -N; nv = -nv; } }
            else if (nv < 0) { N = normalize(N - V * (nv - 0.02f)); nv = 0.02f; }
            float3 Ng = dot(s.Ng, N) < 0 ? -s.Ng : s.Ng;
            float F = schlick(s.f0, nv) * s.specW;
            float ld = lum(s.kd) + (s.type == M_TRANS ? lum(s.kt) : 0.0f);
            float ps = clamp(F / (F + ld * (1 - F) + 1e-4f), 0.10f, 0.85f);
            float3 w; float3 wt;
            if (rnd(rng) < ps) {
                // GGX 镜面反射方向
                float a = max(s.alpha, 0.01f);
                float u1 = rnd(rng), u2 = rnd(rng);
                float ct = sqrt((1 - u1) / (1 + (a * a - 1) * u1)), st = sqrt(max(1 - ct * ct, 0.0f)), ph = TAU * u2;
                float3 t, b; basis(N, t, b);
                float3 H = normalize(t * (st * cos(ph)) + b * (st * sin(ph)) + N * ct);
                w = reflect(d, H);
                float nl = dot(N, w);
                if (nl <= 0 || dot(Ng, w) <= 0) { w = reflect(d, N); nl = dot(N, w); }
                float vh = saturate(dot(V, H));
                wt = float3(schlick(s.f0, vh) * s.specW * ggxG1(max(nl, 1e-3f), a) * vh / (max(nv, 1e-3f) * max(ct, 1e-3f)) / ps);
                wt = min(wt, float3(4.0f));
                L += thr * wt * bounceLight(S, s.P + Ng * 3e-3f, w, time, rng, false, 0xffffffffu, 0xffffffffu);
            } else {
                float pt = s.type == M_TRANS ? lum(s.kt) / (ld + 1e-5f) : 0.0f;
                if (rnd(rng) < pt) {
                    w = cosHemi(-N, rnd(rng), rnd(rng));
                    wt = s.kt / (pt * (1 - ps));
                    L += thr * wt * bounceLight(S, s.P - Ng * 3e-3f, w, time, rng, false, 0xffffffffu, 0xffffffffu);
                } else {
                    w = cosHemi(N, rnd(rng), rnd(rng));
                    if (dot(Ng, w) <= 0) w = reflect(w, Ng);
                    wt = s.kd * (1 - F) / ((1 - pt) * (1 - ps));
                    L += thr * wt * bounceLight(S, s.P + Ng * 3e-3f, w, time, rng, false, 0xffffffffu, 0xffffffffu);
                }
            }
        }
        thr = float3(0);
        break;
    }
    float a = 1 - saturate((thr.x + thr.y + thr.z) / 3.0f);
    return float4(L, a);
}

// ---------------- 镜头 ----------------
static inline void lensSample(float u1, float u2, float bladeRot, float rimK, thread float& lx, thread float& ly, thread float& w) {
    float a = 2 * u1 - 1, b = 2 * u2 - 1, r, phi;
    if (a == 0 && b == 0) { r = 0; phi = 0; }
    else if (fabs(a) > fabs(b)) { r = a; phi = (PI / 4) * (b / a); }
    else { r = b; phi = PI / 2 - (PI / 4) * (a / b); }
    if (r < 0) { r = -r; phi += PI; }
    const float seg = TAU / 9.0f;
    float pa = fmod(phi + bladeRot + 10 * TAU, seg) - seg / 2;
    float poly = cos(seg / 2) / cos(pa);
    float rr = r * mix(1.0f, poly, 0.16f);
    lx = rr * cos(phi); ly = rr * sin(phi);
    float r2 = r * r; w = (1 + rimK * r2 * r2 * r2) / (1 + rimK / 4) * smoothstep(1.0f, 0.955f, r);
}

kernel void render(device float4* acc0 [[buffer(0)]], device float4* acc1 [[buffer(1)]],
                   const device float4* U [[buffer(2)]],
                   const device Node* tlas [[buffer(3)]], const device Inst* inst [[buffer(4)]], const device Key* keys [[buffer(5)]],
                   const device Node* nodes [[buffer(6)]], const device Tri* tris [[buffer(7)]], const device TriIdx* tidx [[buffer(8)]], const device Vert* verts [[buffer(9)]],
                   const device Cyl* cyls [[buffer(10)]], const device CylAttr* cattr [[buffer(11)]], const device Sph* sphs [[buffer(12)]], const device SphAttr* sattr [[buffer(13)]],
                   const device MatG* mats [[buffer(14)]], const device float4* veins [[buffer(15)]],
                   uint2 gid [[thread_position_in_grid]]) {
    uint W = uint(U[U_RES].x), H = uint(U[U_RES].y);
    if (gid.x >= W || gid.y >= H) return;
    Scn S; S.tlas = tlas; S.inst = inst; S.keys = keys; S.nodes = nodes; S.tris = tris; S.tidx = tidx; S.verts = verts;
    S.cyls = cyls; S.cattr = cattr; S.sphs = sphs; S.sattr = sattr; S.mats = mats; S.veins = veins; S.U = U;
    uint idx = gid.y * W + gid.x;
    uint frameSeed = uint(U[U_FRAME].x), sampleBase = uint(U[U_FRAME].y), spd = uint(U[U_FRAME].z), mask = uint(U[U_FRAME].w);
    uint ph = hashU(gid.x * 1973u + gid.y * 9277u + frameSeed * 26699u + 1u);
    float h1 = hashF(ph), h2 = hashF(ph + 1u), h3 = hashF(ph + 2u);
    float fieldW = U[U_LENS].z, apR = U[U_LENS].w, fieldH = fieldW * float(H) / float(W);
    float catD = U[U_LENS2].x, catM = U[U_LENS2].y, rimK = U[U_LENS2].z, bladeRot = U[U_LENS2].w;
    float4 a0 = acc0[idx], a1 = acc1[idx];
    if (sampleBase == 0u) { a0 = float4(0); a1 = float4(0, 1e9f, 0, 0); }
    for (uint si = 0; si < spd; si++) {
        uint sn = sampleBase + si;
        RNG rng; rng.s = hashU(ph ^ (sn * 0x9E3779B9u + 77u));
        float u1 = fract(h1 + float(sn) * 0.7548776662f), u2 = fract(h2 + float(sn) * 0.5698402910f);
        float time = fract(h3 + float(sn) * 0.6180339887f);
        float jx = rnd(rng) + rnd(rng) - 1.0f, jy = rnd(rng) + rnd(rng) - 1.0f;
        float sx = ((float(gid.x) + 0.5f + jx * 0.75f) / float(W)) * 2 - 1, sy = 1 - ((float(gid.y) + 0.5f + jy * 0.75f) / float(H)) * 2;
        float3 pos = mix(U[U_CAM_POS0].xyz, U[U_CAM_POS1].xyz, time);
        float3 cr = normalize(mix(U[U_CAM_R0].xyz, U[U_CAM_R1].xyz, time)), cu = normalize(mix(U[U_CAM_U0].xyz, U[U_CAM_U1].xyz, time)), cf = normalize(mix(U[U_CAM_F0].xyz, U[U_CAM_F1].xyz, time));
        float focus = mix(U[U_LENS].x, U[U_LENS].y, time);
        float lx, ly, w; lensSample(u1, u2, bladeRot, rimK, lx, ly, w);
        float fx = sx * fieldW * 0.5f, fy = sy * fieldH * 0.5f;
        float k = catD / focus;
        float qx = lx * apR * (1 - k) + fx * k, qy = ly * apR * (1 - k) + fy * k;
        float catR = apR * (1 - k) + catM;
        a1.w += 1.0f;
        if (qx * qx + qy * qy > catR * catR) continue;
        float3 Fp = pos + cf * focus + cr * fx + cu * fy;
        float3 o = pos + cr * (lx * apR) + cu * (ly * apR);
        float3 d = normalize(Fp - o);
        float coc;
        float fpk = fieldW / (float(W) * focus);
        float4 r = samplePath(S, o, d, time, mask, rng, fpk, coc, pos, cf, focus);
        // 有偏的亮度截断（防萤火虫），见 README
        float mx = max(r.x, max(r.y, r.z));
        float cl = (mask & L_FG) != 0u ? 40.0f : 10.0f;
        if (mx > cl) r.xyz *= cl / mx;
        if (!(mx >= 0.0f)) r = float4(0);
        a0 += float4(r.xyz, r.w) * w;
        a1.x += w;
        if (r.w > 0.5f) a1.y = min(a1.y, coc);
    }
    acc0[idx] = a0; acc1[idx] = a1;
}

// ---------------- 远景（无穷远的虚化色块与光斑）与合成 ----------------
// 远处的东西在画面上是“被光圈形状卷积过”的：色块用按模糊半径滤掉高频的噪声，高光用光圈形状的圆斑。
static float3 farField(const device float4* U, const device float4* hl, float3 dir, float2 scr, float time) {
    float blur = U[U_FAR0].x;                       // 光斑的角半径 ≈ 入瞳半径 / 对焦距离
    float3 cA = U[U_FAR1].xyz, cB = U[U_FAR2].xyz, cC = U[U_FAR3].xyz;
    float3 q = dir + U[U_WIND].xyz;
    // 被光圈卷积过的远景：只保留比光斑大的结构。频率按光斑半径定标，各倍频程按高斯衰减
    float n = 0, n2 = 0, n3 = 0, tot = 0;
    float fr[5] = {0.045f, 0.09f, 0.18f, 0.36f, 0.72f};
    for (int i = 0; i < 5; i++) {
        float f = fr[i] / blur;
        float att = exp(-sqr(fr[i] * 2.4f)) * (i == 0 ? 0.7f : 1.0f);
        n += att * noise3(q * f + float3(1.7f, 9.2f, 4.1f) + U[U_FAR0].z);
        n2 += att * noise3(q * f * 1.13f + float3(31.7f, 2.2f, 14.1f) + U[U_FAR0].z);
        n3 += att * noise3(q * f * 0.9f + float3(77.0f, 3.0f, 5.0f) + U[U_FAR0].z);
        tot += att;
    }
    n /= tot * 0.55f; n2 /= tot * 0.55f; n3 /= tot * 0.55f;
    float3 sd = U[U_SUN_DIR].xyz;
    float toSun = saturate(dot(normalize(float3(dir.x, 0, dir.z)), normalize(float3(sd.x, 0, sd.z))));
    // 暗处（叶丛深处）— 亮处（受光的叶）— 更亮的黄绿（透光的叶）
    float3 c = mix(cA, cB, smoothstep(-0.55f, 0.55f, n));
    c = mix(c, cB * float3(1.9f, 1.7f, 0.9f) + float3(0.04f, 0.05f, 0.0f), smoothstep(0.15f, 0.9f, n2) * (0.35f + 0.65f * toSun));
    // 上方更亮更淡（远处的天光透进来），下方更暗
    float up = smoothstep(-0.25f, 0.45f, dir.y + 0.12f * n2);
    c *= mix(0.55f, 1.25f, up);
    c = mix(c, float3(0.75f, 0.85f, 0.80f) * (0.6f + 0.6f * toSun), smoothstep(0.35f, 1.0f, n3 + 1.2f * (dir.y - 0.12f)) * 0.7f);
    // 远处的花：稀疏的粉色块
    float fl = smoothstep(0.42f, 0.75f, n3 * 0.6f - n * 0.5f + 0.2f * n2);
    c = mix(c, cC, fl * 0.75f);
    c *= U[U_FAR0].y * (0.8f + 0.5f * toSun * toSun);
    // 光斑：叶隙间的天光与叶面反光，光圈形状（九边形圆角 + 口径蚀裁切）
    int nh = int(U[U_FAR0].w);
    float catD = U[U_LENS2].x, catM = U[U_LENS2].y, apR = U[U_LENS].w, focus = U[U_LENS].x, fieldW = U[U_LENS].z;
    float3 cr = U[U_CAM_R0].xyz, cu = U[U_CAM_U0].xyz, cf = U[U_CAM_F0].xyz;
    for (int i = 0; i < nh; i++) {
        float4 h = hl[2 * i]; float4 hc = hl[2 * i + 1];
        float3 hd = normalize(h.xyz + U[U_WIND].xyz * h.w);
        float cz = dot(hd, cf); if (cz < 0.3f) continue;
        // 高光中心在焦平面上的位置（mm），像素在焦平面上的位置
        float2 hp = float2(dot(hd, cr), dot(hd, cu)) / cz * focus;
        float2 dp = (scr - hp) / (apR * hc.w);            // 以光斑半径归一
        float r = length(dp);
        if (r > 1.05f) continue;
        float phi = atan2(dp.y, dp.x);
        const float seg = TAU / 9.0f;
        float pa = fmod(phi + U[U_LENS2].w + 10 * TAU, seg) - seg / 2;
        float poly = mix(1.0f, cos(seg / 2) / cos(pa), 0.16f);
        float edge = smoothstep(poly, poly * 0.93f, r);
        // 口径蚀：光圈上的点 l = -dp（像倒置），要通过第二光阑
        float k = catD / focus;
        float2 qv = -dp * apR * (1 - k) + scr * k;
        float catR = apR * (1 - k) + catM;
        edge *= smoothstep(catR, catR * 0.96f, length(qv));
        float rim = 1 + 0.35f * r * r * r * r;
        float tw = 0.75f + 0.25f * sin(time * hc.x * 0.0f + h.w * 40.0f + U[U_WIND].w * (0.5f + h.w));
        c += hc.xyz * (edge * rim * tw / (hc.w * hc.w));
    }
    return c;
}

static inline float4 bilin(const device float4* b, uint W, uint H, float2 uv) {
    float2 p = uv * float2(W, H) - 0.5f;
    float2 f = floor(p); float2 w = p - f;
    int x0 = clamp(int(f.x), 0, int(W) - 1), x1 = clamp(int(f.x) + 1, 0, int(W) - 1), y0 = clamp(int(f.y), 0, int(H) - 1), y1 = clamp(int(f.y) + 1, 0, int(H) - 1);
    return mix(mix(b[y0 * int(W) + x0], b[y0 * int(W) + x1], w.x), mix(b[y1 * int(W) + x0], b[y1 * int(W) + x1], w.x), w.y);
}
// 双三次（Catmull-Rom）上采样低分辨率层
static inline float cr1(float x) { x = fabs(x); return x < 1 ? 1.5f * x * x * x - 2.5f * x * x + 1 : (x < 2 ? -0.5f * x * x * x + 2.5f * x * x - 4 * x + 2 : 0.0f); }
static float4 bicub(const device float4* b, uint W, uint H, float2 uv, thread float4& aux, const device float4* b1) {
    float2 p = uv * float2(W, H) - 0.5f;
    float2 f = floor(p);
    float4 s = float4(0), s1 = float4(0); float ws = 0;
    for (int j = -1; j <= 2; j++) for (int i = -1; i <= 2; i++) {
        int x = clamp(int(f.x) + i, 0, int(W) - 1), y = clamp(int(f.y) + j, 0, int(H) - 1);
        float w = exp(-0.5f * (sqr(p.x - (f.x + float(i))) + sqr(p.y - (f.y + float(j)))) / sqr(0.75f));
        s += w * b[y * int(W) + x]; s1 += w * b1[y * int(W) + x]; ws += w;
    }
    aux = s1 / ws;
    return s / ws;
}

// 合成：前景(全分辨率) 盖在 中景(低分辨率) 盖在 远景(解析) 上；NEAR 层（靠近镜头的虚影）盖在最上面。
kernel void resolve(device float4* outb [[buffer(0)]], const device float4* fg0 [[buffer(1)]], const device float4* fg1 [[buffer(2)]],
                    const device float4* mid0 [[buffer(3)]], const device float4* mid1 [[buffer(4)]],
                    const device float4* near0 [[buffer(5)]], const device float4* near1 [[buffer(6)]],
                    const device float4* U [[buffer(7)]], const device float4* hl [[buffer(8)]],
                    uint2 gid [[thread_position_in_grid]]) {
    uint W = uint(U[U_RES].x), H = uint(U[U_RES].y);
    if (gid.x >= W || gid.y >= H) return;
    uint idx = gid.y * W + gid.x;
    uint LW = uint(U[U_LOWRES].x), LH = uint(U[U_LOWRES].y);
    float2 uv = (float2(gid) + 0.5f) / float2(W, H);
    float fieldW = U[U_LENS].z, fieldH = fieldW * float(H) / float(W), focus = U[U_LENS].x;
    float2 scr = float2((uv.x * 2 - 1) * fieldW * 0.5f, (1 - uv.y * 2) * fieldH * 0.5f);
    float3 dir = normalize(U[U_CAM_F0].xyz * focus + U[U_CAM_R0].xyz * scr.x + U[U_CAM_U0].xyz * scr.y);
    float3 far = farField(U, hl, dir, scr, U[U_MISC].x);
    float4 f0 = fg0[idx], f1 = fg1[idx];
    float n = max(f1.w, 1.0f);
    float3 fgc = f0.xyz / n; float fga = f0.w / n, fgp = f1.x / n;
    float4 m1, n1;
    float4 m0 = bicub(mid0, LW, LH, uv, m1, mid1);
    float4 n0 = bicub(near0, LW, LH, uv, n1, near1);
    float mn = max(m1.w, 1.0f), nn = max(n1.w, 1.0f);
    float3 midc = m0.xyz / mn; float mida = m0.w / mn, midp = max(m1.x / mn, 1e-3f);
    float3 nearc = n0.xyz / nn; float neara = n0.w / nn, nearp = max(n1.x / nn, 1e-3f);
    // 中景层去掉自身的渐晕后，与远景合成
    float3 behind = (midc + (midp - mida) * far) / midp;
    float3 c = fgc + max(fgp - fga, 0.0f) * behind;
    // NEAR 层
    float na = saturate(neara / nearp);
    c = c * (1 - na) + nearc / nearp * fgp;
    outb[idx] = float4(c, f1.y);
}
