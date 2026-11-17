// terrain.h — host side of the terrain: eroded maps, the precise height function, route finding
// for the trail, and baking of the two local clip levels (6.25 cm and 1 m) for a shot.
#pragma once
#include "common.h"
#include <queue>

struct Trail {
    std::vector<vec2> p;      // 1 m spacing
    std::vector<float> s, bed, grade;   // arc length, smoothed bed height, d(bed)/ds
    int n() const { return (int)p.size(); }
    vec2 pos(float ss) const { float f = clampf(ss, 0, s.back() - 1e-3f); int i = (int)f; float t = f - i; return p[i] + (p[i + 1] - p[i]) * t; }
    float bedAt(float ss) const { float f = clampf(ss, 0, s.back() - 1e-3f); int i = (int)f; float t = f - i; return bed[i] + (bed[i + 1] - bed[i]) * t; }
    float gradeAt(float ss) const { float f = clampf(ss, 0, s.back() - 1e-3f); int i = (int)f; float t = f - i; return grade[i] + (grade[i + 1] - grade[i]) * t; }
    vec2 dir(float ss) const { return normalize(pos(ss + 0.5f) - pos(ss - 0.5f)); }
};

struct TrailHit { float d, s, bed, grade, side; };   // distance to centre line, arc length, bed height, grade, signed side

struct Terrain {
    int NF = 0; float dxF = 0; std::vector<float> hF, areaF; std::vector<uint8_t> geoF;   // fine eroded map + geo RGBA
    int NR = 0; float dxR = 0, offR = 0; std::vector<float> hR;                           // far ranges
    Trail trail;
    vec3 O = v3(0, 0, 0);     // shot origin: the GPU works in coordinates relative to it
    // spatial hash for nearest-trail queries
    float tcell = 8.0f; int tgN = 0; std::vector<std::vector<int>> tgrid;
    // clip levels
    static const int NC = 4096;
    float c0x = 0, c0z = 0, c1x = 0, c1z = 0;        // world position of texel (0,0) centre
    static constexpr float d0 = 0.0625f, d1 = 1.0f;
    std::vector<float> h0, h1, w0; std::vector<uint8_t> m0, m1;

