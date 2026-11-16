// cat.h — the cat's head (camera) simulation and all animated creatures
#pragma once
#include "geom.h"
#include "world3.h"

struct CamPose { V3 pos, R, U, F; float focus = 1, aperture = 0.0022f; };
struct Ripple { float x, z, t0, amp; };

// monotone cubic (PCHIP) track through timed keys
struct Track {
  std::vector<float> t, y, m;
  void add(float tt, float yy) { t.push_back(tt); y.push_back(yy); }
  void build() {
    int n = (int)t.size(); m.assign(n, 0);
    if (n < 2) return;
    std::vector<float> d(n - 1), h(n - 1);
    for (int i = 0; i < n - 1; i++) { h[i] = t[i + 1] - t[i]; d[i] = (y[i + 1] - y[i]) / h[i]; }
    m[0] = d[0]; m[n - 1] = d[n - 2];
    for (int i = 1; i < n - 1; i++) {
      if (d[i - 1] * d[i] <= 0) m[i] = 0;
      else { float w1 = 2 * h[i] + h[i - 1], w2 = h[i] + 2 * h[i - 1]; m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]); }
    }
  }
  float at(double tt) const {
    int n = (int)t.size();
    if (tt <= t[0]) return y[0];
    if (tt >= t[n - 1]) return y[n - 1];
    int i = (int)(std::upper_bound(t.begin(), t.end(), (float)tt) - t.begin()) - 1;
    float h = t[i + 1] - t[i], s = (float)(tt - t[i]) / h, s2 = s * s, s3 = s2 * s;
    return (2 * s3 - 3 * s2 + 1) * y[i] + (s3 - 2 * s2 + s) * h * m[i] + (-2 * s3 + 3 * s2) * y[i + 1] + (s3 - s2) * h * m[i + 1];
  }
};
struct Track3 {
  Track x, y, z;
  void add(float t, V3 p) { x.add(t, p.x); y.add(t, p.y); z.add(t, p.z); }
  void build() { x.build(); y.build(); z.build(); }
  V3 at(double t) const { return V3(x.at(t), y.at(t), z.at(t)); }
  V3 vel(double t) const { return (at(t + 0.01) - at(t - 0.01)) * 50.0f; }
};
static inline float snoise1(float x, float seed) { return vnoise(V3(x, seed, 0.37f)) * 2 - 1; }

enum { G_FWD, G_POINT, G_LADY, G_BEE, G_HUMAN, G_DRAGON, G_KOI, G_BFLY, G_WBFLY };
struct GazeK { float t; int type; V3 p; float om; float roll; float focus; };
struct Koi { float cx, cz, a, b, w, ph, depth, len, seed; };

struct Cat {
  World* W = nullptr;
  Track tx, tz, th; std::vector<G::PK> keys;
  std::vector<GazeK> gz;
  Track3 bee, dragon, bfly, wbfly, wbfly2, koiHero;
  std::vector<std::pair<float, float>> dragonPerch; // [t0,t1) wings still
  Koi koi[5];
  float xs = 0, zs = 8.12f;
  // simulated head state, 240 Hz
  double dt = 1.0 / 240.0; int N = 0;
  std::vector<V3> P; std::vector<float> Yaw, Pitch, Roll, Focus;
  struct Step { float t, strength; int surf; };
  std::vector<Step> steps;
  std::vector<std::pair<float, float>> sniffs = {{10.0f, 10.9f}, {11.5f, 12.4f}, {19.7f, 20.6f}, {33.3f, 33.9f}, {75.0f, 75.8f}, {76.3f, 77.0f}, {82.6f, 83.9f}};
  std::vector<float> laps;
  float tSplash = 58.72f, tPrick = 84.0f, tLand = 98.26f, tPounce = 89.35f;

