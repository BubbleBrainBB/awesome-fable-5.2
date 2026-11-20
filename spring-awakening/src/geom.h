// geom.h — procedural mesh helpers (grids, tubes, lathes, blobs).
#pragma once
#include "scene.h"
#include <functional>

// Grid of nu x nv points (index i*nv + j). Normals from finite differences. UV/attr optional.
static inline void addGrid(Mesh& m, int nu, int nv, const std::vector<V3>& P, const std::vector<V2>* UV, const std::vector<float>* A,
                           uint32_t mat, bool wrapU = false, bool wrapV = false, float rnd = 0, bool flip = false, const std::vector<V3>* Nin = nullptr) {
    std::vector<V3> N(nu * nv);
    auto at = [&](int i, int j) -> const V3& {
        if (wrapU) i = (i % nu + nu) % nu; else i = std::max(0, std::min(nu - 1, i));
        if (wrapV) j = (j % nv + nv) % nv; else j = std::max(0, std::min(nv - 1, j));
        return P[i * nv + j];
    };
    if (Nin) N = *Nin;
    else for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        V3 du = at(i + 1, j) - at(i - 1, j), dv = at(i, j + 1) - at(i, j - 1);
        V3 n = cross(du, dv);
        if (dot(n, n) < 1e-30f) { // degenerate (pole/tip): try one-sided
            du = at(i + 1, j) - at(i, j); if (dot(du, du) < 1e-30f) du = at(i, j) - at(i - 1, j);
            dv = at(i, j + 1) - at(i, j); if (dot(dv, dv) < 1e-30f) dv = at(i, j) - at(i, j - 1);
            n = cross(du, dv);
        }
        n = norm(n); N[i * nv + j] = flip ? -n : n;
    }
    int iu = wrapU ? nu : nu - 1, jv = wrapV ? nv : nv - 1;
    for (int i = 0; i < iu; i++) for (int j = 0; j < jv; j++) {
        int i1 = (i + 1) % nu, j1 = (j + 1) % nv;
        int a = i * nv + j, b = i1 * nv + j, c = i1 * nv + j1, d = i * nv + j1;
        auto uv = [&](int k, int ii, int jj) { if (UV) { return (*UV)[k]; } return V2((float)ii / std::max(1, iu), (float)jj / std::max(1, jv)); };
        V2 ua = uv(a, i, j), ub = uv(b, i + 1, j), uc = uv(c, i + 1, j + 1), ud = uv(d, i, j + 1);
        if (UV && wrapV && j1 == 0) { uc.y = ud.y = (*UV)[i * nv + j].y + ((*UV)[i * nv + j].y - (*UV)[i * nv + std::max(0, j - 1)].y); }
        float aa = A ? (*A)[a] : 0, ab = A ? (*A)[b] : 0, ac = A ? (*A)[c] : 0, ad = A ? (*A)[d] : 0;
        if (!flip) {
            if (len(cross(P[b] - P[a], P[c] - P[a])) > 1e-18f) m.tri(P[a], P[b], P[c], N[a], N[b], N[c], ua, ub, uc, aa, ab, ac, mat, rnd);
            if (len(cross(P[c] - P[a], P[d] - P[a])) > 1e-18f) m.tri(P[a], P[c], P[d], N[a], N[c], N[d], ua, uc, ud, aa, ac, ad, mat, rnd);
        } else { // keep the geometric winding consistent with the flipped shading normal
            if (len(cross(P[b] - P[a], P[c] - P[a])) > 1e-18f) m.tri(P[a], P[c], P[b], N[a], N[c], N[b], ua, uc, ub, aa, ac, ab, mat, rnd);
            if (len(cross(P[c] - P[a], P[d] - P[a])) > 1e-18f) m.tri(P[a], P[d], P[c], N[a], N[d], N[c], ua, ud, uc, aa, ad, ac, mat, rnd);
        }
    }
}

