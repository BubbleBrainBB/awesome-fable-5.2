// core.h — 数学、随机数、噪声。全部自写，无第三方依赖。
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <functional>

static const float PI = 3.14159265358979f;
static const float TAU = 6.28318530717959f;

struct V3 {
    float x, y, z;
    V3() : x(0), y(0), z(0) {}
    V3(float a) : x(a), y(a), z(a) {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return V3(x + o.x, y + o.y, z + o.z); }
    V3 operator-(const V3& o) const { return V3(x - o.x, y - o.y, z - o.z); }
    V3 operator*(const V3& o) const { return V3(x * o.x, y * o.y, z * o.z); }
    V3 operator/(const V3& o) const { return V3(x / o.x, y / o.y, z / o.z); }
    V3 operator*(float s) const { return V3(x * s, y * s, z * s); }
    V3 operator/(float s) const { float i = 1.f / s; return V3(x * i, y * i, z * i); }
    V3 operator-() const { return V3(-x, -y, -z); }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(const V3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float operator[](int i) const { return (&x)[i]; }
    float& operator[](int i) { return (&x)[i]; }
};
static inline V3 operator*(float s, const V3& v) { return v * s; }
static inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(const V3& a, const V3& b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(const V3& a) { return std::sqrt(dot(a, a)); }
static inline float len2(const V3& a) { return dot(a, a); }
static inline V3 norm(const V3& a) { float l = len(a); return l > 0 ? a / l : V3(0, 1, 0); }
static inline V3 vmin(const V3& a, const V3& b) { return V3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
static inline V3 vmax(const V3& a, const V3& b) { return V3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }
static inline V3 mix(const V3& a, const V3& b, float t) { return a + (b - a) * t; }
static inline float mix(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float sat(float x) { return clampf(x, 0.f, 1.f); }
static inline float sqr(float x) { return x * x; }
static inline float smooth(float a, float b, float x) { float t = sat((x - a) / (b - a)); return t * t * (3 - 2 * t); }
static inline float lum(const V3& c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }
static inline float maxc(const V3& c) { return std::max(c.x, std::max(c.y, c.z)); }
static inline V3 vexp(const V3& a) { return V3(std::exp(a.x), std::exp(a.y), std::exp(a.z)); }
static inline V3 vpow(const V3& a, float p) { return V3(std::pow(a.x, p), std::pow(a.y, p), std::pow(a.z, p)); }
static inline float safesqrt(float x) { return std::sqrt(std::max(0.f, x)); }
static inline bool finite3(const V3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

// 由一个方向构造正交基
static inline void basis(const V3& n, V3& t, V3& b) {
    float s = n.z >= 0 ? 1.f : -1.f;
    float a = -1.f / (s + n.z), c = n.x * n.y * a;
    t = V3(1 + s * n.x * n.x * a, s * c, -s * n.x);
    b = V3(c, s + n.y * n.y * a, -n.y);
}
// 绕任意轴旋转
static inline V3 rotAxis(const V3& v, const V3& k, float ang) {
    float c = std::cos(ang), s = std::sin(ang);
    return v * c + cross(k, v) * s + k * (dot(k, v) * (1 - c));
}

// ---------- 随机数 PCG32 ----------
struct RNG {
    uint64_t s, inc;
    RNG(uint64_t seed = 1, uint64_t seq = 1) { s = 0; inc = (seq << 1) | 1; nextU(); s += seed; nextU(); }
    uint32_t nextU() {
        uint64_t o = s; s = o * 6364136223846793005ULL + inc;
        uint32_t x = (uint32_t)(((o >> 18) ^ o) >> 27), r = (uint32_t)(o >> 59);
        return (x >> r) | (x << ((-r) & 31));
    }
    float f() { return (nextU() >> 8) * (1.f / 16777216.f); }
    float range(float a, float b) { return a + (b - a) * f(); }
    float gauss() { float u = std::max(f(), 1e-7f), v = f(); return std::sqrt(-2 * std::log(u)) * std::cos(TAU * v); }
    V3 onSphere() { float z = 1 - 2 * f(), r = safesqrt(1 - z * z), p = TAU * f(); return V3(r * std::cos(p), r * std::sin(p), z); }
};
static inline uint32_t hashU(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline float hashF(uint32_t x) { return (hashU(x) >> 8) * (1.f / 16777216.f); }
static inline uint32_t hash3i(int x, int y, int z) { return hashU((uint32_t)x * 73856093u ^ hashU((uint32_t)y * 19349663u ^ hashU((uint32_t)z * 83492791u + 0x9e3779b9u))); }

// ---------- 梯度噪声 ----------
static inline float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float gradn(uint32_t h, float x, float y, float z) {
    switch (h & 15) {
        case 0: return x + y; case 1: return -x + y; case 2: return x - y; case 3: return -x - y;
        case 4: return x + z; case 5: return -x + z; case 6: return x - z; case 7: return -x - z;
        case 8: return y + z; case 9: return -y + z; case 10: return y - z; case 11: return -y - z;
        case 12: return x + y; case 13: return -x + y; case 14: return -y + z; default: return -y - z;
    }
}
static inline float noise3(V3 p) {  // 约 [-1,1]
    float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    int X = (int)fx, Y = (int)fy, Z = (int)fz;
    float x = p.x - fx, y = p.y - fy, z = p.z - fz;
    float u = fade(x), v = fade(y), w = fade(z);
    float n000 = gradn(hash3i(X, Y, Z), x, y, z), n100 = gradn(hash3i(X + 1, Y, Z), x - 1, y, z);
    float n010 = gradn(hash3i(X, Y + 1, Z), x, y - 1, z), n110 = gradn(hash3i(X + 1, Y + 1, Z), x - 1, y - 1, z);
    float n001 = gradn(hash3i(X, Y, Z + 1), x, y, z - 1), n101 = gradn(hash3i(X + 1, Y, Z + 1), x - 1, y, z - 1);
    float n011 = gradn(hash3i(X, Y + 1, Z + 1), x, y - 1, z - 1), n111 = gradn(hash3i(X + 1, Y + 1, Z + 1), x - 1, y - 1, z - 1);
    return mix(mix(mix(n000, n100, u), mix(n010, n110, u), v), mix(mix(n001, n101, u), mix(n011, n111, u), v), w);
}
static inline float fbm3(V3 p, int oct) {
    float a = 0.5f, s = 0;
    for (int i = 0; i < oct; i++) { s += a * noise3(p); p = p * 2.03f + V3(17.1f, 3.7f, 9.2f); a *= 0.5f; }
    return s;
}
// Worley 胞元噪声：返回到最近特征点的距离（0..~1）
static inline float cell3(V3 p, float* id = nullptr) {
    int X = (int)std::floor(p.x), Y = (int)std::floor(p.y), Z = (int)std::floor(p.z);
    float best = 9; uint32_t bid = 0;
    for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
        uint32_t h = hash3i(X + i, Y + j, Z + k);
        V3 q(X + i + hashF(h), Y + j + hashF(h + 1), Z + k + hashF(h + 2));
        float d = len2(q - p);
        if (d < best) { best = d; bid = h; }
    }
    if (id) *id = hashF(bid + 7);
    return std::sqrt(best);
}