  // ---------------- creature paths ----------------
  float humanZ(double t) const { return 16.0f - 1.25f * (float)(t - 30.5); }
  V3 ladyPos(double t, V3* fwd = nullptr, V3* up = nullptr, bool* fly = nullptr) const {
    const auto& B = W->heroBladePts;
    float v = 0.34f + 0.46f * smooth(6.5f, 12.4f, (float)t) + 0.012f * sinf((float)t * 2.1f) * smooth(7.0f, 8.0f, (float)t);
    float fi = v * LEAF_NS; int i = std::min((int)fi, LEAF_NS - 1); float f = fi - i;
    V3 p = lerp(B[i * 3], B[i * 3 + 3], f), n = norm(lerp(B[i * 3 + 1], B[i * 3 + 4], f)), tg = norm(lerp(B[i * 3 + 2], B[i * 3 + 5], f));
    const float t0 = 12.95f;
    if (fly) *fly = t > t0 - 0.25f;
    if (t <= t0) { if (fwd) *fwd = tg; if (up) *up = n; return p + n * 0.0002f; }
    float s = (float)(t - t0);
    V3 d = norm(V3(0.55f, 0.75f, 0.35f));
    V3 q = p + d * (0.05f * s + 0.35f * s * s) + V3(sinf(s * 9) * 0.01f, sinf(s * 13) * 0.008f, cosf(s * 7) * 0.012f) * fminf(s * 3, 1.0f);
    if (fwd) *fwd = norm(d + V3(0, -0.3f, 0)); if (up) *up = V3(0, 1, 0);
    return q;
  }
  V3 hoverJit(double t, float seed, float amp) const { return V3(snoise1((float)t * 5.1f, seed), snoise1((float)t * 6.3f, seed + 3), snoise1((float)t * 4.7f, seed + 7)) * amp; }
  V3 beePos(double t, V3* fwd = nullptr) const {
    V3 p = bee.at(t) + hoverJit(t, 11, 0.012f);
    if (fwd) { V3 v = bee.vel(t); if (len(v) < 0.08f) v = W->beeFlower - p; v.y *= 0.3f; *fwd = norm(v); }
    return p;
  }
  V3 dragonPos(double t, V3* fwd = nullptr, bool* perched = nullptr) const {
    bool pr = false;
    for (auto& q : dragonPerch) if (t >= q.first && t < q.second) pr = true;
    V3 p = dragon.at(t) + (pr ? V3(0, 0, 0) : hoverJit(t, 23, 0.006f));
    if (perched) *perched = pr;
    if (fwd) {
      // heading: direction of the most recent dart
      V3 v(0, 0, 0);
      for (double q = t; q > t - 3.0; q -= 0.05) { v = dragon.vel(q); if (len(v) > 0.4f) break; }
      if (len(v) < 0.4f) v = V3(-1, 0, 0.3f);
      v.y = 0; *fwd = norm(v);
    }
    return p;
  }
  V3 flutter(const Track3& tr, double t, float seed, float amp, V3* fwd) const {
    V3 p = tr.at(t) + V3(snoise1((float)t * 2.3f, seed), snoise1((float)t * 3.1f, seed + 5) * 0.8f, snoise1((float)t * 2.7f, seed + 9)) * amp;
    p.y += 0.012f * sinf(TAU * 9.5f * (float)t);
    if (fwd) { V3 v = tr.vel(t) + V3(snoise1((float)t * 2.3f + 0.2f, seed), 0, snoise1((float)t * 2.7f + 0.2f, seed + 9)) * 0.3f; v.y = 0; *fwd = norm(len(v) > 1e-3f ? v : V3(1, 0, 0)); }
    return p;
  }
  V3 bflyPos(double t, V3* fwd = nullptr) const { return flutter(bfly, t, 31, 0.045f, fwd); }
  V3 wbflyPos(double t, V3* fwd = nullptr) const {
    V3 f0; V3 p0 = flutter(wbfly, t, 47, 0.06f, &f0);
    float w = smooth(33.5f, 35.5f, (float)t) * (1 - smooth(41.5f, 44.5f, (float)t));
    if (w > 0) { V3 f1; V3 p1 = flutter(wbfly2, t, 47, 0.035f, &f1); p0 = lerp(p0, p1, w); f0 = norm(lerp(f0, f1, w)); }
    if (fwd) *fwd = f0;
    return p0;
  }
  V3 koiPos(int i, double t, float* yaw, float* speed) const {
    if (i == 0) {
      V3 p = koiHero.at(t), v = koiHero.vel(t);
      static float lastYaw = 0;
      if (len(V3(v.x, 0, v.z)) > 0.01f) lastYaw = atan2f(v.x, v.z);
      if (yaw) *yaw = atan2f(v.x, v.z); if (speed) *speed = len(v);
      (void)lastYaw;
      return p;
    }
    const Koi& k = koi[i]; float th = k.ph + k.w * (float)t + 0.3f * sinf((float)t * 0.37f + k.seed * 9);
    V3 p(k.cx + k.a * cosf(th), G::WATER_Y - k.depth + 0.03f * sinf((float)t * 0.5f + k.seed * 5), k.cz + k.b * sinf(th));
    V3 v(-k.a * sinf(th) * k.w, 0, k.b * cosf(th) * k.w);
    if (yaw) *yaw = atan2f(v.x, v.z); if (speed) *speed = len(v);
    return p;
  }
  V3 target(const GazeK& g, double t, V3 head, V3 heading) const {
    switch (g.type) {
      case G_POINT: return g.p;
      case G_LADY: return ladyPos(t);
      case G_BEE: return beePos(t);
      case G_HUMAN: { float z = humanZ(t); float y = 0.28f + 0.25f * smooth(2.5f, 0.8f, fabsf(z - head.z)); return V3(G::HUMAN_X, y, z - 0.1f); }
      case G_DRAGON: return dragonPos(t);
      case G_KOI: return koiPos(0, t, nullptr, nullptr) + V3(0, 0.03f, 0);
      case G_BFLY: return bflyPos(t);
      case G_WBFLY: return wbflyPos(t);
      default: return head + heading * 1.4f + V3(0, g.p.y * 1.4f, 0);
    }
  }
  int surfaceAt(float x, float z) const {
    if (x > G::WALL_X0 - 0.3f && x < G::WALL_X0 + 0.3f && z < 11.2f && z > 10.0f) return 4;
    if (G::inGH(x, z, 0.3f)) return 4;                // stone paving
    if (G::pathSD(x, z) < 0.02f) return 3;            // gravel
    if (G::hedgeSD(x, z) < 0.1f || x < -0.4f) return 0; // dry leaves
    if (G::bedSD(x, z) < 0.0f) return 2;              // soil / mulch
    if (len(V2(x - (xs - 0.22f), z - zs)) < 0.3f) return 4;
    return 1;                                         // grass
  }

