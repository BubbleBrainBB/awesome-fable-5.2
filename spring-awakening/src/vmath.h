// vmath.h — small vector maths, hashing and noise used by the host side.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

static const float PI = 3.14159265358979f;

struct V2 { float x = 0, y = 0; V2() {} V2(float a, float b) : x(a), y(b) {} };
struct V3 {
    float x = 0, y = 0, z = 0;
    V3() {}
    V3(float a) : x(a), y(a), z(a) {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
};
static inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline V3 operator-(V3 a) { return {-a.x, -a.y, -a.z}; }
static inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static inline V3 operator*(float s, V3 a) { return {a.x * s, a.y * s, a.z * s}; }
static inline V3 operator*(V3 a, V3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
static inline V3 operator/(V3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
static inline V3& operator+=(V3& a, V3 b) { a = a + b; return a; }
static inline V3& operator-=(V3& a, V3 b) { a = a - b; return a; }
static inline V3& operator*=(V3& a, float s) { a = a * s; return a; }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline float len(V3 a) { return std::sqrt(dot(a, a)); }
static inline V3 norm(V3 a) { float l = len(a); return l > 1e-20f ? a / l : V3(0, 1, 0); }
static inline V3 vmin(V3 a, V3 b) { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
static inline V3 vmax(V3 a, V3 b) { return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}; }
static inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float sat(float x) { return clampf(x, 0.f, 1.f); }
static inline float smooth(float a, float b, float x) { float t = sat((x - a) / (b - a)); return t * t * (3 - 2 * t); }
static inline float smoother(float t) { t = sat(t); return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float ease(float a, float b, float x) { return smoother((x - a) / (b - a)); }
static inline float fract(float x) { return x - std::floor(x); }

// 3x4 affine matrix, rows.
struct M34 {
    float m[12];
    static M34 identity() { M34 r; float t[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}; for (int i = 0; i < 12; i++) r.m[i] = t[i]; return r; }
    static M34 fromBasis(V3 X, V3 Y, V3 Z, V3 T) { M34 r; float t[12] = {X.x, Y.x, Z.x, T.x, X.y, Y.y, Z.y, T.y, X.z, Y.z, Z.z, T.z}; for (int i = 0; i < 12; i++) r.m[i] = t[i]; return r; }
    static M34 translate(V3 t) { return fromBasis({1, 0, 0}, {0, 1, 0}, {0, 0, 1}, t); }
    static M34 scale(V3 s) { return fromBasis({s.x, 0, 0}, {0, s.y, 0}, {0, 0, s.z}, {0, 0, 0}); }
    static M34 scale(float s) { return scale(V3(s)); }
    static M34 rotAxis(V3 a, float ang) {
        a = norm(a); float c = std::cos(ang), s = std::sin(ang), k = 1 - c;
        V3 X(c + a.x * a.x * k, a.y * a.x * k + a.z * s, a.z * a.x * k - a.y * s);
        V3 Y(a.x * a.y * k - a.z * s, c + a.y * a.y * k, a.z * a.y * k + a.x * s);
        V3 Z(a.x * a.z * k + a.y * s, a.y * a.z * k - a.x * s, c + a.z * a.z * k);
        return fromBasis(X, Y, Z, {0, 0, 0});
    }
    static M34 rotX(float a) { return rotAxis({1, 0, 0}, a); }
    static M34 rotY(float a) { return rotAxis({0, 1, 0}, a); }
    static M34 rotZ(float a) { return rotAxis({0, 0, 1}, a); }
    V3 X() const { return {m[0], m[4], m[8]}; }
    V3 Y() const { return {m[1], m[5], m[9]}; }
    V3 Z() const { return {m[2], m[6], m[10]}; }
    V3 T() const { return {m[3], m[7], m[11]}; }
    V3 pt(V3 p) const { return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]}; }
    V3 dir(V3 p) const { return {m[0] * p.x + m[1] * p.y + m[2] * p.z, m[4] * p.x + m[5] * p.y + m[6] * p.z, m[8] * p.x + m[9] * p.y + m[10] * p.z}; }
    M34 inverse() const {
        V3 a = X(), b = Y(), c = Z();
        float det = dot(a, cross(b, c));
        float id = 1.0f / det;
        V3 r0 = cross(b, c) * id, r1 = cross(c, a) * id, r2 = cross(a, b) * id;
        V3 t = T();
        M34 r;
        r.m[0] = r0.x; r.m[1] = r0.y; r.m[2] = r0.z; r.m[3] = -dot(r0, t);
        r.m[4] = r1.x; r.m[5] = r1.y; r.m[6] = r1.z; r.m[7] = -dot(r1, t);
        r.m[8] = r2.x; r.m[9] = r2.y; r.m[10] = r2.z; r.m[11] = -dot(r2, t);
        return r;
    }
};
static inline M34 operator*(const M34& A, const M34& B) {
    return M34::fromBasis(A.dir(B.X()), A.dir(B.Y()), A.dir(B.Z()), A.pt(B.T()));
}
// frame with +Y along `up` direction d, at position p, uniform scale s, twist about d
static inline M34 frameY(V3 p, V3 d, float s = 1, float twist = 0) {
    d = norm(d);
    V3 a = std::fabs(d.y) < 0.99f ? V3(0, 1, 0) : V3(1, 0, 0);
    V3 x = norm(cross(a, d)), z = cross(x, d);
    float c = std::cos(twist), sn = std::sin(twist);
    V3 x2 = x * c + z * sn, z2 = z * c - x * sn;
    return M34::fromBasis(x2 * s, d * s, z2 * s, p);
}

// ---------- hashing / rng ----------
static inline uint32_t hashU(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline float hash1(uint32_t x) { return (hashU(x) >> 8) * (1.0f / 16777216.0f); }
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed * 747796405u + 2891336453u) {}
    uint32_t u() { s = s * 747796405u + 2891336453u; uint32_t w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
    float f() { return (u() >> 8) * (1.0f / 16777216.0f); }
    float r(float a, float b) { return a + (b - a) * f(); }
    float n() { float u1 = std::max(f(), 1e-7f), u2 = f(); return std::sqrt(-2 * std::log(u1)) * std::cos(2 * PI * u2); }
    V3 dir() { float z = r(-1, 1), a = r(0, 2 * PI), q = std::sqrt(std::max(0.f, 1 - z * z)); return {q * std::cos(a), z, q * std::sin(a)}; }
    V3 disk() { float a = r(0, 2 * PI), q = std::sqrt(f()); return {q * std::cos(a), 0, q * std::sin(a)}; }
};

// ---------- gradient noise (value+simple), fbm, worley ----------
static inline float h3(int x, int y, int z) { return hash1((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u); }
static inline float vnoise(V3 p) {
    int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
    float ux = fx * fx * fx * (fx * (fx * 6 - 15) + 10), uy = fy * fy * fy * (fy * (fy * 6 - 15) + 10), uz = fz * fz * fz * (fz * (fz * 6 - 15) + 10);
    float c000 = h3(ix, iy, iz), c100 = h3(ix + 1, iy, iz), c010 = h3(ix, iy + 1, iz), c110 = h3(ix + 1, iy + 1, iz);
    float c001 = h3(ix, iy, iz + 1), c101 = h3(ix + 1, iy, iz + 1), c011 = h3(ix, iy + 1, iz + 1), c111 = h3(ix + 1, iy + 1, iz + 1);
    float a = lerp(lerp(c000, c100, ux), lerp(c010, c110, ux), uy);
    float b = lerp(lerp(c001, c101, ux), lerp(c011, c111, ux), uy);
    return lerp(a, b, uz) * 2 - 1; // [-1,1]
}
static inline float fbm(V3 p, int oct = 4, float gain = 0.5f, float lac = 2.02f) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < oct; i++) { s += a * vnoise(p); p = p * lac + V3(17.1f, 9.2f, 5.3f); a *= gain; }
    return s;
}
static inline float ridged(V3 p, int oct = 4) {
    float s = 0, a = 0.5f;
    for (int i = 0; i < oct; i++) { s += a * (1 - std::fabs(vnoise(p))); p = p * 2.03f + V3(3.1f, 7.7f, 1.3f); a *= 0.5f; }
    return s;
}
static inline float worley(V3 p, float* f2 = nullptr) {
    int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    float d1 = 1e9f, d2 = 1e9f;
    for (int a = -1; a <= 1; a++) for (int b = -1; b <= 1; b++) for (int c = -1; c <= 1; c++) {
        int x = ix + a, y = iy + b, z = iz + c;
        V3 q(x + h3(x, y, z), y + h3(x + 31, y + 17, z + 5), z + h3(x + 7, y + 3, z + 53));
        float d = len(q - p);
        if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d;
    }
    if (f2) *f2 = d2;
    return d1;
}
