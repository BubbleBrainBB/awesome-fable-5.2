// water.h — the moving sea surface (FFT wave spectrum) and the sunlight traced forward through it.
//   Waves::eval(t)        height + slopes on a periodic 1024^2 tile (25.6 m), every frame
//   Caustics::build()     ~4.2 M sun rays refracted through that surface and splatted onto 36 horizontal slices
//                         (0.35 m .. 66 m deep), sheared along the mean refracted sun direction. Sand, coral, fish,
//                         particles and the light shafts in the water all read their sunlight from this one field.
#pragma once
#include "world.h"

static float g_rollLam = 0.9f, g_rollPow = 1.4f, g_blurA = 0.006f, g_blurB = 0.0035f;
struct Waves {
    int N = 1024; float L = CAU_L;
    std::vector<Cx> h0; std::vector<float> om;
    std::vector<Cx> H, HX, HZ;
    std::vector<float> surf;          // N*N*4 : h, dh/dx, dh/dz, 0
    void init(uint32_t seed, float rmsSlope, float windDeg) {
        h0.assign((size_t)N * N, Cx(0, 0)); om.assign((size_t)N * N, 0);
        H.resize((size_t)N * N); HX.resize((size_t)N * N); HZ.resize((size_t)N * N); surf.assign((size_t)N * N * 4, 0);
        float wa = windDeg * PI / 180, wx = cosf(wa), wz = sinf(wa);
        const float lamPeak = 3.0f, lamMin = 0.10f, lamMax = 13.0f; float kp = 2 * PI / lamPeak;
        RNG r(seed);
        for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) {
            float g1 = r.gauss(), g2 = r.gauss();
            float kx = 2 * PI / L * (i < N / 2 ? i : i - N), kz = 2 * PI / L * (j < N / 2 ? j : j - N);
            float k = sqrtf(kx * kx + kz * kz); if (k <= 0) continue;
            float lam = 2 * PI / k;
            float amp = 1.0f / (k * k) * std::min(1.0f, powf(lam / g_rollLam, g_rollPow)) * expf(-0.6f * (kp / k) * (kp / k)) * sstep(lamMin * 0.85f, lamMin * 1.5f, lam) * (1 - sstep(lamMax * 0.7f, lamMax, lam));
            float c = (kx * wx + kz * wz) / k;
            float sp = lam > 0.8f ? 2.0f : 0.8f;                       // long waves follow the wind, ripples go everywhere
            float dirw = powf(fabsf(c), sp) * (c > 0 ? 1.0f : 0.22f);
            amp *= sqrtf(dirw);
            h0[(size_t)j * N + i] = Cx(g1, g2) * amp;
            om[(size_t)j * N + i] = sqrtf(9.81f * k);
        }
        eval(0);
        double s2 = 0; for (size_t q = 0; q < (size_t)N * N; q++) s2 += (double)surf[q * 4 + 1] * surf[q * 4 + 1] + (double)surf[q * 4 + 2] * surf[q * 4 + 2];
        float sc = rmsSlope / (float)sqrt(s2 / ((size_t)N * N));
        for (auto& v : h0) v *= sc;
    }
    void eval(float t) {
        int n = N; float dk = 2 * PI / L;
        parallelFor(n, [&](size_t j) {
            for (int i = 0; i < n; i++) {
                size_t q = j * n + i; size_t qm = (size_t)((n - (int)j) % n) * n + (size_t)((n - i) % n);
                float w = om[q] * t; Cx e(cosf(w), sinf(w));
                Cx h = h0[q] * std::conj(e) + std::conj(h0[qm]) * e;
                float kx = dk * (i < n / 2 ? i : i - n), kz = dk * ((int)j < n / 2 ? (int)j : (int)j - n);
                if (i == n / 2) kx = 0; if ((int)j == n / 2) kz = 0;
                H[q] = h; HX[q] = h * Cx(0, kx); HZ[q] = h * Cx(0, kz);
            }
        });
        fft2(H, n); fft2(HX, n); fft2(HZ, n);
        parallelFor(n, [&](size_t j) {
            for (int i = 0; i < n; i++) { size_t q = j * n + i; surf[q * 4] = H[q].real(); surf[q * 4 + 1] = HX[q].real(); surf[q * 4 + 2] = HZ[q].real(); surf[q * 4 + 3] = 0; }
        });
    }
};

static inline bool refractV(V3 d, V3 n, float eta, V3& t) {
    float c = -dot(d, n); float k = 1 - eta * eta * (1 - c * c); if (k < 0) return false;
    t = norm(d * eta + n * (eta * c - sqrtf(k))); return true;
}
static inline float fresnelD(float ci, float eta) {
    ci = clampf(ci, 0, 1); float s2 = eta * eta * (1 - ci * ci); if (s2 >= 1) return 1;
    float ct = sqrtf(1 - s2); float rs = (eta * ci - ct) / (eta * ci + ct), rp = (ci - eta * ct) / (ci + eta * ct);
    return 0.5f * (rs * rs + rp * rp);
}

