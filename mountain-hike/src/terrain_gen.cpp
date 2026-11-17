// terrain_gen.cpp — landscape evolution model (stream-power incision + threshold hillslopes)
// Produces the height maps the renderer walks on. No external data: everything grows from
// a seeded uplift field. Usage: terrain_gen fine|far out.bin
//   fine: 4096^2 @ 7.5 m (30.7 km)  — the massif we hike on
//   far : 1024^2 @ 96 m (98 km)    — surrounding ranges, lowland hole in the middle
// File format: int32 N, float32 dx, float32 h[N*N] (row-major, z then x), float32 area[N*N]
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

static inline uint32_t hashu(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline float hash2(int x, int y, uint32_t s) { return (hashu(uint32_t(x) * 0x9E3779B1U ^ hashu(uint32_t(y) + s * 0x85EBCA6BU)) >> 8) * (1.0f / 16777216.0f); }
static float vnoise(float x, float y, uint32_t s) {
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    float ux = fx * fx * fx * (fx * (fx * 6 - 15) + 10), uy = fy * fy * fy * (fy * (fy * 6 - 15) + 10);
    float a = hash2(ix, iy, s), b = hash2(ix + 1, iy, s), c = hash2(ix, iy + 1, s), d = hash2(ix + 1, iy + 1, s);
    return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
static float fbm(float x, float y, int oct, uint32_t s) {
    float a = 0.5f, f = 0, n = 0;
    for (int i = 0; i < oct; i++) { f += a * vnoise(x, y, s + i * 17); n += a; a *= 0.5f; float nx = 1.6f * x + 1.2f * y, ny = -1.2f * x + 1.6f * y; x = nx + 3.1f; y = ny + 1.7f; }
    return f / n;
}

struct Params {
    bool far;
    float L;        // domain size, metres
    float base;     // base level
    float target;   // desired summit height
    uint32_t seed;
};

// uplift in [0,1.x], u,v in [0,1]
static float upliftField(const Params& P, float u, float v) {
    float wx = u + 0.08f * (fbm(u * 3 + 5, v * 3, 3, P.seed + 100) - 0.5f);
    float wv = v + 0.08f * (fbm(u * 3, v * 3 + 9, 3, P.seed + 200) - 0.5f);
    float edge = std::min(std::min(u, 1 - u), std::min(v, 1 - v));
    if (P.far) {
        float n = fbm(wx * 5, wv * 5, 4, P.seed + 1);
        float r = fbm(wx * 2.2f + 7, wv * 2.2f + 3, 3, P.seed + 2);
        float U = 0.15f + 1.3f * powf(std::max(0.f, n * 0.6f + r * 0.6f - 0.25f), 1.0f);
        float e = std::min(1.f, edge / 0.06f);
        float dx = u - 0.5f, dy = v - 0.5f;
        float hole = std::min(1.f, std::max(0.f, (sqrtf(dx * dx + dy * dy) - 0.125f) / 0.07f));
        return U * e * hole * hole;
    }
    // main massif: broad plateau of uplift with a dominant central culmination; the
    // drainage network that grows on it decides where ridges and valleys go
    auto blob = [&](float cx, float cy, float sx, float sy, float rot) {
        float dx = wx - cx, dy = wv - cy; float c = cosf(rot), s = sinf(rot);
        float a = (c * dx + s * dy) / sx, b = (-s * dx + c * dy) / sy;
        return expf(-(a * a + b * b));
    };
    float U = 0.42f;
    U += 0.62f * blob(0.50f, 0.52f, 0.20f, 0.15f, 0.5f);
    U += 0.22f * blob(0.30f, 0.30f, 0.16f, 0.10f, 0.9f);
    U += 0.25f * blob(0.72f, 0.70f, 0.15f, 0.10f, -0.3f);
    U += 0.18f * blob(0.25f, 0.72f, 0.10f, 0.13f, 0.2f);
    U += 0.20f * blob(0.76f, 0.28f, 0.10f, 0.12f, -0.6f);
    U *= 0.70f + 0.6f * fbm(wx * 7, wv * 7, 3, P.seed + 3);
    float e = std::min(1.f, edge / 0.16f);
    return U * e * e * (3 - 2 * e);
}
// rock hardness 0..1 (massive blobs, more with altitude)
static float hardField(const Params& P, float u, float v, float h) {
    float n = fbm(u * 22 + 3, v * 22 + 8, 4, P.seed + 40);
    float m = fbm(u * 60 + 1, v * 60 + 2, 3, P.seed + 41);
    float alt = (h - (P.base + 0.55f * (P.target - P.base))) / (0.35f * (P.target - P.base));
    float x = (n - 0.56f) * 5.0f + (m - 0.5f) * 1.5f + std::max(-1.3f, std::min(1.2f, alt)) * 0.42f;
    return std::max(0.f, std::min(1.f, x));
}

struct Grid {
    int N; float dx;
    std::vector<float> h, U, area;
    std::vector<uint32_t> rcv, order;
};

struct Heap {
    std::vector<uint64_t> a;
    void push(uint64_t k) { a.push_back(k); size_t i = a.size() - 1; while (i) { size_t p = (i - 1) >> 1; if (a[p] <= a[i]) break; std::swap(a[p], a[i]); i = p; } }
    uint64_t pop() {
        uint64_t r = a[0]; a[0] = a.back(); a.pop_back(); size_t n = a.size(), i = 0;
        for (;;) { size_t l = 2 * i + 1, rr = l + 1, m = i; if (l < n && a[l] < a[m]) m = l; if (rr < n && a[rr] < a[m]) m = rr; if (m == i) break; std::swap(a[m], a[i]); i = m; }
        return r;
    }
    bool empty() const { return a.empty(); }
};
static inline uint64_t key(float h, uint32_t i) { uint32_t b; memcpy(&b, &h, 4); return (uint64_t(b) << 32) | i; }

static const int DX8[8] = {1, 1, 0, -1, -1, -1, 0, 1}, DY8[8] = {0, 1, 1, 1, 0, -1, -1, -1};

static std::vector<float> hf;
static void flowRoute(Grid& g, uint32_t jitterSeed) {
    int N = g.N; size_t M = (size_t)N * N;
    hf.assign(M, 0); std::vector<uint8_t> done(M, 0);
    g.rcv.assign(M, 0); g.order.clear(); g.order.reserve(M);
    Heap pq; pq.a.reserve(M / 4);
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) if (x == 0 || y == 0 || x == N - 1 || y == N - 1) {
        uint32_t i = y * N + x; hf[i] = g.h[i]; done[i] = 1; g.rcv[i] = i; pq.push(key(hf[i], i));
    }
    while (!pq.empty()) {
        uint64_t k = pq.pop(); uint32_t i = (uint32_t)k; g.order.push_back(i);
        int x = i % N, y = i / N;
        for (int d = 0; d < 8; d++) {
            int nx = x + DX8[d], ny = y + DY8[d]; if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
            uint32_t j = ny * N + nx; if (done[j]) continue; done[j] = 1;
            // tiny deterministic jitter decorrelates channels from the grid axes
            float jit = 0.02f * hash2(nx, ny, jitterSeed);
            hf[j] = std::max(g.h[j] + jit, hf[i] + 1e-3f); g.rcv[j] = i; pq.push(key(hf[j], j));
        }
    }
    // Rho8-style stochastic receivers: among already-lower neighbours pick one with probability
    // ~ slope^1.5, re-drawn every iteration, so planar slopes drain along the true fall line on
    // average instead of along the 8 grid directions.
    #pragma omp parallel for
    for (int y = 1; y < N - 1; y++) for (int x = 1; x < N - 1; x++) {
        uint32_t i = y * N + x; float w[8], sum = 0;
        for (int d = 0; d < 8; d++) { uint32_t j = (y + DY8[d]) * N + x + DX8[d];
            float s = (hf[i] - hf[j]) / ((d & 1) ? 1.41421356f : 1.f); w[d] = s > 0 ? s * sqrtf(s) : 0; sum += w[d]; }
        if (sum <= 0) continue;
        float r = hash2(x, y, jitterSeed * 977 + 13) * sum;
        for (int d = 0; d < 8; d++) { if (w[d] <= 0) continue; r -= w[d]; if (r <= 0) { g.rcv[i] = (y + DY8[d]) * N + x + DX8[d]; break; } }
    }
    g.area.assign(M, g.dx * g.dx);
    for (size_t k = M; k-- > 0;) { uint32_t i = g.order[k]; uint32_t r = g.rcv[i]; if (r != i) g.area[r] += g.area[i]; }
}

static void step(Grid& g, const Params& P, float dt, float Kbase, float mexp, float scaleU, int it, bool thresholds) {
    int N = g.N; size_t M = (size_t)N * N; float dx = g.dx;
    flowRoute(g, P.seed * 31 + it * 7);
    std::vector<float> hard(M);
    #pragma omp parallel for
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) hard[y * N + x] = P.far ? 0.3f : hardField(P, (x + 0.5f) / N, (y + 0.5f) / N, g.h[y * N + x]);
    std::vector<float> tcv(M);
    #pragma omp parallel for
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) { size_t i = (size_t)y * N + x; float u = (x + 0.5f) / N, v = (y + 0.5f) / N;
        float soft = 0.40f + 0.36f * fbm(u * 26 + 11, v * 26 + 5, 6, P.seed + 77) + 0.10f * (fbm(u * 170, v * 170, 2, P.seed + 78) - 0.5f);   // 20..37 deg debris / soil, varied at 80 m .. 1 km
        tcv[i] = soft + 1.15f * hard[i] * hard[i]; }
    for (size_t k = 0; k < M; k++) {
        uint32_t i = g.order[k], r = g.rcv[i]; if (r == i) continue;
        int xi = i % N, xr = r % N, yi = i / N, yr = r / N;
        float dist = dx * ((xi != xr && yi != yr) ? 1.41421356f : 1.f);
        float K = Kbase * (1.0f - 0.65f * hard[i]);
        float c = K * powf(g.area[i], mexp) * dt / dist;
        float hn = (g.h[i] + dt * g.U[i] * scaleU + c * g.h[r]) / (1 + c);

        g.h[i] = hn;
    }
    if (thresholds) {
        // isotropic critical-slope collapse: h <= viscosity solution of |grad u| = tc (fast sweeping)
        for (int sweep = 0; sweep < 8; sweep++) {
            int x0 = (sweep & 1) ? N - 2 : 1, x1 = (sweep & 1) ? 0 : N - 1, sx = (sweep & 1) ? -1 : 1;
            int y0 = (sweep & 2) ? N - 2 : 1, y1 = (sweep & 2) ? 0 : N - 1, sy = (sweep & 2) ? -1 : 1;
            for (int y = y0; y != y1; y += sy) for (int x = x0; x != x1; x += sx) {
                size_t i = (size_t)y * N + x; float a = std::min(g.h[i - 1], g.h[i + 1]), b = std::min(g.h[i - N], g.h[i + N]);
                float f = tcv[i] * dx, u;
                if (fabsf(a - b) >= f) u = std::min(a, b) + f; else u = 0.5f * (a + b + sqrtf(2 * f * f - (a - b) * (a - b)));
                if (u < g.h[i]) g.h[i] = u;
            }
        }
    }
    // hillslope creep on soil-mantled cells
    std::vector<float> t(g.h);
    for (int y = 1; y < N - 1; y++) for (int x = 1; x < N - 1; x++) {
        size_t i = (size_t)y * N + x; float lap = t[i - 1] + t[i + 1] + t[i - N] + t[i + N] - 4 * t[i];
        g.h[i] += 0.30f * (1 - hard[i]) * lap * 0.25f;
    }
}