// Parallel-transport frames along a polyline.
struct Frames { std::vector<V3> T, N, B; };
static inline Frames ptFrames(const std::vector<V3>& p, V3 n0 = V3(0, 0, 1)) {
    int n = (int)p.size(); Frames f; f.T.resize(n); f.N.resize(n); f.B.resize(n);
    for (int i = 0; i < n; i++) f.T[i] = norm(p[std::min(n - 1, i + 1)] - p[std::max(0, i - 1)]);
    V3 nn = n0 - f.T[0] * dot(n0, f.T[0]);
    if (len(nn) < 1e-4f) { V3 a = std::fabs(f.T[0].y) < 0.9f ? V3(0, 1, 0) : V3(1, 0, 0); nn = cross(a, f.T[0]); }
    nn = norm(nn);
    for (int i = 0; i < n; i++) {
        if (i > 0) { nn = nn - f.T[i] * dot(nn, f.T[i]); nn = norm(nn); }
        f.N[i] = nn; f.B[i] = cross(f.T[i], nn);
    }
    return f;
}
// Tube along path; u = along (0..1 scaled by uScale), v = around (0..1). attr = along 0..1.
static inline void addTube(Mesh& m, const std::vector<V3>& p, const std::vector<float>& r, int sides, uint32_t mat, float rnd = 0, float uScale = 1,
                           const Frames* fin = nullptr, std::function<float(float, float)> rmod = nullptr) {
    int n = (int)p.size(); if (n < 2) return;
    Frames f = fin ? *fin : ptFrames(p);
    std::vector<V3> P(n * sides); std::vector<V2> UV(n * sides); std::vector<float> A(n * sides);
    float L = 0; std::vector<float> acc(n, 0);
    for (int i = 1; i < n; i++) { L += len(p[i] - p[i - 1]); acc[i] = L; }
    for (int i = 0; i < n; i++) for (int j = 0; j < sides; j++) {
        float a = 2 * PI * j / sides; float u = acc[i] / std::max(L, 1e-9f);
        float rr = r[i] * (rmod ? rmod(u, (float)j / sides) : 1.0f);
        P[i * sides + j] = p[i] + (f.N[i] * std::cos(a) + f.B[i] * std::sin(a)) * rr;
        UV[i * sides + j] = V2(u * uScale, (float)j / sides); A[i * sides + j] = u;
    }
    addGrid(m, n, sides, P, &UV, &A, mat, false, true, rnd, true);
}
// Lathe around +Y: profile(u) returns (radius, y). nv around.
static inline void addLathe(Mesh& m, int nu, int nv, std::function<V2(float)> prof, const M34& xf, uint32_t mat, float rnd = 0,
                            std::function<float(float, float)> rmod = nullptr) {
    std::vector<V3> P(nu * nv); std::vector<V2> UV(nu * nv); std::vector<float> A(nu * nv);
    for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        float u = (float)i / (nu - 1), v = (float)j / nv; V2 q = prof(u); float a = 2 * PI * v;
        float rr = q.x * (rmod ? rmod(u, v) : 1.0f);
        P[i * nv + j] = xf.pt(V3(rr * std::cos(a), q.y, rr * std::sin(a)));
        UV[i * nv + j] = V2(u, v); A[i * nv + j] = u;
    }
    addGrid(m, nu, nv, P, &UV, &A, mat, false, true, rnd, false);
}
// Ellipsoid / displaced sphere. disp(dir) multiplies the radius.
static inline void addBlob(Mesh& m, V3 c, V3 rad, int nu, int nv, uint32_t mat, float rnd = 0, std::function<float(V3)> disp = nullptr, const M34* xf = nullptr) {
    std::vector<V3> P(nu * nv); std::vector<V2> UV(nu * nv); std::vector<float> A(nu * nv);
    for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        float u = (float)i / (nu - 1), v = (float)j / nv; float th = PI * u, ph = 2 * PI * v;
        V3 d(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
        float k = disp ? disp(d) : 1.0f;
        V3 q = c + d * rad * k; if (xf) q = xf->pt(q);
        P[i * nv + j] = q; UV[i * nv + j] = V2(u, v); A[i * nv + j] = u;
    }
    addGrid(m, nu, nv, P, &UV, &A, mat, false, true, rnd, true);
}
// Catmull-Rom sampling of a polyline of control points.
static inline std::vector<V3> spline(const std::vector<V3>& c, int n) {
    std::vector<V3> o(n); int k = (int)c.size();
    for (int i = 0; i < n; i++) {
        float t = (float)i / (n - 1) * (k - 1); int s = std::min(k - 2, (int)t); float f = t - s;
        V3 p0 = c[std::max(0, s - 1)], p1 = c[s], p2 = c[s + 1], p3 = c[std::min(k - 1, s + 2)];
        o[i] = 0.5f * ((2 * p1) + (p2 - p0) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * (f * f) + (3 * p1 - p0 - 3 * p2 + p3) * (f * f * f));
    }
    return o;
}
static inline V3 splineAt(const std::vector<V3>& c, float u) {
    int k = (int)c.size(); float t = sat(u) * (k - 1); int s = std::min(k - 2, (int)t); float f = t - s;
    V3 p0 = c[std::max(0, s - 1)], p1 = c[s], p2 = c[s + 1], p3 = c[std::min(k - 1, s + 2)];
    return 0.5f * ((2 * p1) + (p2 - p0) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * (f * f) + (3 * p1 - p0 - 3 * p2 + p3) * (f * f * f));
}
