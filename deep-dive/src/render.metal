// render.metal — the underwater ray marcher (one compute kernel). shared.h is prepended by the host.
// Everything is evaluated per sample: geometry by sphere tracing signed distance fields, light in the water by
// marching the view ray through the caustic volume that the host traced through this frame's wave surface.

struct Uniforms {
    float4 camPos;   // xyz, time
    float4 camR;     // xyz, lens T
    float4 camU;     // xyz, lens A (rad)
    float4 camF;     // xyz, h/w
    float4 res;      // w, h, pass index, spp per pass
    float4 sunW;     // unit vector toward the sun under water (mean refracted), w = total spp
    float4 sunA;     // unit vector toward the sun in air
    float4 caus;     // tile L, shear x, shear z, K
    float4 torchP;   // xyz, intensity
    float4 torchD;   // xyz, cos(outer)
    float4 cnt;      // nInst, nFish, nBubbles, frame
    float4 gridO;    // instance grid origin, cell size
    float4 gridN;    // nx, ny, nz, screen tile size (px)
    float4 surge;    // surgeX(t), surgeZ(t), tilesX, tilesY
    float4 dbg;      // x: debug view, y: quality (0 preview, 1 final)
    float4 mantaP;   // xyz, scale
    float4 mantaF;   // forward, flap phase
    float4 mantaU;   // up, flap amp
    float4 fade;     // y: duration of one sub-frame (s)
    float4 camVel;   // camera velocity (m/s): each sample sits at its own instant inside the sub-frame
};
struct Inst { float4 pos; float4 ax; float4 ay; float4 az; float4 bnd; };   // pos.w scale, ax.w type, ay.w seed, az.w hue
struct Cell { uint off; uint cnt; float empty; float pad; };
struct Fish { float4 pos; float4 fwd; float4 up; float4 par; float4 vel; };             // pos.w length, fwd.w swim phase, up.w species, par: bend, turn, seed, flash
struct Bubble { float4 pos; float4 par; float4 vel; };                                   // pos.w radius, par: squash, tiltx, tiltz, seed

constexpr sampler sLin(filter::linear, address::repeat);
constexpr sampler sNear(filter::nearest, address::repeat);

#define SCN_D texture3d<float> nzT, texture3d<float> cauT, texture3d<float> cauLo, texture2d<float> surfT, constant Uniforms& U, const device Inst* insts, const device Cell* cells, const device uint* cellIdx
#define SCN nzT, cauT, cauLo, surfT, U, insts, cells, cellIdx
#define DYN_D const device Fish* fishes, const device Bubble* bubs, const device uint* tiles, const device uint* tileIdx
#define DYN fishes, bubs, tiles, tileIdx

constant float PI = 3.14159265f;
constant float3 SIG_A = float3(0.30f, 0.055f, 0.022f);
constant float3 SIG_S = float3(0.027f, 0.035f, 0.052f);
constant float3 SIG_T = float3(0.330f, 0.093f, 0.072f);
constant float3 KD = float3(0.318f, 0.0616f, 0.0281f);     // diffuse attenuation
constant float3 KS = float3(0.3066f, 0.0634f, 0.033f);     // attenuation of the (forward-scattered) sun beam
constant float3 SUN_E = float3(3.1416f, 2.98f, 2.70f);     // sun irradiance just under the surface (perpendicular)
constant float3 SKY_E = float3(0.50f, 0.78f, 1.10f);       // diffuse irradiance just under the surface
constant float3 AMB = float3(0.0110f, 0.0152f, 0.0236f);   // source of multiply scattered light (per metre, per sr)

