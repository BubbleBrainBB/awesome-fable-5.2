// shader.metal — the renderer proper. Compiled at run time after shared.h.
// One compute kernel: for every pixel, sppN camera samples are path traced (terrain height field +
// instanced triangle BVH), lit by the sun (shadow rays), the sky (one bounce ray), seen through the
// atmosphere / mist / cloud sea, and accumulated into a float buffer whose alpha counts samples.

constant float PI = 3.14159265358979f;
constexpr sampler sLin(coord::normalized, address::clamp_to_edge, filter::linear);
constexpr sampler sSky(coord::normalized, s_address::repeat, t_address::clamp_to_edge, filter::linear);

struct BNode { packed_float3 mn; uint a; packed_float3 mx; uint b; };
struct Tri { packed_float3 v0; uint mat; packed_float3 e1; uint pad1; packed_float3 e2; uint pad2; };
struct TriAttr { packed_float3 n0, n1, n2; packed_float2 t0, t1, t2; float pad; };
struct Inst { float4 r0, r1, r2; uint blasRoot, seed, flags, pad; float4 tint; };

struct Ctx {
    texture2d<float> h0, h1, h2, h3, m0, m1, g2, sky, wat;
    const device Uniforms* U; const device float4* atmo;
    const device BNode* tlas; const device BNode* nodes; const device Tri* tris; const device TriAttr* attrs; const device Inst* insts;
};

// ------------------------------------------------------------------ random numbers
struct Rng { uint s; };
static inline float rnd(thread Rng& r) { r.s = r.s * 747796405u + 2891336453u; uint w = ((r.s >> ((r.s >> 28) + 4)) ^ r.s) * 277803737u; w = (w >> 22) ^ w; return float(w >> 8) * (1.0f / 16777216.0f); }
// Roberts R-sequence value for sample k in dimension d, with a per-pixel shift
static inline float rseq(uint k, uint d, uint pix) {
    const float al[8] = {0.8566748838545029f, 0.733891856627126f, 0.6287067210378087f, 0.5385972572236101f, 0.4614027427763898f, 0.3952715488497748f, 0.3386186803870473f, 0.2900857013998108f};
    return fract(float(k) * al[d & 7] + sh_u2f(sh_hu(pix * 31u + d * 7919u + 17u)));
}

// ------------------------------------------------------------------ terrain height field
struct HInfo { float h; float rough; };
static float4 canopyAt(thread Ctx& c, float2 xz, float hGround, float R, float t);

// Height of the ground at xz. py: height of the querying point — when it is safely above every
// possible detail we return a conservative upper bound instead of evaluating noise.
static float heightAt(thread Ctx& c, float2 xz, float t, float py) {
    const device Uniforms& U = *c.U;
    float2 u0 = (xz - U.clip0.xy) * U.clip0.z; float e0 = max(fabs(u0.x - 0.5f), fabs(u0.y - 0.5f)) * 2.0f;
    float w0 = 1.0f - smoothstep(0.80f, 0.97f, e0);
    float h = 0.0f; float4 m = float4(0.0f, 0.5f, 0.0f, 0.0f);
    if (w0 > 0.0f) { h = c.h0.sample(sLin, u0, level(0)).x; m = c.m0.sample(sLin, u0, level(0)); }
    if (w0 < 1.0f) {
        float2 u1 = (xz - U.clip1.xy) * U.clip1.z; float e1 = max(fabs(u1.x - 0.5f), fabs(u1.y - 0.5f)) * 2.0f;
        float w1 = 1.0f - smoothstep(0.80f, 0.97f, e1); float hh = 0.0f; float4 mm = float4(0.0f, 0.5f, 0.0f, 0.0f);
        if (w1 > 0.0f) { hh = c.h1.sample(sLin, u1, level(0)).x; mm = c.m1.sample(sLin, u1, level(0)); }
        if (w1 < 1.0f) {
            float2 u2 = (xz - U.clip2.xy) * U.clip2.z; float h2;
            if (u2.x > 0.0008f && u2.y > 0.0008f && u2.x < 0.9992f && u2.y < 0.9992f) {
                float hb = c.h2.sample(sLin, u2, level(0)).x; float gx = c.g2.sample(sLin, u2, level(0)).x; float amp = gx * MESO_MAX; float big = smoothstep(0.25f, 0.62f, gx);
                float bnd = amp * 1.25f + big * BIGROCK_MAX * 0.6f;
                if (py + U.origin.y - hb > bnd + 2.0f) h2 = hb + bnd;
                else h2 = hb + amp * sh_meso(xz.x + U.origin.x, xz.y + U.origin.z, hb) + (big > 0.0f ? big * sh_bigrock(xz.x + U.origin.x, xz.y + U.origin.z) : 0.0f);
            } else {
                float2 u3 = (xz - U.clip3.xy) * U.clip3.z; h2 = c.h3.sample(sLin, u3, level(0)).x;
            }
            h2 -= U.origin.y;
            hh = mix(h2, hh, w1); mm = mix(float4(0.0f, 0.5f, 0.0f, 0.0f), mm, w1);
        }
        h = mix(hh, h, w0); m = mix(mm, m, w0);
    }
    if (t < 900.0f) {
        float sc = m.x > 0.4f ? 0.0f : m.z; float bound = 0.25f * m.y + ROCK_MAXH * m.w + 0.3f * ROCK_MAXH * sc;
        if (py - h > bound + 0.3f) h += bound;
        else {
            h += 2.0f * m.y * sh_detail(xz.x, xz.y, t);
            if (m.w > 0.01f) h += m.w * sh_rock(xz.x, xz.y, t);
            if (sc > 0.01f) h += sc * 0.3f * sh_rock(xz.x / 0.3f, xz.y / 0.3f, t / 0.3f);
        }
    }
    // beyond the radius of real tree geometry the forest is a canopy height field built from the same tree cells
    float2 dc = xz - U.shot.xy; float dcl = dot(dc, dc);
    float ha = h + U.origin.y;
    if (dcl > U.shot.z * U.shot.z * 0.8f && ha < 2300.0f && ha > 1380.0f && t > 60.0f) {
        if (py - h > 36.0f) return h + 36.0f * step(py, 1e8f);
        h += canopyAt(c, xz, ha, U.shot.z, t).x;
    }
    return h;
}


// canopy height above ground (x), tree kind 0 spruce / 1 larch (y), per-tree random (z), relative height in crown (w)
static float4 canopyAt(thread Ctx& c, float2 xz, float hGround, float R, float t) {
    const device Uniforms& U = *c.U;
    float2 u2 = (xz - U.clip2.xy) * U.clip2.z; if (u2.x < 0.001f || u2.y < 0.001f || u2.x > 0.999f || u2.y > 0.999f) return float4(0.0f);
    float4 g = c.g2.sample(sLin, u2, level(0)); float2 sc = U.shot.xy + U.origin.xz; xz += U.origin.xz;
    float n1 = sh_vn2(xz.x / 260.0f, xz.y / 260.0f, 61U), n2 = sh_fbm2(xz.x / 90.0f, xz.y / 90.0f, 3, 62U);
    Eco e = sh_eco(hGround, g.w, g.y * 2.0f - 1.0f, 0.5f, g.z, n1, n2);
    if (e.forest <= 0.01f) return float4(0.0f);
    float lodw = smoothstep(1300.0f, 2600.0f, t);
    float4 smoothC = float4(0.0f);
    if (lodw > 0.0f) { float b = sh_fbm2(xz.x / 13.0f, xz.y / 13.0f, 2, 105U), b2 = sh_vn2(xz.x / 4.2f, xz.y / 4.2f, 107U); float kind = (sh_vn2(xz.x / 6.0f, xz.y / 6.0f, 106U) * 0.8f + 0.1f < e.larch) ? 1.0f : 0.0f;
        smoothC = float4(smoothstep(0.05f, 0.6f, e.forest) * (6.0f + 7.0f * b + 5.0f * b2), kind, b2, 0.25f + 0.9f * b2); if (lodw >= 1.0f) return smoothC; }
    float fx = xz.x / TREE_CELL, fz = xz.y / TREE_CELL; int ix = int(floor(fx)), iz = int(floor(fz));
    float4 best = float4(0.0f);
    for (int j = -1; j < 2; j++) for (int i = -1; i < 2; i++) {
        int cx = ix + i, cz = iz + j; uint hh = sh_hc2(cx, cz, 9100U); if (sh_u2f(hh) > e.forest) continue;
        TreeCell tc = sh_treeCell(cx, cz); float2 d0 = float2(tc.px, tc.pz) - sc; if (dot(d0, d0) <= R * R) continue;
        bool larch = tc.kind < e.larch; float H = tc.height * (larch ? 0.72f : 1.0f); float rc = H * (larch ? 0.17f : 0.15f);
        float r = length(xz - float2(tc.px, tc.pz)) / rc; if (r >= 1.0f) continue;
        float hc = H * pow(1.0f - r, 1.25f); if (hc > best.x) best = float4(hc, larch ? 1.0f : 0.0f, tc.rnd, hc / H);
    }
    return mix(best, smoothC, lodw);
}