    bool load(const char* fine, const char* far) {
        FILE* f = fopen(fine, "rb"); if (!f) return false; int32_t n; fread(&n, 4, 1, f); fread(&dxF, 4, 1, f); NF = n;
        hF.resize((size_t)n * n); areaF.resize((size_t)n * n); fread(hF.data(), 4, hF.size(), f); fread(areaF.data(), 4, areaF.size(), f); fclose(f);
        f = fopen(far, "rb"); if (!f) return false; fread(&n, 4, 1, f); fread(&dxR, 4, 1, f); NR = n; hR.resize((size_t)n * n); fread(hR.data(), 4, hR.size(), f); fclose(f);
        offR = 0.5f * NF * dxF - 0.5f * NR * dxR;
        // Knife-edge crests running diagonally across the 7.5 m grid come out of the erosion model as a row of
        // teeth. Shave only what sticks up above its 5x5 neighbourhood mean (valleys and slopes are untouched).
        for (int rep = 0; rep < 2; rep++) { std::vector<float> src(hF);
            parallel_for(NF - 4, [&](size_t zz) { int z = (int)zz + 2; for (int x = 2; x < NF - 2; x++) { float a = 0; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) a += src[(size_t)(z + j) * NF + x + i]; a /= 25.f;
                float d = src[(size_t)z * NF + x] - a; if (d > 0) hF[(size_t)z * NF + x] -= 0.8f * d; } }); }
        geoF.resize((size_t)NF * NF * 4);
        parallel_for(NF, [&](size_t z) { for (int x = 0; x < NF; x++) {
            int x0 = std::max(0, x - 1), x1 = std::min(NF - 1, x + 1), z0 = std::max(0, (int)z - 1), z1 = std::min(NF - 1, (int)z + 1);
            float gx = (hF[z * NF + x1] - hF[z * NF + x0]) / ((x1 - x0) * dxF), gz = (hF[(size_t)z1 * NF + x] - hF[(size_t)z0 * NF + x]) / ((z1 - z0) * dxF);
            float ny = 1 / sqrtf(1 + gx * gx + gz * gz), steep = 1 - ny;
            float meso = 0.10f + 0.90f * smooth(0.12f, 0.42f, steep);
            float hard = -(0.57f * gx + 0.82f * gz) * ny * 0.5f + 0.5f;      // northness: the sun culminates at azimuth 235 deg, so 'north' is azimuth 55
            float flow = sat((log10f(areaF[z * NF + x]) - 3.3f) / 3.7f);
            uint8_t* g = &geoF[(z * NF + x) * 4]; g[0] = (uint8_t)(meso * 255); g[1] = (uint8_t)(hard * 255); g[2] = (uint8_t)(flow * 255); g[3] = (uint8_t)(ny * 255);
        } });
        return true;
    }
    // cubic B-spline of the fine map
    float fineB(float x, float z) const {
        float fx = x / dxF, fz = z / dxF; int ix = (int)floorf(fx), iz = (int)floorf(fz); fx -= ix; fz -= iz;
        float wx[4] = {(1 - fx) * (1 - fx) * (1 - fx) / 6, (3 * fx * fx * fx - 6 * fx * fx + 4) / 6, (-3 * fx * fx * fx + 3 * fx * fx + 3 * fx + 1) / 6, fx * fx * fx / 6};
        float wz[4] = {(1 - fz) * (1 - fz) * (1 - fz) / 6, (3 * fz * fz * fz - 6 * fz * fz + 4) / 6, (-3 * fz * fz * fz + 3 * fz * fz + 3 * fz + 1) / 6, fz * fz * fz / 6};
        float s = 0;
        for (int j = 0; j < 4; j++) { int zz = std::clamp(iz - 1 + j, 0, NF - 1); float r = 0;
            for (int i = 0; i < 4; i++) { int xx = std::clamp(ix - 1 + i, 0, NF - 1); r += wx[i] * hF[(size_t)zz * NF + xx]; } s += wz[j] * r; }
        return s;
    }
    vec4 geoAt(float x, float z) const {   // bilinear, 0..1
        float fx = clampf(x / dxF, 0, NF - 1.001f), fz = clampf(z / dxF, 0, NF - 1.001f); int ix = (int)fx, iz = (int)fz; fx -= ix; fz -= iz; vec4 r = v4(0, 0, 0, 0);
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { const uint8_t* g = &geoF[((size_t)(iz + j) * NF + ix + i) * 4]; float w = (i ? fx : 1 - fx) * (j ? fz : 1 - fz);
            r += v4(g[0], g[1], g[2], g[3]) * (w / 255.f); }
        return r;
    }
    bool inFine(float x, float z) const { float L = NF * dxF; return x > 2 * dxF && z > 2 * dxF && x < L - 3 * dxF && z < L - 3 * dxF; }
    float farH(float x, float z) const {
        float fx = clampf((x - offR) / dxR, 0, NR - 1.001f), fz = clampf((z - offR) / dxR, 0, NR - 1.001f); int ix = (int)fx, iz = (int)fz; fx -= ix; fz -= iz;
        const float* h = hR.data(); size_t i = (size_t)iz * NR + ix;
        return lerp(lerp(h[i], h[i + 1], fx), lerp(h[i + NR], h[i + NR + 1], fx), fz);
    }
    float baseH(float x, float z) const { return inFine(x, z) ? fineB(x, z) : farH(x, z); }
    // eroded map + meso relief: the terrain as seen from a distance (no trail)
    float macroH(float x, float z) const {
        if (!inFine(x, z)) return farH(x, z);
        float hb = fineB(x, z); vec4 g = geoAt(x, z); float big = smooth(0.25f, 0.62f, g.x); return hb + g.x * MESO_MAX * sh_meso(x, z, hb) + big * sh_bigrock(x, z);
    }
    vec3 macroN(float x, float z, float e = 2.0f) const {
        float hx = macroH(x + e, z) - macroH(x - e, z), hz = macroH(x, z + e) - macroH(x, z - e); return normalize(v3(-hx, 2 * e, -hz));
    }

    // ---------------- route finding ----------------
    // Least-cost path on a 15 m grid: cost grows with trail grade and with the steepness of the
    // slope being crossed, so the path contours and switchbacks like a built trail.
    std::vector<float> djDist; std::vector<int> djPar; int djN = 0; float djDx = 0;
    void dijkstraField(vec2 from) {
        const int st = 2, N = NF / st; float dx = dxF * st; djN = N; djDx = dx;
        auto H = [&](int i) { int x = i % N, z = i / N; return hF[(size_t)(z * st) * NF + x * st]; };
        int a = (int)clampf(from.y / dx, 1, N - 2) * N + (int)clampf(from.x / dx, 1, N - 2);
        djDist.assign((size_t)N * N, 1e30f); djPar.assign((size_t)N * N, -1);
        typedef std::pair<float, int> QE; std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
        djDist[a] = 0; pq.push({0, a});
        static const int ox[16] = {1, 1, 0, -1, -1, -1, 0, 1, 2, 1, -1, -2, -2, -1, 1, 2}, oz[16] = {0, 1, 1, 1, 0, -1, -1, -1, 1, 2, 2, 1, -1, -2, -2, -1};
        while (!pq.empty()) {
            auto [d, i] = pq.top(); pq.pop(); if (d > djDist[i]) continue;
            int x = i % N, z = i / N; float hi = H(i);
            for (int k = 0; k < 16; k++) {
                int nx = x + ox[k], nz = z + oz[k]; if (nx < 2 || nz < 2 || nx >= N - 2 || nz >= N - 2) continue; int j = nz * N + nx;
                float L = dx * sqrtf((float)(ox[k] * ox[k] + oz[k] * oz[k])); float hj = H(j), g = fabsf(hj - hi) / L;
                const uint8_t* geo = &geoF[((size_t)(nz * st) * NF + nx * st) * 4]; float ny = geo[3] / 255.f, slope = sqrtf(std::max(0.f, 1 - ny * ny)) / std::max(0.2f, ny);
                float ar = areaF[(size_t)(nz * st) * NF + nx * st];
                float c = 1 + 0.6f * (g / 0.13f) * (g / 0.13f) + 3.0f * powf(std::max(0.f, g - 0.20f) / 0.05f, 2) + 0.9f * (slope / 0.6f) * (slope / 0.6f) + 6.0f * powf(std::max(0.f, slope - 0.85f) / 0.2f, 2);
                if (ar > 4e5f) c += 250.f / L;
                if (hj < 1520) c += 20;
                float nd = d + L * c; if (nd < djDist[j]) { djDist[j] = nd; djPar[j] = i; pq.push({nd, j}); }
            }
        }
    }
    // Direction-aware search (state = cell x heading) on a 22.5 m grid: turning costs extra, so the
    // route is made of long traverses joined by a few deliberate switchbacks, like a built trail.
    std::vector<vec2> routeDirectional(vec2 from, vec2 to) const {
        const int st = 3, N = NF / st, ND = 16; float dx = dxF * st;
        static const int ox[16] = {1, 2, 1, 1, 0, -1, -1, -2, -1, -2, -1, -1, 0, 1, 1, 2}, oz[16] = {0, 1, 1, 2, 1, 2, 1, 1, 0, -1, -1, -2, -1, -2, -1, -1};   // ordered by angle
        auto H = [&](int i) { int x = i % N, z = i / N; return hF[(size_t)(z * st) * NF + x * st]; };
        int a = (int)clampf(from.y / dx, 2, N - 3) * N + (int)clampf(from.x / dx, 2, N - 3), b = (int)clampf(to.y / dx, 2, N - 3) * N + (int)clampf(to.x / dx, 2, N - 3);
        vec2 mid = (from + to) * 0.5f; float rad = length(from - to) * 0.75f + 1500;
        std::vector<float> dist((size_t)N * N * ND, 1e30f); std::vector<int> par((size_t)N * N * ND, -1);
        typedef std::pair<float, int> QE; std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
        for (int d = 0; d < ND; d++) { dist[(size_t)a * ND + d] = 0; pq.push({0, a * ND + d}); }
        int goal = -1;
        while (!pq.empty()) {
            auto [d, sidx] = pq.top(); pq.pop(); if (d > dist[sidx]) continue; int i = sidx / ND, dir = sidx % ND; if (i == b) { goal = sidx; break; }
            int x = i % N, z = i / N; float hi = H(i);
            for (int k = 0; k < ND; k++) {
                int turn = abs(k - dir); if (turn > 8) turn = 16 - turn; float tp = turn == 0 ? 0 : turn == 1 ? 4 : turn == 2 ? 14 : turn <= 4 ? 45 : 70; if (d == 0) tp = 0;
                int nx = x + ox[k], nz = z + oz[k]; if (nx < 3 || nz < 3 || nx >= N - 3 || nz >= N - 3) continue; int j = nz * N + nx;
                vec2 pj = v2(nx * dx, nz * dx); if (length(pj - mid) > rad) continue;
                float L = dx * sqrtf((float)(ox[k] * ox[k] + oz[k] * oz[k])); float hj = H(j), g = fabsf(hj - hi) / L;
                const uint8_t* geo = &geoF[((size_t)(nz * st) * NF + nx * st) * 4]; float ny = geo[3] / 255.f, slope = sqrtf(std::max(0.f, 1 - ny * ny)) / std::max(0.2f, ny);
                float ar = areaF[(size_t)(nz * st) * NF + nx * st];
                float c = 1 + 0.6f * (g / 0.13f) * (g / 0.13f) + 3.0f * powf(std::max(0.f, g - 0.20f) / 0.05f, 2) + 0.9f * (slope / 0.6f) * (slope / 0.6f) + 6.0f * powf(std::max(0.f, slope - 0.85f) / 0.2f, 2);
                if (ar > 4e5f) c += 250.f / L; if (hj < 1520) c += 20;
                float nd = d + L * c + tp; size_t js = (size_t)j * ND + k; if (nd < dist[js]) { dist[js] = nd; par[js] = sidx; pq.push({nd, (int)js}); }
            }
        }
        std::vector<vec2> path; for (int sidx = goal; sidx >= 0; sidx = par[sidx]) { int i = sidx / ND; path.push_back(v2((i % N) * dx, (i / N) * dx)); if (dist[sidx] == 0) break; }
        return path;   // to -> from
    }
    int djIndex(vec2 p) const { return (int)clampf(p.y / djDx, 1, djN - 2) * djN + (int)clampf(p.x / djDx, 1, djN - 2); }
    std::vector<vec2> pathFrom(vec2 start) const {   // start -> field source
        std::vector<vec2> path; for (int i = djIndex(start); i >= 0; i = djPar[i]) path.push_back(v2((i % djN) * djDx, (i / djN) * djDx)); return path;
    }
    void buildTrail(const std::vector<vec2>& coarse) {
        // resample to 1 m, smooth
        std::vector<vec2> q; for (size_t i = 0; i + 1 < coarse.size(); i++) { float L = length(coarse[i + 1] - coarse[i]); int n = std::max(1, (int)(L)); for (int k = 0; k < n; k++) q.push_back(coarse[i] + (coarse[i + 1] - coarse[i]) * ((float)k / n)); }
        q.push_back(coarse.back());
        for (int it = 0; it < 90; it++) { std::vector<vec2> t(q); for (size_t i = 1; i + 1 < q.size(); i++) q[i] = t[i] * 0.5f + (t[i - 1] + t[i + 1]) * 0.25f; }
        // re-space at exactly 1 m
        trail.p.clear(); trail.p.push_back(q[0]); float acc = 0;
        for (size_t i = 0; i + 1 < q.size(); i++) { vec2 a = q[i], b = q[i + 1]; float L = length(b - a); while (acc + L >= 1.0f) { float t = (1.0f - acc) / L; a = a + (b - a) * t; L = length(b - a); trail.p.push_back(a); acc = 0; } acc += L; }
        finishTrail();
    }
    // bed heights for the current centre line
    void finishTrail() {
        int n = trail.n(); trail.s.resize(n); trail.bed.resize(n); trail.grade.resize(n);
        for (int i = 0; i < n; i++) { trail.s[i] = (float)i; trail.bed[i] = macroH(trail.p[i].x, trail.p[i].y); }
        for (int it = 0; it < 40; it++) { std::vector<float> t(trail.bed); for (int i = 1; i + 1 < n; i++) trail.bed[i] = t[i] * 0.5f + (t[i - 1] + t[i + 1]) * 0.25f; }
        // a trail is cut into the slope, it is not carried on embankments: never sit above the natural ground
        { std::vector<float> nat(n); for (int i = 0; i < n; i++) { vec2 d = normalize(trail.p[std::min(i + 1, n - 1)] - trail.p[std::max(i - 1, 0)]), nn = v2(-d.y, d.x) * 0.6f; vec2 p = trail.p[i];
            // full-bench construction: the tread sits at the level of the natural ground on its downhill edge
            nat[i] = std::min(macroH(p.x, p.y), std::min(macroH(p.x + nn.x, p.y + nn.y), macroH(p.x - nn.x, p.y - nn.y)) + 0.06f); }
          for (int rep = 0; rep < 4; rep++) { for (int i = 0; i < n; i++) trail.bed[i] = std::min(trail.bed[i], nat[i] + 0.04f); for (int it = 0; it < 4; it++) { std::vector<float> t(trail.bed); for (int i = 1; i + 1 < n; i++) trail.bed[i] = t[i] * 0.5f + (t[i - 1] + t[i + 1]) * 0.25f; } }
          for (int i = 0; i < n; i++) trail.bed[i] = std::min(trail.bed[i], nat[i] + 0.10f); }
        for (int i = 0; i < n; i++) { int a = std::max(0, i - 1), b = std::min(n - 1, i + 1); trail.grade[i] = (trail.bed[b] - trail.bed[a]) / (b - a); }
        buildTrailGrid();
    }
    void buildTrailGrid() {
        tgN = (int)(NF * dxF / tcell) + 1; tgrid.assign((size_t)tgN * tgN, {});
        for (int i = 0; i < trail.n(); i++) { int cx = (int)(trail.p[i].x / tcell), cz = (int)(trail.p[i].y / tcell);
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) { int x = cx + dx, z = cz + dz; if (x < 0 || z < 0 || x >= tgN || z >= tgN) continue; tgrid[(size_t)z * tgN + x].push_back(i); } }
    }
    void saveTrail(const char* path) const { FILE* f = fopen(path, "wb"); for (int i = 0; i < trail.n(); i++) { float v[4] = {trail.p[i].x, trail.p[i].y, trail.bed[i], trail.s[i]}; fwrite(v, 4, 4, f); } fclose(f); }
    bool loadTrail(const char* path) {
        FILE* f = fopen(path, "rb"); if (!f) return false; float v[4]; trail = Trail();
        while (fread(v, 4, 4, f) == 4) { trail.p.push_back(v2(v[0], v[1])); trail.bed.push_back(v[2]); trail.s.push_back(v[3]); } fclose(f);
        int n = trail.n(); trail.grade.resize(n); for (int i = 0; i < n; i++) { int a = std::max(0, i - 1), b = std::min(n - 1, i + 1); trail.grade[i] = (trail.bed[b] - trail.bed[a]) / (b - a); }
        buildTrailGrid(); return true;
    }
    // nearest point on the trail (within ~8 m), false if none
    bool trailQuery(float x, float z, TrailHit& out) const {
        int cx = (int)(x / tcell), cz = (int)(z / tcell); if (cx < 0 || cz < 0 || cx >= tgN || cz >= tgN) return false;
        const auto& c = tgrid[(size_t)cz * tgN + cx]; if (c.empty()) return false; float best = 1e30f; vec2 q = v2(x, z); bool ok = false;
        for (int i : c) { if (i + 1 >= trail.n()) continue; vec2 a = trail.p[i], b = trail.p[i + 1], ab = b - a; float t = clampf(simd_dot(q - a, ab) / simd_dot(ab, ab), 0, 1); vec2 pp = a + ab * t; float d = simd_length_squared(q - pp);
            if (d < best) { best = d; out.s = trail.s[i] + t; out.bed = lerp(trail.bed[i], trail.bed[i + 1], t); out.grade = lerp(trail.grade[i], trail.grade[i + 1], t);
                out.side = (ab.x * (q.y - a.y) - ab.y * (q.x - a.x)) > 0 ? 1.f : -1.f; ok = true; } }
        out.d = sqrtf(best); return ok;
    }

    // ---------------- the brook the trail crosses ----------------
    struct Stream { bool on = false; std::vector<vec2> p; std::vector<float> bed; float crossS = 0; vec2 crossP = v2(0, 0); float trailS = 0; float deckA = 0, deckB = 0, deckYA = 0, deckYB = 0; } stream;
    struct StreamHit { float d, s, bed; };
    std::vector<std::vector<int>> sgrid;
    // follow the drainage line through the point where the trail crosses it (largest catchment near trail station ts)
    void buildStream(float ts) {
        int best = -1; float ba = 0; for (int i = (int)ts - 25; i <= (int)ts + 25; i++) { if (i < 0 || i >= trail.n()) continue; float a = areaF[(size_t)(trail.p[i].y / dxF + 0.5f) * NF + (size_t)(trail.p[i].x / dxF + 0.5f)]; if (a > ba) { ba = a; best = i; } }
        if (best < 0 || ba < 2e5f) return; int cx = (int)(trail.p[best].x / dxF + 0.5f), cz = (int)(trail.p[best].y / dxF + 0.5f);
        static const int ox[8] = {1, 1, 0, -1, -1, -1, 0, 1}, oz[8] = {0, 1, 1, 1, 0, -1, -1, -1};
        std::vector<vec2> up, down; int x = cx, z = cz;
        for (int k = 0; k < 30; k++) { int bx = -1, bz = -1; float bh = hF[(size_t)z * NF + x]; for (int d = 0; d < 8; d++) { float h = hF[(size_t)(z + oz[d]) * NF + x + ox[d]]; if (h < bh) { bh = h; bx = x + ox[d]; bz = z + oz[d]; } } if (bx < 0) break; x = bx; z = bz; down.push_back(v2(x * dxF, z * dxF)); }
        x = cx; z = cz;
        for (int k = 0; k < 30; k++) { int bx = -1, bz = -1; float bar = 0, h0 = hF[(size_t)z * NF + x]; for (int d = 0; d < 8; d++) { size_t j = (size_t)(z + oz[d]) * NF + x + ox[d]; if (hF[j] > h0 && areaF[j] > bar) { bar = areaF[j]; bx = x + ox[d]; bz = z + oz[d]; } } if (bx < 0 || bar < 5e4f) break; x = bx; z = bz; up.push_back(v2(x * dxF, z * dxF)); }
        std::vector<vec2> c(up.rbegin(), up.rend()); c.push_back(v2(cx * dxF, cz * dxF)); c.insert(c.end(), down.begin(), down.end());
        // resample at 0.5 m, relax, then pin the line onto the trail crossing
        std::vector<vec2> q; for (size_t i = 0; i + 1 < c.size(); i++) { int n = std::max(1, (int)(length(c[i + 1] - c[i]) / 0.5f)); for (int k = 0; k < n; k++) q.push_back(c[i] + (c[i + 1] - c[i]) * ((float)k / n)); }
        for (int it = 0; it < 400; it++) { std::vector<vec2> t(q); for (size_t i = 1; i + 1 < q.size(); i++) q[i] = t[i] * 0.5f + (t[i - 1] + t[i + 1]) * 0.25f; }
        stream.p = q; int n = (int)q.size(); stream.bed.resize(n);
        for (int i = 0; i < n; i++) stream.bed[i] = macroH(q[i].x, q[i].y);
        for (int i = 1; i < n; i++) stream.bed[i] = std::min(stream.bed[i], stream.bed[i - 1] - 0.01f);                  // always downhill
        for (int it = 0; it < 60; it++) { std::vector<float> t(stream.bed); for (int i = 1; i + 1 < n; i++) stream.bed[i] = t[i] * 0.5f + (t[i - 1] + t[i + 1]) * 0.25f; }
        // pools and small drops: part of the fall is taken in steps
        { std::vector<float> b(stream.bed); for (int i = 0; i < n; i++) { float sm = i * 0.5f; float Lp = 5.5f; float u = sm / Lp + 0.3f * sh_vn2(sm * 0.21f, 7.f, 301U); float fr = u - floorf(u); float st = floorf(u) + smooth(0.78f, 1.0f, fr);
              int a = std::max(0, i - 6), e = std::min(n - 1, i + 6); float slope = (b[a] - b[e]) / ((e - a) * 0.5f); stream.bed[i] = b[i] - 0.55f * slope * Lp * (st - u); } }
        sgrid.assign((size_t)tgN * tgN, {});
        for (int i = 0; i < n; i++) { int gx = (int)(q[i].x / tcell), gz = (int)(q[i].y / tcell); for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) sgrid[(size_t)(gz + dz) * tgN + gx + dx].push_back(i); }
        // where exactly does the trail cross?
        float bd = 1e9f; for (int i = best - 30; i <= best + 30; i++) { if (i < 0 || i >= trail.n()) continue; for (int j = 0; j < n; j += 2) { float d = length(trail.p[i] - q[j]); if (d < bd) { bd = d; stream.trailS = (float)i; stream.crossS = j * 0.5f; stream.crossP = q[j]; } } }
        stream.on = true;
        // deck of the footbridge: between the trail points 3.2 m either side of the crossing
        stream.deckA = stream.trailS - 3.4f; stream.deckB = stream.trailS + 3.4f; float wl = streamBedAt(stream.crossS) - 0.06f;
        stream.deckYA = std::max(trail.bedAt(stream.deckA), wl + 0.75f); stream.deckYB = std::max(trail.bedAt(stream.deckB), wl + 0.75f);
        int e = n - 1; fprintf(stderr, "stream: %d points, crossing at trail s=%.0f, fall %.1f m over %.0f m, deck %.2f..%.2f above water %.2f\n", n, stream.trailS, stream.bed[0] - stream.bed[e], e * 0.5f, stream.deckYA, stream.deckYB, wl);
    }
    float streamBedAt(float ss) const { float f = clampf(ss / 0.5f, 0, stream.bed.size() - 1.001f); int i = (int)f; return lerp(stream.bed[i], stream.bed[i + 1], f - i); }
    bool streamQuery(float x, float z, StreamHit& out) const {
        if (!stream.on) return false; int cx = (int)(x / tcell), cz = (int)(z / tcell); if (cx < 0 || cz < 0 || cx >= tgN || cz >= tgN) return false;
        const auto& c = sgrid[(size_t)cz * tgN + cx]; if (c.empty()) return false; float best = 1e30f; vec2 q = v2(x, z);
        for (int i : c) { if (i + 1 >= (int)stream.p.size()) continue; vec2 a = stream.p[i], b = stream.p[i + 1], ab = b - a; float t = clampf(simd_dot(q - a, ab) / simd_dot(ab, ab), 0, 1); float d = simd_length_squared(q - (a + ab * t)); if (d < best) { best = d; out.s = (i + t) * 0.5f; } }
        out.d = sqrtf(best); out.bed = streamBedAt(out.s); return true;
    }
    // water surface height (absolute), or a large negative number away from the brook
    float waterH(float x, float z) const { StreamHit sh; if (!streamQuery(x, z, sh) || sh.d > 9.0f) return -1e4f; return sh.bed - 0.06f; }
    // height of the bridge deck if (trail station s) is on it, else -inf
    float deckY(float s) const { if (!stream.on || s < stream.deckA || s > stream.deckB) return -1e9f; float u = (s - stream.deckA) / (stream.deckB - stream.deckA); return lerp(stream.deckYA, stream.deckYB, u) + 0.06f; }

    // ---------------- final ground function near a shot ----------------
    // mat: x = trail (1 tread, ~0.5 disturbed soil of cut / fill banks), y = roughness factor (x2 in shader),
    //      z = step riser on the tread, loose scree elsewhere, w = exposed bedrock
    float groundH(float x, float z, bool fineTrail, vec4* mat = nullptr) const {
        float h = macroH(x, z); vec4 m = v4(0, 0.5f, 0, 0);
        if (mat && inFine(x, z)) {
            vec4 g = geoAt(x, z); float alt = h; 
            Eco e = sh_eco(alt, g.w, g.y * 2 - 1, 0.5f, g.z, sh_vn2(x / 260, z / 260, 61U), sh_fbm2(x / 90, z / 90, 3, 62U));
            vec3 n = macroN(x, z, 1.5f); float steep = 1 - n.y; float nmid = sh_fbm2(x / 7.0f, z / 7.0f, 3, 63U);
            float knob = smooth(0.70f, 0.78f, sh_fbm2(x / 31.0f + 4, z / 31.0f, 3, 64U)) * smooth(1950, 2150, alt) * (1 - e.forest);     // bedrock knobs in the turf
            float rock = sat(std::max(std::max(e.rock, knob), smooth(0.30f, 0.46f, steep + 0.12f * (nmid - 0.5f))));
            float scree = sat(e.scree * 1.2f + 0.5f * smooth(0.55f, 0.75f, sh_fbm2(x / 17.0f, z / 17.0f + 9, 3, 65U)) * smooth(2100, 2300, alt) * smooth(0.10f, 0.2f, steep)) * (1 - rock);
            m.w = rock; m.z = scree; m.y = lerp(0.5f, 0.15f, rock);
            { TrailHit tq; if (trailQuery(x, z, tq)) { float kf = smooth(2.0f, 6.0f, tq.d); knob *= kf; rock = sat(std::max(std::max(e.rock, knob), smooth(0.30f, 0.46f, steep + 0.12f * (nmid - 0.5f)))); m.w = rock; m.y = lerp(0.5f, 0.15f, rock); } }
            h += knob * 0.6f;
            if (fineTrail && e.forest > 0.3f) h += 0.05f * sh_roots(x - O.x, z - O.z) * smooth(0.3f, 0.6f, e.forest);
        }
        TrailHit th;
        if (trailQuery(x, z, th) && th.d < 4.0f) {
            float wob = 0.18f * (sh_vn2(x * 0.9f, z * 0.9f, 31U) - 0.5f) + 0.08f * (sh_vn2(x * 3.1f, z * 3.1f, 32U) - 0.5f);
            float halfW = 0.40f + 0.10f * (sh_vn2(th.s * 0.21f, 3.f, 33U) - 0.5f), d = th.d + wob;
            float w = 1 - smooth(halfW, halfW + 0.65f + 0.4f * sh_vn2(th.s * 0.13f, 9.f, 34U), d);
            float bed = th.bed; float riser = 0;
            if (fineTrail) {
                float g = th.grade, ag = fabsf(g);
                float stepAmt = smooth(0.30f, 0.40f, ag);
                if (stepAmt > 0) {     // stone / log steps where it is steep
                    float Lst = clampf(0.19f / ag, 0.45f, 1.3f); float u = th.s / Lst + 0.35f * sh_vn2(th.s * 0.7f, 1.f, 35U);
                    float fr = u - floorf(u); float stair = floorf(u) + smooth(0.88f, 0.99f, fr);
                    bed += stepAmt * g * Lst * (stair - u + 0.4f); riser = stepAmt * smooth(0.78f, 0.86f, fr);
                }
                float e = sat(d / halfW); bed -= 0.05f * (1 - e * e);          // worn-in tread
                bed += 0.035f * (sh_fbm2(x / 0.8f, z / 0.8f, 3, 37U) - 0.5f);
                vec2 ts = sh_treadStones(x - O.x, z - O.z); bed += 0.035f * ts.x * (0.5f + ts.y);
            }
            float cut = fabsf(bed - h) * w; if (fineTrail) h = lerp(h, bed, w); else cut = 0;     // the 1 m level only carries the trail's colour: carving 0.8 m at 1 m sampling aliases into teeth
            float tread = 1 - smooth(halfW * 0.70f, halfW * 1.15f, d + 0.10f * (sh_vn2(x * 6.0f, z * 6.0f, 36U) - 0.5f));
            m.x = std::max(0.5f * smooth(0.10f, 0.35f, cut), tread > 0.02f ? 0.5f + 0.5f * tread : 0.f);
            if (fineTrail) { m.x *= (tread > 0.02f) ? 1.f : (1 - m.w); m.w *= (1 - tread) * lerp(0.35f, 1.0f, smooth(0.6f, 3.0f, th.d)); m.z = lerp(m.z * (1 - w), riser, tread); m.y = lerp(m.y, 0.10f, w); }
            else m.x = 0.75f * (1 - smooth(0.2f, 1.1f, th.d));      // 1 m level: colour only, no relief changes (they alias into teeth on ridge crests)
        }
        StreamHit sq;
        if (streamQuery(x, z, sq) && sq.d < 6.0f) {
            float Wc = 1.35f + 0.6f * sh_vn2(sq.s / 3.0f, 2.f, 302U); float w = 1 - smooth(Wc, Wc + 2.0f, sq.d + 0.35f * (sh_vn2(x / 1.3f, z / 1.3f, 303U) - 0.5f));
            float e = sat(sq.d / Wc); float hb = sq.bed - 0.46f + 0.30f * e * e * e + 0.10f * (sh_fbm2(x / 1.1f, z / 1.1f, 3, 304U) - 0.5f);
            h = lerp(h, std::min(h, hb), w);
            m.x *= (1 - w); m.w *= (1 - w); m.z = std::max(m.z * (1 - w), 0.62f * w); m.y = lerp(m.y, 0.12f, w);
        }
        if (mat) *mat = m;
        return h;
    }
    // ground including the shader's detail layers — what objects stand on
    float surfaceH(float x, float z) const { vec4 m; float h = groundH(x, z, true, &m); float rx = x - O.x, rz = z - O.z;
        h += 2 * m.y * sh_detail(rx, rz, 0); if (m.w > 0.01f) h += m.w * sh_rock(rx, rz, 0);
        float sc = m.x > 0.4f ? 0 : m.z; if (sc > 0.01f) h += sc * 0.3f * sh_rock(rx / 0.3f, rz / 0.3f, 0); return h; }
    vec3 surfaceN(float x, float z, float e = 0.15f) const { float hx = surfaceH(x + e, z) - surfaceH(x - e, z), hz = surfaceH(x, z + e) - surfaceH(x, z - e); return normalize(v3(-hx, 2 * e, -hz)); }

    void bakeClip(float cx, float cz) {
        auto snap = [](float v, float d) { return floorf(v / d) * d; };
        O = v3(snap(cx, 16), floorf(macroH(cx, cz)), snap(cz, 16));
        c0x = snap(cx, d0 * 16) - NC / 2 * d0; c0z = snap(cz, d0 * 16) - NC / 2 * d0; c1x = snap(cx, d1 * 16) - NC / 2 * d1; c1z = snap(cz, d1 * 16) - NC / 2 * d1;
        h0.resize((size_t)NC * NC); h1.resize((size_t)NC * NC); m0.resize((size_t)NC * NC * 4); m1.resize((size_t)NC * NC * 4);
        const int NW = NC / 4; w0.assign((size_t)NW * NW, -1e4f);
        if (stream.on) parallel_for(NW, [&](size_t j) { for (int i = 0; i < NW; i++) { float wv = waterH(c0x + (i * 4 + 1.5f) * d0, c0z + (j * 4 + 1.5f) * d0); w0[j * NW + i] = wv > -1e3f ? wv - O.y : -1e4f; } });
        parallel_for(NC, [&](size_t j) { for (int i = 0; i < NC; i++) {
            vec4 m; size_t k = j * NC + i;
            h1[k] = groundH(c1x + i * d1, c1z + j * d1, false, &m) - O.y; for (int c = 0; c < 4; c++) m1[k * 4 + c] = (uint8_t)(sat(m[c]) * 255 + 0.5f);
            h0[k] = groundH(c0x + i * d0, c0z + j * d0, true, &m) - O.y; for (int c = 0; c < 4; c++) m0[k * 4 + c] = (uint8_t)(sat(m[c]) * 255 + 0.5f);
        } });
    }
};