// ------------------------------------------------------------------ small helpers
static inline uint pcg(uint v) { uint s = v * 747796405u + 2891336453u; uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
static inline float rnd(uint& s) { s = pcg(s); return float(s >> 8) * (1.0f / 16777216.0f); }
static inline float hash1(float n) { return fract(sin(n * 127.1f + 11.7f) * 43758.5453f); }
static inline float3 hash3i(int3 c) {
    uint h = pcg(uint(c.x) * 73856093u ^ pcg(uint(c.y) * 19349663u ^ pcg(uint(c.z) * 83492791u)));
    uint h2 = pcg(h), h3 = pcg(h2);
    return float3(float(h >> 8), float(h2 >> 8), float(h3 >> 8)) * (1.0f / 16777216.0f);
}
static inline float sat(float x) { return clamp(x, 0.0f, 1.0f); }
static inline float2 rot2(float2 p, float a) { float c = cos(a), s = sin(a); return float2(c * p.x - s * p.y, s * p.x + c * p.y); }
static inline float smin(float a, float b, float k) { float h = sat(0.5f + 0.5f * (b - a) / k); return mix(b, a, h) - k * h * (1.0f - h); }
static inline float4 NZ(texture3d<float> nzT, float3 p) { return nzT.sample(sLin, p * (1.0f / 16.0f), level(0)); }
static inline float hgPhase(float c, float g) { float g2 = g * g; return (1.0f - g2) / (4.0f * PI * pow(max(1e-4f, 1.0f + g2 - 2.0f * g * c), 1.5f)); }
static inline float waterPhase(float c) { return 0.62f * hgPhase(c, 0.86f) + 0.38f * hgPhase(c, 0.10f); }

// ------------------------------------------------------------------ caustics
static inline float causticHi(float3 p, texture3d<float> cauT, constant Uniforms& U) {
    float D = max(-p.y, 0.0f);
    float2 uv = (p.xz + U.caus.yz * D) / U.caus.x;
    float w = (causSlice(D) + 0.5f) / float(CAU_K);
    return cauT.sample(sLin, float3(uv, w), level(0)).x;
}
static inline float causticLo(float3 p, texture3d<float> cauLo, constant Uniforms& U) {
    float D = max(-p.y, 0.0f);
    float2 uv = (p.xz + U.caus.yz * D) / U.caus.x;
    float w = (causSlice(D) + 0.5f) / float(CAU_K);
    return cauLo.sample(sLin, float3(uv, w), level(0)).x;
}
// sun irradiance (perpendicular to the beam) and diffuse irradiance at depth D
static inline float3 sunBeam(float D, float mu) { return SUN_E * exp(-KS * (D / mu)); }
static inline float3 diffuseE(float D, float mu) {
    float3 scat = SUN_E * mu * max(exp(-KD * D) - exp(-KS * (D / mu)), 0.0f) * 0.75f;
    return SKY_E * exp(-KD * D) + scat;
}
// radiance of the multiply-scattered light field seen in direction rd from depth D (closed form, infinite path)
static inline float3 fogClosed(float D, float3 rd, constant Uniforms& U) {
    float g = 1.0f / (1.0f - 0.62f * rd.y);
    float dn = -rd.y;
    float3 amb = AMB * g * exp(-KD * D) / max(SIG_T + KD * dn, 0.02f);
    float mu = U.sunW.y;
    float3 sun = SIG_S * waterPhase(dot(rd, U.sunW.xyz)) * sunBeam(D, mu) / max(SIG_T + KS * (dn / mu), 0.02f);
    return amb + sun;
}

// ------------------------------------------------------------------ sky seen through the surface
static float3 skyRad(float3 d, float3 sunA) {
    float mu = clamp(dot(d, sunA), -1.0f, 1.0f);
    float h = max(d.y, 0.0f);
    float3 c = mix(float3(0.50f, 0.66f, 0.88f), float3(0.07f, 0.19f, 0.52f), pow(h, 0.5f));
    float ang = acos(mu);
    c += float3(1.0f, 0.94f, 0.82f) * (260.0f * exp(-ang * ang / (0.018f * 0.018f)) + 5.0f * exp(-ang / 0.05f) + 0.22f * exp(-ang / 0.28f));
    return c;
}
// ------------------------------------------------------------------ coral / sponge instances (local space, base at origin, +y up)
static float sdTable(float3 p, float seed) {
    float r = length(p.xz); float th = atan2(p.z, p.x);
    float R = 1.0f + 0.10f * sin(3.0f * th + seed * 6.28f) + 0.07f * sin(7.0f * th + seed * 17.0f) + 0.03f * sin(13.0f * th + seed * 5.0f);
    float yTop = 0.40f + 0.07f * r * r + 0.025f * sin(5.0f * p.x + seed * 9.0f) * sin(5.0f * p.z) + 0.006f * sin(th * 31.0f + 3.0f * sin(r * 9.0f + seed * 5.0f)) * sat(r * 2.0f) + 0.005f * sin(r * 37.0f + seed * 11.0f);
    float thick = mix(0.065f, 0.022f, sat(r / R));
    float2 w = float2(r - R, abs(p.y - yTop) - thick);
    float dp = min(max(w.x, w.y), 0.0f) + length(max(w, 0.0f)) - 0.012f;
    float hy = sat(p.y / 0.40f);
    float rs = mix(0.11f, 0.42f, hy * hy);
    float ds = max(r - rs, max(-p.y - 0.15f, p.y - yTop));
    float d = smin(dp, ds, 0.07f);
    if (d < 0.08f) { float idd; float w = worleyS(p.x * 17.0f + seed * 31.0f, p.y * 17.0f, p.z * 17.0f, 61u, idd); float k = 1.0f - min(w * 1.35f, 1.0f); d -= 0.016f * k * k - 0.004f; }
    return d * 0.75f;
}
static float sdBush(float3 p, float seed) {
    float d = 1e9f; float L = 0.26f, w = 0.050f;
    for (int i = 0; i < 5; i++) {
        float h = clamp(p.y / L, 0.0f, 1.0f);
        d = min(d, length(p - float3(0, L * h, 0)) - w * (1.0f - 0.2f * h));
        p.y -= L;
        p.xz = rot2(p.xz, 0.9f + seed * 3.0f + float(i) * 0.7f);
        p.xz = abs(p.xz);
        float a = 0.36f + 0.10f * float(i & 1);
        p.xy = rot2(p.xy, a); p.zy = rot2(p.zy, a);
        L *= 0.74f; w *= 0.76f;
    }
    float h = clamp(p.y / L, 0.0f, 1.0f);
    d = min(d, length(p - float3(0, L * h, 0)) - w);
    return d * 0.85f;
}
static float sdFan(float3 p, float seed, float3 sway) {
    float h = clamp(p.y, 0.0f, 1.3f);
    p.xz -= sway.xz * h * h * 1.1f;
    p.z -= 0.10f * sin(p.y * 2.3f + seed * 6.0f) * h;
    float2 q = p.xy;
    q.x += 0.04f * sin(q.y * 8.0f + seed * 7.0f) * q.y + 0.012f * sin(q.y * 31.0f + seed * 3.0f);
    float d2 = 1e9f; float L = 0.27f, w = 0.020f;
    for (int i = 0; i < 8; i++) {
        float hh = clamp(q.y / L, 0.0f, 1.0f);
        d2 = min(d2, length(q - float2(0, L * hh)) - w);
        q.y -= L; q.x = abs(q.x);
        float a = (i == 0) ? 0.55f : 0.34f + 0.13f * float(i & 1);
        float c = cos(a), s = sin(a);
        q = float2(c * q.x - s * q.y, s * q.x + c * q.y);
        L *= 0.80f; w *= 0.79f;
    }
    float hh = clamp(q.y / L, 0.0f, 1.0f);
    d2 = min(d2, length(q - float2(0, L * hh)) - w);
    const float wr = 0.004f;
    return (length(float2(max(d2 + wr, 0.0f), p.z)) - wr + min(d2 + wr, 0.0f)) * 0.75f;
}
static float sdWhip(float3 p, float seed, float3 sway) {
    float d = 1e9f;
    for (int i = 0; i < 4; i++) {
        float fi = float(i);
        float a = seed * 6.28f + fi * 1.9f; float len = 1.0f + 0.6f * hash1(seed * 13.0f + fi);
        float2 dir = float2(cos(a), sin(a));
        float hy = clamp(p.y, 0.0f, len);
        float u = hy / len;
        float2 c = dir * (0.05f + 0.22f * u + 0.25f * u * u * sin(seed * 9.0f + fi * 2.0f)) + sway.xz * u * u * 1.3f;
        float rr = mix(0.013f, 0.006f, u);
        d = min(d, length(float3(p.xz - c, p.y - hy)) - rr);
    }
    return d * 0.7f;
}
static float sdBarrel(float3 p, float seed) {
    float r = length(p.xz); float th = atan2(p.z, p.x);
    float y = clamp(p.y, 0.0f, 1.0f);
    float prof = 0.26f + 0.24f * sin(y * 2.4f + 0.25f);
    prof *= 1.0f + 0.06f * sin(th * 13.0f + seed * 6.0f + 2.0f * sin(y * 4.0f + seed)) + 0.03f * sin(th * 29.0f + y * 9.0f);
    float dOut = max(r - prof, max(p.y - 1.0f, -p.y - 0.1f));
    float dIn = max(r - prof * 0.66f, 0.3f - p.y);
    return max(dOut, -dIn) * 0.65f;
}
static float sdDome(float3 p, float seed, float3 pw, texture3d<float> nzT) {
    float3 q = p - float3(0, 0.12f, 0);
    q.y *= 1.2f;
    float lob = 0.06f * sin(4.3f * q.x + seed * 5.0f) * sin(4.7f * q.z + seed * 3.0f);
    float d = length(q) - 0.78f - lob;
    if (d < 0.25f) {
        // knobbly lobes like a Porites head: Worley bumps in world space
        float idd; float3 q1 = pw * 2.3f + seed * 9.0f, q2 = pw * 6.5f + seed * 3.0f;
        float w1 = worleyS(q1.x, q1.y, q1.z, 41u, idd);
        float w2 = worleyS(q2.x, q2.y, q2.z, 43u, idd);
        float k1 = 1.0f - min(w1 * 1.3f, 1.0f), k2 = 1.0f - min(w2 * 1.3f, 1.0f);
        d -= 0.16f * k1 * k1 + 0.035f * k2 * k2 - 0.05f;
    }
    return d * 0.6f;
}
static float instSDFb(float3 p, Inst I, constant Uniforms& U, bool& real, texture3d<float> nzT) {
    float db = length(p - I.bnd.xyz) - I.bnd.w;
    real = db <= 0.2f;
    if (U.dbg.z > 0.5f && int(I.ax.w) == int(U.dbg.z) - 1) { real = false; return 1e3f; }
    if (!real) return db;
    float3 dl = p - I.pos.xyz; float sc = I.pos.w;
    float3 q = float3(dot(dl, I.ax.xyz), dot(dl, I.ay.xyz), dot(dl, I.az.xyz)) / sc;
    int type = int(I.ax.w); float seed = I.ay.w;
    float3 sw = float3(U.surge.x, 0.0f, U.surge.y) * surgeAmp(I.pos.y);
    float3 sl = float3(dot(sw, I.ax.xyz), dot(sw, I.ay.xyz), dot(sw, I.az.xyz)) / sc;
    float d;
    if (type == 0) d = sdTable(q, seed);
    else if (type == 1) d = sdBush(q, seed);
    else if (type == 2) d = sdFan(q, seed, sl);
    else if (type == 3) d = sdWhip(q, seed, sl);
    else if (type == 4) d = sdBarrel(q, seed);
    else d = sdDome(q, seed, p, nzT);
    return max(d * sc, db);
}
static float instSDF(float3 p, Inst I, constant Uniforms& U, texture3d<float> nzT) { bool r; return instSDFb(p, I, U, r, nzT); }

// ------------------------------------------------------------------ scene distance
// Lower bound of the distance to terrain + instances. The instance grid makes the step safe: inside a cell only the
// instances registered there can be hit, and an empty cell knows how far the nearest instance is.
static float sceneSDF(float3 p, float3 rd, float3 ird, int& hitId, float& dHit, float& dPen, SCN_D) {
    float d = terrainSDF(p.x, p.y, p.z, nzT);
    hitId = -1; dHit = d; dPen = d;
    float cs = U.gridO.w;
    float3 g = (p - U.gridO.xyz) / cs;
    float3 dims = U.gridN.xyz;
    if (all(g >= 0.0f) && all(g < dims)) {
        int3 ci = int3(floor(g));
        uint cidx = uint(ci.x) + uint(dims.x) * (uint(ci.y) + uint(dims.y) * uint(ci.z));
        Cell ce = cells[cidx];
        float3 tEx = ((float3(ci) + step(0.0f, rd)) * cs + U.gridO.xyz - p) * ird;
        float ex = min(min(tEx.x, tEx.y), tEx.z) + 0.003f;
        if (ce.cnt == 0u) {
            d = min(d, max(ce.empty, ex));
        } else {
            float di = ex;
            for (uint k = 0u; k < ce.cnt; k++) {
                uint id = cellIdx[ce.off + k];
                bool real;
                float dd = instSDFb(p, insts[id], U, real, nzT);
                if (dd < dHit) { dHit = dd; hitId = int(id); }
                float ty = insts[id].ax.w;
                if (real) dPen = min(dPen, (ty > 1.5f && ty < 3.5f) ? dd * 4.0f + 0.004f : dd);
                di = min(di, dd);
            }
            d = min(d, di);
        }
    } else {
        float3 lo = U.gridO.xyz, hi = lo + dims * cs;
        float3 q = max(max(lo - p, p - hi), 0.0f);
        d = min(d, length(q) + 0.01f);
    }
    return d;
}
// exact-ish distance for normals / AO (no ray, no step clamp)
static float sceneSDFx(float3 p, SCN_D) {
    float d = terrainSDF(p.x, p.y, p.z, nzT);
    float cs = U.gridO.w;
    float3 g = (p - U.gridO.xyz) / cs;
    float3 dims = U.gridN.xyz;
    if (all(g >= 0.0f) && all(g < dims)) {
        int3 ci = int3(floor(g));
        uint cidx = uint(ci.x) + uint(dims.x) * (uint(ci.y) + uint(dims.y) * uint(ci.z));
        Cell ce = cells[cidx];
        for (uint k = 0u; k < ce.cnt; k++) d = min(d, instSDF(p, insts[cellIdx[ce.off + k]], U, nzT));
    }
    return d;
}
static float softShadow(float3 ro, float3 rd, float kPen, int steps, SCN_D) {
    float res = 1.0f; float t = 0.04f; float3 ird = 1.0f / rd; int id;
    for (int i = 0; i < steps; i++) {
        float3 p = ro + rd * t;
        if (p.y > -0.2f) break;
        float dH, dP;
        float d = sceneSDF(p, rd, ird, id, dH, dP, SCN);
        res = min(res, kPen * dP / t);
        if (res < 0.01f) break;
        t += clamp(d, 0.03f, 1.5f);
    }
    return sat(res);
}

// ------------------------------------------------------------------ materials
static float3 hueShift(float3 c, float h) {   // cheap palette variation
    return c * (0.82f + 0.36f * float3(hash1(h * 3.1f), hash1(h * 5.3f + 1.0f), hash1(h * 7.7f + 2.0f)));
}
static void terrainMaterial(float3 p, float3& n, float t, float3& alb, SCN_D) {
    float ch = sstepf(0.45f, 0.95f, chanMask(p.x, p.z));
    ch = sat(ch * 2.6f - 0.12f + 0.35f * NZ(nzT, p * 1.3f).x);
    float flat = sat((n.y - 0.72f) * 5.0f);
    float id, id3, id4;
    float w2b, w3b;
    float w2 = worleyS2(p.x * 1.05f + 5.0f, p.y * 1.05f, p.z * 1.05f + 2.0f, 5u, id, w2b);
    float w3 = worleyS2(p.x * 2.9f, p.y * 2.9f + 7.0f, p.z * 2.9f, 9u, id3, w3b);
    float w4 = worleyS(p.x * 8.7f, p.y * 8.7f, p.z * 8.7f, 31u, id4);
    float4 f1 = NZ(nzT, p * 6.0f + 1.7f);
    float4 f2 = NZ(nzT, p * 23.0f);
    float4 f3 = NZ(nzT, p * 61.0f + 3.3f);
    // sand settles in the flat pockets between coral heads
    float sand = max(ch, flat * sat((w2 - 0.66f) * 6.0f) * sat((w3 - 0.42f) * 5.0f) * 0.9f);
    // --- living crust: each cellular lump is one colony with its own colour; small knobs vary it
    float pick = fract(id * 7.0f + floor(id3 * 3.0f) * 0.13f);
    float3 c;
    if (pick < 0.18f) c = float3(0.46f, 0.35f, 0.22f);          // tan
    else if (pick < 0.32f) c = float3(0.36f, 0.33f, 0.17f);     // olive brown
    else if (pick < 0.46f) c = float3(0.55f, 0.42f, 0.15f);     // mustard
    else if (pick < 0.58f) c = float3(0.42f, 0.31f, 0.42f);     // lavender
    else if (pick < 0.70f) c = float3(0.50f, 0.24f, 0.14f);     // rust
    else if (pick < 0.82f) c = float3(0.58f, 0.33f, 0.36f);     // coralline pink
    else if (pick < 0.92f) c = float3(0.60f, 0.56f, 0.48f);     // pale dead coral / rubble
    else c = float3(0.22f, 0.25f, 0.13f);                        // algal turf
    c = hueShift(c, id + floor(id3 * 4.0f) * 0.31f);
    c = mix(c, float3(0.40f, 0.36f, 0.27f) * (0.8f + 0.4f * id3), sat(chanMask(p.x, p.z) * 4.0f - 0.8f));
    c *= 0.86f + 0.28f * id4;
    float crev = sat(max(w2, w3 * 0.92f) * 1.5f - 0.40f);        // 0 on the knobs, 1 in the crevices
    c *= 1.0f + 0.26f * f1.x + 0.2f * f2.z;
    c = mix(c, float3(0.085f, 0.085f, 0.06f), crev * 0.85f);
    c = mix(float3(0.33f, 0.30f, 0.22f) * (0.75f + 0.5f * id3), c, sstepf(0.0f, 0.38f, w2b - w2 + 0.10f * f1.x));       // colonies fade into a neutral crust toward their borders
    c *= 0.72f + 0.30f * sat(1.3f - w4 * 1.6f);                  // polyps / knobs catch the light, pits stay dark
    c *= 0.86f + 0.22f * sat(f3.y * 1.6f);
    float nf = exp(-t * 0.07f);
    float3 bump = float3(f2.x, f2.z, f2.y - 0.5f) * 0.26f + float3(f3.x, f3.z, 0.0f) * 0.30f * nf;
    float3 nr = normalize(n + bump * nf);
    // --- sand with wave ripples, grains and bits of shell
    float fadeR = exp(-t * 0.09f);
    float ph = p.x * 60.0f + 11.0f * NZ(nzT, float3(p.x * 0.5f, 0.3f, p.z * 0.5f)).x + 4.5f * NZ(nzT, float3(p.x * 1.7f, 1.3f, p.z * 1.7f)).z + p.z * 7.0f;
    float ripA = sat(0.55f + 0.9f * NZ(nzT, float3(p.x * 0.9f, 7.7f, p.z * 0.9f)).x) * sat((n.y - 0.86f) * 9.0f);
    float rip = cos(ph) * ripA;
    float4 g1 = NZ(nzT, p * 140.0f);
    float3 cs = float3(0.74f, 0.68f, 0.56f) * (0.86f + 0.10f * f1.z + 0.07f * f2.x + 0.10f * f3.x + 0.10f * g1.x * fadeR);
    cs *= 1.0f - 0.12f * fadeR * (0.5f - 0.5f * sin(ph)) * ch * ripA;
    cs *= 1.0f - 0.55f * sat((0.22f - w4) * 9.0f) * sat(id4 * 6.0f - 4.6f);        // scattered dark shell / rubble bits
    cs = mix(cs, cs * float3(0.62f, 0.60f, 0.52f), sat(1.0f - ch));               // pocket sand is greyer
    float3 ns = normalize(n + float3(1.0f, 0.0f, 0.1f) * rip * 0.36f * fadeR * ch + float3(f2.x, 0.0f, f2.z) * 0.08f + float3(g1.x, 0.0f, g1.z) * 0.10f * fadeR);
    alb = mix(c, cs, sand);
    n = normalize(mix(nr, ns, sand));
}
static void instMaterial(float3 p, Inst I, float3& n, float t, float3& alb, SCN_D) {
    float3 dl = p - I.pos.xyz; float sc = I.pos.w;
    float3 q = float3(dot(dl, I.ax.xyz), dot(dl, I.ay.xyz), dot(dl, I.az.xyz)) / sc;
    int type = int(I.ax.w); float seed = I.ay.w; float hue = I.az.w;
    float4 f = NZ(nzT, p * 9.0f); float4 f2 = NZ(nzT, p * 31.0f); float4 f3 = NZ(nzT, p * 70.0f);
    float nf = exp(-t * 0.08f);
    float3 c;
    if (type == 0) {             // table coral: brown / green-brown / blue-grey with a pale growing rim
        float r = length(q.xz);
        c = hue < 0.4f ? float3(0.44f, 0.34f, 0.21f) : (hue < 0.75f ? float3(0.34f, 0.35f, 0.20f) : float3(0.36f, 0.37f, 0.45f));
        c *= 0.75f + 0.3f * sat(f3.y * 1.5f) + 0.15f * f.x;
        c = mix(c, float3(0.66f, 0.62f, 0.52f), sstepf(0.86f, 1.03f, r) * 0.6f);
        if (q.y < 0.36f) c *= 0.45f;
        n = normalize(n + (float3(f2.x, f2.z * 0.3f, f2.z) * 0.40f + float3(f3.x, 0.0f, f3.z) * 0.35f) * nf);
    } else if (type == 1) {      // branching coral
        c = hue < 0.35f ? float3(0.60f, 0.50f, 0.32f) : (hue < 0.7f ? float3(0.50f, 0.40f, 0.52f) : float3(0.42f, 0.47f, 0.30f));
        c = mix(c, float3(0.84f, 0.82f, 0.78f), sstepf(0.55f, 0.95f, q.y) * 0.45f);
        c *= 0.55f + 0.45f * sstepf(0.0f, 0.5f, q.y);
        c *= 0.85f + 0.3f * f3.x;
        n = normalize(n + float3(f2.x, f2.y - 0.5f, f2.z) * 0.25f * nf);
    } else if (type == 2) {      // sea fan
        c = hue < 0.45f ? float3(0.66f, 0.07f, 0.05f) : (hue < 0.8f ? float3(0.74f, 0.27f, 0.05f) : float3(0.44f, 0.14f, 0.50f));
        c *= 0.8f + 0.3f * f.x;
    } else if (type == 3) {      // whip coral
        c = hue < 0.5f ? float3(0.62f, 0.16f, 0.08f) : float3(0.55f, 0.52f, 0.18f);
    } else if (type == 4) {      // barrel sponge
        c = float3(0.42f, 0.18f, 0.12f) * (0.75f + 0.4f * f.x + 0.2f * f3.x);
        if (length(q.xz) < 0.30f && q.y > 0.5f) c *= 0.35f;
        n = normalize(n + float3(f2.x, f2.y - 0.5f, f2.z) * 0.40f * nf);
    } else {                     // boulder coral: lobes, darker seams, meandering grooves up close
        c = hue < 0.5f ? float3(0.50f, 0.42f, 0.17f) : (hue < 0.8f ? float3(0.40f, 0.38f, 0.22f) : float3(0.46f, 0.33f, 0.30f));
        float3 qq = q - float3(0, 0.15f, 0);
        float lob = sin(4.3f * qq.x + seed * 5.0f) * sin(4.7f * qq.z + seed * 3.0f) + 0.6f * sin(9.0f * qq.x + 2.0f * qq.y + seed) * sin(8.3f * qq.z - 3.0f * qq.y);
        c *= 0.70f + 0.22f * lob + 0.2f * f.x;
        float3 w = q * 26.0f + 2.5f * float3(f.x, f.z, f.y);
        float m = sin(w.x + 1.7f * sin(w.z * 0.9f)) * sin(w.z + 1.7f * sin(w.y)) ;
        c *= 0.80f + 0.22f * m * nf + 0.12f * f3.x;
        n = normalize(n + (float3(f.x, 0.0f, f.z) * 0.25f * m + float3(f2.x, f2.y - 0.5f, f2.z) * 0.22f) * nf);
    }
    alb = hueShift(c, seed);
}

static inline float torchSpot(float3 dirFromTorch, constant Uniforms& U) {
    float c = dot(dirFromTorch, U.torchD.xyz);
    return sstepf(U.torchD.w, mix(U.torchD.w, 1.0f, 0.55f), c) * (0.35f + 0.65f * sstepf(mix(U.torchD.w, 1.0f, 0.5f), 1.0f, c));
}
// irradiance on a surface point, times albedo/pi
static float3 lightSurface(float3 p, float3 n, float3 alb, float ao, float sh, SCN_D) {
    float D = max(-p.y, 0.0f); float mu = U.sunW.y;
    float cau = causticHi(p, cauT, U);
    float ndl = sat(dot(n, U.sunW.xyz));
    float3 E = sunBeam(D, mu) * (cau * ndl * sh);
    float up = 0.5f + 0.5f * n.y;
    float3 Ed = diffuseE(D, mu);
    E += Ed * (0.22f + 0.78f * up) * ao;
    E += Ed * float3(0.10f, 0.13f, 0.15f) * (1.0f - up) * ao;        // light bounced up from below
    float3 tl = U.torchP.xyz - p; float td = length(tl); tl /= td;
    E += float3(1.0f, 0.95f, 0.86f) * (3.2f * U.torchP.w * torchSpot(-tl, U) * sat(dot(n, tl)) / (td * td + 0.08f)) * exp(-SIG_T * td);
    return alb * (1.0f / PI) * E;
}
// what a mirror pointing along dir would show from depth D: the scattered light field plus, inside Snell's cone, the sky
static float3 envRad(float D, float3 dir, constant Uniforms& U) {
    float3 L = fogClosed(D, dir, U);
    float cw = dir.y;
    float win = sstepf(0.64f, 0.70f, cw);
    if (win > 0.0f) {
        float sw = sqrt(max(1.0f - cw * cw, 0.0f)); float sa = min(sw * N_WATER, 0.999f); float ca = sqrt(1.0f - sa * sa);
        float3 h = normalize(float3(dir.x, 0.0f, dir.z) + 1e-5f);
        float3 da = h * sa + float3(0, ca, 0);
        L += skyRad(da, U.sunA.xyz) * 1.5f * win * exp(-SIG_T * (D / max(cw, 0.2f)));
    }
    return L;
}

// ------------------------------------------------------------------ light scattered into the view ray
static float3 inscatter(float3 ro, float3 rd, float tmax, float jit, int NS, SCN_D) {
    float cm = SIG_T.z;
    float Tm = exp(-cm * tmax);
    float ph = waterPhase(dot(rd, U.sunW.xyz));
    float mu = U.sunW.y;
    float g = 1.0f / (1.0f - 0.62f * rd.y);
    float3 acc = float3(0.0f);
    float norm = (1.0f - Tm) / cm;
    for (int i = 0; i < NS; i++) {
        float u = (float(i) + jit) / float(NS);
        float s = -log(1.0f - u * (1.0f - Tm)) / cm;
        float3 p = ro + rd * s;
        float D = max(-p.y, 0.0f);
        float cau = causticLo(p, cauLo, U);
        float3 src = SIG_S * (ph * cau) * sunBeam(D, mu) + AMB * g * exp(-KD * D);
        acc += src * exp(-(SIG_T - cm) * s);
    }
    return acc * (norm / float(NS));
}
static float3 torchBeam(float3 ro, float3 rd, float tmax, float jit, constant Uniforms& U) {
    if (U.torchP.w <= 0.0f) return float3(0.0f);
    float3 acc = float3(0.0f); float tm = min(tmax, 6.0f); const int NS = 10;
    for (int i = 0; i < NS; i++) {
        float s = tm * (float(i) + jit) / float(NS);
        float3 p = ro + rd * s;
        float3 tl = p - U.torchP.xyz; float td = length(tl); tl /= td;
        float ph = waterPhase(dot(tl, rd));
        acc += SIG_S * (ph * torchSpot(tl, U) * U.torchP.w / (td * td + 0.08f)) * exp(-SIG_T * (td + s));
    }
    return acc * (tm / float(NS)) * float3(1.0f, 0.95f, 0.86f) * 0.09f;
}

// ------------------------------------------------------------------ marine snow
static float3 particles(float3 ro, float3 rd, float tmax, float pixAng, SCN_D) {
    const float CS = 0.27f;
    float tm = U.camPos.w;
    float amp = surgeAmp(ro.y);
    float3 drift = float3(U.surge.x * amp, -0.012f * tm, U.surge.y * amp + 0.02f * tm);
    float3 o = (ro - drift) / CS;
    int3 c = int3(floor(o));
    float3 ird = 1.0f / (abs(rd) + 1e-6f);
    int3 stp = int3(sign(rd));
    float3 tMax = abs(((float3(c) + step(0.0f, rd)) - o) * ird);
    float3 acc = float3(0.0f);
    float mu = U.sunW.y;
    float phS = waterPhase(dot(rd, U.sunW.xyz)) * 4.0f * PI;
    float lim = min(tmax, 7.5f) / CS;
    for (int i = 0; i < 40; i++) {
        float3 h = hash3i(c);
        if (h.x < 0.62f) {
            float3 h2 = hash3i(c + int3(71, 13, 37));
            float3 wob = 0.05f * sin(tm * (0.3f + h2 * 0.5f) + h * 30.0f);
            float3 pp = ((float3(c) + 0.18f + 0.64f * h2 + wob) * CS) + drift;
            float tt = dot(pp - ro, rd);
            if (tt > 0.06f && tt < tmax) {
                float3 dv = pp - (ro + rd * tt);
                float d2 = dot(dv, dv);
                float rphys = mix(0.0006f, 0.0030f, h.y * h.y * h.y);
                float rdraw = max(rphys, pixAng * tt * 0.75f);
                if (d2 < rdraw * rdraw * 6.0f) {
                    float w = exp(-d2 / (rdraw * rdraw) * 1.6f) * min(1.0f, (rphys * rphys) / (rdraw * rdraw)) * sstepf(0.06f, 0.25f, tt);
                    float D = max(-pp.y, 0.0f);
                    float3 L = sunBeam(D, mu) * causticLo(pp, cauLo, U) * (0.10f + 0.06f * phS) + diffuseE(D, mu) * 0.16f;
                    float3 tl = pp - U.torchP.xyz; float td = length(tl); tl /= td;
                    L += float3(1.0f, 0.95f, 0.86f) * (0.30f * U.torchP.w * torchSpot(tl, U) / (td * td + 0.08f)) * exp(-SIG_T * td);
                    acc += L * w * exp(-SIG_T * tt) * (0.5f + h.z);
                }
            }
        }
        if (tMax.x < tMax.y && tMax.x < tMax.z) { if (tMax.x > lim) break; c.x += stp.x; tMax.x += ird.x; }
        else if (tMax.y < tMax.z) { if (tMax.y > lim) break; c.y += stp.y; tMax.y += ird.y; }
        else { if (tMax.z > lim) break; c.z += stp.z; tMax.z += ird.z; }
    }
    return acc;
}

// ------------------------------------------------------------------ fish (local frame: +x nose, +y up, length 1)
static inline float fishH(float sp) { return sp < 0.5f ? 0.118f : (sp < 1.5f ? 0.165f : 0.19f); }
static inline float3 fishUnbend(float3 p, float sp, float phase, float turn) {
    float uc = clamp(0.5f - p.x, 0.0f, 1.0f);
    float amp = sp < 0.5f ? 0.075f : 0.06f;
    p.z -= amp * uc * uc * sin(phase - 4.5f * uc) + turn * 0.22f * uc * uc;
    return p;
}
static float sdFish(float3 p, float sp, float phase, float turn) {
    p = fishUnbend(p, sp, phase, turn);
    float u = 0.5f - p.x; float uc = clamp(u, 0.0f, 1.0f);
    float H = fishH(sp);
    const float bl = 0.80f;
    float s = clamp(uc / bl, 0.0f, 1.0f);
    float prof = pow(max(4.0f * s * (1.0f - s), 0.0f), 0.60f) * (1.0f - 0.40f * s);
    float hy = H * prof + 0.014f, hz = hy * 0.48f + 0.004f;
    float k = length(float2(p.y / hy, p.z / hz));
    float d = (k - 1.0f) * min(hy, hz);
    d = max(d, max(-u - 0.004f, u - bl - 0.03f));
    float az = abs(p.z), ay = abs(p.y);
    // caudal fin (forked)
    float v = (u - 0.76f) / 0.24f;
    float yT = 0.018f + v * H * (sp < 0.5f ? 1.35f : 1.15f);
    float yI = max(0.0f, v - 0.42f) * H * 1.9f;
    float d2 = max(max(ay - yT, yI - ay), max(0.76f - u, u - 1.0f));
    float dT = length(max(float2(d2, az - 0.0035f), 0.0f)) + min(max(d2, az - 0.0035f), 0.0f);
    d = min(d, dT);
    // dorsal and anal fins
    float fd = sin(clamp((u - 0.26f) / 0.40f, 0.0f, 1.0f) * PI);
    float d3 = max(max(p.y - (hy + 0.040f * sqrt(max(fd, 0.0f))), hy * 0.8f - p.y), max(0.26f - u, u - 0.66f));
    float fa = sin(clamp((u - 0.50f) / 0.24f, 0.0f, 1.0f) * PI);
    float d4 = max(max(-p.y - (hy + 0.030f * sqrt(max(fa, 0.0f))), hy * 0.8f + p.y), max(0.50f - u, u - 0.74f));
    float d34 = min(d3, d4);
    d = min(d, length(max(float2(d34, az - 0.003f), 0.0f)) + min(max(d34, az - 0.003f), 0.0f));
    return d * 0.6f;
}
static float3 fishAlbedo(float3 pl, float sp, float seed, float& silver) {
    float u = 0.5f - pl.x; float H = fishH(sp); float y = pl.y / H;
    float3 c; silver = 0.0f;
    if (sp < 0.5f) {              // fusilier: blue back, silver flank, yellow band and tail
        c = mix(float3(0.78f, 0.80f, 0.80f), float3(0.42f, 0.56f, 0.70f), sstepf(-0.45f, 0.15f, y));
        c = mix(c, float3(0.08f, 0.22f, 0.40f), sstepf(0.42f, 0.72f, y));
        float band = sstepf(0.22f, 0.30f, y) * (1.0f - sstepf(0.46f, 0.56f, y)) * sstepf(0.12f, 0.2f, u);
        c = mix(c, float3(0.95f, 0.76f, 0.08f), band);
        float tail = sstepf(0.74f, 0.80f, u);
        c = mix(c, float3(0.93f, 0.74f, 0.10f), tail);
        c = mix(c, float3(0.06f, 0.06f, 0.08f), sstepf(0.93f, 0.98f, u));
        silver = 0.55f * (1.0f - band) * (1.0f - tail) * (1.0f - sstepf(0.3f, 0.7f, y));
    } else if (sp < 1.5f) {       // anthias: orange, paler belly, violet fins
        c = mix(float3(1.0f, 0.62f, 0.36f), float3(0.96f, 0.40f, 0.10f), sstepf(-0.5f, 0.3f, y));
        c = mix(c, float3(0.80f, 0.28f, 0.50f), sstepf(0.76f, 0.86f, u) * 0.7f + sstepf(0.95f, 1.2f, abs(y)) * 0.6f);
        c *= 0.9f + 0.2f * hash1(seed * 3.7f);
    } else {                      // jack: plain silver, dark back
        c = mix(float3(0.80f, 0.82f, 0.82f), float3(0.20f, 0.26f, 0.30f), sstepf(0.35f, 0.8f, y));
        c = mix(c, float3(0.15f, 0.16f, 0.16f), sstepf(0.80f, 0.9f, u) * 0.6f);
        silver = 0.6f * (1.0f - sstepf(0.3f, 0.7f, y));
    }
    // eye
    float2 ev = float2(u - 0.105f, pl.y - 0.22f * H);
    float er = length(ev);
    c = mix(c, float3(0.85f, 0.85f, 0.80f), 1.0f - sstepf(0.026f, 0.031f, er));
    c = mix(c, float3(0.02f, 0.02f, 0.02f), 1.0f - sstepf(0.015f, 0.019f, er));
    // gill line
    c *= 1.0f - 0.25f * (1.0f - sstepf(0.0f, 0.012f, abs(u - 0.215f - 0.03f * (1.0f - y * y)))) * sstepf(-0.8f, -0.6f, -abs(y));
    return c;
}

// ------------------------------------------------------------------ manta ray (local: +x forward, +y up, z across; wingspan 1)
// un-curl the wings: each wing bends along a circular arc whose curvature follows the beat, lagging toward the tip
static float3 mantaUnbend(float3 p, float phase, float amp) {
    float az = abs(p.z); float w = min(az / 0.5f, 1.3f);
    float kap = amp * 1.45f * sin(phase - 1.25f * w) + 0.22f;
    if (abs(kap) < 0.02f) kap = 0.02f;
    float c = 1.0f / kap, sg = sign(c), ac = abs(c);
    float phi = atan2(az, sg * (c - p.y));
    float r = length(float2(az, p.y - c));
    float core = sstepf(0.035f, 0.11f, az);                 // the body between the wing roots stays rigid
    float zs = mix(az, phi * ac, core), yn = mix(p.y, sg * (ac - r), core);
    return float3(p.x - 0.05f * w * w * w * amp * cos(phase - 1.25f * w), yn, zs);
}
static inline float mantaLE(float wc) { return 0.205f - 0.185f * pow(wc, 1.5f) - 0.115f * wc * wc * wc * wc; }
static inline float mantaTE(float wc) { return -0.235f + 0.175f * pow(wc, 0.9f) - 0.055f * wc * wc * wc * wc; }
static float sdManta(float3 p0, float phase, float amp) {
    float3 p = mantaUnbend(p0, phase, amp);
    float az = p.z; float wc = clamp(az / 0.5f, 0.0f, 1.0f);
    float le = mantaLE(wc), te = mantaTE(wc);
    le = min(le, 0.188f + 0.4f * max(az - 0.055f, 0.0f));                 // the mouth: a straight front between the head fins
    float chord = max(le - te, 1e-3f);
    float s = clamp((le - p.x) / chord, 0.0f, 1.0f);
    float tmax = 0.046f * pow(1.0f - wc, 1.25f) + 0.004f;
    float body = exp(-az * az / (0.085f * 0.085f));
    float prof = 2.3f * sqrt(s + 0.003f) * (1.0f - s) * (1.0f + 0.45f * (1.0f - s));
    float thU = tmax * prof * (1.0f + 0.55f * body) + 0.002f;            // domed back
    float thL = tmax * prof * (0.75f + 0.10f * body) + 0.002f;           // flatter belly
    float dy = p.y > 0.0f ? p.y - thU : -p.y - thL;
    float dpl = max(max(p.x - le, te - p.x), az - 0.5f);
    float d = length(max(float2(dy, dpl), 0.0f)) + min(max(dy, dpl), 0.0f);
    // cephalic lobes (rolled, pointing forward and slightly down/in)
    float3 a = float3(0.176f, -0.006f, 0.068f), b = float3(0.262f, -0.026f, 0.056f);
    float3 q = float3(p0.x, p0.y, abs(p0.z));
    float h = clamp(dot(q - a, b - a) / dot(b - a, b - a), 0.0f, 1.0f);
    float3 dq = q - (a + (b - a) * h); dq.y *= 0.55f; dq.z *= 1.7f;
    d = smin(d, length(dq) - mix(0.024f, 0.012f, h), 0.025f);
    // tail
    float ht = clamp((-0.22f - p0.x) / 0.44f, 0.0f, 1.0f);
    d = min(d, length(float3(p0.x + 0.22f + 0.44f * ht, p0.y - 0.004f + 0.02f * ht * ht, p0.z)) - mix(0.0075f, 0.0015f, ht));
    // small dorsal fin at the tail base
    float3 df = float3(p0.x + 0.215f, p0.y - 0.022f, p0.z);
    d = min(d, max(length(float2(df.x * 0.6f, df.y)) - 0.022f, abs(df.z) - 0.004f));
    return d * 0.55f;
}
static float3 mantaAlbedo(float3 p0, float3 nl, float phase, float amp, constant Uniforms& U, texture3d<float> nzT) {
    float3 pl = mantaUnbend(p0, phase, amp);
    float az = pl.z; float w = clamp(az / 0.5f, 0.0f, 1.0f);
    float le = mantaLE(w), te = mantaTE(w);
    float s = clamp((le - pl.x) / max(le - te, 1e-3f), 0.0f, 1.0f);
    float4 f = NZ(nzT, pl * 40.0f); float4 f2 = NZ(nzT, pl * 7.0f + 3.0f);
    bool top = pl.y > 0.0f;
    float3 c;
    if (top) {
        c = float3(0.030f, 0.033f, 0.040f) * (0.8f + 0.4f * f2.x);
        // pale shoulder chevrons
        float u = (0.150f - pl.x) / 0.10f, v = (az - 0.035f) / 0.20f;
        float sh = sstepf(0.0f, 0.25f, u) * (1.0f - sstepf(0.55f + 0.5f * v, 1.0f + 0.5f * v, u)) * sstepf(0.0f, 0.15f, v) * (1.0f - sstepf(0.7f, 1.0f, v));
        c = mix(c, float3(0.46f, 0.48f, 0.50f), sh * (0.75f + 0.25f * f.x));
        c = mix(c, float3(0.12f, 0.13f, 0.14f), sstepf(0.9f, 1.0f, w) * 0.5f);
    } else {
        c = float3(0.82f, 0.83f, 0.82f) * (0.92f + 0.08f * f2.z);
        c = mix(c, float3(0.20f, 0.22f, 0.25f), sat(sstepf(0.74f, 1.0f, s) * 0.85f + sstepf(0.70f, 1.0f, w) * 0.75f));   // grey trailing edge and tips
        // gill slits: five crescents each side
        float gx = (0.105f - pl.x) / 0.024f; float gi = floor(gx);
        float zin = 0.030f + 0.004f * gi, zout = 0.088f + 0.006f * gi;
        if (gi >= 0.0f && gi < 5.0f && az > zin && az < zout) c *= 0.22f + 0.78f * sstepf(0.0f, 0.20f, abs(fract(gx) - 0.5f + 0.25f * (az - zin) / (zout - zin) - 0.12f));
        // belly spots, different on every animal
        float idd; float sp = worleyS(pl.x * 26.0f, 3.0f, pl.z * 26.0f * sign(p0.z) + 40.0f, 77u, idd);
        c *= 1.0f - 0.8f * (1.0f - sstepf(0.16f, 0.24f, sp)) * step(0.55f, idd) * sstepf(0.30f, 0.45f, s) * (1.0f - sstepf(0.16f, 0.26f, az));
    }
    // mouth and eyes
    if (pl.x > 0.176f && az < 0.052f && abs(pl.y) < 0.02f) c = mix(c, float3(0.02f), 0.85f);
    float3 q = float3(p0.x, p0.y, abs(p0.z));
    c = mix(c, float3(0.01f), 1.0f - sstepf(0.007f, 0.010f, length(q - float3(0.172f, 0.004f, 0.083f))));
    return c;
}

// ------------------------------------------------------------------ quick trace for mirror images in the surface
static float3 traceLite(float3 ro, float3 rd, SCN_D) {
    float3 ird = 1.0f / rd; float t = 0.05f; int hitId = -1; bool hit = false;
    for (int i = 0; i < 70; i++) {
        float3 p = ro + rd * t; int id; float dH, dP;
        float d = sceneSDF(p, rd, ird, id, dH, dP, SCN);
        if (dH < 0.002f * t + 0.002f) { hit = true; hitId = id; break; }
        t += d * 1.1f;
        if (t > 45.0f) break;
    }
    float D0 = max(-ro.y, 0.0f);
    float3 fog = fogClosed(D0 + 2.0f, rd, U);
    if (!hit) return fog;
    float3 p = ro + rd * t; float e = 0.004f * t + 0.004f; float3 n;
    float2 k = float2(1.0f, -1.0f);
    if (hitId >= 0) { Inst I = insts[hitId]; n = normalize(k.xyy * instSDF(p + k.xyy * e, I, U, nzT) + k.yyx * instSDF(p + k.yyx * e, I, U, nzT) + k.yxy * instSDF(p + k.yxy * e, I, U, nzT) + k.xxx * instSDF(p + k.xxx * e, I, U, nzT)); }
    else { float3 a = p + k.xyy * e, b = p + k.yyx * e, c = p + k.yxy * e, dd = p + k.xxx * e;
        n = normalize(k.xyy * terrainSDF(a.x, a.y, a.z, nzT) + k.yyx * terrainSDF(b.x, b.y, b.z, nzT) + k.yxy * terrainSDF(c.x, c.y, c.z, nzT) + k.xxx * terrainSDF(dd.x, dd.y, dd.z, nzT)); }
    float3 alb;
    if (hitId >= 0) instMaterial(p, insts[hitId], n, t, alb, SCN); else terrainMaterial(p, n, t, alb, SCN);
    float3 col = lightSurface(p, n, alb, 0.8f, 0.8f, SCN);
    float3 T = exp(-SIG_T * t);
    return (col * T + fog * (1.0f - T)) * 0.85f;
}

// radiance arriving from the underside of the sea surface at point p (on y ~ 0), looking along rd (rd.y > 0)
static float3 surfaceRadiance(float3 p, float3 rd, float dist, SCN_D) {
    float tm = U.camPos.w;
    float2 uv = p.xz / U.caus.x;
    float4 s = surfT.sample(sLin, uv, level(0));
    float fd = exp(-dist * 0.035f);
    float4 r1 = NZ(nzT, float3(p.x * 2.3f + tm * 0.31f, tm * 0.55f, p.z * 2.3f));
    float4 r2 = NZ(nzT, float3(p.x * 6.1f - tm * 0.4f, tm * 0.9f + 4.0f, p.z * 6.1f + 3.0f));
    float2 sl = s.yz + fd * (0.030f * float2(r1.x, r1.z) + 0.018f * float2(r2.x, r2.z));
    float3 nI = normalize(float3(sl.x, -1.0f, sl.y));            // normal on the water side
    float c = -dot(rd, nI);
    if (c < 0.02f) { nI = normalize(nI - rd * (0.02f - c)); c = max(-dot(rd, nI), 0.01f); }
    float eta = N_WATER;
    float k = 1.0f - eta * eta * (1.0f - c * c);
    float ct = sqrt(max(k, 0.0f));
    float rs = (eta * c - ct) / (eta * c + ct), rp = (c - eta * ct) / (c + eta * ct);
    float F = k <= 0.0f ? 1.0f : sat(0.5f * (rs * rs + rp * rp));
    float3 Lr = float3(0.0f);
    if (F > 0.015f) {
        float3 refl = reflect(rd, nI);
        if (refl.y > -0.03f) refl.y = -0.03f;
        refl = normalize(refl);
        Lr = traceLite(p + refl * 0.05f - float3(0, 0.02f, 0), refl, SCN);
    }
    if (k <= 0.0f) return Lr;
    float3 t = eta * rd + (eta * c - ct) * nI;
    t.y = max(t.y, 0.0f);
    float3 Ls = skyRad(normalize(t), U.sunA.xyz) * 1.5f;
    return mix(Ls, Lr, F);
}

// ------------------------------------------------------------------ one camera sample
static float3 radiance(float3 ro, float3 rd, float pixAng, uint2 gid, uint& seed, float tj, SCN_D, DYN_D) {
    float tSurf = rd.y > 1e-4f ? (-ro.y) / rd.y : 1e9f;
    float tmax = min(tSurf, 85.0f);
    bool fin = U.dbg.y > 0.5f;
    // --- sphere trace terrain + instances
    float3 ird = 1.0f / rd; float t = 0.02f; int hitId = -1; int hitKind = 0;      // 0 none, 1 terrain/instance, 2 fish, 3 manta
    int maxSteps = fin ? 260 : 150;
    for (int i = 0; i < maxSteps; i++) {
        float3 p = ro + rd * t;
        int id; float dH, dP;
        float d = sceneSDF(p, rd, ird, id, dH, dP, SCN);
        if (dH < 0.0006f * t + 0.0004f) { hitKind = 1; hitId = id; break; }
        t += d;
        if (t > tmax) break;
    }
    float tEnd = hitKind == 1 ? t : tmax;
    // --- fish and bubbles registered in this screen tile
    float ts = U.gridN.w;
    uint tix = uint(float(gid.x) / ts), tiy = uint(float(gid.y) / ts);
    uint tcell = (tix + uint(U.surge.z) * tiy) * 2u;
    uint toff = tiles[tcell], tcnt = tiles[tcell + 1u];
    int fishId = -1; float3 fishPl = float3(0.0f);
    for (uint k = 0u; k < tcnt; k++) {
        uint e = tileIdx[toff + k];
        if (e & 0x80000000u) continue;
        Fish F = fishes[e];
        float L = F.pos.w;
        float3 oc = ro - (F.pos.xyz + F.vel.xyz * tj);
        float b = dot(oc, rd), c2 = dot(oc, oc) - 0.33f * L * L;
        float disc = b * b - c2; if (disc <= 0.0f) continue;
        float sq = sqrt(disc); float t0 = max(-b - sq, 0.0f), t1 = min(-b + sq, tEnd);
        if (t0 >= t1) continue;
        float3 fz = normalize(cross(F.fwd.xyz, F.up.xyz));
        float3 lo = float3(dot(oc, F.fwd.xyz), dot(oc, F.up.xyz), dot(oc, fz)) / L;
        float3 ld = float3(dot(rd, F.fwd.xyz), dot(rd, F.up.xyz), dot(rd, fz));
        float tt = t0 / L, te = t1 / L;
        for (int i = 0; i < 26; i++) {
            float3 pl = lo + ld * tt;
            float d = sdFish(pl, F.up.w, F.fwd.w, F.par.y);
            if (d < 0.0012f + 0.0005f * tt) { tEnd = tt * L; hitKind = 2; fishId = int(e); fishPl = pl; break; }
            tt += d; if (tt > te) break;
        }
    }
    // --- manta
    float3 mPl = float3(0.0f);
    if (U.mantaP.w > 0.0f) {
        float S = U.mantaP.w;
        float3 oc = ro - (U.mantaP.xyz - U.mantaF.xyz * (0.12f * S));
        float b = dot(oc, rd), c2 = dot(oc, oc) - 0.50f * S * S;
        float disc = b * b - c2;
        if (disc > 0.0f) {
            float sq = sqrt(disc); float t0 = max(-b - sq, 0.0f), t1 = min(-b + sq, tEnd);
            if (t0 < t1) {
                float3 mz = normalize(cross(U.mantaF.xyz, U.mantaU.xyz));
                float3 om = ro - U.mantaP.xyz;
                float3 lo = float3(dot(om, U.mantaF.xyz), dot(om, U.mantaU.xyz), dot(om, mz)) / S;
                float3 ld = float3(dot(rd, U.mantaF.xyz), dot(rd, U.mantaU.xyz), dot(rd, mz));
                float tt = t0 / S, te = t1 / S;
                for (int i = 0; i < 70; i++) {
                    float3 pl = lo + ld * tt;
                    float d = sdManta(pl, U.mantaF.w, U.mantaU.w);
                    if (d < 0.0003f + 0.0004f * tt) { tEnd = tt * S; hitKind = 3; mPl = pl; break; }
                    tt += d; if (tt > te) break;
                }
            }
        }
    }
    float3 col = float3(0.0f);
    if (hitKind == 1) {
        float3 p = ro + rd * tEnd;
        float e = 0.0012f * tEnd + 0.0015f;
        float3 n; float2 k = float2(1.0f, -1.0f);
        if (hitId >= 0) {
            Inst I = insts[hitId];
            n = normalize(k.xyy * instSDF(p + k.xyy * e, I, U, nzT) + k.yyx * instSDF(p + k.yyx * e, I, U, nzT) + k.yxy * instSDF(p + k.yxy * e, I, U, nzT) + k.xxx * instSDF(p + k.xxx * e, I, U, nzT));
        } else {
            float3 a = p + k.xyy * e, b = p + k.yyx * e, c = p + k.yxy * e, dd = p + k.xxx * e;
            n = normalize(k.xyy * terrainSDF(a.x, a.y, a.z, nzT) + k.yyx * terrainSDF(b.x, b.y, b.z, nzT) + k.yxy * terrainSDF(c.x, c.y, c.z, nzT) + k.xxx * terrainSDF(dd.x, dd.y, dd.z, nzT));
        }
        if (dot(n, rd) > 0.0f) n = normalize(n - rd * dot(n, rd) * 1.05f);
        float3 ng = n; float3 alb;
        if (hitId >= 0) instMaterial(p, insts[hitId], n, tEnd, alb, SCN);
        else terrainMaterial(p, n, tEnd, alb, SCN);
        float ao = 0.0f; float sca = 1.0f;
        for (int i = 0; i < 5; i++) {
            float hh = 0.03f + 0.15f * float(i);
            float dd = sceneSDFx(p + ng * hh, SCN) / 0.56f;
            ao += max(hh - dd, 0.0f) * sca; sca *= 0.7f;
        }
        ao = sat(1.0f - 1.5f * ao);
        float sh = softShadow(p + ng * (0.02f + 0.002f * tEnd), U.sunW.xyz, 15.0f, fin ? 40 : 20, SCN);
        col = lightSurface(p, n, alb, ao, sh, SCN);
        if (U.dbg.x == 1.0f) return n * 0.5f + 0.5f;
        if (U.dbg.x == 2.0f) return alb;
        if (U.dbg.x == 4.0f) { if (hitId < 0) return float3(0.25f) * (0.5f + 0.5f * ng.y); float ty = insts[hitId].ax.w; return (ty < 0.5f ? float3(1,0,0) : ty < 1.5f ? float3(0,1,0) : ty < 2.5f ? float3(0,0,1) : ty < 3.5f ? float3(1,1,0) : ty < 4.5f ? float3(1,0,1) : float3(0,1,1)) * (0.4f + 0.6f * abs(ng.y)); }
        if (U.dbg.x == 3.0f) return float3(ao, sh, causticHi(p, cauT, U) * 0.25f);
    } else if (hitKind == 2) {
        Fish F = fishes[fishId];
        float3 fz = normalize(cross(F.fwd.xyz, F.up.xyz));
        float e = 0.004f; float2 k = float2(1.0f, -1.0f);
        float3 nl = normalize(k.xyy * sdFish(fishPl + k.xyy * e, F.up.w, F.fwd.w, F.par.y) + k.yyx * sdFish(fishPl + k.yyx * e, F.up.w, F.fwd.w, F.par.y) + k.yxy * sdFish(fishPl + k.yxy * e, F.up.w, F.fwd.w, F.par.y) + k.xxx * sdFish(fishPl + k.xxx * e, F.up.w, F.fwd.w, F.par.y));
        float3 n = normalize(F.fwd.xyz * nl.x + F.up.xyz * nl.y + fz * nl.z);
        if (dot(n, rd) > 0.0f) n = normalize(n - rd * dot(n, rd) * 1.05f);
        float silver;
        float3 alb = fishAlbedo(fishUnbend(fishPl, F.up.w, F.fwd.w, F.par.y), F.up.w, F.par.z, silver);
        float3 p = ro + rd * tEnd;
        col = lightSurface(p, n, alb, 0.9f, 1.0f, SCN);
        float fr = silver * (0.55f + 0.45f * pow(1.0f - sat(-dot(n, rd)), 3.0f));
        col = col * (1.0f - 0.5f * silver) + fr * envRad(max(-p.y, 0.0f), reflect(rd, n), U) * 0.8f;
    } else if (hitKind == 3) {
        float S = U.mantaP.w; float3 mz = normalize(cross(U.mantaF.xyz, U.mantaU.xyz));
        float e = 0.002f; float2 k = float2(1.0f, -1.0f);
        float3 nl = normalize(k.xyy * sdManta(mPl + k.xyy * e, U.mantaF.w, U.mantaU.w) + k.yyx * sdManta(mPl + k.yyx * e, U.mantaF.w, U.mantaU.w) + k.yxy * sdManta(mPl + k.yxy * e, U.mantaF.w, U.mantaU.w) + k.xxx * sdManta(mPl + k.xxx * e, U.mantaF.w, U.mantaU.w));
        float3 n = normalize(U.mantaF.xyz * nl.x + U.mantaU.xyz * nl.y + mz * nl.z);
        if (dot(n, rd) > 0.0f) n = normalize(n - rd * dot(n, rd) * 1.05f);
        float3 alb = mantaAlbedo(mPl, nl, U.mantaF.w, U.mantaU.w, U, nzT);
        float3 p = ro + rd * tEnd;
        float occ = mantaUnbend(mPl, U.mantaF.w, U.mantaU.w).y < 0.0f ? 0.85f : 1.0f;
        col = lightSurface(p, n, alb, occ, 1.0f, SCN);
        col += alb * (1.0f / PI) * diffuseE(max(-p.y, 0.0f), U.sunW.y) * 0.42f * sat(0.5f - 0.5f * n.y);
        col += 0.03f * pow(1.0f - sat(-dot(n, rd)), 4.0f) * envRad(max(-p.y, 0.0f), reflect(rd, n), U);
    } else if (tSurf < 85.0f) {
        float3 p = ro + rd * tSurf;
        float h0 = surfT.sample(sLin, p.xz / U.caus.x, level(0)).x;
        float t2 = (h0 - ro.y) / rd.y;
        p = ro + rd * t2; tEnd = t2;
        col = surfaceRadiance(p, rd, t2, SCN);
    }
    float jit = rnd(seed);
    float3 Lin = inscatter(ro, rd, tEnd, jit, fin ? 48 : 24, SCN);
    col = col * exp(-SIG_T * tEnd) + Lin;
    col += torchBeam(ro, rd, tEnd, jit, U);
    col += particles(ro, rd, tEnd, pixAng, SCN);
    // --- air bubbles: mirror-bright where the view grazes them (total internal reflection), clear in the middle.
    // The layer is cheap, so it is evaluated at four instants per camera sample: the thin bright rims blur smoothly.
    if (tcnt > 0u) {
        float3 base = col; float3 accB = float3(0.0f);
        for (int m = 0; m < 4; m++) {
            float tb4 = (float(m) + fract(jit + 0.37f * float(m))) * 0.25f * U.fade.y;
            float3 rob = ro + U.camVel.xyz * (tb4 - tj);
            float3 cc = base;
            for (uint k = 0u; k < tcnt; k++) {
                uint e = tileIdx[toff + k];
                if (!(e & 0x80000000u)) continue;
                Bubble B = bubs[e & 0x7fffffffu];
                float R = B.pos.w; float sqy = B.par.x;                      // vertical squash (<1 = flattened cap)
                float3 tilt = normalize(float3(B.par.y, 1.0f, B.par.z));
                float3 oc = rob - (B.pos.xyz + B.vel.xyz * tb4);
                // scale space along the tilt axis so the ellipsoid becomes a sphere
                float3 o2 = oc + tilt * (dot(oc, tilt) * (1.0f / sqy - 1.0f));
                float3 d2 = rd + tilt * (dot(rd, tilt) * (1.0f / sqy - 1.0f));
                float a = dot(d2, d2), b = dot(o2, d2), c = dot(o2, o2) - R * R;
                float disc = b * b - a * c; if (disc <= 0.0f) continue;
                float tb = (-b - sqrt(disc)) / a;
                if (tb < 0.03f || tb > tEnd) continue;
                float3 pn = o2 + d2 * tb;
                float3 n = normalize(pn + tilt * (dot(pn, tilt) * (1.0f / sqy - 1.0f)));
                float ci = sat(-dot(rd, n));
                float si2 = N_WATER * N_WATER * (1.0f - ci * ci);
                float Fr = 1.0f;
                if (si2 < 1.0f) { float ct = sqrt(1.0f - si2); float rs = (N_WATER * ci - ct) / (N_WATER * ci + ct), rp = (ci - N_WATER * ct) / (ci + N_WATER * ct); Fr = 0.5f * (rs * rs + rp * rp); }
                Fr = sat(Fr + 0.04f);
                float edge = sat(sqrt(disc / a) / max(pixAng * tb * 0.8f, 1e-5f));   // soften bubbles only a pixel or two wide
                edge *= sstepf(0.07f, 0.14f, tb);                                     // a bubble touching the mask would only be a one-frame flash: let it fade out
                float D = max(-(rob.y + rd.y * tb), 0.0f);
                float3 env = envRad(D, reflect(rd, n), U);
                float Tb = exp(-SIG_T.y * tb);
                cc = mix(cc, env * Tb + Lin * (1.0f - Tb), Fr * edge * 0.92f);
            }
            accB += cc;
        }
        col = accB * 0.25f;
    }
    return col;
}

kernel void renderK(texture2d<float, access::read_write> accT [[texture(0)]],
                    texture3d<float> nzT [[texture(1)]],
                    texture3d<float> cauT [[texture(2)]],
                    texture3d<float> cauLo [[texture(3)]],
                    texture2d<float> surfT [[texture(4)]],
                    constant Uniforms& U [[buffer(0)]],
                    const device Inst* insts [[buffer(1)]],
                    const device Cell* cells [[buffer(2)]],
                    const device uint* cellIdx [[buffer(3)]],
                    const device Fish* fishes [[buffer(4)]],
                    const device Bubble* bubs [[buffer(5)]],
                    const device uint* tiles [[buffer(6)]],
                    const device uint* tileIdx [[buffer(7)]],
                    uint2 gid [[thread_position_in_grid]]) {
    uint W = uint(U.res.x), H = uint(U.res.y);
    if (gid.x >= W || gid.y >= H) return;
    int pass = int(U.res.z), spp = int(U.res.w);
    uint frame = uint(U.cnt.w);
    uint seed0 = pcg(gid.x + 4099u * gid.y + 7919u * 1031u * frame);
    float3 sum = float3(0.0f);
    float2 rot = float2(float(pcg(seed0) >> 8), float(pcg(seed0 + 77u) >> 8)) * (1.0f / 16777216.0f);
    float pixAng = 2.0f * U.camU.w / U.res.x;      // rough radians per pixel
    for (int k = 0; k < spp; k++) {
        int si = pass * spp + k;
        uint seed = pcg(seed0 ^ (uint(si) * 0x9E3779B9u));
        // R2 low-discrepancy point, Cranley-Patterson rotated per pixel, warped to a tent filter
        float2 u = fract(rot + float(si + 1) * float2(0.7548776662f, 0.5698402910f));
        float2 j = float2(u.x < 0.5f ? sqrt(2.0f * u.x) - 1.0f : 1.0f - sqrt(2.0f - 2.0f * u.x),
                          u.y < 0.5f ? sqrt(2.0f * u.y) - 1.0f : 1.0f - sqrt(2.0f - 2.0f * u.y));
        float2 px = float2(gid) + 0.5f + j * 0.72f;
        float2 ndc = float2((px.x / U.res.x) * 2.0f - 1.0f, (1.0f - 2.0f * px.y / U.res.y) * U.camF.w);
        float r = max(length(ndc), 1e-6f);
        float th = U.camU.w * atan(r * U.camR.w);
        float3 rd = normalize(U.camF.xyz * cos(th) + (U.camR.xyz * ndc.x + U.camU.xyz * ndc.y) * (sin(th) / r));
        float tj = fract(rot.x * 7.31f + float(si) * 0.6180339887f) * U.fade.y;      // this sample's instant inside the sub-frame
        float3 c = radiance(U.camPos.xyz + U.camVel.xyz * tj, rd, pixAng, gid, seed, tj, SCN, DYN);
        c = min(c, float3(60.0f));
        if (!(c.x >= 0.0f && c.y >= 0.0f && c.z >= 0.0f)) c = float3(0.0f);
        sum += c;
    }
    float4 prev = pass == 0 ? float4(0.0f) : accT.read(gid);
    accT.write(prev + float4(sum, float(spp)), gid);
}
