// post.h — "camera" stage on the CPU: exposure, lens bloom / veiling glare, vignette, filmic tone curve,
// sensor grain, sRGB encode with dither. Input is the linear accumulation buffer (rgb sum, sample count).
#pragma once
#include "common.h"

struct PostParams { float nr = 0; vec2 sunUV = v2(0, 0); float sunVis = 0; float localTM = 0.30f; float exposure = 1; float bloom = 0.06f; float vignette = 0.28f; float grain = 0.0075f; float sat = 1.06f; float contrast = 1.0f; vec3 wb = v3(1, 1, 1); float fade = 1; uint32_t seed = 0; };

static void blurSep(std::vector<vec3>& img, int w, int h, float sigma) {
    int r = (int)ceilf(sigma * 2.5f); std::vector<float> k(2 * r + 1); float sum = 0; for (int i = -r; i <= r; i++) { k[i + r] = expf(-0.5f * i * i / (sigma * sigma)); sum += k[i + r]; } for (auto& v : k) v /= sum;
    std::vector<vec3> tmp(img.size());
    parallel_for(h, [&](size_t y) { for (int x = 0; x < w; x++) { vec3 a = v3(0, 0, 0); for (int i = -r; i <= r; i++) { int xx = std::clamp(x + i, 0, w - 1); a += img[y * w + xx] * k[i + r]; } tmp[y * w + x] = a; } });
    parallel_for(h, [&](size_t y) { for (int x = 0; x < w; x++) { vec3 a = v3(0, 0, 0); for (int i = -r; i <= r; i++) { int yy = std::clamp((int)y + i, 0, h - 1); a += tmp[(size_t)yy * w + x] * k[i + r]; } img[y * w + x] = a; } });
}
static inline float filmic(float x) { x = std::max(0.f, x); return sat((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f)); }