static bool marchTerrain(thread Ctx& c, float3 ro, float3 rd, float tmin, float tmax, int maxSteps, bool primary, float tLod, thread float& tHit) {
    float t = tmin, tPrev = tmin; bool inside = false;
    for (int i = 0; i < maxSteps; i++) {
        float3 p = ro + rd * t;
        if (p.y + c.U->origin.y > 3500.0f && rd.y >= 0.0f) return false;
        float d = p.y - heightAt(c, p.xz, t + tLod, p.y);
        if (d < 0.0f) { inside = true; break; }
        if (d < 0.0005f * t) {      // heightAt may have returned a conservative bound: confirm against the exact surface
            d = p.y - heightAt(c, p.xz, t + tLod, -1e9f); if (d < 0.0f) { inside = true; break; } if (d < 0.0005f * t) { tHit = t; return true; } }
        tPrev = t; t += max(d * 0.42f, (0.006f + t * mix(0.0025f, 0.006f, smoothstep(200.0f, 1500.0f, t))) * (1.0f + 0.02f * float(max(i - 120, 0))));
        if (t > tmax) return false;
    }
    if (!inside) { tHit = t; float3 p = ro + rd * t; return primary && (p.y - heightAt(c, p.xz, t + tLod, p.y) < 0.0008f * t + 0.05f); }      // out of steps while skimming the ground: for camera rays it is ground
    float a = tPrev, b = t;
    for (int i = 0; i < 9; i++) { float m = 0.5f * (a + b); float3 p = ro + rd * m; if (p.y - heightAt(c, p.xz, m + tLod, p.y) < 0.0f) b = m; else a = m; }
    tHit = 0.5f * (a + b); return tHit <= tmax;
}
static float3 terrainNormal(thread Ctx& c, float3 p, float t) {
    float e = max(0.02f, t * 0.0011f);
    float hx = heightAt(c, p.xz + float2(e, 0), t, -1e9f) - heightAt(c, p.xz - float2(e, 0), t, -1e9f);
    float hz = heightAt(c, p.xz + float2(0, e), t, -1e9f) - heightAt(c, p.xz - float2(0, e), t, -1e9f);
    return normalize(float3(-hx, 2.0f * e, -hz));
}

// ------------------------------------------------------------------ BVH
struct MeshHit { float t; uint inst; uint tri; float2 bary; };

static inline bool hitBox(float3 mn, float3 mx, float3 ro, float3 inv, float tmax, thread float& tn) {
    float3 t0 = (mn - ro) * inv, t1 = (mx - ro) * inv; float3 a = min(t0, t1), b = max(t0, t1);
    tn = max(max(a.x, a.y), max(a.z, 0.0f)); float tf = min(min(b.x, b.y), min(b.z, tmax)); return tn <= tf;
}
// alpha of card materials (needle sprays, leaves, fern fronds) — analytic patterns in uv
static float cardAlpha(uint mat, float2 uv, float hw, uint seed) {
    uint m = mat & 0xffu;
    if (m == 1u || m == 2u) {   // conifer spray. uv.x = metres along the shoot, uv.y 0..1 across, hw = half width (m)
        float v = fabs(uv.y - 0.5f) * 2.0f, vw = v * hw;
        if (vw < 0.004f) return 1.0f;                                     // the shoot itself
        bool larch = (m == 2u);
        float n = uv.x / (larch ? 0.038f : 0.050f) - vw / 0.036f + ((uv.y > 0.5f) ? 0.0f : 0.43f);
        float fi = floor(n), f = n - fi; float dn = fabs(f - 0.5f);
        float hsh = sh_u2f(sh_hu(uint(int(fi) + 4096) * 13u + seed + ((uv.y > 0.5f) ? 7u : 0u)));
        if (v > 0.50f + 0.50f * hsh) return 0.0f;                         // side shoots of different length
        if (dn < 0.055f) return 1.0f;
        if (larch) { float tuft = fract(vw / 0.021f + hsh * 3.0f); return (dn < 0.40f && tuft < 0.55f && fract(vw / 0.0031f + f * 3.0f) < 0.45f) ? 1.0f : 0.0f; }
        float tooth = fract(vw / 0.0040f + f * 2.0f + hsh * 5.0f);
        return (dn < 0.40f - 0.12f * v && tooth < 0.56f) ? 1.0f : 0.0f;
    }
    if (m == 6u) {              // broad leaf: ellipse
        float2 q = (uv - 0.5f) * 2.0f; return (q.x * q.x + q.y * q.y * (1.0f + 0.8f * q.x)) < 1.0f ? 1.0f : 0.0f;
    }
    if (m == 8u) {              // fern frond: pinnae along the rachis (uv.x 0..1)
        float v = fabs(uv.y - 0.5f) * 2.0f; float w = sin(PI * sqrt(saturate(uv.x))) * (0.35f + 0.65f * (1.0f - uv.x));
        float pin = fabs(fract(uv.x * 21.0f - v * 2.2f) - 0.5f) * 2.0f;
        if (v < 0.03f) return 1.0f; if (v > w) return 0.0f;
        float tooth = fract(v * 9.0f + uv.x * 40.0f);
        return (pin < 0.70f - 0.35f * v / max(w, 0.01f) && (pin < 0.3f || tooth < 0.6f)) ? 1.0f : 0.0f;
    }
    return 1.0f;
}
static bool traverseBlas(thread Ctx& c, uint root, float3 ro, float3 rd, bool anyHit, uint seed, thread Rng& rng, thread float& tmax, thread uint& triOut, thread float2& baryOut) {
    uint st[28]; int sp = 0; st[sp++] = root; bool found = false; float3 inv = 1.0f / rd; float tn;
    { const device BNode& n = c.nodes[root]; if (!hitBox(n.mn, n.mx, ro, inv, tmax, tn)) return false; }
    while (sp > 0) {
        uint ni = st[--sp]; const device BNode& n = c.nodes[ni];
        if (n.b > 0u) {
            for (uint k = 0; k < n.b; k++) {
                uint ti = n.a + k; const device Tri& tr = c.tris[ti];
                float3 e1 = tr.e1, e2 = tr.e2; float3 pv = cross(rd, e2); float det = dot(e1, pv);
                if (fabs(det) < 1e-12f) continue; float idet = 1.0f / det; float3 tv = ro - float3(tr.v0);
                float u = dot(tv, pv) * idet; if (u < 0.0f || u > 1.0f) continue; float3 qv = cross(tv, e1);
                float v = dot(rd, qv) * idet; if (v < 0.0f || u + v > 1.0f) continue; float t = dot(e2, qv) * idet;
                if (t <= 1e-4f || t >= tmax) continue;
                if (tr.mat & 0x100u) {
                    const device TriAttr& at = c.attrs[ti]; float2 uv = float2(at.t0) * (1.0f - u - v) + float2(at.t1) * u + float2(at.t2) * v;
                    if (cardAlpha(tr.mat, uv, at.pad, seed) < 0.5f) continue;
                }
                tmax = t; triOut = ti; baryOut = float2(u, v); found = true; if (anyHit) return true;
            }
        } else {
            const device BNode& l = c.nodes[n.a]; const device BNode& r = c.nodes[n.a + 1u]; float tl, tr2;
            bool hl = hitBox(l.mn, l.mx, ro, inv, tmax, tl), hr = hitBox(r.mn, r.mx, ro, inv, tmax, tr2);
            if (hl && hr) { if (tl < tr2) { st[sp++] = n.a + 1u; st[sp++] = n.a; } else { st[sp++] = n.a; st[sp++] = n.a + 1u; } }
            else if (hl) st[sp++] = n.a; else if (hr) st[sp++] = n.a + 1u;
            if (sp > 26) sp = 26;
        }
    }
    return found;
}
static inline void instRay(thread Ctx& c, const device Inst& in, float3 ro, float3 rd, thread float3& o, thread float3& d) {
    o = float3(dot(in.r0.xyz, ro) + in.r0.w, dot(in.r1.xyz, ro) + in.r1.w, dot(in.r2.xyz, ro) + in.r2.w);
    d = float3(dot(in.r0.xyz, rd), dot(in.r1.xyz, rd), dot(in.r2.xyz, rd));
    if (in.tint.w > 0.0f) {   // wind: shear the plant, i.e. inverse-shear the ray
        const device Uniforms& U = *c.U; float ph = sh_u2f(in.seed) * 6.28f; float tt = U.lens.w;
        float gust = 0.55f + 0.45f * sin(tt * 0.7f + ph * 0.3f + in.r0.w * 0.05f);
        float2 w = U.wind.xy * (U.wind.z * gust * (0.6f + 0.4f * sin(tt * U.wind.w + ph))) + float2(cos(ph), sin(ph)) * (0.25f * U.wind.z * sin(tt * 2.3f * U.wind.w + ph * 3.0f));
        float2 wo = float2(dot(in.r0.xz, w), dot(in.r2.xz, w)) * in.tint.w;
        o.xz -= wo * o.y; d.xz -= wo * d.y;
    }
}
static bool traceMesh(thread Ctx& c, float3 ro, float3 rd, float tmaxIn, bool anyHit, thread Rng& rng, thread MeshHit& hit) {
    const device Uniforms& U = *c.U; if (U.tlasCount == 0u) return false;
    uint st[30]; int sp = 0; st[sp++] = 0u; float tmax = tmaxIn; bool found = false; float3 inv = 1.0f / rd; float tn;
    { const device BNode& n = c.tlas[0]; if (!hitBox(n.mn, n.mx, ro, inv, tmax, tn)) return false; }
    while (sp > 0) {
        uint ni = st[--sp]; const device BNode& n = c.tlas[ni];
        if (n.b > 0u) {
            for (uint k = 0; k < n.b; k++) {
                uint ii = n.a + k; const device Inst& in = c.insts[ii]; float3 o, d; instRay(c, in, ro, rd, o, d);
                uint tri; float2 bary;
                if (traverseBlas(c, in.blasRoot, o, d, anyHit, in.seed, rng, tmax, tri, bary)) { found = true; hit.t = tmax; hit.inst = ii; hit.tri = tri; hit.bary = bary; if (anyHit) return true; }
            }
        } else {
            const device BNode& l = c.tlas[n.a]; const device BNode& r = c.tlas[n.a + 1u]; float tl, tr2;
            bool hl = hitBox(l.mn, l.mx, ro, inv, tmax, tl), hr = hitBox(r.mn, r.mx, ro, inv, tmax, tr2);
            if (hl && hr) { if (tl < tr2) { st[sp++] = n.a + 1u; st[sp++] = n.a; } else { st[sp++] = n.a; st[sp++] = n.a + 1u; } }
            else if (hl) st[sp++] = n.a; else if (hr) st[sp++] = n.a + 1u;
            if (sp > 28) sp = 28;
        }
    }
    return found;
}

