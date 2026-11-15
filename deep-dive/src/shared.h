// shared.h — scalar-only code compiled BOTH by clang++ (host) and by the Metal shader compiler.
// The includer must define:
//   SH_INLINE            function qualifier
//   CTXD / CTX           extra parameter declaration / pass-through (the noise texture on the GPU, nothing on the CPU)
//   NOISE4(x,y,z)        tileable 3-D noise lookup, lattice period 16; returns something with .x .y .z .w
//                        x: gradient noise A (-1..1)  y: Worley F1 (0..~1)  z: gradient noise B (-1..1)  w: Worley cell id (0..1)

// ---- water constants ----
#define N_WATER 1.333f
#define CAU_K 36            // caustic depth slices
#define CAU_L 25.6f         // caustic / wave tile size in metres

// depth (m) of caustic slice k, and its inverse (fractional slice index for depth D)
SH_INLINE float causDepth(float k) {
    if (k <= 27.0f) return 0.35f + 0.4f * k;
    return 11.15f * pow(1.25f, k - 27.0f);
}
SH_INLINE float causSlice(float D) {
    if (D <= 11.15f) return fmax((D - 0.35f) / 0.4f, 0.0f);
    return fmin(27.0f + log(D / 11.15f) / log(1.25f), (float)(CAU_K - 1));
}

// ---- surge: the slow back-and-forth of the whole water mass (everything soft follows it in phase) ----
SH_INLINE float surgeAmp(float y) { return 0.17f * exp(0.075f * y) + 0.022f; }
SH_INLINE float surgeX(float t) { return sin(t * 1.18f) + 0.35f * sin(t * 2.07f + 1.3f); }
SH_INLINE float surgeZ(float t) { return 0.45f * sin(t * 0.83f + 0.7f) + 0.2f * sin(t * 1.71f + 2.9f); }

// ---- lattice noise, computed (not looked up) so that the geometry is exactly the same on CPU and GPU ----
SH_INLINE uint shHash(uint v) { uint s = v * 747796405u + 2891336453u; uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
SH_INLINE uint shCell(int x, int y, int z, uint seed) { return shHash((shHash(uint(z) * 2654435761u + uint(y)) ^ (uint(x) * 2246822519u)) + seed); }
SH_INLINE float shGrad(uint h, float x, float y, float z) {
    uint g = h & 15u;
    float u = g < 8u ? x : y;
    float v = g < 4u ? y : ((g == 12u || g == 14u) ? x : z);
    return ((g & 1u) == 0u ? u : -u) + ((g & 2u) == 0u ? v : -v);
}
SH_INLINE float shFade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
SH_INLINE float shLerp(float a, float b, float t) { return a + (b - a) * t; }
// gradient noise, roughly -1..1
SH_INLINE float perlinS(float x, float y, float z, uint seed) {
    float fx = floor(x), fy = floor(y), fz = floor(z);
    int ix = int(fx), iy = int(fy), iz = int(fz);
    x -= fx; y -= fy; z -= fz;
    float u = shFade(x), v = shFade(y), w = shFade(z);
    float a = shLerp(shGrad(shCell(ix, iy, iz, seed), x, y, z), shGrad(shCell(ix + 1, iy, iz, seed), x - 1.0f, y, z), u);
    float b = shLerp(shGrad(shCell(ix, iy + 1, iz, seed), x, y - 1.0f, z), shGrad(shCell(ix + 1, iy + 1, iz, seed), x - 1.0f, y - 1.0f, z), u);
    float c = shLerp(shGrad(shCell(ix, iy, iz + 1, seed), x, y, z - 1.0f), shGrad(shCell(ix + 1, iy, iz + 1, seed), x - 1.0f, y, z - 1.0f), u);
    float d = shLerp(shGrad(shCell(ix, iy + 1, iz + 1, seed), x, y - 1.0f, z - 1.0f), shGrad(shCell(ix + 1, iy + 1, iz + 1, seed), x - 1.0f, y - 1.0f, z - 1.0f), u);
    return shLerp(shLerp(a, b, v), shLerp(c, d, v), w);
}
// cellular noise: distance to the nearest feature point (one per unit cell); id = random number of that cell
SH_INLINE float worleyS(float x, float y, float z, uint seed, float& id) {
    float fx = floor(x), fy = floor(y), fz = floor(z);
    int ix = int(fx), iy = int(fy), iz = int(fz);
    float px = x - fx, py = y - fy, pz = z - fz;
    float best = 1e9f; uint bh = 0u;
    for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        uint h = shCell(ix + dx, iy + dy, iz + dz, seed);
        float ex = float(dx) + float(h & 1023u) * (1.0f / 1023.0f) - px;
        float ey = float(dy) + float((h >> 10u) & 1023u) * (1.0f / 1023.0f) - py;
        float ez = float(dz) + float((h >> 20u) & 1023u) * (1.0f / 1023.0f) - pz;
        float d2 = ex * ex + ey * ey + ez * ez;
        if (d2 < best) { best = d2; bh = h; }
    }
    id = float(shHash(bh) >> 8u) * (1.0f / 16777216.0f);
    return sqrt(best);
}