// returns mean log2 luminance (before exposure) for metering
static float postProcess(const vec4* acc, int W, int H, const PostParams& P, std::vector<uint8_t>& out) {
    std::vector<vec3> lin((size_t)W * H); double logsum = 0; 
    std::vector<double> rows(H, 0.0);
    parallel_for(H, [&](size_t y) { double ls = 0; for (int x = 0; x < W; x++) { vec4 a = acc[y * W + x]; vec3 c = v3(a.x, a.y, a.z) / std::max(1.f, a.w); lin[y * W + x] = c;
        float l = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; float wgt = 1; ls += log2f(std::min(l, 60.f) + 1e-4f) * wgt; } rows[y] = ls; });
    for (double r : rows) logsum += r; float meanLog = (float)(logsum / ((double)W * H));
    if (P.nr > 0) {    // in-camera noise reduction for the dim forest clips: small joint-bilateral on log luminance
        std::vector<vec3> src(lin); float sr = 0.35f + 0.5f * P.nr;
        parallel_for(H, [&](size_t y) { for (int x = 0; x < W; x++) { vec3 c0 = src[y * W + x]; float l0 = logf(0.2126f * c0.x + 0.7152f * c0.y + 0.0722f * c0.z + 0.02f / P.exposure); vec3 a = v3(0, 0, 0); float ws = 0;
            for (int j = -2; j <= 2; j++) { int yy = std::clamp((int)y + j, 0, H - 1); for (int i = -2; i <= 2; i++) { int xx = std::clamp(x + i, 0, W - 1); vec3 c = src[(size_t)yy * W + xx]; float l = logf(0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z + 0.02f / P.exposure);
                float w = expf(-(i * i + j * j) / (2 * 1.3f * 1.3f) - (l - l0) * (l - l0) / (2 * sr * sr)); a += c * w; ws += w; } }
            lin[y * W + x] = lerp(c0, a / ws, P.nr); } });
    }
    // bloom at quarter res, two radii, highlights only weigh in naturally because they are large
    int w4 = W / 4, h4 = H / 4; std::vector<vec3> small((size_t)w4 * h4);
    parallel_for(h4, [&](size_t y) { for (int x = 0; x < w4; x++) { vec3 a = v3(0, 0, 0); for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) { vec3 c = lin[(y * 4 + j) * W + x * 4 + i]; a += simd_min(c, v3(60, 60, 60) / P.exposure); } small[y * w4 + x] = a / 16.f; } });
    std::vector<vec3> b1(small), b2(small); float sc = W / 1920.f; blurSep(b1, w4, h4, 2.5f * sc); blurSep(b2, w4, h4, 14.f * sc);
    // veiling glare: very wide, from 1/16 res
    int w16 = w4 / 4, h16 = h4 / 4; std::vector<vec3> b3((size_t)w16 * h16);
    for (int y = 0; y < h16; y++) for (int x = 0; x < w16; x++) { vec3 a = v3(0, 0, 0); for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) a += small[(size_t)(y * 4 + j) * w4 + x * 4 + i]; b3[(size_t)y * w16 + x] = a / 16.f; }
    blurSep(b3, w16, h16, 13.f * sc); blurSep(b3, w16, h16, 13.f * sc);
    // how bright is the sun as seen by the lens (0 when occluded): sample the image at its position
    float sunSeen = 0; vec2 sunPx = v2((P.sunUV.x * 0.5f + 0.5f) * W, (0.5f - P.sunUV.y * 0.5f) * H);
    if (P.sunVis > 0 && sunPx.x >= 3 && sunPx.y >= 3 && sunPx.x < W - 3 && sunPx.y < H - 3) { float a = 0; for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) { vec3 c = lin[(size_t)((int)sunPx.y + j) * W + (int)sunPx.x + i]; a += std::min((c.x + c.y + c.z) / 3 * P.exposure, 150.f) / 150.f; } sunSeen = P.sunVis * sat(a / 49.f * 3.0f); }
    // local tone mapping (what phone / action cameras do): large-scale log-luminance base is compressed
    std::vector<vec3> base((size_t)w4 * h4); float bsum = 0;
    for (size_t i = 0; i < base.size(); i++) { vec3 c = small[i]; float l = log2f(0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z + 1e-4f); base[i] = v3(l, 0, 0); }
    blurSep(base, w4, h4, 22.f * sc); blurSep(base, w4, h4, 22.f * sc);
    for (auto& b : base) bsum += b.x; float bmean = bsum / base.size();
    out.resize((size_t)W * H * 3);
    parallel_for(H, [&](size_t y) {
        Rng rng((uint32_t)(y * 9781 + P.seed * 6271 + 5));
        for (int x = 0; x < W; x++) {
            float fx = (x + 0.5f) / 4 - 0.5f, fy = (y + 0.5f) / 4 - 0.5f; int ix = std::clamp((int)floorf(fx), 0, w4 - 2), iy = std::clamp((int)floorf(fy), 0, h4 - 2); float tx = sat(fx - ix), ty = sat(fy - iy);
            auto bil = [&](const std::vector<vec3>& b) { return lerp(lerp(b[iy * w4 + ix], b[iy * w4 + ix + 1], tx), lerp(b[(iy + 1) * w4 + ix], b[(iy + 1) * w4 + ix + 1], tx), ty); };
            vec3 c = lin[y * W + x] * (1 - P.bloom) + bil(b1) * (P.bloom * 0.45f) + bil(b2) * (P.bloom * 0.55f);
            { float gx = (x + 0.5f) / 16 - 0.5f, gy = (y + 0.5f) / 16 - 0.5f; int jx = std::clamp((int)floorf(gx), 0, w16 - 2), jy = std::clamp((int)floorf(gy), 0, h16 - 2); float ux = sat(gx - jx), uy = sat(gy - jy);
              c += lerp(lerp(b3[jy * w16 + jx], b3[jy * w16 + jx + 1], ux), lerp(b3[(jy + 1) * w16 + jx], b3[(jy + 1) * w16 + jx + 1], ux), uy) * 0.035f; }
            if (sunSeen > 0) {   // lens flare: two soft ghosts and a faint halo mirrored through the image centre
                vec2 pp = v2((x + 0.5f) / W * 2 - 1, (1 - (y + 0.5f) / H * 2) * H / W), sp = v2(P.sunUV.x, P.sunUV.y * H / W);
                float g1 = length(pp + sp * 0.45f), g2 = length(pp + sp * 1.1f), g3 = length(pp - sp * 0.35f), ds = length(pp - sp);
                vec3 fl = v3(0.9f, 1.0f, 0.6f) * (0.020f * smooth(0.11f, 0.09f, g1)) + v3(0.5f, 0.8f, 1.0f) * (0.012f * smooth(0.20f, 0.17f, g2) * smooth(0.10f, 0.16f, g2)) + v3(1.0f, 0.7f, 0.5f) * (0.016f * smooth(0.05f, 0.035f, g3))
                        + v3(1.0f, 0.90f, 0.74f) * (0.20f / (1 + 40 * ds * ds) + 1.1f / (1 + 1800 * ds * ds));
                c += fl * (sunSeen / P.exposure * 1.0f);
            }
            float u = (x + 0.5f) / W * 2 - 1, v = ((y + 0.5f) / H * 2 - 1) * H / W; float r2 = u * u + v * v;
            float lb = bil(base).x; c *= powf(2.f, -P.localTM * clampf(lb - bmean, -5.f, 3.f));
            c *= P.exposure * (1 - P.vignette * r2 * (0.6f + 0.4f * r2)) * P.fade; c *= P.wb;
            float g = rng.g(), gr = P.grain; float l = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
            c += v3(1, 1, 1) * (g * gr * sqrtf(l + 0.004f)) + v3(rng.g(), rng.g(), rng.g()) * (gr * 0.35f * sqrtf(l + 0.004f));
            vec3 t = v3(filmic(c.x), filmic(c.y), filmic(c.z));
            float tl = 0.2126f * t.x + 0.7152f * t.y + 0.0722f * t.z; t = v3(tl, tl, tl) + (t - v3(tl, tl, tl)) * P.sat;
            if (P.contrast != 1) for (int k = 0; k < 3; k++) { float q = sat(t[k]); t[k] = powf(q, P.contrast) / (powf(q, P.contrast) + powf(1 - q, P.contrast) + 1e-9f); }
            for (int k = 0; k < 3; k++) { float q = sat(t[k]); q = q <= 0.0031308f ? 12.92f * q : 1.055f * powf(q, 1 / 2.4f) - 0.055f; out[(y * W + x) * 3 + k] = (uint8_t)std::clamp((int)(q * 255.f + rng.f()), 0, 255); }
        }
    });
    return meanLog;
}
