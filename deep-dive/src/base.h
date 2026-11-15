// base.h — host-side basics: vectors, hashing, parallel loops, the 3-D noise volume, FFT, PNG writer.
#pragma once
#import <Foundation/Foundation.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <complex>
#include <algorithm>
#include <chrono>
#include <dispatch/dispatch.h>
#include <zlib.h>

static const float PI = 3.14159265358979f;
struct V3 {
    float x = 0, y = 0, z = 0;
    V3() {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(V3 o) const { return V3(x + o.x, y + o.y, z + o.z); }
    V3 operator-(V3 o) const { return V3(x - o.x, y - o.y, z - o.z); }
    V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
    V3 operator/(float s) const { return V3(x / s, y / s, z / s); }
    V3& operator+=(V3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(V3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
static inline V3 norm(V3 a) { float l = len(a); return l > 1e-12f ? a / l : V3(0, 1, 0); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
static inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static inline float sstep(float a, float b, float x) { return smooth01((x - a) / (b - a)); }
struct F4 { float x, y, z, w; };
static inline F4 f4(V3 v, float w) { return F4{v.x, v.y, v.z, w}; }

static inline uint32_t pcg(uint32_t v) { uint32_t s = v * 747796405u + 2891336453u; uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
struct RNG {
    uint32_t s;
    explicit RNG(uint32_t seed) : s(pcg(seed * 2654435761u + 12345u)) {}
    float f() { s = pcg(s); return (s >> 8) * (1.0f / 16777216.0f); }
    float range(float a, float b) { return a + (b - a) * f(); }
    float gauss() { float u1 = std::max(f(), 1e-7f), u2 = f(); return sqrtf(-2 * logf(u1)) * cosf(2 * PI * u2); }
};
static inline float hashf(uint32_t a, uint32_t b = 0, uint32_t c = 0) { return (pcg(a ^ pcg(b ^ pcg(c + 0x9e3779b9u))) >> 8) * (1.0f / 16777216.0f); }

template <class F> static void parallelFor(size_t n, F f) {
    dispatch_apply(n, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t i) { f(i); });
}
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// ------------------------------------------------------------ 3-D noise volume (tileable, lattice period 16, 8 texels / cell)
static const int NZ_N = 128, NZ_P = 16;
static std::vector<float> g_nz;          // NZ_N^3 * 4
struct N4 { float x, y, z, w; };
static inline N4 noise4(float x, float y, float z) {
    const float sc = (float)NZ_N / NZ_P;
    float fx = x * sc - 0.5f, fy = y * sc - 0.5f, fz = z * sc - 0.5f;
    float flx = floorf(fx), fly = floorf(fy), flz = floorf(fz);
    int ix = (int)flx, iy = (int)fly, iz = (int)flz; float ax = fx - flx, ay = fy - fly, az = fz - flz;
    ax = ax * ax * ax * (ax * (ax * 6 - 15) + 10); ay = ay * ay * ay * (ay * (ay * 6 - 15) + 10); az = az * az * az * (az * (az * 6 - 15) + 10);
    const int m = NZ_N - 1;
    N4 r{0, 0, 0, 0};
    for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
        float w = (i ? ax : 1 - ax) * (j ? ay : 1 - ay) * (k ? az : 1 - az);
        const float* p = &g_nz[(((size_t)((iz + k) & m) * NZ_N + ((iy + j) & m)) * NZ_N + ((ix + i) & m)) * 4];
        r.x += w * p[0]; r.y += w * p[1]; r.z += w * p[2]; r.w += w * p[3];
    }
    return r;
}
static inline V3 latGrad(int x, int y, int z, uint32_t seed) {
    uint32_t h = pcg((uint32_t)(x & (NZ_P - 1)) + 131u * (uint32_t)(y & (NZ_P - 1)) + 17161u * (uint32_t)(z & (NZ_P - 1)) + seed * 2246822519u);
    float u = (h & 0xffff) / 65535.0f * 2 - 1, ph = ((h >> 16) & 0xffff) / 65535.0f * 2 * PI;
    float s = sqrtf(std::max(0.f, 1 - u * u));
    return V3(s * cosf(ph), u, s * sinf(ph));
}
static float perlin3(float x, float y, float z, uint32_t seed) {
    int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };
    float ux = fade(fx), uy = fade(fy), uz = fade(fz), r = 0;
    for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
        V3 g = latGrad(ix + i, iy + j, iz + k, seed);
        float d = g.x * (fx - i) + g.y * (fy - j) + g.z * (fz - k);
        r += d * (i ? ux : 1 - ux) * (j ? uy : 1 - uy) * (k ? uz : 1 - uz);
    }
    return r * 1.55f;
}
static void buildNoise() {
    g_nz.resize((size_t)NZ_N * NZ_N * NZ_N * 4);
    const float sc = (float)NZ_P / NZ_N;
    parallelFor(NZ_N, [&](size_t k) {
        for (int j = 0; j < NZ_N; j++) for (int i = 0; i < NZ_N; i++) {
            float x = (i + 0.5f) * sc, y = (j + 0.5f) * sc, z = (k + 0.5f) * sc;
            float* o = &g_nz[(((size_t)k * NZ_N + j) * NZ_N + i) * 4];
            o[0] = perlin3(x, y, z, 1);
            o[2] = perlin3(x, y, z, 2);
            int cx = (int)floorf(x), cy = (int)floorf(y), cz = (int)floorf(z);
            float best = 1e9f; uint32_t bid = 0;
            for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int ax = cx + dx, ay = cy + dy, az = cz + dz;
                uint32_t h = pcg((uint32_t)(ax & (NZ_P - 1)) + 131u * (uint32_t)(ay & (NZ_P - 1)) + 17161u * (uint32_t)(az & (NZ_P - 1)) + 977u);
                uint32_t h2 = pcg(h), h3 = pcg(h2);
                float px = ax + (h >> 8) / 16777216.0f, py = ay + (h2 >> 8) / 16777216.0f, pz = az + (h3 >> 8) / 16777216.0f;
                float d = (px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z);
                if (d < best) { best = d; bid = pcg(h3); }
            }
            o[1] = sqrtf(best);
            o[3] = (bid >> 8) / 16777216.0f;
        }
    });
}

// ------------------------------------------------------------ FFT (radix 2, in place, e^{+i})
typedef std::complex<float> Cx;
static void fft1(Cx* a, int n, const Cx* tw) {
    for (int i = 1, j = 0; i < n; i++) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (int l = 2; l <= n; l <<= 1) {
        int half = l >> 1, stp = n / l;
        for (int i = 0; i < n; i += l) for (int k = 0; k < half; k++) {
            Cx u = a[i + k], v = a[i + k + half] * tw[k * stp];
            a[i + k] = u + v; a[i + k + half] = u - v;
        }
    }
}
static void fft2(std::vector<Cx>& a, int n) {
    std::vector<Cx> tw(n / 2);
    for (int k = 0; k < n / 2; k++) tw[k] = Cx(cosf(2 * PI * k / n), sinf(2 * PI * k / n));
    const Cx* twp = tw.data(); Cx* ap = a.data();
    parallelFor(n, [=](size_t j) { fft1(ap + j * n, n, twp); });
    const int B = 16;
    parallelFor(n / B, [=](size_t b) {
        std::vector<Cx> col(n);
        for (int i = (int)b * B; i < (int)(b + 1) * B; i++) {
            for (int j = 0; j < n; j++) col[j] = ap[(size_t)j * n + i];
            fft1(col.data(), n, twp);
            for (int j = 0; j < n; j++) ap[(size_t)j * n + i] = col[j];
        }
    });
}

// ------------------------------------------------------------ PNG (8-bit RGB) via zlib
static bool writePNG(const std::string& path, const uint8_t* rgb, int w, int h) {
    std::vector<uint8_t> raw((size_t)h * (w * 3 + 1));
    for (int y = 0; y < h; y++) {
        uint8_t* row = &raw[(size_t)y * (w * 3 + 1)];
        const uint8_t* s = rgb + (size_t)y * w * 3;
        row[0] = 1;                                   // Sub filter
        row[1] = s[0]; row[2] = s[1]; row[3] = s[2];
        for (int i = 3; i < w * 3; i++) row[1 + i] = (uint8_t)(s[i] - s[i - 3]);
    }
    uLongf clen = compressBound(raw.size());
    std::vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), raw.size(), 2) != Z_OK) return false;
    std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb"); if (!f) return false;
    auto be32 = [](uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; };
    auto chunk = [&](const char* type, const uint8_t* data, uint32_t n) {
        uint8_t hd[8]; be32(hd, n); memcpy(hd + 4, type, 4); fwrite(hd, 1, 8, f);
        if (n) fwrite(data, 1, n, f);
        uint32_t c = crc32(0, (const Bytef*)type, 4); if (n) c = crc32(c, data, n);
        uint8_t cb[4]; be32(cb, c); fwrite(cb, 1, 4, f);
    };
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; fwrite(sig, 1, 8, f);
    uint8_t ihdr[13]; be32(ihdr, w); be32(ihdr + 4, h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk("IHDR", ihdr, 13); chunk("IDAT", comp.data(), (uint32_t)clen); chunk("IEND", nullptr, 0);
    bool ok = fclose(f) == 0;
    if (ok) ok = rename(tmp.c_str(), path.c_str()) == 0;
    return ok;
}
