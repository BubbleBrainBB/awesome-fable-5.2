// atmos.h — physically based clear-sky model, computed on the CPU once per shot.
// Rayleigh + Mie + ozone; transmittance table (Bruneton parametrisation), multiple-scattering
// table (Hillaire 2020), then a sky-view radiance map and 1-D tables the shader uses for
// aerial perspective. Units inside: kilometres. Radiance is per unit of top-of-atmosphere sun irradiance.
#pragma once
#include "common.h"

struct Atmos {
    static constexpr float Rg = 6360.0f, Rt = 6460.0f;
    vec3 bR = v3(5.802e-3f, 13.558e-3f, 33.1e-3f);     // /km
    float bMs = 3.996e-3f, bMe = 4.44e-3f, mieG = 0.80f;
    vec3 bO = v3(0.650e-3f, 1.881e-3f, 0.085e-3f);
    float HR = 8.0f, HM = 1.2f, mieScale = 1.0f;
    static const int TW = 256, TH = 64, MS = 32;
    std::vector<vec3> T, Psi;

    vec3 sigS_R(float h) const { return bR * expf(-h / HR); }
    float sigS_M(float h) const { return bMs * mieScale * expf(-h / HM); }
    vec3 sigE(float h) const {
        float oz = std::max(0.f, 1 - fabsf(h - 25.f) / 15.f);
        return bR * expf(-h / HR) + v3(1, 1, 1) * (bMe * mieScale * expf(-h / HM)) + bO * oz;
    }
    static float distTop(float r, float mu) { return -r * mu + sqrtf(std::max(0.f, r * r * (mu * mu - 1) + Rt * Rt)); }
    static bool hitsGround(float r, float mu) { return mu < 0 && r * r * (mu * mu - 1) + Rg * Rg >= 0; }
    static float distGround(float r, float mu) { return -r * mu - sqrtf(std::max(0.f, r * r * (mu * mu - 1) + Rg * Rg)); }

    vec3 opticalDepthToTop(float r, float mu) const {
        float d = distTop(r, mu); const int n = 48; vec3 od = v3(0, 0, 0);
        for (int i = 0; i < n; i++) { float t = (i + 0.5f) / n * d; float rr = sqrtf(r * r + t * t + 2 * r * mu * t); od += sigE(rr - Rg) * (d / n); }
        return od;
    }
    void build() {
        T.assign(TW * TH, v3(0, 0, 0)); float H = sqrtf(Rt * Rt - Rg * Rg);
        parallel_for(TH, [&](size_t j) {
            for (int i = 0; i < TW; i++) {
                float xmu = (i + 0.5f) / TW, xr = (j + 0.5f) / TH; float rho = H * xr, r = sqrtf(rho * rho + Rg * Rg);
                float dmin = Rt - r, dmax = rho + H, d = dmin + xmu * (dmax - dmin);
                float mu = d == 0 ? 1 : (H * H - rho * rho - d * d) / (2 * r * d); mu = clampf(mu, -1, 1);
                vec3 od = opticalDepthToTop(r, mu); T[j * TW + i] = v3(expf(-od.x), expf(-od.y), expf(-od.z));
            }
        });
        Psi.assign(MS * MS, v3(0, 0, 0));
        parallel_for(MS, [&](size_t j) {
            for (int i = 0; i < MS; i++) {
                float mus = (i + 0.5f) / MS * 2 - 1, h = (j + 0.5f) / MS * (Rt - Rg), r = Rg + h;
                vec3 sun = v3(sqrtf(1 - mus * mus), mus, 0); vec3 L2 = v3(0, 0, 0), fms = v3(0, 0, 0); const int ND = 8; int cnt = 0;
                for (int a = 0; a < ND; a++) for (int b = 0; b < ND * 2; b++) {
                    float ct = 1 - 2 * (a + 0.5f) / ND, st = sqrtf(1 - ct * ct), ph = 2 * PI * (b + 0.5f) / (ND * 2);
                    vec3 dir = v3(st * cosf(ph), ct, st * sinf(ph)); float mu = dir.y; bool gnd = hitsGround(r, mu);
                    float d = gnd ? distGround(r, mu) : distTop(r, mu); const int n = 20; vec3 tr = v3(1, 1, 1);
                    for (int k = 0; k < n; k++) {
                        float t = (k + 0.5f) / n * d; vec3 p = v3(0, r, 0) + dir * t; float rr = length(p), hh = rr - Rg;
                        vec3 se = sigE(hh), ss = sigS_R(hh) + v3(1, 1, 1) * sigS_M(hh);
                        vec3 st_ = v3(expf(-se.x * d / n), expf(-se.y * d / n), expf(-se.z * d / n));
                        float musL = dot(p, sun) / rr; vec3 Ts = transToSun(rr, musL);
                        vec3 integ = (v3(1, 1, 1) - st_) / se;   // analytic segment integration
                        L2 += tr * ss * Ts * (1 / (4 * PI)) * integ; fms += tr * ss * integ; tr *= st_;
                    }
                    if (gnd) { vec3 p = v3(0, r, 0) + dir * d; float rr = length(p); float musL = dot(p, sun) / rr;
                        L2 += tr * transToSun(rr, musL) * std::max(0.f, musL) * (0.3f / PI); }
                    cnt++;
                }
                L2 /= (float)cnt; fms /= (float)cnt;
                Psi[j * MS + i] = L2 / (v3(1, 1, 1) - fms);
            }
        });
    }
    vec3 transLUT(float r, float mu) const {
        float H = sqrtf(Rt * Rt - Rg * Rg), rho = sqrtf(std::max(0.f, r * r - Rg * Rg));
        float d = distTop(r, mu), dmin = Rt - r, dmax = rho + H; float x = (d - dmin) / (dmax - dmin) * TW - 0.5f, y = rho / H * TH - 0.5f;
        x = clampf(x, 0, TW - 1.001f); y = clampf(y, 0, TH - 1.001f); int ix = (int)x, iy = (int)y; float fx = x - ix, fy = y - iy;
        return lerp(lerp(T[iy * TW + ix], T[iy * TW + ix + 1], fx), lerp(T[(iy + 1) * TW + ix], T[(iy + 1) * TW + ix + 1], fx), fy);
    }
    vec3 transToSun(float r, float mu) const {   // with smooth ground shadow
        float cosH = -sqrtf(std::max(0.f, 1 - (Rg / r) * (Rg / r)));
        float vis = smooth(cosH - 0.01f, cosH + 0.01f, mu);
        return vis > 0 ? transLUT(r, std::max(mu, cosH)) * vis : v3(0, 0, 0);
    }
    vec3 psi(float h, float mus) const {
        float x = (mus * 0.5f + 0.5f) * MS - 0.5f, y = h / (Rt - Rg) * MS - 0.5f;
        x = clampf(x, 0, MS - 1.001f); y = clampf(y, 0, MS - 1.001f); int ix = (int)x, iy = (int)y; float fx = x - ix, fy = y - iy;
        return lerp(lerp(Psi[iy * MS + ix], Psi[iy * MS + ix + 1], fx), lerp(Psi[(iy + 1) * MS + ix], Psi[(iy + 1) * MS + ix + 1], fx), fy);
    }
    static float phaseR(float c) { return 3.f / (16 * PI) * (1 + c * c); }
    float phaseM(float c) const { float g = mieG; return 3.f / (8 * PI) * ((1 - g * g) * (1 + c * c)) / ((2 + g * g) * powf(1 + g * g - 2 * g * c, 1.5f)); }