// same, also returning the distance to the second-nearest feature (f2 - f1 is small on the borders between cells)
SH_INLINE float worleyS2(float x, float y, float z, uint seed, float& id, float& f2) {
    float fx = floor(x), fy = floor(y), fz = floor(z);
    int ix = int(fx), iy = int(fy), iz = int(fz);
    float px = x - fx, py = y - fy, pz = z - fz;
    float best = 1e9f, sec = 1e9f; uint bh = 0u;
    for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        uint h = shCell(ix + dx, iy + dy, iz + dz, seed);
        float ex = float(dx) + float(h & 1023u) * (1.0f / 1023.0f) - px;
        float ey = float(dy) + float((h >> 10u) & 1023u) * (1.0f / 1023.0f) - py;
        float ez = float(dz) + float((h >> 20u) & 1023u) * (1.0f / 1023.0f) - pz;
        float d2 = ex * ex + ey * ey + ez * ez;
        if (d2 < best) { sec = best; best = d2; bh = h; } else if (d2 < sec) sec = d2;
    }
    id = float(shHash(bh) >> 8u) * (1.0f / 16777216.0f);
    f2 = sqrt(sec);
    return sqrt(best);
}

// ---- terrain ----
SH_INLINE float sstepf(float a, float b, float x) { float t = fmin(fmax((x - a) / (b - a), 0.0f), 1.0f); return t * t * (3.0f - 2.0f * t); }
SH_INLINE float smaxf(float a, float b, float k) { float h = a - b; return 0.5f * (a + b + sqrt(h * h + k * k)); }
SH_INLINE float edgeX(float z) { return 1.6f * sin(0.13f * z + 0.5f) + 0.8f * sin(0.31f * z + 2.1f); }
// sand channel running seaward across the reef flat (1 in the channel, 0 on the reef)
SH_INLINE float chanMask(float x, float z) {   // analytic only (also used for materials)
    float zc = 0.5f * sin(0.16f * x + 0.4f) + 0.7f + 0.45f * sin(0.83f * x + 1.9f) * sin(0.37f * z + 0.4f);
    float w = 2.5f + 0.5f * sin(0.31f * x + 1.0f) + 0.45f * sin(0.71f * x + 0.57f * z);
    float m = exp(-(z - zc) * (z - zc) / (w * w));
    float ax = sstepf(-34.0f, -27.0f, x) * (1.0f - sstepf(-8.5f, -4.5f, x));
    // a second pocket of sand off to the side
    float dx = x + 13.0f, dz = z - 9.5f;
    float m2 = exp(-(dx * dx * 0.05f + dz * dz * 0.11f));
    return fmin(1.0f, m * ax + m2);
}
// signed distance (conservative) to the sea bed: reef flat, sand channel, reef crest, and the wall
SH_INLINE float terrainSDF(float x, float y, float z CTXD) {
    float s = x - edgeX(z);
    float ch = chanMask(x, z);
    float chs = sstepf(0.12f, 0.90f, ch);      // the bed drops gently into the channel ...
    float hf = -7.5f + 0.6f * sin(0.21f * x + 1.3f) * sin(0.17f * z + 0.4f) + 0.3f * sin(0.53f * x + 0.37f * z + 0.9f);
    hf += 1.1f * sstepf(-8.0f, -1.5f, s);
    float nA = 1.5f * perlinS(x * 0.20f + 3.3f, 1.7f, z * 0.20f, 11u);      // 5 m swells of the reef top (bommies)
    hf += 1.25f * nA * (1.0f - chs);
    float sandH = -8.75f + 0.12f * sin(0.45f * x + 0.3f) + 0.10f * sin(0.7f * z);
    hf = hf + (sandH - hf) * chs;
    float dTop = y - hf;
    float sw = 0.13f * (-y - 7.0f) + 0.9f * sin(0.45f * z + 0.22f * y + 1.0f) + 0.55f * sin(0.85f * y + 0.3f * z + 0.6f) + 0.75f * perlinS(z * 0.37f + 7.0f, y * 0.37f, 3.3f, 17u);
    float dWall = (s - sw) * 0.8f;
    float d = smaxf(dTop, dWall, 0.9f);
    // Relief of the reef rock: big lumps, coral heads (cellular), smaller knobs and - close up - finger-size knobs.
    // The noise is read at the foot point on the smooth base surface, so the relief is a displacement of that
    // surface (lumps can bulge and undercut a little, but nothing can break off and float).
    float rock = 1.0f - sstepf(0.45f, 0.95f, ch);
    if (rock > 0.004f && d < 2.3f) {
        float wg = 0.5f + 0.5f * (dWall - dTop) / sqrt((dWall - dTop) * (dWall - dTop) + 0.81f);
        float nl = 1.0f / sqrt(wg * wg + (1.0f - wg) * (1.0f - wg));
        float dc = fmax(d, -1.0f);
        float qx = x - dc * wg * nl, qy = y - dc * (1.0f - wg) * nl, qz = z;
        float idd;
        float l1 = 1.5f * perlinS(qx * 0.45f, qy * 0.45f, qz * 0.45f, 23u);
        float w2 = worleyS(qx * 1.05f + 5.0f, qy * 1.05f, qz * 1.05f + 2.0f, 5u, idd);
        float w3 = worleyS(qx * 2.9f, qy * 2.9f + 7.0f, qz * 2.9f, 9u, idd);
        float k2 = 1.0f - fmin(w2 * 1.25f, 1.0f); k2 = k2 * k2;
        float k3 = 1.0f - fmin(w3 * 1.25f, 1.0f); k3 = k3 * k3;
        d -= rock * (0.36f * l1 + 0.42f * k2 + 0.13f * k3 - 0.12f);
        if (d < 0.11f) {
            float w4 = worleyS(qx * 8.7f, qy * 8.7f, qz * 8.7f, 31u, idd);
            float k4 = 1.0f - fmin(w4 * 1.3f, 1.0f);
            d -= rock * 0.034f * k4 * k4;
        }
    }
    return d * 0.56f;
}