  void build(World& w) {
    W = &w; keys = G::routeKeys(); xs = G::shoreX(zs);
    for (auto& k : keys) { tx.add(k.t, k.x); tz.add(k.t, k.z); th.add(k.t, k.abs ? k.h : G::groundH(k.x, k.z) + k.h + (G::inGH(k.x, k.z, 0.05f) ? 0.012f : 0.0f)); }
    tx.build(); tz.build(); th.build();
    V3 BF = W->beeFlower, BF2 = W->beeFlower2;
    // ---- bee
    bee.add(18.0f, BF + V3(0.3f, 0.5f, -0.6f)); bee.add(21.0f, BF + V3(0.02f, 0.06f, -0.07f)); bee.add(23.0f, BF + V3(-0.055f, 0.0f, 0.01f)); bee.add(24.3f, BF + V3(-0.057f, 0.005f, 0.012f));
    bee.add(24.7f, BF + V3(-0.03f, 0.04f, 0.045f)); bee.add(26.3f, BF + V3(-0.032f, 0.042f, 0.047f));
    bee.add(27.35f, V3(10.385f, 0.235f, 7.175f)); bee.add(28.05f, V3(10.375f, 0.225f, 7.185f)); bee.add(28.5f, V3(10.30f, 0.30f, 7.36f)); bee.add(28.95f, V3(10.12f, 0.33f, 7.30f));
    bee.add(29.45f, V3(10.38f, 0.42f, 7.02f)); bee.add(30.5f, BF2 + V3(-0.055f, 0.01f, 0.0f)); bee.add(32.0f, BF2 + V3(-0.056f, 0.012f, 0.005f)); bee.add(34.5f, BF2 + V3(1.2f, 0.6f, -1.5f)); bee.add(200.0f, BF2 + V3(1.2f, 0.6f, -1.5f));
    bee.build();
    // ---- dragonfly: hover / dart sequence over the pond
    {
      Rng r(4242); float t = 38.0f; V3 p(xs + 0.9f, G::WATER_Y + 0.26f, zs + 0.2f);
      V3 perch(xs + 0.70f, G::WATER_Y + 0.075f, zs + 0.60f);
      while (t < 66) {
        float hold = r.r(0.7f, 2.0f);
        dragon.add(t, p); dragon.add(t + hold, p + V3(r.r(-0.03f, 0.03f), r.r(-0.02f, 0.02f), r.r(-0.03f, 0.03f)));
        t += hold + r.r(0.2f, 0.32f);
        if (t > 49.6f && dragonPerch.empty()) { dragon.add(t, perch + V3(0, 0.02f, 0)); t += 0.15f; dragon.add(t, perch); dragonPerch.push_back({t, 53.3f}); dragon.add(53.3f, perch); t = 53.55f; p = perch + V3(0.25f, 0.25f, -0.35f); continue; }
        p = V3(xs + r.r(0.28f, 1.05f), G::WATER_Y + r.r(0.10f, 0.36f), zs + r.r(-0.6f, 0.7f));
      }
      dragon.build();
    }
    // ---- butterflies
    bfly.add(80.0f, V3(20.5f, 1.3f, 18.3f)); bfly.add(84.6f, V3(22.5f, 0.75f, 17.8f)); bfly.add(85.5f, V3(24.2f, 0.5f, 17.55f)); bfly.add(86.05f, V3(25.15f, 0.42f, 17.42f)); bfly.add(86.7f, V3(26.0f, 0.33f, 17.52f));
    bfly.add(87.35f, V3(26.55f, 0.30f, 17.44f)); bfly.add(87.9f, V3(27.1f, 0.42f, 17.55f)); bfly.add(88.5f, V3(28.05f, 0.52f, 17.48f)); bfly.add(88.95f, V3(28.95f, 0.66f, 17.52f)); bfly.add(89.6f, V3(29.9f, 1.1f, 17.6f));
    bfly.add(90.6f, V3(30.9f, 2.0f, 17.2f)); bfly.add(92.5f, V3(32.5f, 3.6f, 16.0f)); bfly.add(200.0f, V3(40, 8, 10));
    bfly.build();
    wbfly.add(0.0f, V3(8, 1.2f, 10)); wbfly.add(13.0f, V3(6.5f, 0.8f, 8.6f)); wbfly.add(15.2f, V3(5.2f, 0.42f, 7.5f)); wbfly.add(16.4f, V3(5.9f, 0.5f, 6.5f)); wbfly.add(17.6f, V3(7.3f, 0.45f, 7.3f)); wbfly.add(19.0f, V3(8.2f, 0.7f, 6.2f));
    wbfly.add(22.0f, V3(9.8f, 0.9f, 8.6f)); wbfly.add(27.0f, V3(11.5f, 1.2f, 10)); wbfly.add(40.0f, V3(15.5f, 0.9f, 10.5f)); wbfly.add(60.0f, V3(22, 1.2f, 11.5f)); wbfly.add(95.0f, V3(27.5f, 0.8f, 12.5f));
    wbfly.add(100.0f, V3(27.2f, 0.75f, 11.2f)); wbfly.add(102.5f, V3(26.4f, 0.6f, 9.9f)); wbfly.add(105.0f, V3(25.2f, 0.75f, 10.9f)); wbfly.add(108.0f, V3(23.5f, 1.0f, 9.0f)); wbfly.add(120.0f, V3(20, 1.5f, 8));
    wbfly.build();
    // the white butterfly drifts down the path and crosses close in front of the cat while it sits at the path edge
    wbfly2.add(33.0f, V3(13.2f, 1.05f, 10.3f)); wbfly2.add(35.5f, V3(13.6f, 0.95f, 10.2f)); wbfly2.add(36.8f, V3(13.5f, 0.70f, 9.3f)); wbfly2.add(37.7f, V3(13.0f, 0.38f, 8.3f));
    wbfly2.add(38.3f, V3(12.88f, 0.30f, 7.65f)); wbfly2.add(38.9f, V3(12.86f, 0.26f, 7.22f)); wbfly2.add(39.5f, V3(13.5f, 0.33f, 6.7f)); wbfly2.add(40.2f, V3(14.4f, 0.55f, 7.0f)); wbfly2.add(41.0f, V3(15.1f, 0.9f, 8.4f)); wbfly2.add(43.0f, V3(16.2f, 0.95f, 10.3f)); wbfly2.add(46.0f, V3(17.4f, 0.97f, 10.8f));
    wbfly2.build();
    // ---- koi
    float wy = G::WATER_Y;
    koiHero.add(0, V3(G::PCX + 0.5f, wy - 0.28f, G::PCZ - 0.6f)); koiHero.add(40, V3(G::PCX + 0.5f, wy - 0.28f, G::PCZ - 0.6f)); koiHero.add(44, V3(G::PCX - 0.5f, wy - 0.25f, G::PCZ - 1.0f)); koiHero.add(47, V3(G::PCX - 1.4f, wy - 0.22f, G::PCZ + 0.3f));
    koiHero.add(50, V3(xs + 1.4f, wy - 0.2f, zs + 0.55f)); koiHero.add(52, V3(xs + 0.78f, wy - 0.14f, zs + 0.28f)); koiHero.add(53.3f, V3(xs + 0.44f, wy - 0.10f, zs + 0.06f)); koiHero.add(54.6f, V3(xs + 0.56f, wy - 0.13f, zs - 0.46f));
    koiHero.add(56.0f, V3(xs + 1.0f, wy - 0.2f, zs - 0.6f)); koiHero.add(57.5f, V3(xs + 0.92f, wy - 0.15f, zs - 0.08f)); koiHero.add(58.45f, V3(xs + 0.42f, wy - 0.07f, zs + 0.03f)); koiHero.add(58.72f, V3(xs + 0.325f, wy - 0.030f, zs + 0.035f));
    koiHero.add(59.15f, V3(xs + 0.62f, wy - 0.2f, zs + 0.42f)); koiHero.add(61.0f, V3(G::PCX - 0.8f, wy - 0.3f, G::PCZ + 1.0f)); koiHero.add(66.0f, V3(G::PCX + 0.6f, wy - 0.3f, G::PCZ + 0.2f)); koiHero.add(200.0f, V3(G::PCX + 0.6f, wy - 0.3f, G::PCZ + 0.2f));
    koiHero.build();
    koi[0] = {0, 0, 0, 0, 0, 0, 0, 0.46f, 0.07f};
    koi[1] = {G::PCX + 0.2f, G::PCZ + 0.1f, 1.7f, 1.1f, 0.21f, 0.5f, 0.2f, 0.40f, 0.45f};
    koi[2] = {G::PCX - 0.3f, G::PCZ - 0.2f, 1.9f, 1.3f, -0.17f, 2.4f, 0.26f, 0.36f, 0.71f};
    koi[3] = {G::PCX + 0.6f, G::PCZ + 0.3f, 1.2f, 0.9f, 0.27f, 4.0f, 0.16f, 0.30f, 0.85f};
    koi[4] = {G::PCX - 0.6f, G::PCZ + 0.2f, 1.5f, 1.2f, -0.24f, 1.2f, 0.3f, 0.42f, 0.93f};
    for (float t = 54.6f; t < 58.5f; t += 0.27f) laps.push_back(t + 0.03f * sinf(t * 7));
    // ---- gaze script
    float wyTop = G::WALL_H + 0.045f;
    gz = {
        {0.0f, G_FWD, V3(0, 0.03f, 0), 4, 0, 0.5f},
        {1.6f, G_POINT, V3(0.4f, 0.17f, 6.25f), 5, 0, 0.7f},
        {3.0f, G_FWD, V3(0, -0.04f, 0), 5, 0, 0.5f},
        {5.1f, G_POINT, V3(2.2f, 0.75f, 5.2f), 4, 0.05f, 3.0f},
        {6.2f, G_POINT, V3(3.2f, 0.45f, 8.8f), 5, -0.04f, 3.0f},
        {7.2f, G_FWD, V3(0, -0.10f, 0), 5, 0, 1.0f},
        {8.45f, G_LADY, V3(), 13, 0.10f, 0},
        {14.0f, G_FWD, V3(0, -0.06f, 0), 6, 0, 1.2f},
        {15.5f, G_WBFLY, V3(), 9, -0.05f, 0},
        {16.9f, G_FWD, V3(0, -0.08f, 0), 6, 0, 1.2f},
        {18.3f, G_POINT, V3(7.26f, 0.21f, 6.80f), 8, 0.06f, 0},
        {21.0f, G_FWD, V3(0, -0.05f, 0), 6, 0, 1.2f},
        {23.3f, G_FWD, V3(0, -0.16f, 0), 5, 0, 0.5f},
        {24.7f, G_POINT, V3(10.78f, 0.62f, 7.62f), 6.5f, 0.08f, 0},
        {26.25f, G_BEE, V3(), 12, -0.08f, 0},
        {30.2f, G_FWD, V3(0, -0.06f, 0), 6, 0, 1.0f},
        {32.5f, G_POINT, V3(12.635f, 0.045f, 7.37f), 8, 0.05f, 0},
        {34.85f, G_POINT, V3(14.3f, 0.32f, 3.2f), 6, -0.07f, 3.5f},   // head up: down the path to the right
        {35.80f, G_POINT, V3(16.5f, 2.4f, 7.0f), 5, 0.10f, 6.0f},     // up at the trees beyond the bench, head tilted
        {36.75f, G_WBFLY, V3(), 12, 0.06f, 0},                        // white butterfly coming down the path: follow it across
        {39.8f, G_FWD, V3(0, -0.04f, 0), 7, 0, 1.3f},
        {42.7f, G_POINT, V3(17.6f, 0.0f, 8.2f), 5, 0, 2.2f},
        {43.5f, G_POINT, V3(15.20f, 0.44f, 8.55f), 5, 0.06f, 0.6f},
        {44.5f, G_POINT, V3(17.6f, 0.0f, 8.2f), 5, 0, 2.2f},
        {45.2f, G_FWD, V3(0, -0.07f, 0), 6, 0, 1.2f},
        {46.7f, G_DRAGON, V3(), 13, 0.06f, 0},
        {51.5f, G_KOI, V3(), 8, -0.05f, 0},
        {53.8f, G_POINT, V3(xs + 0.20f, G::WATER_Y, zs + 0.01f), 6, 0, 0.30f},
        {58.74f, G_KOI, V3(), 24, 0, 0},
        {60.3f, G_FWD, V3(0, -0.04f, 0), 7, 0, 1.4f},
        {62.0f, G_POINT, V3(20.5f, 1.7f, 15.2f), 4, 0, 5.0f},
        {64.4f, G_FWD, V3(0, -0.02f, 0), 5, 0, 1.5f},
        {66.3f, G_POINT, V3(18.4f, 1.5f, 17.6f), 5, 0, 3.0f},
        {68.4f, G_POINT, V3(16.2f, 1.2f, 15.9f), 4.5f, 0.06f, 1.8f},
        {69.5f, G_POINT, V3(19.4f, 0.9f, 15.9f), 4.5f, -0.05f, 1.6f},
        {70.7f, G_FWD, V3(0, -0.04f, 0), 5, 0, 1.3f},
        {72.5f, G_POINT, W->orchidPos, 5, 0.05f, 0},
        {73.8f, G_POINT, V3(20.95f, 0.27f, 16.48f), 7, 0.09f, 0},
        {77.0f, G_POINT, V3(21.72f, 0.05f, 16.66f), 8, -0.05f, 0},
        {77.9f, G_FWD, V3(0, -0.04f, 0), 6, 0, 1.3f},
        {79.3f, G_POINT, V3(26.6f, 0.45f, 15.6f), 4, 0, 2.0f},
        {80.6f, G_POINT, V3(G::CACTUS.x, 0.13f, G::CACTUS.z), 7, 0.07f, 0},
        {84.05f, G_POINT, V3(G::CACTUS.x + 0.05f, 0.2f, G::CACTUS.z), 9, 0, 0},
        {85.25f, G_BFLY, V3(), 15, 0.1f, 0},
        {91.6f, G_FWD, V3(0, 0.0f, 0), 5, 0, 1.6f},
        {92.6f, G_POINT, V3(29.3f, 0.3f, 12.0f), 4, 0, 2.5f},
        {96.5f, G_POINT, V3(G::WALL_X0 + 0.05f, wyTop + 0.05f, 10.6f), 7, 0, 0},
        {98.28f, G_POINT, V3(33.5f, 1.3f, 9.6f), 6, 0, 4.0f},
        {99.4f, G_POINT, V3(26.0f, 0.75f, 13.5f), 3.2f, 0, 5.0f},
        {100.4f, G_POINT, V3(22.0f, 0.5f, 9.3f), 2.8f, 0, 7.0f},
        {102.4f, G_WBFLY, V3(), 3.5f, 0.04f, 0},
        {104.3f, G_POINT, V3(27.0f, 0.10f, 10.3f), 1.6f, 0, 6.0f},
    };
    simulate();
  }