// ------------------------------------------------------------------ atmosphere
constant float3 BR = float3(5.802e-3f, 13.558e-3f, 33.1e-3f);
constant float3 BO = float3(0.650e-3f, 1.881e-3f, 0.085e-3f);
static inline float3 atmoT(thread Ctx& c, float hkm) { float x = saturate(hkm / ATMO_HMAX) * float(ATMO_N - 1); int i = min(int(x), ATMO_N - 2); return mix(c.atmo[i].xyz, c.atmo[i + 1].xyz, x - float(i)); }
static inline float3 atmoPsi(thread Ctx& c, float hkm) { float x = saturate(hkm / ATMO_HMAX) * float(ATMO_N - 1); int i = min(int(x), ATMO_N - 2); return mix(c.atmo[ATMO_N + i].xyz, c.atmo[ATMO_N + i + 1].xyz, x - float(i)); }
static inline float phaseR(float cs) { return 3.0f / (16.0f * PI) * (1.0f + cs * cs); }
static inline float phaseHG(float cs, float g) { return (1.0f - g * g) / (4.0f * PI * pow(1.0f + g * g - 2.0f * g * cs, 1.5f)); }
static inline float phaseM(float cs) { float g = 0.80f; return 3.0f / (8.0f * PI) * ((1.0f - g * g) * (1.0f + cs * cs)) / ((2.0f + g * g) * pow(1.0f + g * g - 2.0f * g * cs, 1.5f)); }
// sunlight (irradiance on a surface facing the sun) at altitude y metres
static inline float3 sunAt(thread Ctx& c, float y) { return c.U->sunE.xyz * atmoT(c, (y + c.U->origin.y) * 0.001f); }
static float3 skyLookup(thread Ctx& c, float3 d) {
    float el = asin(clamp(d.y, -1.0f, 1.0f)); float v = 0.5f + 0.5f * sign(el) * sqrt(fabs(el) / (PI * 0.5f));
    float u = atan2(d.z, d.x) / (2.0f * PI); return c.sky.sample(sSky, float2(u, v), level(0)).xyz * c.U->sunE.xyz;
}
// transmittance and in-scatter along a view segment of length t (metres)
static void aerial(thread Ctx& c, float3 ro, float3 rd, float t, thread float3& T, thread float3& S) {
    const device Uniforms& U = *c.U; const int n = 6; float cs = dot(rd, U.sunDir.xyz); float pr = phaseR(cs), pm = phaseM(cs);
    T = float3(1.0f); S = float3(0.0f); float tk = t * 0.001f, prev = 0.0f;
    for (int i = 0; i < n; i++) {
        float u = float(i + 1) / float(n); float t1 = tk * u * (0.4f + 0.6f * u); float tm = 0.5f * (prev + t1), ds = t1 - prev; prev = t1;
        float h = max(0.0f, (ro.y + U.origin.y) * 0.001f + rd.y * tm + tm * tm * (0.5f / 6360.0f));
        float dR = exp(-h / 8.0f), dM = exp(-h / 1.2f) * U.sunE.w; float3 sr = BR * dR; float sm = 3.996e-3f * dM;
        float3 se = sr + 4.44e-3f * dM + BO * max(0.0f, 1.0f - fabs(h - 25.0f) / 15.0f);
        float3 seg = exp(-se * ds);
        float3 src = (sr * pr + sm * pm) * atmoT(c, h) + (sr + sm) * atmoPsi(c, h);
        S += T * src * (1.0f - seg) / se; T *= seg;
    }
    S *= U.sunE.xyz;
}


// ------------------------------------------------------------------ mist (thin radiation fog in the forest, sun shafts)
static inline float mistDensity(thread Ctx& c, float3 p) {     // extinction per metre
    const device Uniforms& U = *c.U; float d = U.mist.x * 0.001f * exp(-max(p.y + U.origin.y - U.mist.y, -40.0f) / U.mist.z);
    if (U.mist.w > 0.0f) { float tt = U.lens.w; float n = sh_fbm3(p.x * 0.035f + tt * 0.02f, p.y * 0.09f, p.z * 0.035f + tt * 0.012f, 3, 90U); d *= max(0.0f, 1.0f + U.mist.w * (n - 0.5f) * 3.0f); }
    return d;
}
static inline float mistTauToSun(thread Ctx& c, float3 p) {
    const device Uniforms& U = *c.U; float d0 = U.mist.x * 0.001f * exp(-max(p.y + U.origin.y - U.mist.y, -40.0f) / U.mist.z);
    return d0 * U.mist.z / max(U.sunDir.y, 0.06f);
}
static inline float mistTau(thread Ctx& c, float3 ro, float3 rd, float t) {   // optical depth along a view segment (smooth part only)
    const device Uniforms& U = *c.U; float H = U.mist.z; float d0 = U.mist.x * 0.001f * exp(-(ro.y + U.origin.y - U.mist.y) / H); float k = rd.y / H;
    return fabs(k) < 1e-5f ? d0 * t : d0 * (1.0f - exp(-k * t)) / k;
}

// ------------------------------------------------------------------ cloud sea (stratus deck filling the lowlands)
static float cloudTop(float2 xz, float base, float tt) {
    float2 q = xz + float2(tt * 3.0f, tt * 1.2f);
    float a = sh_fbm2(q.x / 5200.0f, q.y / 5200.0f, 3, 95U) - 0.5f;
    float3 w = sh_worley2(q.x / 900.0f + 3.0f * a, q.y / 900.0f, 96U); float3 w2 = sh_worley2(q.x / 260.0f, q.y / 260.0f + 2.0f * a, 97U);
    return base + 150.0f * a + 55.0f * (1.0f - w.x) * (1.0f - w.x) + 16.0f * (1.0f - w2.x);
}
static float cloudDensity(float3 p, float base, float tt, bool detail) {
    float top = cloudTop(p.xz, base, tt); float d = top - p.y; if (d <= 0.0f) return 0.0f;
    float den = smoothstep(0.0f, 45.0f, d);
    if (detail) { float n = sh_fbm3(p.x / 70.0f + tt * 0.04f, p.y / 45.0f, p.z / 70.0f, 3, 98U); den *= saturate(0.25f + 1.6f * n - 0.9f * smoothstep(40.0f, 0.0f, d)); }
    return den * 0.035f;     // extinction per metre
}
static void cloudSea(thread Ctx& c, float3 ro, float3 rd, float tEnd, float jitter, thread float3& T, thread float3& S, thread float& tMean) {
    const device Uniforms& U = *c.U; T = float3(1.0f); S = float3(0.0f); tMean = tEnd; ro += U.origin.xyz;
    float base = U.cloud.x, yMax = base + 150.0f; float tt = U.cloud.z;
    if (rd.y >= -1e-4f && ro.y > yMax) return;
    float t0 = ro.y > yMax ? (yMax - ro.y) / rd.y : 0.0f; if (t0 >= tEnd) return;
    float t1 = min(tEnd, t0 + 9000.0f); if (rd.y < -1e-4f) t1 = min(t1, (base - 600.0f - ro.y) / rd.y);
    const int N = 26; float tr = 1.0f; float3 sun = U.sunDir.xyz; float cs = dot(rd, sun);
    float ph = mix(phaseHG(cs, 0.72f), phaseHG(cs, -0.15f), 0.35f) * 4.0f * PI;      // normalised so isotropic = 1
    float3 Esun = sunAt(c, base - U.origin.y); float3 amb = U.skyAmb.xyz / PI; float acc = 0.0f, wsum = 0.0f;
    float dt = (t1 - t0) / float(N); float t = t0 + dt * jitter; bool far = t0 > 15000.0f;
    for (int i = 0; i < N; i++) {
        float3 p = ro + rd * t; float den = cloudDensity(p, base, tt, !far);
        if (den > 0.0f) {
            // light march toward the sun: 3 samples with growing spacing
            float tau = 0.0f; float ls = 28.0f; float3 q = p;
            for (int j = 0; j < 3; j++) { q += sun * ls; tau += cloudDensity(q, base, tt, false) * ls; ls *= 2.2f; }
            float lit = exp(-tau) + 0.35f * exp(-0.22f * tau);
            float depth = saturate((cloudTop(p.xz, base, tt) - p.y) / 120.0f);
            float3 Lsrc = Esun * (lit * ph * 0.25f + 0.06f * lit) + amb * (1.15f - 0.75f * depth);
            float a = 1.0f - exp(-den * dt); S += Lsrc * (tr * a * 0.97f); acc += tr * a * t; wsum += tr * a; tr *= 1.0f - a;
            if (tr < 0.02f) { tr = 0.0f; break; }
        }
        t += dt;
    }
    T = float3(tr); if (wsum > 0.0f) tMean = mix(tEnd, acc / wsum, 1.0f - tr);
}


