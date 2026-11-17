// camera.h — the hiker's head: walks the trail with gait bob, sway, breathing drift and scripted glances
#pragma once

struct GazeKey { float t, yaw, pitch; };          // degrees, relative to the walking direction / gait-neutral pitch
struct SpeedKey { float t, mult; };
struct Shot {
    const char* name = ""; float s0 = 0, dur = 10, speed = 1.1f; std::vector<SpeedKey> speedKeys; std::vector<GazeKey> gaze;
    Look look; float eye = 1.66f; float lateral = 0;     // lateral offset from the centre line (m)
    uint32_t seed = 1; float shake = 1.0f; float t0Film = 0; int spp = 16;
};

static float noise1(float x, uint32_t s) { float f = floorf(x); int i = (int)f; float t = x - f; t = t * t * t * (t * (t * 6 - 15) + 10); float a = sh_u2f(sh_hu((uint32_t)i * 2654435761u + s)), b = sh_u2f(sh_hu((uint32_t)(i + 1) * 2654435761u + s)); return (a + (b - a) * t) * 2 - 1; }
static float fnoise1(float x, uint32_t s) { return 0.6f * noise1(x, s) + 0.3f * noise1(x * 2.13f + 7, s + 11) + 0.1f * noise1(x * 4.7f + 3, s + 23); }

template <class K> static float keyLerp(const std::vector<K>& k, float t, float K::*a) {
    if (k.empty()) return 0; if (t <= k.front().t) return k.front().*a; if (t >= k.back().t) return k.back().*a;
    size_t i = 1; while (k[i].t < t) i++; float u = (t - k[i - 1].t) / (k[i].t - k[i - 1].t); u = u * u * u * (u * (u * 6 - 15) + 10); return k[i - 1].*a + (k[i].*a - k[i - 1].*a) * u;
}