  void simulate() {
    N = (int)((G::T_END + 2.0) / dt) + 1;
    P.resize(N); Yaw.resize(N); Pitch.resize(N); Roll.resize(N); Focus.resize(N);
    V3 heading(1, 0, 0.02f);
    double phase = 0; int stepIdx = 0;
    float yaw = atan2f(1.0f, 0.02f), yv = 0, pitch = 0, pv = 0, roll = 0, rv = 0, focus = 0.5f, fv = 0;
    size_t gi = 0;
    for (int i = 0; i < N; i++) {
      double t = i * dt;
      V3 base(tx.at(t), th.at(t), tz.at(t));
      V3 vel((tx.at(t + 0.004) - tx.at(t - 0.004)) / 0.008f, 0, (tz.at(t + 0.004) - tz.at(t - 0.004)) / 0.008f);
      float sp = len(vel);
      if (sp > 0.08f) { float k = 1 - expf(-(float)dt / 0.22f); heading = norm(lerp(heading, vel / sp, k)); }
      bool air = (t > 97.72 && t < 98.26) || (t > 88.6 && t < 89.35);
      // gait
      float stride = 0.25f + 0.13f * clampf((sp - 1.0f) / 1.2f, 0, 1);
      double ph0 = phase; if (!air) phase += sp / stride * dt;
      if (floor(phase) > floor(ph0) && sp > 0.1f) { steps.push_back({(float)t, clampf(sp / 0.9f, 0.25f, 1.8f), surfaceAt(base.x, base.z)}); stepIdx++; }
      if (floor(phase + 0.5) > floor(ph0 + 0.5) && sp > 0.1f) steps.push_back({(float)t, 0.6f * clampf(sp / 0.9f, 0.25f, 1.8f), surfaceAt(base.x - heading.x * 0.25f, base.z - heading.z * 0.25f)});
      float amp = clampf(sp / 0.55f, 0, 1), trot = clampf((sp - 1.1f) / 0.9f, 0, 1);
      float jit = 1.0f + 0.35f * snoise1((float)floor(phase) * 1.7f, 3.0f);
      float bobY = -(0.0032f + 0.0045f * trot) * amp * jit * cosf(TAU * (float)phase);
      float sway = (0.0045f + 0.003f * trot) * amp * sinf(PI * (float)phase);
      V3 side = norm(cross(heading, V3(0, 1, 0)));
      V3 pos = base + V3(0, bobY, 0) + side * sway;
      // sniffing: tiny forward pulses
      float snf = 0;
      for (auto& s : sniffs) if (t > s.first && t < s.second) snf = sinf(TAU * 6.5f * (float)(t - s.first)) * sinf(PI * (float)((t - s.first) / (s.second - s.first)));
      // drinking: head dips with each lap
      float lap = 0;
      for (float tl : laps) if (t > tl && t < tl + 0.2f) lap = sinf(PI * (float)(t - tl) / 0.2f);
      // gaze
      while (gi + 1 < gz.size() && gz[gi + 1].t <= t) gi++;
      const GazeK& g = gz[gi];
      V3 tg = target(g, t, pos, heading);
      V3 d = tg - pos; float dist = len(d);
      float ty = atan2f(d.x, d.z), tp = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
      if (g.type == G_FWD) { ty = atan2f(heading.x, heading.z); tp = g.p.y + 0.5f * atan2f((float)(th.at(t + 0.15) - th.at(t - 0.15)), fmaxf(sp * 0.3f, 0.05f)) * (air ? 1.0f : 0.25f); }
      tp = clampf(tp, -1.25f, 1.25f);
      // micro head noise and gait coupled wobble
      ty += 0.006f * snoise1((float)t * 0.9f, 5) + 0.003f * snoise1((float)t * 3.1f, 6) + 0.010f * amp * sinf(PI * (float)phase + 0.4f);
      tp += 0.005f * snoise1((float)t * 0.8f, 7) + 0.003f * snoise1((float)t * 2.9f, 8) + 0.007f * amp * sinf(TAU * (float)phase + 1.1f) - 0.03f * lap;
      float dy = ty - yaw; while (dy > PI) dy -= TAU; while (dy < -PI) dy += TAU;
      float om = g.om, zeta = om > 10 ? 0.72f : 0.9f;
      yv += (om * om * dy - 2 * zeta * om * yv) * (float)dt; yaw += yv * (float)dt;
      pv += (om * om * (tp - pitch) - 2 * zeta * om * pv) * (float)dt; pitch += pv * (float)dt;
      float tr = g.roll + 0.014f * amp * sinf(PI * (float)phase + 0.9f) + 0.006f * snoise1((float)t * 0.7f, 9);
      // head shake after the cactus prick
      if (t > tPrick + 0.3f && t < tPrick + 1.25f) tr += 0.26f * sinf(TAU * 8.5f * (float)(t - tPrick - 0.3f)) * sinf(PI * (float)(t - tPrick - 0.3f) / 0.95f);
      rv += (49 * (tr - roll) - 2 * 0.8f * 7 * rv) * (float)dt; roll += rv * (float)dt;
      float tf = g.focus > 0 ? g.focus : fmaxf(dist, 0.045f);
      fv += (36 * (tf - focus) - 2 * 0.85f * 6 * fv) * (float)dt; focus += fv * (float)dt;
      V3 F(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
      pos += F * (0.0022f * snf) + V3(0, -0.006f * lap, 0);
      P[i] = pos; Yaw[i] = yaw; Pitch[i] = pitch; Roll[i] = roll; Focus[i] = fmaxf(focus, 0.04f);
    }
  }
  CamPose pose(double t) const {
    double f = t / dt; int i = std::max(0, std::min((int)f, N - 2)); float a = (float)(f - i);
    CamPose c; c.pos = lerp(P[i], P[i + 1], a);
    float yaw = lerpf(Yaw[i], Yaw[i + 1], a), pitch = lerpf(Pitch[i], Pitch[i + 1], a), roll = lerpf(Roll[i], Roll[i + 1], a);
    c.F = V3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
    V3 R0 = norm(cross(c.F, V3(0, 1, 0))), U0 = cross(R0, c.F);
    c.R = R0 * cosf(roll) + U0 * sinf(roll); c.U = U0 * cosf(roll) - R0 * sinf(roll);
    c.focus = lerpf(Focus[i], Focus[i + 1], a);
    return c;
  }
  float lensFog(double t) const { return 0.45f * smooth(67.2f, 68.3f, (float)t) * smooth(71.8f, 69.0f, (float)t); }
  float blink(double t) const {
    auto b = [&](float t0, float close, float hold, float open) { float x = (float)t; return smooth(t0, t0 + close, x) * (1 - smooth(t0 + close + hold, t0 + close + hold + open, x)); };
    float v = fmaxf(b(105.9f, 0.45f, 0.35f, 0.7f) * 0.92f, smooth(107.9f, 108.7f, (float)t));
    v = fmaxf(v, b(98.45f, 0.12f, 0.1f, 0.3f) * 0.45f); // squint into the sun after landing
    return v;
  }
  float fade(double t) const { return smooth(0.0f, 0.7f, (float)t) * (1 - smooth(G::T_END - 1.0f, G::T_END - 0.1f, (float)t)); }
  void ripples(double t, std::vector<Ripple>& out) const {
    std::vector<Ripple> all;
    for (float tl : laps) all.push_back({xs + 0.075f + 0.012f * sinf(tl * 31), zs + 0.01f + 0.012f * cosf(tl * 17), tl + 0.08f, 0.20f});
    all.push_back({xs + 0.33f, zs + 0.035f, tSplash, 1.6f}); all.push_back({xs + 0.45f, zs + 0.2f, tSplash + 0.22f, 1.0f});
    for (float tt : {48.3f, 49.1f, 56.2f, 61.5f, 63.0f}) { V3 p = dragon.at(tt); all.push_back({p.x, p.z, tt, 0.3f}); }
    for (int k = 1; k < 5; k++) for (float tt = 41 + k * 1.7f; tt < 66; tt += 6.5f + k) { V3 p = koiPos(k, tt, nullptr, nullptr); all.push_back({p.x, p.z, tt, 0.45f}); }
    for (auto& r : all) if (t > r.t0 && t < r.t0 + 5.0f) out.push_back(r);
    if (out.size() > 16) out.erase(out.begin(), out.end() - 16);
  }

  // ---------------- dynamic instances ----------------
  void dynamics(double t, const CamPose& cam, Scene& S, std::vector<InstCPU>& out) const {
    const CreatureProtos& C = W->cp;
    auto put = [&](int proto, const M34& x, float seed = 0.5f, u32 flags = 0, float e0 = 0, float e1 = 0, float e2 = 0, float e3 = 0, u32 tint = TINT1) { if (proto >= 0) out.push_back({proto, x, 0, seed, tint, flags, {e0, e1, e2, e3}}); };
    M34 mir = mScale(V3(-1, 1, 1));
    // the cat: whiskers (visible), body proxy (shadow + reflections only)
    put(C.whisker, mBasis(cam.R, cam.U, cam.F, cam.pos));
    { V3 f = norm(V3(cam.F.x, 0, cam.F.z)); float lie = smooth(103.4f, 105.2f, (float)t); put(C.catProxy, mLookZ(f, cam.pos + V3(0, 0.02f * lie, 0)) * mScale(V3(1, 1 - 0.3f * lie, 1)), 0.3f, F_NOPRIMARY); }
    if (t > 103.2) { // fore paws slide forward as the cat lies down on the wall
      float k = smooth(103.6f, 105.1f, (float)t);
      V3 f = norm(V3(-1, 0, -0.12f)), r = norm(cross(f, V3(0, 1, 0)));
      V3 base(G::WALL_X0 + 0.02f, G::WALL_H + 0.045f + 0.022f, 10.6f);
      for (int s = -1; s <= 1; s += 2) put(C.paw, mLookZ(norm(f + r * (0.10f * s)), base + f * (0.0f + 0.19f * k) + r * (0.038f * s)) * mRotZ(0.15f * s), 0.4f + 0.1f * s);
    }
    // ladybird
    if (t < 16.5) {
      V3 f, u; bool fly; V3 p = ladyPos(t, &f, &u, &fly);
      V3 X = norm(cross(u, f)); u = cross(f, X);
      M34 x = mBasis(X, u, f, p);
      put(C.ladyBody, x, 0.2f);
      if (fly) for (int s = -1; s <= 1; s += 2) put(C.ladyWing, x * mTrans(V3(0, 0.0026f, 0.001f)) * (s < 0 ? mir : M34()), 0.2f, F_FLAP | F_NOSHADOW, 1.0f, 85, 0, 0.55f);
    }
    // bee
    if (t > 17 && t < 36) {
      V3 f; V3 p = beePos(t, &f);
      M34 x = mLookZ(f, p) * mRotX(-0.35f);
      put(C.beeBody, x, 0.3f);
      for (int s = -1; s <= 1; s += 2) put(C.beeWing, x * mTrans(V3(0, 0.0042f, 0.002f)) * mRotY(-0.5f * s) * (s < 0 ? mir : M34()), 0.3f, F_FLAP | F_NOSHADOW, 1.05f, 190, 0, 0.35f);
    }
    // ants along the brick edging
    if (t > 28 && t < 42)
      for (int i = 0; i < 16; i++) {
        float dirn = (i % 3 == 0) ? -1.0f : 1.0f;
        float z = 6.75f + fmodf(i * 0.083f + dirn * 0.028f * (float)t + 40.0f, 1.25f);
        float x = 12.640f + 0.012f * sinf(z * 23 + i) + (dirn < 0 ? 0.018f : 0.0f);
        float y = 0.006f + 0.0345f + 0.004f * sinf(z * 28.3f + 1.0f);
        put(C.ant, place(V3(x, y, z), (dirn > 0 ? 0.0f : PI) + 0.25f * sinf((float)t * 9 + i), 1.0f + 0.2f * sinf(i * 3.3f)), 0.1f * i);
      }
    // dragonfly
    if (t > 37 && t < 67) {
      V3 f; bool pr; V3 p = dragonPos(t, &f, &pr);
      M34 x = mLookZ(f, p) * mRotX(pr ? 0.15f : 0.05f);
      put(C.dfBody, x, 0.4f);
      for (int s = -1; s <= 1; s += 2) {
        put(C.dfWing, x * mTrans(V3(0, 0.004f, 0.021f)) * mRotY(0.1f * s) * (s < 0 ? mir : M34()), 0.4f, F_FLAP | F_NOSHADOW, pr ? 0.0f : 0.5f, 34, 0, pr ? -0.08f : 0.08f);
        put(C.dfWing, x * mTrans(V3(0, 0.004f, 0.0155f)) * mRotY(-0.12f * s) * (s < 0 ? mir : M34()), 0.4f, F_FLAP | F_NOSHADOW, pr ? 0.0f : 0.5f, 34, 1.6f, pr ? -0.10f : 0.06f);
      }
    }
    // koi
    if (t > 36 && t < 70)
      for (int i = 0; i < 5; i++) {
        float yaw, sp; V3 p = koiPos(i, t, &yaw, &sp);
        float L = koi[i].len; float fq = 1.1f + 2.2f * sp;
        static const float cut[6] = {0.0f, 0.24f, 0.44f, 0.62f, 0.78f, 0.93f}, Aj[6] = {0.05f, 0.10f, 0.17f, 0.26f, 0.36f, 0.5f};
        float ya = yaw + Aj[0] * sinf(TAU * fq * (float)t + i);
        V3 j = p;
        for (int k = 0; k < 5; k++) {
          M34 x = mTrans(j) * mRotY(ya) * mScale(L);
          put(C.koiSeg[k], x, koi[i].seed);
          if (k == 1) for (int s = -1; s <= 1; s += 2) put(C.koiFin, x * mTrans(V3(0.065f * s, -0.055f, 0.05f)) * mRotY(PI - 0.9f * s + 0.25f * s * sinf((float)t * 3 + i)) * mRotX(0.5f), koi[i].seed);
          j -= V3(sinf(ya), 0, cosf(ya)) * ((cut[k + 1] - cut[k]) * L);
          ya += Aj[k + 1] * sinf(TAU * fq * (float)t + i - 0.9f * (k + 1));
        }
        put(C.koiTail, mTrans(j) * mRotY(ya) * mScale(L), koi[i].seed);
      }
    // butterflies
    if (t > 79 && t < 94) {
      V3 f; V3 p = bflyPos(t, &f);
      M34 x = mLookZ(f, p) * mRotX(-0.4f);
      put(C.bfBody, x, 0.5f);
      for (int s = -1; s <= 1; s += 2) put(C.bfWing, x * (s < 0 ? mir : M34()), 0.5f, F_FLAP, 1.05f, 9.5f, 0, 0.30f);
    }
    {
      V3 f; V3 p = wbflyPos(t, &f);
      // a larger individual (cabbage-white size) for the close pass at the path; the blend is done while it is metres away
      float big = 1.0f + 0.8f * smooth(33.5f, 35.5f, (float)t) * (1 - smooth(41.5f, 44.5f, (float)t));
      M34 x = mLookZ(f, p) * mRotX(-0.4f) * mScale(V3(big, big, big));
      put(C.bfBody2, x, 0.7f);
      for (int s = -1; s <= 1; s += 2) put(C.bfWing2, x * (s < 0 ? mir : M34()), 0.7f, F_FLAP, 1.05f, 11.0f, 1.0f, 0.30f);
    }
  }

  // ---------------- event timeline for the sound track ----------------
  void writeTimeline(const char* path, float fps) const {
    FILE* f = fopen(path, "w");
    fprintf(f, "{\n\"duration\": %.3f,\n\"steps\": [", G::T_END);
    for (size_t i = 0; i < steps.size(); i++) fprintf(f, "%s[%.3f,%.2f,%d]", i ? "," : "", steps[i].t, steps[i].strength, steps[i].surf);
    fprintf(f, "],\n\"sniffs\": [");
    for (size_t i = 0; i < sniffs.size(); i++) fprintf(f, "%s[%.2f,%.2f]", i ? "," : "", sniffs[i].first, sniffs[i].second);
    fprintf(f, "],\n\"laps\": [");
    for (size_t i = 0; i < laps.size(); i++) fprintf(f, "%s%.3f", i ? "," : "", laps[i]);
    fprintf(f, "],\n\"splash\": %.2f, \"prick\": %.2f, \"land\": %.2f, \"pounce\": %.2f, \"ladyfly\": 12.95,\n", tSplash, tPrick, tLand, tPounce);
    fprintf(f, "\"human\": [],\n");   // (the person on the path was removed; key kept for the audio script)
    // per-frame tracks: speed, inside-greenhouse factor, creature distance / pan relative to the head
    auto track = [&](const char* name, const std::function<V3(double)>& fn, double t0, double t1) {
      fprintf(f, "\"%s\": [", name);
      int n = (int)(G::T_END * fps);
      for (int i = 0; i < n; i++) {
        double t = i / (double)fps; float d = -1, pan = 0;
        if (t >= t0 && t <= t1) { CamPose c = pose(t); V3 rel = fn(t) - c.pos; d = len(rel); pan = dot(norm(rel), c.R); }
        fprintf(f, "%s[%.3f,%.2f]", i ? "," : "", d, pan);
      }
      fprintf(f, "],\n");
    };
    track("bee", [&](double t) { return beePos(t); }, 17.5, 35.5);
    track("dragon", [&](double t) { return dragonPos(t); }, 44, 66);
    track("bfly", [&](double t) { return bflyPos(t); }, 84, 92);
    track("lady", [&](double t) { return ladyPos(t); }, 12.7, 15.5);
    fprintf(f, "\"cam\": [");
    int n = (int)(G::T_END * fps);
    for (int i = 0; i < n; i++) {
      double t = i / (double)fps; CamPose c = pose(t), c2 = pose(t + 1.0 / fps);
      float sp = len(c2.pos - c.pos) * fps;
      float inside = G::inGH(c.pos.x, c.pos.z, 0.0f) ? 1.0f : 0.0f;
      float pondD = len(V2(c.pos.x - G::PCX, c.pos.z - G::PCZ));
      float turn = acosf(clampf(dot(c.F, c2.F), -1, 1)) * fps;
      fprintf(f, "%s[%.3f,%.0f,%.2f,%.2f,%.3f]", i ? "," : "", sp, inside, pondD, turn, c.pos.y);
    }
    fprintf(f, "]\n}\n");
    fclose(f);
  }
};