// ------------------------------------------------------------------ high cirrus (a thin sheet at 9 km)
static float3 cirrus(thread Ctx& c, float3 ro, float3 rd, float3 L) {
    const device Uniforms& U = *c.U; if (U.misc.x <= 0.0f || rd.y < 0.012f) return L;
    float tt = U.cloud.z; float d = (9000.0f - (ro.y + U.origin.y)) / rd.y; float2 q = (ro.xz + U.origin.xz + rd.xz * d) + float2(tt * 18.0f, tt * 7.0f);
    float2 w = float2(0.83f * q.x + 0.55f * q.y, -0.55f * q.x + 0.83f * q.y);       // streaks along the upper wind
    float band = sh_fbm2(w.x / 26000.0f, w.y / 9000.0f, 3, 101U);
    float fib = sh_fbm2(w.x / 5200.0f + 3.0f * band, w.y / 900.0f, 4, 102U), fine = sh_fbm2(w.x / 1400.0f, w.y / 260.0f, 3, 103U);
    float den = smoothstep(1.0f - U.misc.x, 1.0f - U.misc.x + 0.35f, band * 0.7f + fib * 0.5f) * (0.35f + 0.65f * fine);
    float a = den * 0.55f * exp(-d / 90000.0f) * smoothstep(0.012f, 0.06f, rd.y);
    float cs = dot(rd, U.sunDir.xyz); float3 Es = U.sunE.xyz * atmoT(c, 9.0f);
    float3 col = Es * (0.045f + 0.25f * phaseHG(cs, 0.6f)) + U.skyAmb.xyz * 0.12f;
    float3 T, S; aerial(c, ro, rd, min(d, 120000.0f), T, S);
    return mix(L, col * T + S, a);
}


// ------------------------------------------------------------------ the brook
// water surface height (shot-relative) at xz, very negative where there is none
static inline float waterLevel(thread Ctx& c, float2 xz) {
    const device Uniforms& U = *c.U; float2 u0 = (xz - U.clip0.xy) * U.clip0.z;
    if (u0.x < 0.01f || u0.y < 0.01f || u0.x > 0.99f || u0.y > 0.99f) return -1e4f;
    return c.wat.sample(sLin, u0, level(0)).x;
}
struct Water { float t; float3 n; float foam; float depth; };
// ripples and foam are noise advected along the fall line of the surface (two-phase flow map)
static void waterSurface(thread Ctx& c, float2 xz, float bedY, thread Water& w) {
    const device Uniforms& U = *c.U; float e = 0.3f; float h0 = waterLevel(c, xz);
    float2 g = float2(waterLevel(c, xz + float2(e, 0)) - waterLevel(c, xz - float2(e, 0)), waterLevel(c, xz + float2(0, e)) - waterLevel(c, xz - float2(0, e))) / (2.0f * e);
    float slope = length(g); float2 dir = slope > 1e-4f ? -g / slope : float2(1.0f, 0.0f); float2 perp = float2(-dir.y, dir.x);
    float depth = max(h0 - bedY, 0.0f); float speed = 0.5f + 5.0f * min(slope, 0.6f); float tt = U.lens.w;
    float2 grad = float2(0.0f); float fo = 0.0f;
    for (int k = 0; k < 2; k++) {
        float ph = fract(tt * 0.55f + 0.5f * float(k)); float wgt = 1.0f - fabs(2.0f * ph - 1.0f); float2 q = xz - dir * (speed * (ph - 0.5f) * 1.8f);
        float a = dot(q, dir), b = dot(q, perp); float ee = 0.02f; uint sd = 400U + uint(k) * 31U;
        float n0 = sh_fbm2(a / 0.42f, b / 0.13f, 3, sd), na = sh_fbm2((a + ee) / 0.42f, b / 0.13f, 3, sd), nb = sh_fbm2(a / 0.42f, (b + ee) / 0.13f, 3, sd);
        grad += wgt * (dir * (na - n0) + perp * (nb - n0)) / ee;
        fo += wgt * (0.6f * sh_fbm2(a / 0.30f, b / 0.10f, 3, sd + 7U) + 0.4f * sh_vn2(a / 0.07f, b / 0.035f, sd + 9U));
    }
    float amp = 0.010f + 0.05f * min(slope, 0.5f) + 0.012f * saturate(1.0f - depth / 0.15f);
    w.n = normalize(float3(-grad.x * amp, 1.0f, -grad.y * amp) + float3(-g.x, 0.0f, -g.y) * 0.6f);
    float energy = saturate(slope * 2.6f - 0.42f) + 0.55f * saturate(1.0f - depth / 0.05f) * saturate(slope * 5.0f);
    float streak = fo * (0.75f + 0.5f * sh_vn2(dot(xz, perp) / 0.045f, dot(xz, dir) / 0.6f - tt * speed * 0.6f, 431U));
    w.foam = smoothstep(1.02f - 0.8f * energy, 1.24f - 0.6f * energy, streak + 0.28f) * saturate(energy * 2.5f) * (0.50f + 0.50f * sh_fbm2(dot(xz, perp) / 0.09f, dot(xz, dir) / 0.5f - tt * speed * 0.8f, 2, 433U)); w.depth = depth;
}

// ------------------------------------------------------------------ surfaces
struct Surf { float3 p, n, ng, albedo; float trans; float3 transCol; float spec, rough; };

static float3 hsvTint(float3 c, float v) { return c * v; }

