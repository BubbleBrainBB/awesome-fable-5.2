// base.h — vector math, RNG, noise (CPU side)
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
typedef uint32_t u32;
static const float PI = 3.14159265358979f;

struct V2 { float x = 0, y = 0; V2() {} V2(float a, float b) : x(a), y(b) {} };
static inline V2 operator+(V2 a, V2 b) { return V2(a.x + b.x, a.y + b.y); }
static inline V2 operator-(V2 a, V2 b) { return V2(a.x - b.x, a.y - b.y); }
static inline V2 operator*(V2 a, float s) { return V2(a.x * s, a.y * s); }
static inline float dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
static inline float len(V2 a) { return sqrtf(dot(a, a)); }

struct V3 {
  float x = 0, y = 0, z = 0;
  V3() {}
  V3(float a, float b, float c) : x(a), y(b), z(c) {}
  float& operator[](int i) { return (&x)[i]; }
  float operator[](int i) const { return (&x)[i]; }
};
static inline V3 operator+(V3 a, V3 b) { return V3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 operator-(V3 a, V3 b) { return V3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 operator-(V3 a) { return V3(-a.x, -a.y, -a.z); }
static inline V3 operator*(V3 a, float s) { return V3(a.x * s, a.y * s, a.z * s); }
static inline V3 operator*(float s, V3 a) { return a * s; }
static inline V3 operator*(V3 a, V3 b) { return V3(a.x * b.x, a.y * b.y, a.z * b.z); }
static inline V3 operator/(V3 a, float s) { return a * (1.0f / s); }
static inline V3& operator+=(V3& a, V3 b) { a = a + b; return a; }
static inline V3& operator-=(V3& a, V3 b) { a = a - b; return a; }
static inline V3& operator*=(V3& a, float s) { a = a * s; return a; }
static inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 cross(V3 a, V3 b) { return V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float len(V3 a) { return sqrtf(dot(a, a)); }
static inline V3 norm(V3 a) { float l = len(a); return l > 1e-20f ? a * (1.0f / l) : V3(0, 1, 0); }
static inline V3 vmin(V3 a, V3 b) { return V3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)); }
static inline V3 vmax(V3 a, V3 b) { return V3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
static inline float smooth(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
static inline float sq(float x) { return x * x; }

// 3x4 affine matrix, row-major
struct M34 {
  float m[12];
  M34() { static const float I[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}; memcpy(m, I, sizeof m); }
  V3 pt(V3 p) const { return V3(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]); }
  V3 dir(V3 p) const { return V3(m[0] * p.x + m[1] * p.y + m[2] * p.z, m[4] * p.x + m[5] * p.y + m[6] * p.z, m[8] * p.x + m[9] * p.y + m[10] * p.z); }
  V3 origin() const { return V3(m[3], m[7], m[11]); }
};
static inline M34 operator*(const M34& a, const M34& b) {
  M34 r;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      r.m[i * 4 + j] = a.m[i * 4 + 0] * b.m[0 + j] + a.m[i * 4 + 1] * b.m[4 + j] + a.m[i * 4 + 2] * b.m[8 + j] + (j == 3 ? a.m[i * 4 + 3] : 0);
  return r;
}
static inline M34 mTrans(V3 t) { M34 r; r.m[3] = t.x; r.m[7] = t.y; r.m[11] = t.z; return r; }
static inline M34 mScale(V3 s) { M34 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
static inline M34 mScale(float s) { return mScale(V3(s, s, s)); }
static inline M34 mRotX(float a) { M34 r; float c = cosf(a), s = sinf(a); r.m[5] = c; r.m[6] = -s; r.m[9] = s; r.m[10] = c; return r; }
static inline M34 mRotY(float a) { M34 r; float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[2] = s; r.m[8] = -s; r.m[10] = c; return r; }
static inline M34 mRotZ(float a) { M34 r; float c = cosf(a), s = sinf(a); r.m[0] = c; r.m[1] = -s; r.m[4] = s; r.m[5] = c; return r; }
static inline M34 mBasis(V3 X, V3 Y, V3 Z, V3 o) {
  M34 r;
  r.m[0] = X.x; r.m[1] = Y.x; r.m[2] = Z.x; r.m[3] = o.x;
  r.m[4] = X.y; r.m[5] = Y.y; r.m[6] = Z.y; r.m[7] = o.y;
  r.m[8] = X.z; r.m[9] = Y.z; r.m[10] = Z.z; r.m[11] = o.z;
  return r;
}
// frame whose local +Y points along 'up' (arbitrary roll fixed by 'hint')
static inline M34 mAlignY(V3 up, V3 o, V3 hint = V3(0, 0, 1)) {
  V3 Y = norm(up);
  V3 X = cross(Y, hint);
  if (len(X) < 1e-4f) X = cross(Y, V3(1, 0, 0));
  X = norm(X);
  V3 Z = cross(X, Y);
  return mBasis(X, Y, Z, o);
}
// frame whose local +Z points along 'fwd', Y as close to 'up' as possible
static inline M34 mLookZ(V3 fwd, V3 o, V3 up = V3(0, 1, 0)) {
  V3 Z = norm(fwd);
  V3 X = cross(up, Z);
  if (len(X) < 1e-4f) X = cross(V3(1, 0, 0), Z);
  X = norm(X);
  V3 Y = cross(Z, X);
  return mBasis(X, Y, Z, o);
}
static inline M34 mInverse(const M34& a) {
  const float* m = a.m;
  float c00 = m[5] * m[10] - m[6] * m[9], c01 = m[6] * m[8] - m[4] * m[10], c02 = m[4] * m[9] - m[5] * m[8];
  float det = m[0] * c00 + m[1] * c01 + m[2] * c02;
  float id = 1.0f / det;
  M34 r;
  r.m[0] = c00 * id; r.m[1] = (m[2] * m[9] - m[1] * m[10]) * id; r.m[2] = (m[1] * m[6] - m[2] * m[5]) * id;
  r.m[4] = c01 * id; r.m[5] = (m[0] * m[10] - m[2] * m[8]) * id; r.m[6] = (m[2] * m[4] - m[0] * m[6]) * id;
  r.m[8] = c02 * id; r.m[9] = (m[1] * m[8] - m[0] * m[9]) * id; r.m[10] = (m[0] * m[5] - m[1] * m[4]) * id;
  r.m[3] = -(r.m[0] * m[3] + r.m[1] * m[7] + r.m[2] * m[11]);
  r.m[7] = -(r.m[4] * m[3] + r.m[5] * m[7] + r.m[6] * m[11]);
  r.m[11] = -(r.m[8] * m[3] + r.m[9] * m[7] + r.m[10] * m[11]);
  return r;
}

static inline u32 pcg(u32 v) {
  u32 s = v * 747796405u + 2891336453u;
  u32 w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
  return (w >> 22u) ^ w;
}
struct Rng {
  u32 s;
  explicit Rng(u32 seed = 1) : s(pcg(seed * 2654435761u + 12345u)) {}
  u32 next() { s = pcg(s); return s; }
  float f() { return (next() >> 8) * (1.0f / 16777216.0f); }
  float r(float a, float b) { return a + (b - a) * f(); }
  int i(int n) { return (int)(next() % (u32)n); }
  float g() { float a = f() + f() + f() + f(); return (a - 2.0f) * 1.7320508f; } // ~gaussian
  bool chance(float p) { return f() < p; }
};
static inline float hash3i(int x, int y, int z) { return (pcg((u32)x + pcg((u32)y + pcg((u32)z))) >> 8) * (1.0f / 16777216.0f); }
static inline float vnoise(V3 p) {
  int ix = (int)floorf(p.x), iy = (int)floorf(p.y), iz = (int)floorf(p.z);
  float fx = p.x - ix, fy = p.y - iy, fz = p.z - iz;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
  float a = lerpf(lerpf(hash3i(ix, iy, iz), hash3i(ix + 1, iy, iz), fx), lerpf(hash3i(ix, iy + 1, iz), hash3i(ix + 1, iy + 1, iz), fx), fy);
  float b = lerpf(lerpf(hash3i(ix, iy, iz + 1), hash3i(ix + 1, iy, iz + 1), fx), lerpf(hash3i(ix, iy + 1, iz + 1), hash3i(ix + 1, iy + 1, iz + 1), fx), fy);
  return lerpf(a, b, fz);
}
static inline float fbm(V3 p, int oct) {
  float s = 0, a = 0.5f;
  for (int i = 0; i < oct; i++) { s += a * vnoise(p); p = p * 2.03f + V3(17.1f, 3.7f, 9.2f); a *= 0.5f; }
  return s;
}
static inline float noise2(float x, float z, float seed = 0) { return vnoise(V3(x, seed, z)); }
