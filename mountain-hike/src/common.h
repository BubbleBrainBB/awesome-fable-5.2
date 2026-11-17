// common.h — host-side basics: vector helpers, RNG, parallel-for
#pragma once
#include "shared.h"
#include <dispatch/dispatch.h>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <functional>

typedef simd_float3 vec3; typedef simd_float2 vec2; typedef simd_float4 vec4;
static inline vec3 v3(float x, float y, float z) { return simd_make_float3(x, y, z); }
static inline vec2 v2(float x, float y) { return simd_make_float2(x, y); }
static inline vec4 v4(vec3 a, float w) { return simd_make_float4(a.x, a.y, a.z, w); }
static inline vec4 v4(float x, float y, float z, float w) { return simd_make_float4(x, y, z, w); }
static inline float dot(vec3 a, vec3 b) { return simd_dot(a, b); }
static inline vec3 cross(vec3 a, vec3 b) { return simd_cross(a, b); }
static inline float length(vec3 a) { return simd_length(a); }
static inline float length(vec2 a) { return simd_length(a); }
static inline vec3 normalize(vec3 a) { return simd_normalize(a); }
static inline vec2 normalize(vec2 a) { return simd_normalize(a); }
static inline vec3 vmin(vec3 a, vec3 b) { return simd_min(a, b); }
static inline vec3 vmax(vec3 a, vec3 b) { return simd_max(a, b); }
static inline vec3 lerp(vec3 a, vec3 b, float t) { return a + (b - a) * t; }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float sat(float x) { return clampf(x, 0, 1); }
static inline float smooth(float e0, float e1, float x) { float t = sat((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
static const float PI = 3.14159265358979f;

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 747796405u + 2891336453u) {}
    uint32_t u() { s = s * 747796405u + 2891336453u; uint32_t w = ((s >> ((s >> 28) + 4)) ^ s) * 277803737u; return (w >> 22) ^ w; }
    float f() { return (u() >> 8) * (1.0f / 16777216.0f); }
    float r(float a, float b) { return a + (b - a) * f(); }
    float g() { float a = f() + 1e-7f, b = f(); return sqrtf(-2 * logf(a)) * cosf(2 * PI * b); }
};

static inline void parallel_for(size_t n, const std::function<void(size_t)>& fn) {
    dispatch_apply(n, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t i) { fn(i); });
}

struct Mat34 { float m[12]; };   // row-major 3x4 affine
static inline Mat34 mat34(vec3 X, vec3 Y, vec3 Z, vec3 T) { return Mat34{{X.x, Y.x, Z.x, T.x, X.y, Y.y, Z.y, T.y, X.z, Y.z, Z.z, T.z}}; }
static inline vec3 xfPoint(const Mat34& a, vec3 p) { return v3(a.m[0] * p.x + a.m[1] * p.y + a.m[2] * p.z + a.m[3], a.m[4] * p.x + a.m[5] * p.y + a.m[6] * p.z + a.m[7], a.m[8] * p.x + a.m[9] * p.y + a.m[10] * p.z + a.m[11]); }
static inline vec3 xfDir(const Mat34& a, vec3 p) { return v3(a.m[0] * p.x + a.m[1] * p.y + a.m[2] * p.z, a.m[4] * p.x + a.m[5] * p.y + a.m[6] * p.z, a.m[8] * p.x + a.m[9] * p.y + a.m[10] * p.z); }
static inline Mat34 invert(const Mat34& a) {
    vec3 c0 = v3(a.m[0], a.m[4], a.m[8]), c1 = v3(a.m[1], a.m[5], a.m[9]), c2 = v3(a.m[2], a.m[6], a.m[10]), t = v3(a.m[3], a.m[7], a.m[11]);
    float det = dot(c0, cross(c1, c2)); vec3 r0 = cross(c1, c2) / det, r1 = cross(c2, c0) / det, r2 = cross(c0, c1) / det;
    return Mat34{{r0.x, r0.y, r0.z, -dot(r0, t), r1.x, r1.y, r1.z, -dot(r1, t), r2.x, r2.y, r2.z, -dot(r2, t)}};
}