// micro relief used for bump shading (metres)
static float microBump(float2 q, float tread, float rock, float scree, float turf) {
    float b = 0.0f;
    if (tread > 0.05f) { float3 w = sh_worley2(q.x * 14.0f, q.y * 14.0f, 76U); b += tread * (0.010f * (1.0f - smoothstep(0.0f, 0.55f, w.x)) * step(0.45f, w.z) + 0.004f * sh_vn2(q.x * 55.0f, q.y * 55.0f, 77U)); }
    if (rock + scree > 0.05f) b += (rock + scree) * (0.006f * sh_fbm2(q.x * 21.0f, q.y * 21.0f, 3, 73U) + 0.0025f * sh_vn2(q.x * 130.0f, q.y * 130.0f, 74U));
    if (turf > 0.05f) b += turf * 0.012f * sh_fbm2(q.x * 16.0f, q.y * 16.0f, 3, 78U);
    return b;
}
static void shadeTerrain(thread Ctx& c, float3 p, float t, thread Surf& s) {
    const device Uniforms& U = *c.U;
    float3 n = terrainNormal(c, p, t); s.p = p; s.ng = n; float3 A = p + U.origin.xyz;
    // masks
    float2 u0 = (p.xz - U.clip0.xy) * U.clip0.z; float e0 = max(fabs(u0.x - 0.5f), fabs(u0.y - 0.5f)) * 2.0f; float w0 = 1.0f - smoothstep(0.80f, 0.97f, e0);
    float2 u1 = (p.xz - U.clip1.xy) * U.clip1.z; float e1 = max(fabs(u1.x - 0.5f), fabs(u1.y - 0.5f)) * 2.0f; float w1 = 1.0f - smoothstep(0.80f, 0.97f, e1);
    float4 m = float4(0, 0.5f, 0, 0);
    if (w1 > 0.0f) m = mix(m, c.m1.sample(sLin, u1, level(0)), w1);
    if (w0 > 0.0f) m = mix(m, c.m0.sample(sLin, u0, level(0)), w0);
    float wNear = max(w0, w1);
    float2 u2 = saturate((p.xz - U.clip2.xy) * U.clip2.z); float4 g = c.g2.sample(sLin, u2, level(0));
    bool fine = u2.x > 0.001f && u2.y > 0.001f && u2.x < 0.999f && u2.y < 0.999f;
    float nyM = fine ? g.w : n.y; float north = fine ? (g.y * 2.0f - 1.0f) : -n.z;
    float n1 = sh_vn2(A.x / 260.0f, A.z / 260.0f, 61U), n2 = sh_fbm2(A.x / 90.0f, A.z / 90.0f, 3, 62U);
    Eco e = sh_eco(A.y, nyM, north, 0.5f, fine ? g.z : 0.0f, n1, n2);
    float steep = 1.0f - n.y; float fp = t * 0.0012f;             // pixel footprint (m)
    float nmid = sh_fbm2(A.x / 7.0f, A.z / 7.0f, 3, 63U);
    float tread = smoothstep(0.62f, 0.90f, m.x), soil = smoothstep(0.10f, 0.40f, m.x) * (1.0f - tread);
    float riser = m.x > 0.4f ? m.z : 0.0f;
    float rockFar = saturate(max(e.rock, smoothstep(0.30f, 0.50f, steep + 0.12f * (nmid - 0.5f))));
    float rock = mix(rockFar, max(m.w, smoothstep(0.42f, 0.60f, steep) * (1.0f - soil - tread)), wNear);
    float scree = mix(e.scree * (1.0f - rockFar), (m.x > 0.4f ? 0.0f : m.z), wNear) * (1.0f - rock);
    float snow = e.snow * smoothstep(0.52f, 0.30f, steep); snow = saturate(snow * 1.5f + (snow > 0.02f ? (nmid - 0.5f) * 0.9f : 0.0f)) * (1.0f - 0.7f * tread);
    float turf = saturate(1.0f - rock - scree);
    float nearW = 1.0f - smoothstep(12.0f, 55.0f, length(p.xz - U.shot.xy));
    // ---- turf / forest floor
    float var = sh_fbm2(p.x / 1.7f, p.z / 1.7f, 3, 64U), var2 = sh_vn2(A.x / 23.0f, A.z / 23.0f, 65U), var3 = sh_fbm2(A.x / 140.0f, A.z / 140.0f, 3, 79U);
    float lush = saturate(0.25f + 0.9f * (var3 - 0.5f) + 0.35f * north + 1.2f * (fine ? g.z : 0.0f) - smoothstep(2150.0f, 2450.0f, A.y) * 0.3f);
    float3 straw = mix(float3(0.300f, 0.235f, 0.105f), float3(0.215f, 0.175f, 0.085f), var2);
    float3 olive = mix(float3(0.090f, 0.125f, 0.040f), float3(0.135f, 0.150f, 0.050f), var);
    float3 turfC = mix(straw, olive, smoothstep(0.30f, 0.70f, lush + 0.35f * (var - 0.5f)));
    float russet = smoothstep(0.56f, 0.72f, sh_fbm2(A.x / 4.3f + 9.0f, A.z / 4.3f, 3, 66U)) * smoothstep(1950.0f, 2200.0f, A.y);
    turfC = mix(turfC, float3(0.165f, 0.040f, 0.028f), russet * 0.75f);
    float3 soilC = mix(float3(0.085f, 0.060f, 0.040f), float3(0.150f, 0.110f, 0.070f), var);
    turfC = mix(turfC, mix(soilC, turfC * 0.6f, 0.45f), nearW * 0.85f);                  // under real blades: darker thatch and soil
    float nl = sh_fbm2(p.x / 0.6f, p.z / 0.6f, 3, 80U); float needles = sh_vn2(p.x * 37.0f, p.z * 9.0f, 108U) * sh_vn2(p.x * 11.0f + 5.0f, p.z * 41.0f, 109U);
    float3 litter = mix(float3(0.075f, 0.050f, 0.032f), float3(0.150f, 0.105f, 0.060f), nl) * (0.8f + 0.7f * needles * saturate(0.02f / (fp + 1e-4f)));
    float mossy = smoothstep(0.42f, 0.60f, sh_fbm2(A.x / 3.4f, A.z / 3.4f + 4.0f, 3, 67U) + 0.15f * north + 0.25f * (nl - 0.5f));
    float mt = sh_fbm2(p.x / 0.23f, p.z / 0.23f, 3, 110U), mt2 = sh_vn2(p.x * 60.0f, p.z * 60.0f, 111U);
    float3 mossC = mix(float3(0.026f, 0.055f, 0.014f), float3(0.080f, 0.105f, 0.026f), mt) * (0.7f + 0.6f * mt2 * saturate(0.012f / (fp + 1e-4f)));
    mossC = mix(mossC, litter, 0.25f * smoothstep(0.5f, 0.75f, sh_vn2(p.x * 7.0f, p.z * 7.0f, 112U)));
    litter = mix(litter, mossC, mossy * 0.88f);
    float forestW = smoothstep(0.15f, 0.5f, e.forest);
    float3 ground = mix(turfC, litter, forestW);
    // ---- bedrock: per-block tone, joints, lichen, staining
    float3 f1 = sh_facet(p.x, p.z, 5.5f, 0.5f, 0.10f, 201U), f2 = sh_facet(p.x + 13.0f, p.z, 1.7f, 1.3f, 0.22f, 202U);
    float warm = sh_fbm2(A.x / 60.0f, A.z / 60.0f + 0.02f * A.y, 3, 69U);
    float3 rockC = mix(float3(0.215f, 0.215f, 0.225f), float3(0.315f, 0.275f, 0.225f), smoothstep(0.35f, 0.7f, warm));
    rockC = mix(rockC, float3(0.125f, 0.125f, 0.135f), smoothstep(0.62f, 0.8f, sh_fbm2(A.x / 33.0f + 7.0f, A.z / 33.0f, 3, 81U)) * 0.7f);
    float l2 = saturate(1.7f / (fp * 3.0f + 1e-3f) - 0.6f), l1 = saturate(5.5f / (fp * 3.0f + 1e-3f) - 0.6f);
    rockC *= mix(1.0f, 0.72f + 0.56f * f2.z, l2) * mix(1.0f, 0.85f + 0.30f * f1.z, l1);
    rockC *= mix(1.0f, 0.35f + 0.65f * smoothstep(0.0f, 0.06f, f2.y), l2 * 0.9f) * mix(1.0f, 0.6f + 0.4f * smoothstep(0.0f, 0.03f, f1.y), l1 * 0.7f);
    float streak = sh_fbm2(A.x / 1.3f, A.z / 1.3f, 3, 82U); rockC *= 1.0f - 0.30f * smoothstep(0.5f, 0.75f, streak) * smoothstep(0.25f, 0.5f, steep) * saturate(1.3f / (fp * 4.0f + 1e-3f) - 0.5f);
    {   // large-scale character seen from afar: dipping beds, gully darkening, pale scree-dusted ledges
        float bed = sin((A.y + 0.45f * A.x + 0.25f * A.z) / 14.0f + 6.0f * sh_fbm2(A.x / 300.0f, A.z / 300.0f, 2, 87U)); rockC *= 1.0f + 0.11f * bed;
        float big = sh_fbm2(A.x / 170.0f, A.z / 170.0f + 0.01f * A.y, 3, 88U); rockC *= 0.82f + 0.36f * big;
        rockC = mix(rockC, float3(0.30f, 0.29f, 0.275f), smoothstep(0.20f, 0.05f, steep) * 0.5f);
        if (fine) rockC *= 1.0f - 0.25f * smoothstep(0.25f, 0.6f, g.z);
    }
    float lf = saturate(0.5f / (fp * 4.0f + 1e-3f) - 0.3f);
    if (lf > 0.0f) {
        float lp = sh_fbm3(p.x / 0.9f, p.y / 0.9f, p.z / 0.9f, 2, 83U), lq = sh_fbm3(p.x / 0.09f, p.y / 0.09f, p.z / 0.09f, 3, 70U);
        float ly = smoothstep(0.62f, 0.68f, lq) * smoothstep(0.50f, 0.62f, lp);  rockC = mix(rockC, float3(0.27f, 0.29f, 0.10f), ly * 0.45f * lf);
        float lw = smoothstep(0.62f, 0.68f, sh_fbm3(p.x / 0.2f + 3.0f, p.y / 0.2f, p.z / 0.2f, 3, 84U)) * smoothstep(0.55f, 0.4f, lp); rockC = mix(rockC, float3(0.40f, 0.40f, 0.37f), lw * 0.45f * lf);
        float lb = smoothstep(0.66f, 0.70f, sh_fbm3(p.x / 0.33f, p.y / 0.33f, p.z / 0.33f + 5.0f, 3, 85U)); rockC = mix(rockC, float3(0.04f, 0.04f, 0.04f), lb * 0.5f * lf);
        rockC *= 0.88f + 0.24f * sh_vn3(p.x * 90.0f, p.y * 90.0f, p.z * 90.0f, 86U);
    }
    // ---- scree: every stone its own tone, dark gaps
    float3 screeC = float3(0.27f, 0.265f, 0.26f);
    if (scree > 0.02f) {
        float3 s1 = sh_facet(p.x / 0.3f, p.z / 0.3f, 5.5f, 0.5f, 0.10f, 201U), s2 = sh_facet(p.x / 0.3f + 13.0f, p.z / 0.3f, 1.7f, 1.3f, 0.22f, 202U);
        float k2 = saturate(0.5f / (fp * 3.0f + 1e-3f) - 0.6f), k1 = saturate(1.65f / (fp * 3.0f + 1e-3f) - 0.6f);
        float3 tone = mix(float3(0.20f, 0.20f, 0.21f), float3(0.36f, 0.33f, 0.29f), mix(0.5f, s1.z, k1));
        tone *= mix(1.0f, 0.65f + 0.7f * s2.z, k2);
        tone *= mix(1.0f, 0.3f + 0.7f * smoothstep(0.0f, 0.10f, s2.y), k2 * 0.9f) * mix(1.0f, 0.45f + 0.55f * smoothstep(0.0f, 0.05f, s1.y), k1 * 0.9f);
        screeC = tone * mix(float3(1.0f), rockC / 0.25f, 0.35f);
    }
    float3 col = ground * turf + rockC * rock + screeC * scree;
    // ---- trail: compacted mineral soil with pebbles; duff-covered in the forest; cut banks are raw soil
    float3 dirt = mix(float3(0.175f, 0.125f, 0.085f), float3(0.285f, 0.225f, 0.160f), sh_fbm2(p.x / 0.31f, p.z / 0.31f, 3, 72U));
    dirt = mix(dirt, mix(float3(0.10f, 0.065f, 0.04f), float3(0.17f, 0.11f, 0.06f), var), forestW * 0.75f);
    if (fp < 0.05f) { float3 w = sh_worley2(p.x * 14.0f, p.z * 14.0f, 76U); float peb = (1.0f - smoothstep(0.25f, 0.5f, w.x)) * step(0.45f, w.z); dirt = mix(dirt, mix(float3(0.22f, 0.21f, 0.20f), float3(0.40f, 0.38f, 0.35f), fract(w.z * 7.0f)), peb * 0.8f * (1.0f - forestW * 0.6f)); }
    if (tread > 0.05f && fp < 0.15f) { float2 ts = sh_treadStones(p.x, p.z); dirt = mix(dirt, mix(dirt, rockC * (0.7f + 0.4f * fract(ts.y * 5.0f)), 0.75f), smoothstep(0.25f, 0.5f, ts.x)); }
    if (forestW > 0.3f && fp < 0.08f) { float rt = sh_roots(p.x, p.z); float3 rootC = mix(float3(0.13f, 0.09f, 0.06f), float3(0.25f, 0.19f, 0.13f), var); dirt = mix(dirt, rootC, smoothstep(0.15f, 0.5f, rt)); ground = mix(ground, rootC, smoothstep(0.15f, 0.5f, rt) * 0.8f); col = mix(col, rootC, smoothstep(0.15f, 0.5f, rt) * 0.8f * turf); }
    float3 bank = mix(soilC * 1.25f, ground, 0.3f + 0.3f * var);
    col = mix(col, bank, soil); col = mix(col, dirt, tread); col = mix(col, rockC * 1.1f, riser * tread);
    col = mix(col, float3(0.86f, 0.88f, 0.92f), snow);
    s.albedo = col; s.trans = 0.0f; s.transCol = float3(0.0f); s.spec = 0.02f + 0.03f * snow; s.rough = 0.8f;
    { float2 dc = p.xz - U.shot.xy;
      if (dot(dc, dc) > U.shot.z * U.shot.z * 0.8f && A.y < 2340.0f && t > 60.0f && fine) {
        float hg = A.y; float4 cn = canopyAt(c, p.xz, hg, U.shot.z, t);
        if (cn.x <= 0.3f) { cn = canopyAt(c, p.xz, hg - 12.0f, U.shot.z, t); }
        if (cn.x > 0.3f) {
            float v = 0.75f + 0.5f * cn.z; float tex = 0.75f + 0.5f * sh_vn2(A.x * 1.3f, A.z * 1.3f + A.y * 2.0f, 75U);
            float3 fol = mix(float3(0.034f, 0.058f, 0.026f), float3(0.36f, 0.215f, 0.04f), cn.y) * v;
            s.albedo = fol * tex * (0.55f + 0.45f * smoothstep(0.0f, 0.7f, cn.w)); s.trans = 0.0f; s.spec = 0.03f; s.n = n; return;
        }
      } }
    // micro bump
    float bs = saturate(0.035f / (fp + 1e-4f) - 0.5f) * (1.0f - snow * 0.8f);
    if (bs > 0.0f) {
        float e2 = 0.006f; float tb = turf * (1.0f - tread);
        float b0 = microBump(p.xz, tread, rock, scree, tb), bx = microBump(p.xz + float2(e2, 0), tread, rock, scree, tb), bz = microBump(p.xz + float2(0, e2), tread, rock, scree, tb);
        n = normalize(n + bs * float3(-(bx - b0), 0.0f, -(bz - b0)) / e2);
    }
    s.n = n;
}