static void upsample(Grid& g, const Params& P) {
    int N = g.N, N2 = N * 2; std::vector<float> h2((size_t)N2 * N2);
    auto H = [&](int x, int y) { x = std::max(0, std::min(N - 1, x)); y = std::max(0, std::min(N - 1, y)); return g.h[(size_t)y * N + x]; };
    for (int y = 0; y < N2; y++) for (int x = 0; x < N2; x++) {
        float fx = (x + 0.5f) / 2 - 0.5f, fy = (y + 0.5f) / 2 - 0.5f; int ix = (int)floorf(fx), iy = (int)floorf(fy); fx -= ix; fy -= iy;
        // Catmull-Rom
        float wx[4] = {-0.5f * fx + fx * fx - 0.5f * fx * fx * fx, 1 - 2.5f * fx * fx + 1.5f * fx * fx * fx, 0.5f * fx + 2 * fx * fx - 1.5f * fx * fx * fx, -0.5f * fx * fx + 0.5f * fx * fx * fx};
        float wy[4] = {-0.5f * fy + fy * fy - 0.5f * fy * fy * fy, 1 - 2.5f * fy * fy + 1.5f * fy * fy * fy, 0.5f * fy + 2 * fy * fy - 1.5f * fy * fy * fy, -0.5f * fy * fy + 0.5f * fy * fy * fy};
        float s = 0; for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) s += wx[i] * wy[j] * H(ix - 1 + i, iy - 1 + j);
        float rough = (fbm(x * 0.13f, y * 0.13f, 3, P.seed + N2) - 0.5f) * g.dx * 0.18f;
        h2[(size_t)y * N2 + x] = s + rough;
    }
    g.N = N2; g.dx *= 0.5f; g.h.swap(h2);
    g.U.resize((size_t)N2 * N2);
    for (int y = 0; y < N2; y++) for (int x = 0; x < N2; x++) g.U[(size_t)y * N2 + x] = upliftField(P, (x + 0.5f) / N2, (y + 0.5f) / N2);
    for (int y = 0; y < N2; y++) for (int x = 0; x < N2; x++) if (x == 0 || y == 0 || x == N2 - 1 || y == N2 - 1) g.h[(size_t)y * N2 + x] = P.base;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: terrain_gen fine|far out.bin\n"); return 1; }
    Params P; P.far = !strcmp(argv[1], "far");
    int Nfinal;
    if (P.far) { P.L = 98304; P.base = 900; P.target = 3350; P.seed = 771; Nfinal = 1024; }
    else       { P.L = 30720; P.base = 1000; P.target = 2960; P.seed = 4242; Nfinal = 4096; }
    if (argc > 3) Nfinal = atoi(argv[3]);
    Grid g; g.N = 256; g.dx = P.L / g.N;
    size_t M = (size_t)g.N * g.N; g.h.resize(M); g.U.resize(M);
    for (int y = 0; y < g.N; y++) for (int x = 0; x < g.N; x++) {
        float u = (x + 0.5f) / g.N, v = (y + 0.5f) / g.N;
        g.U[y * g.N + x] = upliftField(P, u, v);
        bool b = (x == 0 || y == 0 || x == g.N - 1 || y == g.N - 1);
        g.h[y * g.N + x] = P.base + (b ? 0 : 5.0f + 30.f * fbm(u * 40, v * 40, 4, P.seed + 9));
    }
    const float mexp = 0.45f, Kb = 2.0e-5f; float U0 = 1.6e-3f, scaleU = U0;
    int it = 0;
    // coarse: run to near steady state, then calibrate uplift so the summit reaches the target
    for (int k = 0; k < 160; k++) step(g, P, 8000, Kb, mexp, scaleU, it++, false);
    for (int cal = 0; cal < 3; cal++) {
        float mx = 0; for (float v : g.h) mx = std::max(mx, v);
        float s = (P.target - P.base) / (mx - P.base);
        fprintf(stderr, "calib %d: max %.0f scale %.3f\n", cal, mx, s);
        for (auto& v : g.h) v = P.base + (v - P.base) * s; scaleU *= s;
        for (int k = 0; k < 60; k++) step(g, P, 6000, Kb, mexp, scaleU, it++, true);
    }
    int iters[] = {0, 70, 45, 28, 14}; float dts[] = {0, 5000, 3500, 2200, 1200}; int lvl = 1;
    while (g.N < Nfinal) {
        upsample(g, P);
        for (int k = 0; k < iters[lvl]; k++) step(g, P, dts[lvl], Kb, mexp, scaleU, it++, true);
        float mx = 0; for (float v : g.h) mx = std::max(mx, v);
        fprintf(stderr, "level N=%d done, max %.0f\n", g.N, mx); lvl++;
    }
    { // final gentle calibration (height only; < few %)
        float mx = 0; for (float v : g.h) mx = std::max(mx, v);
        float s = (P.target - P.base) / (mx - P.base); for (auto& v : g.h) v = P.base + (v - P.base) * s;
        fprintf(stderr, "final scale %.4f\n", s);
    }
    flowRoute(g, 12345);
    FILE* f = fopen(argv[2], "wb"); int32_t N = g.N; fwrite(&N, 4, 1, f); fwrite(&g.dx, 4, 1, f);
    fwrite(g.h.data(), 4, g.h.size(), f); fwrite(g.area.data(), 4, g.area.size(), f); fclose(f);
    return 0;
}