    // sky radiance from altitude h (km) along dir, sun direction sun (unit, y up)
    vec3 skyRadiance(float h, vec3 dir, vec3 sun) const {
        float r = Rg + h, mu = dir.y; bool gnd = hitsGround(r, mu); float d = gnd ? distGround(r, mu) : distTop(r, mu);
        const int n = 40; vec3 tr = v3(1, 1, 1), L = v3(0, 0, 0); float c = dot(dir, sun), pr = phaseR(c), pm = phaseM(c);
        float tprev = 0;
        for (int k = 0; k < n; k++) {
            float u = (k + 1.0f) / n; float t1 = d * u * u, t = 0.5f * (tprev + t1), ds = t1 - tprev; tprev = t1;   // denser near the eye
            vec3 p = v3(0, r, 0) + dir * t; float rr = length(p), hh = rr - Rg; vec3 se = sigE(hh), sr = sigS_R(hh); float sm = sigS_M(hh);
            vec3 st_ = v3(expf(-se.x * ds), expf(-se.y * ds), expf(-se.z * ds)); float musL = dot(p, sun) / rr;
            vec3 S = (sr * pr + v3(1, 1, 1) * (sm * pm)) * transToSun(rr, musL) + (sr + v3(1, 1, 1) * sm) * psi(hh, musL);
            L += tr * S * (v3(1, 1, 1) - st_) / se; tr *= st_;
        }
        if (gnd) { vec3 p = v3(0, r, 0) + dir * d; float rr = length(p); float musL = dot(p, sun) / rr;
            L += tr * (transToSun(rr, musL) * std::max(0.f, musL) + psi(0, musL) * PI) * (0.25f / PI); }
        return L;
    }
};

// Sky-view map: u = azimuth/2pi (atan2(z,x)), v -> elevation = sign(c)*(pi/2)*c^2, c = 2v-1
struct SkyMap {
    static const int W = 512, H = 256;
    std::vector<vec4> px; vec3 irradianceUp;
    static float vToEl(float v) { float c = 2 * v - 1; return (c < 0 ? -1.f : 1.f) * (PI / 2) * c * c; }
    void build(const Atmos& A, float hkm, vec3 sun) {
        px.resize(W * H);
        parallel_for(H, [&](size_t j) {
            float el = vToEl((j + 0.5f) / H);
            for (int i = 0; i < W; i++) { float az = (i + 0.5f) / W * 2 * PI; vec3 d = v3(cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az));
                px[j * W + i] = v4(A.skyRadiance(hkm, d, sun), 1); }
        });
        vec3 E = v3(0, 0, 0); const int n = 64;
        for (int a = 0; a < n; a++) for (int b = 0; b < n * 2; b++) { float ct = (a + 0.5f) / n, st = sqrtf(1 - ct * ct), ph = 2 * PI * (b + 0.5f) / (n * 2);
            E += A.skyRadiance(hkm, v3(st * cosf(ph), ct, st * sinf(ph)), sun) * ct * (2 * PI / (n * n * 2)); }
        irradianceUp = E;
    }
};