static void shadeMesh(thread Ctx& c, float3 ro, float3 rd, thread MeshHit& mh, thread Surf& s) {
    const device Inst& in = c.insts[mh.inst]; const device Tri& tr = c.tris[mh.tri]; const device TriAttr& at = c.attrs[mh.tri];
    float u = mh.bary.x, v = mh.bary.y, w = 1.0f - u - v;
    float3 no = normalize(float3(at.n0) * w + float3(at.n1) * u + float3(at.n2) * v);
    float2 uv = float2(at.t0) * w + float2(at.t1) * u + float2(at.t2) * v;
    float3 ngo = cross(float3(tr.e1), float3(tr.e2));
    // object -> world for normals: transpose of the world->object 3x3
    float3 n = normalize(in.r0.xyz * no.x + in.r1.xyz * no.y + in.r2.xyz * no.z);
    float3 ng = normalize(in.r0.xyz * ngo.x + in.r1.xyz * ngo.y + in.r2.xyz * ngo.z);
    float3 o, d; instRay(c, in, ro, rd, o, d); float3 po = o + d * mh.t;     // object-space hit point
    s.p = ro + rd * mh.t; uint m = tr.mat & 0xffu; float rs = sh_u2f(in.seed);
    s.trans = 0.0f; s.transCol = float3(0.0f); s.spec = 0.03f; s.rough = 0.7f;
    float3 col = float3(0.3f);
    if (m == 0u || m == 3u) {            // bark: furrowed plates (uv: x around in metres, y along in metres)
        bool larch = (m == 3u); float k = larch ? 26.0f : 42.0f; uint sd = 80U + (in.seed & 7u);
        float e = 0.004f; float f0 = sh_fbm2(uv.x * k, uv.y * 4.5f, 3, sd), f1 = sh_fbm2((uv.x + e) * k, uv.y * 4.5f, 3, sd), f2 = sh_fbm2(uv.x * k, (uv.y + e * 4.0f) * 4.5f, 3, sd);
        float plate = smoothstep(0.38f, 0.62f, f0); float scale = sh_vn2(uv.x * 9.0f + rs * 7.0f, uv.y * 14.0f, 81U);
        float3 dark = larch ? float3(0.070f, 0.040f, 0.030f) : float3(0.055f, 0.042f, 0.036f), lightC = larch ? float3(0.27f, 0.155f, 0.105f) : float3(0.205f, 0.170f, 0.140f);
        col = mix(dark, lightC, plate) * (0.75f + 0.5f * scale);
        float gl = smoothstep(0.66f, 0.80f, sh_fbm2(uv.x * 6.0f + rs * 9.0f, uv.y * 2.2f, 3, 82U)); col = mix(col, float3(0.20f, 0.23f, 0.18f), gl * (larch ? 0.25f : 0.45f));
        float mossb = smoothstep(1.4f, 0.1f, po.y) * smoothstep(0.45f, 0.7f, sh_fbm2(uv.x * 5.0f, uv.y * 2.0f, 2, 83U)); col = mix(col, float3(0.06f, 0.10f, 0.025f), mossb * 0.8f);
        float3 tw = normalize(cross(n, float3(0.0f, 1.0f, 0.0f)) + float3(1e-4f, 0.0f, 0.0f)), bw = cross(n, tw);
        n = normalize(n - tw * ((f1 - f0) / e * 0.012f) - bw * ((f2 - f0) / (e * 4.0f) * 0.004f)); s.rough = 0.9f; s.spec = 0.02f;
    } else if (m == 1u) {                // spruce needles
        col = mix(float3(0.028f, 0.055f, 0.022f), float3(0.058f, 0.100f, 0.036f), saturate(fabs(uv.y - 0.5f) * 2.0f + 0.3f * sin(uv.x * 40.0f))) * (0.75f + 0.5f * rs);
        if (fabs(uv.y - 0.5f) * 2.0f * at.pad < 0.004f) col = float3(0.10f, 0.07f, 0.05f);
        s.trans = 0.25f; s.transCol = col * 1.2f; s.spec = 0.06f;
    } else if (m == 2u) {                // larch needles in October
        col = mix(float3(0.36f, 0.20f, 0.030f), float3(0.52f, 0.36f, 0.055f), sh_u2f(sh_hu(in.seed + uint(uv.x * 9.0f))));
        col *= 0.8f + 0.4f * rs;
        if (fabs(uv.y - 0.5f) * 2.0f * at.pad < 0.004f) col = float3(0.14f, 0.10f, 0.07f);
        s.trans = 0.45f; s.transCol = col * 1.3f; s.spec = 0.04f;
    } else if (m == 4u) {                // rock
        float rv = sh_fbm3(po.x * 2.3f, po.y * 2.3f, po.z * 2.3f, 4, 68U + (in.seed & 3u));
        col = mix(float3(0.20f, 0.195f, 0.19f), float3(0.33f, 0.30f, 0.26f), rv);
        float lich = smoothstep(0.60f, 0.70f, sh_fbm3(po.x * 9.0f, po.y * 9.0f, po.z * 9.0f, 3, 70U)); col = mix(col, float3(0.36f, 0.38f, 0.22f), lich * 0.45f);
        float lich2 = smoothstep(0.66f, 0.72f, sh_fbm3(po.x * 4.0f + 7.0f, po.y * 4.0f, po.z * 4.0f, 3, 74U)); col = mix(col, float3(0.05f, 0.05f, 0.05f), lich2 * 0.5f);
    } else if (m == 5u) {                // grass blade: uv.y 0 root .. 1 tip
        float3 base = mix(float3(0.10f, 0.13f, 0.04f), float3(0.30f, 0.24f, 0.09f), saturate(uv.y * 1.2f + (rs - 0.5f)));
        col = mix(base, float3(0.36f, 0.31f, 0.15f), smoothstep(0.6f, 1.0f, uv.y) * 0.7f);
        s.trans = 0.5f; s.transCol = col * 1.4f; s.spec = 0.05f;
    } else if (m == 6u) {                // dwarf shrub leaf (bilberry in autumn)
        col = mix(float3(0.28f, 0.035f, 0.025f), float3(0.36f, 0.16f, 0.03f), rs); s.trans = 0.5f; s.transCol = col * 1.5f;
    } else if (m == 7u || m == 9u) {     // weathered wood
        float gr = sh_fbm2(uv.x * 60.0f, uv.y * 2.0f, 3, 84U);
        col = mix(float3(0.16f, 0.13f, 0.10f), float3(0.34f, 0.30f, 0.25f), gr); if (m == 9u) col *= 0.6f;
    } else if (m == 8u) {                // fern
        col = mix(float3(0.065f, 0.105f, 0.035f), float3(0.20f, 0.16f, 0.05f), rs * rs); s.trans = 0.55f; s.transCol = col * 1.4f;
    }
    col *= in.tint.xyz;
    s.transCol *= in.tint.xyz;
    if (dot(ng, rd) > 0.0f) { ng = -ng; }
    if (dot(n, ng) < 0.0f) n = -n;
    s.n = n; s.ng = ng; s.albedo = col;
}