struct Walker {
    const Terrain* T; Shot sh; static constexpr float HZ = 240.f;
    std::vector<float> sTab, phTab, vTab;  // station, step phase and intended speed at HZ
    std::vector<float> yTab; float yS0 = 0; static constexpr float YD = 0.05f;      // smoothed ground height along the path
    float stepLen(float grade) const { return clampf(0.74f - 0.9f * fabsf(grade), 0.42f, 0.74f); }
    void init(const Terrain& T_, const Shot& s) {
        T = &T_; sh = s; int n = (int)((sh.dur + 2) * HZ) + 2; sTab.resize(n); phTab.resize(n); vTab.resize(n); float st = sh.s0, ph = 0;
        for (int i = 0; i < n; i++) { float t = i / HZ; sTab[i] = st; phTab[i] = ph;
            float g = T->trail.gradeAt(st); float mult = sh.speedKeys.empty() ? 1.f : keyLerp(sh.speedKeys, t, &SpeedKey::mult);
            float v = sh.speed * mult * clampf(1.0f - 1.6f * std::max(0.f, g - 0.06f) + 0.4f * std::max(0.f, -g - 0.04f), 0.45f, 1.1f) * (1 + 0.06f * fnoise1(t * 0.4f, sh.seed + 5));
            vTab[i] = v; st += v / HZ; ph += v / stepLen(g) / HZ; }
        // ground profile under the walker
        float sA = sh.s0 - 3, sB = sTab.back() + 12; int m = (int)((sB - sA) / YD) + 1; yS0 = sA; std::vector<float> raw(m);
        for (int i = 0; i < m; i++) { vec2 p = pathXZ(sA + i * YD); raw[i] = std::max(T->surfaceH(p.x, p.y), T->deckY(sA + i * YD)); }
        yTab.resize(m); int r = (int)(0.9f / YD); for (int i = 0; i < m; i++) { float a = 0, w = 0; for (int k = -r; k <= r; k++) { int j = std::clamp(i + k, 0, m - 1); float g = expf(-0.5f * (k * YD / 0.32f) * (k * YD / 0.32f)); a += raw[j] * g; w += g; } yTab[i] = a / w; }
    }
    vec2 pathXZ(float s) const { float se = T->trail.s.back() - 1.5f; if (s > se) { vec2 d = T->trail.dir(se - 2), q = pathXZ(se); return q + d * (s - se); }
        vec2 p = T->trail.pos(s), d = T->trail.dir(s); vec2 n = v2(-d.y, d.x); float lat = sh.lateral + 0.10f * fnoise1(s * 0.11f, sh.seed + 41); return p + n * lat; }
    float groundY(float s) const { s = std::min(s, T->trail.s.back() - 1.5f); float f = clampf((s - yS0) / YD, 0, yTab.size() - 1.001f); int i = (int)f; return lerp(yTab[i], yTab[i + 1], f - i); }
    float station(float t) const { float f = clampf(t * HZ, 0, sTab.size() - 1.001f); int i = (int)f; return lerp(sTab[i], sTab[i + 1], f - i); }
    float phase(float t) const { float f = clampf(t * HZ, 0, phTab.size() - 1.001f); int i = (int)f; return lerp(phTab[i], phTab[i + 1], f - i); }
    float speedAt(float t) const { float a = std::max(t - 0.05f, 0.f); return (station(a + 0.1f) - station(a)) / 0.1f; }
    // gait amplitude (0 standing .. 1 walking) from the *intended* speed. The station table is accumulated in float, so at a
    // station of 6 km the realised speed moves in steps of 0.12 m/s; deriving this from station differences turned every such
    // step into a small head nod while the walker slows down (and halved it in the first frame of every shot).
    float walkAt(float t) const { float f = clampf(t * HZ, 0, vTab.size() - 1.001f); int i = (int)f; return sat(lerp(vTab[i], vTab[i + 1], f - i) / 0.7f); }
    // absolute world camera; caller subtracts the origin
    Cam at(float t) const {
        float s = station(t), ph = phase(t); float walk = walkAt(t);     // 0 when standing
        vec2 xz = pathXZ(s); vec2 dir = normalize(pathXZ(s + 4.5f) - pathXZ(s - 0.5f)); vec2 nrm = v2(-dir.y, dir.x);
        float y = groundY(s) + sh.eye;
        // gait: two vertical bobs per stride, one lateral sway per stride
        y += walk * (-0.021f * cosf(2 * PI * ph) + 0.004f * cosf(4 * PI * ph + 0.7f));
        xz += nrm * (walk * 0.017f * sinf(PI * ph));
        // standing: slow breathing / weight shift
        y += (1 - walk) * 0.004f * sinf(t * 1.6f); xz += nrm * ((1 - walk) * 0.006f * fnoise1(t * 0.5f, sh.seed + 3));
        float heading = atan2f(dir.y, dir.x) * 180 / PI;
        float ahead = (groundY(s + 7.0f) - groundY(s)) / 7.0f; float pitch = atanf(ahead) * 180 / PI * 0.55f - 5.5f * walk - 1.0f;
        float yaw = heading + keyLerp(sh.gaze, t, &GazeKey::yaw); pitch += keyLerp(sh.gaze, t, &GazeKey::pitch);
        yaw += 2.4f * fnoise1(t * 0.21f, sh.seed + 7) + 0.8f * fnoise1(t * 0.9f, sh.seed + 8); pitch += 1.2f * fnoise1(t * 0.27f, sh.seed + 9) + 0.5f * fnoise1(t * 1.1f, sh.seed + 10);
        float roll = 0.6f * fnoise1(t * 0.33f, sh.seed + 12);
        yaw += walk * 0.40f * sinf(PI * ph + 0.5f); roll += walk * 0.55f * sinf(PI * ph); pitch += walk * 0.30f * cosf(2 * PI * ph + 0.5f);
        float k = sh.shake;      // hand-held tremor
        yaw += k * (0.10f * fnoise1(t * 5.3f, sh.seed + 20) + 0.04f * noise1(t * 11.0f, sh.seed + 21)); pitch += k * (0.09f * fnoise1(t * 5.9f, sh.seed + 22) + 0.04f * noise1(t * 12.0f, sh.seed + 23)); roll += k * 0.07f * fnoise1(t * 4.1f, sh.seed + 24);
        return camLook(v3(xz.x, y, xz.y), yaw, pitch, roll);
    }
};