struct Caustics {
    int N = 1024, K = CAU_K, M = 2, NL = 256;
    std::vector<float> sl;            // K * N * N
    std::vector<__fp16> hi, lo;       // GPU uploads
    std::vector<float> px, pz, bx, bz, pw;
    V3 sunW; float shx = 0, shz = 0;
    void init(V3 sunA) {
        V3 t; refractV(sunA * -1.0f, V3(0, 1, 0), 1.0f / N_WATER, t);
        sunW = t * -1.0f; shx = sunW.x / sunW.y; shz = sunW.z / sunW.y;
        sl.resize((size_t)K * N * N); hi.resize((size_t)K * N * N); lo.resize((size_t)K * NL * NL);
        size_t P = (size_t)N * M; px.resize(P * P); pz.resize(P * P); bx.resize(P * P); bz.resize(P * P); pw.resize(P * P);
    }
    void build(const Waves& w, V3 sunA, float blurGain) {
        const int n = N; const float L = CAU_L; const int P = N * M;
        float Tflat = (1 - fresnelD(sunA.y, 1.0f / N_WATER)) * sunA.y;
        const float* sf = w.surf.data(); const int mask = n - 1;
        // 1. refract one sun ray at each of the P*P sample points
        parallelFor(P, [&](size_t j) {
            for (int i = 0; i < P; i++) {
                float x = (i + 0.5f) / P * L, z = (j + 0.5f) / P * L;
                float fu = x / L * n - 0.5f, fv = z / L * n - 0.5f; float flu = floorf(fu), flv = floorf(fv);
                int i0 = (int)flu & mask, j0 = (int)flv & mask, i1 = (i0 + 1) & mask, j1 = (j0 + 1) & mask; float a = fu - flu, b = fv - flv;
                const float* p00 = sf + ((size_t)j0 * n + i0) * 4, *p10 = sf + ((size_t)j0 * n + i1) * 4, *p01 = sf + ((size_t)j1 * n + i0) * 4, *p11 = sf + ((size_t)j1 * n + i1) * 4;
                float w00 = (1 - a) * (1 - b), w10 = a * (1 - b), w01 = (1 - a) * b, w11 = a * b;
                float h = p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11;
                float sx = p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11;
                float sz = p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11;
                V3 nrm = norm(V3(-sx, 1, -sz));
                size_t q = j * P + i;
                float ci = dot(nrm, sunA); V3 t;
                if (ci <= 0 || !refractV(sunA * -1.0f, nrm, 1.0f / N_WATER, t) || t.y > -0.05f) { pw[q] = 0; px[q] = x; pz[q] = z; bx[q] = bz[q] = 0; continue; }
                float ax = t.x / -t.y, az = t.z / -t.y;
                px[q] = x + ax * h; pz[q] = z + az * h; bx[q] = ax + shx; bz[q] = az + shz;
                pw[q] = ci / nrm.y * (1 - fresnelD(ci, 1.0f / N_WATER)) / (Tflat * M * M);
            }
        });
        // 2. splat onto every depth slice, 3. blur (sun disc + forward scattering widen with depth)
        const size_t PP = (size_t)P * P; const float toTex = n / L;
        parallelFor(K, [&](size_t k) {
            float D = causDepth((float)k);
            float* s = &sl[k * (size_t)n * n]; memset(s, 0, sizeof(float) * (size_t)n * n);
            const float* ppx = px.data(), *ppz = pz.data(), *pbx = bx.data(), *pbz = bz.data(), *ppw = pw.data();
            for (size_t q = 0; q < PP; q++) {
                float fu = (ppx[q] + pbx[q] * D) * toTex - 0.5f, fv = (ppz[q] + pbz[q] * D) * toTex - 0.5f;
                float flu = floorf(fu), flv = floorf(fv); float a = fu - flu, b = fv - flv;
                int i0 = (int)flu & mask, j0 = (int)flv & mask, i1 = (i0 + 1) & mask, j1 = (j0 + 1) & mask;
                float wq = ppw[q];
                s[(size_t)j0 * n + i0] += wq * (1 - a) * (1 - b); s[(size_t)j0 * n + i1] += wq * a * (1 - b);
                s[(size_t)j1 * n + i0] += wq * (1 - a) * b; s[(size_t)j1 * n + i1] += wq * a * b;
            }
            float sigma = (g_blurA + g_blurB * D) * blurGain * toTex;
            int r = std::max(1, (int)ceilf(2.6f * sigma));
            std::vector<float> ker(2 * r + 1); float ks = 0;
            for (int i = -r; i <= r; i++) { ker[i + r] = expf(-0.5f * i * i / (sigma * sigma)); ks += ker[i + r]; }
            for (auto& v : ker) v /= ks;
            std::vector<float> tmp((size_t)n * n);
            for (int j = 0; j < n; j++) {
                const float* row = s + (size_t)j * n; float* o = &tmp[(size_t)j * n];
                for (int i = 0; i < n; i++) { float acc = 0; for (int d = -r; d <= r; d++) acc += ker[d + r] * row[(i + d) & mask]; o[i] = acc; }
            }
            for (int j = 0; j < n; j++) {
                float* o = s + (size_t)j * n;
                for (int i = 0; i < n; i++) o[i] = 0;
                for (int d = -r; d <= r; d++) { const float* row = &tmp[(size_t)((j + d) & mask) * n]; float kw = ker[d + r]; for (int i = 0; i < n; i++) o[i] += kw * row[i]; }
            }
            __fp16* h = &hi[k * (size_t)n * n];
            for (size_t q = 0; q < (size_t)n * n; q++) h[q] = (__fp16)std::min(s[q], 60.0f);
            int f = n / NL; __fp16* l = &lo[k * (size_t)NL * NL];
            for (int j = 0; j < NL; j++) for (int i = 0; i < NL; i++) {
                float acc = 0; for (int b = 0; b < f; b++) for (int a = 0; a < f; a++) acc += s[(size_t)(j * f + b) * n + i * f + a];
                l[(size_t)j * NL + i] = (__fp16)std::min(acc / (f * f), 60.0f);
            }
        });
    }
};