// ------------------------------------------------------------------ tracing
struct Hit { float t; int kind; MeshHit mh; };
static Hit traceClosest(thread Ctx& c, float3 ro, float3 rd, float tmax, int steps, bool primary, float tLod, thread Rng& rng) {
    Hit h; h.kind = 0; h.t = tmax; float tt;
    if (marchTerrain(c, ro, rd, 0.02f, tmax, steps, primary, tLod, tt)) { h.kind = 1; h.t = tt; }
    if (traceMesh(c, ro, rd, h.t, false, rng, h.mh)) { h.kind = 2; h.t = h.mh.t; }
    return h;
}
static float sunVisibility(thread Ctx& c, float3 p, float3 ng, float3 l, thread Rng& rng, bool cheap, float tLod) {
    float3 o = p + ng * 0.01f; MeshHit mh; float tt;
    if (traceMesh(c, o, l, cheap ? 120.0f : 4000.0f, true, rng, mh)) return 0.0f;
    if (marchTerrain(c, o, l, 0.05f, cheap ? 4000.0f : 30000.0f, cheap ? 36 : 90, false, tLod, tt)) return 0.0f;
    return 1.0f;
}
static float3 sampleSunDir(thread Ctx& c, float u1, float u2) {
    float3 s = c.U->sunDir.xyz; float r = c.U->sunDir.w * sqrt(u1), a = 2.0f * PI * u2;
    float3 t = normalize(cross(s, float3(0.0f, 1.0f, 0.1f))), b = cross(s, t); return normalize(s + (t * cos(a) + b * sin(a)) * r);
}
static float3 cosineDir(float3 n, float u1, float u2) {
    float r = sqrt(u1), a = 2.0f * PI * u2; float3 t = normalize(cross(n, fabs(n.y) < 0.9f ? float3(0, 1, 0) : float3(1, 0, 0))), b = cross(n, t);
    return normalize(t * (r * cos(a)) + b * (r * sin(a)) + n * sqrt(max(0.0f, 1.0f - u1)));
}
// radiance arriving from direction d for rays that left the scene (bounce rays)
static float3 envRadiance(thread Ctx& c, float3 d, float y) {
    if (d.y >= 0.0f) return skyLookup(c, d);
    // below the horizon: the cloud sea / hazy lowlands, lit by sun and sky
    float3 sun = sunAt(c, ECO_CLOUDTOP - c.U->origin.y) * max(c.U->sunDir.y, 0.0f); float3 low = (sun + c.U->skyAmb.xyz) * (0.55f / PI);
    return mix(skyLookup(c, float3(d.x, 0.0f, d.z)), low, smoothstep(0.0f, 0.12f, -d.y));
}
static float3 directLight(thread Ctx& c, thread Surf& s, float3 wo, float3 l, float3 E, float vis) {
    float ndl = dot(s.n, l); float3 f = float3(0.0f);
    if (ndl > 0.0f) {
        f = s.albedo * (ndl / PI);
        float3 h = normalize(l + wo); float nh = max(dot(s.n, h), 0.0f); float a = s.rough * s.rough; float a2 = a * a;
        float dd = nh * nh * (a2 - 1.0f) + 1.0f; float D = a2 / (PI * dd * dd); float F = s.spec + (1.0f - s.spec) * pow(1.0f - max(dot(h, wo), 0.0f), 5.0f);
        f += float3(D * F * 0.25f * ndl / max(0.15f, max(dot(s.n, wo), ndl)));
    } else if (s.trans > 0.0f) f = s.transCol * (s.trans * (-ndl) / PI);
    return f * E * vis;
}


// single scattering in the mist: one stratified point per camera sample, shadow ray toward the sun
static float3 mistScatter(thread Ctx& c, float3 ro, float3 rd, float tMax, uint k, uint pix, thread Rng& rng) {
    const device Uniforms& U = *c.U; float tm = min(tMax, 260.0f);
    // sample distance ~ exponential with the local density so near shafts get the samples
    float d0 = max(U.mist.x * 0.001f * exp(-(ro.y + U.origin.y - U.mist.y) / U.mist.z), 2e-4f) * 1.5f + 0.006f; float xi = fract((float(k) + rnd(rng)) / float(max(U.sppTotal, 1u)) + sh_u2f(sh_hu(pix + 77u)));
    float norm = 1.0f - exp(-d0 * tm); float cs = dot(rd, U.sunDir.xyz);
    float ph = 0.50f * phaseHG(cs, 0.70f) + 0.35f * phaseHG(cs, 0.25f) + 0.15f * phaseHG(cs, -0.3f); float3 acc = float3(0.0f);
    int nm = U.cloud.w >= 1.5f ? 2 : 1;
    for (int j = 0; j < nm; j++) {
        float xj = (float(j) + xi) / float(nm); float ts = -log(1.0f - xj * norm) / d0; float pdf = d0 * exp(-d0 * ts) / norm;
        float3 p = ro + rd * ts; float den = mistDensity(c, p); if (den <= 0.0f) continue;
        float Tv = exp(-mistTau(c, ro, rd, ts));
        float3 l = sampleSunDir(c, rnd(rng), rnd(rng)); float vis = sunVisibility(c, p, float3(0.0f), l, rng, true, ts);
        float3 Es = sunAt(c, p.y) * exp(-mistTauToSun(c, p));
        float3 src = Es * (vis * ph) + U.skyAmb.xyz * (U.skyAmb.w * 0.6f / (4.0f * PI));
        acc += src * (den * Tv / pdf);
    }
    return acc / float(nm);
}

static float3 radiance(thread Ctx& c, float3 ro, float3 rd, uint k, uint pix, thread Rng& rng) {
    const device Uniforms& U = *c.U;
    Hit h = traceClosest(c, ro, rd, 120000.0f, 360, true, 0.0f, rng);
    float3 L = float3(0.0f); float tEnd = h.t;
    bool useMist = U.mist.x > 0.0f;
    if (h.kind == 0) {
        L = cirrus(c, ro, rd, skyLookup(c, rd));
        float cs = dot(rd, U.sunDir.xyz); float cr = cos(U.sunDir.w);
        if (cs > cr && rd.y > -0.02f) {   // solar disc, limb darkened, radiance capped (bloom happens in post)
            float mu = sqrt(saturate((cs - cr) / (1.0f - cr))); float3 Ts = atmoT(c, U.shot.w);
            L += U.sunE.xyz * Ts * (0.4f + 0.6f * mu) * 900.0f;
        }
        if (rd.y < 0.02f) { float3 Tc, Sc; float tm; cloudSea(c, ro, rd, 110000.0f, rseq(k, 7u, pix), Tc, Sc, tm);
            if (Tc.x < 1.0f) { float3 Ta, Sa; aerial(c, ro, rd, tm, Ta, Sa); L = mix(Sc * Ta + Sa, L, Tc.x); } }
        if (useMist) { float tm = min(1500.0f, rd.y > 0.01f ? 2500.0f : 1500.0f); float tau = mistTau(c, ro, rd, tm); L *= exp(-tau); L += mistScatter(c, ro, rd, tm, k, pix, rng); }
        return L;
    }
    Surf s;
    if (h.kind == 1) shadeTerrain(c, ro + rd * h.t, h.t, s); else shadeMesh(c, ro, rd, h.mh, s);
    if (U.misc.y == 1.0f) return s.albedo * (0.3f + 0.7f * max(0.0f, dot(s.n, U.sunDir.xyz))) * 6.0f;
    float3 wo = -rd; if (dot(s.n, wo) < 0.0f && s.trans == 0.0f) s.n = normalize(s.n - 1.02f * dot(s.n, wo) * wo);
    float3 l = sampleSunDir(c, rseq(k, 3u, pix), rseq(k, 4u, pix));
    float3 E = sunAt(c, s.p.y); if (useMist) E *= exp(-mistTauToSun(c, s.p));
    float ndl = dot(s.n, l);
    if (ndl > 0.0f || s.trans > 0.0f) {
        float3 off = (ndl > 0.0f) ? s.ng : -s.ng; if (dot(s.ng, l) * ndl < 0.0f) off = s.ng * sign(dot(s.ng, l));
        float vis = sunVisibility(c, s.p, off, l, rng, false, h.t);
        if (vis > 0.0f) L += directLight(c, s, wo, l, E, vis);
    }
    // one diffuse bounce
    if (U.misc.y != 2.0f) {
        float3 nb = s.n; float3 thr = s.albedo;
        if (s.trans > 0.0f && rnd(rng) < 0.35f) { nb = -s.n; thr = s.transCol * s.trans / 0.35f; } else if (s.trans > 0.0f) thr = s.albedo / 0.65f;
        float3 d = cosineDir(nb, rseq(k, 5u, pix), rseq(k, 6u, pix)); if (dot(d, s.ng) * dot(nb, s.ng) < 0.0f) d = d - 2.0f * dot(d, s.ng) * s.ng;
        float3 o = s.p + s.ng * (0.01f * sign(dot(d, s.ng)));
        Hit h2 = traceClosest(c, o, d, 400.0f, 48, false, h.t, rng);
        float3 Li;
        if (h2.kind == 0) Li = envRadiance(c, d, s.p.y);
        else if (U.misc.y == 3.0f) Li = float3(0.0f);
        else {
            Surf s2; if (h2.kind == 1) shadeTerrain(c, o + d * h2.t, h.t + h2.t, s2); else shadeMesh(c, o, d, h2.mh, s2);
            float3 l2 = U.sunDir.xyz; float ndl2 = dot(s2.n, l2); float3 Lo = float3(0.0f);
            if (U.misc.y != 4.0f && (ndl2 > 0.0f || s2.trans > 0.0f)) { float3 off2 = s2.ng * sign(dot(s2.ng, l2)); float vis2 = sunVisibility(c, s2.p, off2, l2, rng, true, h.t + h2.t); if (vis2 > 0.0f) Lo += directLight(c, s2, -d, l2, sunAt(c, s2.p.y), vis2); }
            Lo += s2.albedo * U.skyAmb.xyz * (U.skyAmb.w * (0.5f + 0.5f * s2.n.y) / PI);
            Li = Lo;
        }
        L += thr * Li;
    }
    if (U.misc.z > 0.0f && h.kind == 1 && rd.y < -0.01f) {
        float3 pg = ro + rd * h.t; float wl = waterLevel(c, pg.xz);
        if (wl > pg.y + 0.004f) {
            float tw = h.t; for (int i = 0; i < 4; i++) { float3 pw = ro + rd * tw; tw += (waterLevel(c, pw.xz) - pw.y) / rd.y; }
            tw = clamp(tw, 0.05f, h.t); float3 pw = ro + rd * tw; Water w; waterSurface(c, pw.xz, pg.y, w); float path = h.t - tw;
            float3 irr = L / max(s.albedo, float3(0.02f));                       // light arriving at the bed
            float3 Lbed = L * float3(0.50f, 0.58f, 0.56f) * exp(-float3(1.6f, 0.55f, 0.45f) * path) * (0.8f + 0.5f * w.foam);
            float cosv = saturate(dot(-rd, w.n)); float F = 0.02f + 0.98f * pow(1.0f - cosv, 5.0f);
            float3 rdir = reflect(rd, w.n); if (rdir.y < 0.02f) rdir = normalize(float3(rdir.x, 0.02f, rdir.z));
            Hit hr = traceClosest(c, pw + float3(0.0f, 0.01f, 0.0f), rdir, 300.0f, 40, false, tw, rng); float3 Lr;
            if (hr.kind == 0) Lr = envRadiance(c, rdir, pw.y);
            else { Surf s3; if (hr.kind == 1) shadeTerrain(c, pw + rdir * hr.t, tw + hr.t, s3); else shadeMesh(c, pw + float3(0.0f, 0.01f, 0.0f), rdir, hr.mh, s3);
                   float3 l3 = U.sunDir.xyz; float nd3 = dot(s3.n, l3); Lr = s3.albedo * U.skyAmb.xyz * (U.skyAmb.w * (0.5f + 0.5f * s3.n.y) / PI);
                   if (nd3 > 0.0f || s3.trans > 0.0f) { float v3 = sunVisibility(c, s3.p, s3.ng * sign(dot(s3.ng, l3)), l3, rng, true, tw + hr.t); if (v3 > 0.0f) Lr += directLight(c, s3, -rdir, l3, sunAt(c, s3.p.y), v3); } }
            float3 Lw = mix(Lbed, Lr, F);
            { float3 hv = normalize(U.sunDir.xyz - rd); float nh = saturate(dot(w.n, hv)); float gl = pow(nh, 900.0f) * 60.0f;               // sun glitter
              if (gl > 0.01f) { float vs = sunVisibility(c, pw, float3(0.0f, 1.0f, 0.0f), U.sunDir.xyz, rng, true, tw); Lw += sunAt(c, pw.y) * gl * vs * 0.05f; } }
            L = mix(Lw, irr * 0.60f, w.foam); tEnd = tw;
        }
    }
    // cloud deck, then atmosphere, between eye and surface
    float tA = tEnd;
    if (ro.y + U.origin.y + rd.y * tEnd < U.cloud.x + 150.0f) { float3 Tc, Sc; float tm; cloudSea(c, ro, rd, tEnd, rseq(k, 7u, pix), Tc, Sc, tm); L = L * Tc + Sc; tA = tm; }
    float3 T, S; aerial(c, ro, rd, tA, T, S);
    L = L * T + S;
    if (useMist) { float tm = min(tEnd, 1500.0f); L *= exp(-mistTau(c, ro, rd, tm)); L += mistScatter(c, ro, rd, tm, k, pix, rng); }
    return L;
}

kernel void render(texture2d<float> h0 [[texture(0)]], texture2d<float> h1 [[texture(1)]], texture2d<float> h2 [[texture(2)]], texture2d<float> h3 [[texture(3)]],
                   texture2d<float> m0 [[texture(4)]], texture2d<float> m1 [[texture(5)]], texture2d<float> g2 [[texture(6)]], texture2d<float> sky [[texture(7)]], texture2d<float> wat [[texture(8)]],
                   const device Uniforms& U [[buffer(0)]], device float4* accum [[buffer(1)]], const device float4* atmo [[buffer(2)]],
                   const device BNode* tlas [[buffer(3)]], const device BNode* nodes [[buffer(4)]], const device Tri* tris [[buffer(5)]],
                   const device TriAttr* attrs [[buffer(6)]], const device Inst* insts [[buffer(7)]],
                   uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= U.width || gid.y >= U.height) return;
    Ctx c; c.h0 = h0; c.h1 = h1; c.h2 = h2; c.h3 = h3; c.m0 = m0; c.m1 = m1; c.g2 = g2; c.sky = sky; c.wat = wat; c.U = &U; c.atmo = atmo;
    c.tlas = tlas; c.nodes = nodes; c.tris = tris; c.attrs = attrs; c.insts = insts;
    uint pix = gid.y * U.width + gid.x; uint pixf = sh_hu(pix + U.frame * 7919u * 104729u + 1u);
    float4 acc = (U.spp0 == 0u) ? float4(0.0f) : accum[pix];
    for (uint i = 0; i < U.sppN; i++) {
        uint k = U.spp0 + i; Rng rng; rng.s = sh_hu(pixf ^ (k * 0x9E3779B9u));
        float2 jit = float2(rseq(k, 0u, pixf), rseq(k, 1u, pixf)); float tm = rseq(k, 2u, pixf);
        // tent filter, radius 1 px
        jit = float2(jit.x < 0.5f ? sqrt(2.0f * jit.x) - 1.0f : 1.0f - sqrt(2.0f - 2.0f * jit.x), jit.y < 0.5f ? sqrt(2.0f * jit.y) - 1.0f : 1.0f - sqrt(2.0f - 2.0f * jit.y));
        float2 ndc = (float2(gid) + 0.5f + jit * 0.85f) / float2(U.width, U.height) * 2.0f - 1.0f; ndc.y = -ndc.y;
        float2 q = float2(ndc.x, ndc.y * U.lens.y); float r2 = dot(q, q); q *= 1.0f + U.lens.z * r2 + 0.35f * U.lens.z * r2 * r2;
        float3 R = mix(U.camR0.xyz, U.camR1.xyz, tm), Up = mix(U.camU0.xyz, U.camU1.xyz, tm), F = mix(U.camF0.xyz, U.camF1.xyz, tm);
        float3 ro = mix(U.camPos0.xyz, U.camPos1.xyz, tm); float3 rd = normalize(F + (R * q.x + Up * q.y) * U.lens.x);
        float3 L = radiance(c, ro, rd, k, pixf, rng);
        if (!(L.x >= 0.0f && L.y >= 0.0f && L.z >= 0.0f) || isinf(L.x) || isinf(L.y) || isinf(L.z)) L = float3(0.0f);
        { float cl = U.misc.w > 0.0f ? U.misc.w : 4000.0f; float mx = max(L.x, max(L.y, L.z)); if (mx > cl) L *= cl / mx; }
        acc += float4(L, 1.0f);
    }
    accum[pix] = acc;
}
