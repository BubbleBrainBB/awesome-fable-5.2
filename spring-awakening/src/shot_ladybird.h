// shot_ladybird.h — a ladybird climbs a stem (real time) and takes off from the tip (slow motion).
#pragma once
#include "ladybird.h"

namespace lbs {
using lb::MM;
static lb::Model M;
static BlasH stemB, bgB, dropB;
static int mStem, mBlade, mWater;
static std::vector<V3> SP; static std::vector<float> SL; static float stemLen = 0;
static const float STRIDE = 2.5f * MM, DUTY = 0.62f;
struct DropPt { V3 p; float r; }; static std::vector<DropPt> drops;

static V3 stemAt(float s, V3* T = nullptr) {
    s = clampf(s, 0, stemLen - 1e-6f);
    int lo = 0, hi = (int)SL.size() - 1;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (SL[mid] <= s) lo = mid; else hi = mid; }
    float f = (s - SL[lo]) / std::max(SL[hi] - SL[lo], 1e-9f);
    if (T) *T = norm(SP[hi] - SP[lo]);
    return lerp(SP[lo], SP[hi], f);
}
static float stemR(float s) { float u = s / stemLen; return (1.25f - 0.45f * u) * MM * (u > 0.985f ? std::sqrt(std::max(0.02f, 1 - sat((u - 0.985f) / 0.015f) * sat((u - 0.985f) / 0.015f))) : 1.0f); }
static void stemFrame(float s, V3& O, V3& T, V3& n, V3& X) {
    O = stemAt(s, &T); V3 n0 = norm(V3(-0.97f, 0.05f, 0.16f)); n = norm(n0 - T * dot(n0, T)); X = cross(n, T);
}

static float tipStop() { return stemLen - 3.3f * MM; }
static float S0 = 0.036f;             // arc position of the body at t = 0 (set in init so that the climb ends at the tip)

static void init(Scene& sc) {
    lb::build(sc, M);
    mStem = sc.addMat([] { Mat m = mkMat(V3(0.20f, 0.36f, 0.08f), 0.3f, 0.9f, 4); set4(m.base2, V3(0.26f, 0.44f, 0.09f)); m.trans = 0.25f; set4(m.p0, 0.0f, 1.0f, 26, 0); set4(m.p2, V3(0.35f, 0.5f, 0.15f), 0.25f); return m; }());
    mBlade = sc.addMat([] { Mat m = mkMat(V3(0.13f, 0.30f, 0.05f), 0.28f, 1.0f, 3); set4(m.base2, V3(0.42f, 0.64f, 0.08f)); m.trans = 0.45f; set4(m.p0, 13, 0.3f, 0.3f, 0.0f); set4(m.p1, 0.0f, 0.3f, 0, 0); set4(m.p2, V3(0.3f, 0.42f, 0.12f), 0.25f); return m; }());
    mWater = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 1.0f, 0); m.type = MT_DIEL; m.ior = 1.33f; set4(m.sh, V3(0.9f)); return m; }());
    std::vector<V3> c = {V3(-0.012f, -0.035f, -0.004f), V3(-0.006f, -0.005f, -0.001f), V3(0.0f, 0.025f, 0.0f), V3(0.009f, 0.052f, 0.002f), V3(0.022f, 0.074f, 0.004f)};
    SP = spline(c, 500); SL.assign(500, 0); for (int i = 1; i < 500; i++) SL[i] = SL[i - 1] + len(SP[i] - SP[i - 1]); stemLen = SL.back();
    S0 = tipStop() - 42.7f * MM;
    Mesh m;
    { int n = 200; std::vector<V3> p(n); std::vector<float> rr(n); for (int i = 0; i < n; i++) { float s = stemLen * i / (n - 1.f); p[i] = stemAt(s); rr[i] = i == n - 1 ? 0.0f : stemR(s); }
      addTube(m, p, rr, 20, mStem, 0.2f, 30, nullptr, [](float u, float v) { return 1 + 0.03f * std::sin(v * 2 * PI * 9); });
      // two leaf blades leaving the stem low down
      for (int k = 0; k < 2; k++) { V3 T; V3 o = stemAt(stemLen * (0.12f + 0.2f * k), &T); Blade b; b.len = 0.07f; b.width = 0.0028f; b.maxAt = 0.3f; b.q = 0.3f; b.tipQ = 1.5f; b.baseW = 0.8f; b.fold = 0.45f; b.curl = 0.9f + 0.4f * k; b.curlPow = 1.6f; b.twist = 0.6f; b.nu = 40; b.nv = 4;
          float az = k ? 2.6f : -0.5f; V3 out(std::cos(az), 0, std::sin(az)); V3 Y = norm(T + out * 0.45f), Z = norm(-out + Y * dot(out, Y)); addBlade(m, b, M34::fromBasis(cross(Y, Z), Y, Z, o), mBlade, 0.3f * k, 0); }
      stemB = sc.addBlas(m); m.clear(); }
    Rng r(17);
    for (int k = 0; k < 70; k++) { // out-of-focus grass around
        bool front = k < 6; V3 b(r.r(-0.16f, 0.16f), r.r(-0.09f, -0.03f), front ? r.r(0.05f, 0.1f) : -r.r(0.05f, 0.5f));
        Blade bl; bl.len = r.r(0.08f, 0.2f); bl.width = r.r(0.0016f, 0.0035f); bl.maxAt = 0.3f; bl.q = 0.3f; bl.tipQ = 1.5f; bl.baseW = 0.8f; bl.fold = 0.5f; bl.curl = r.r(0.2f, 1.6f); bl.curlPow = 2; bl.twist = r.r(-1, 1); bl.nu = 20; bl.nv = 2;
        addBlade(m, bl, M34::translate(b) * M34::rotY(r.r(0, 6.28f)) * M34::rotX(-r.r(0.0f, 0.5f)), mBlade, r.f(), 0);
    }
    bgB = sc.addBlas(m); m.clear();
    addBlob(m, V3(0), V3(1), 20, 32, mWater); dropB = sc.addBlas(m); m.clear();
    drops.clear();
    for (int k = 0; k < 9; k++) { float s = stemLen * r.r(0.15f, 0.9f); V3 O, T, n, X; stemFrame(s, O, T, n, X); float a = r.r(2.2f, 4.2f); float rr = r.r(0.25f, 0.6f) * MM; V3 d = n * std::cos(a) + X * std::sin(a); drops.push_back({O + d * (stemR(s) + rr * 0.55f), rr}); }
}

// distance walked along the stem at film time t (climb shot)
static float walked(float t) {
    auto seg = [](float t, float a, float b, float d) { return d * ease(a, b, t); };
    return seg(t, -0.4f, 2.5f, 15.5f * MM) + seg(t, 3.7f, 6.4f, 24.0f * MM) + seg(t, 6.3f, 7.3f, 3.2f * MM);
}

static lb::Pose poseClimb(float t) {
    lb::Pose P;
    float D = walked(t); float sb = std::min(S0 + D, tipStop());
    float speed = (walked(t + 0.02f) - walked(t - 0.02f)) / 0.04f; float mv = sat(speed / (5 * MM));
    float cyc = D / STRIDE;
    V3 O, T, n, X; stemFrame(sb, O, T, n, X);
    float roll = (0.06f * std::sin(2 * PI * cyc) ) * mv + 0.015f * std::sin(t * 2.1f);
    float pitch = 0.035f * std::sin(4 * PI * cyc + 0.6f) * mv - 0.05f * (1 - mv) * (0.5f + 0.5f * std::sin(t * 1.7f));
    float sway = 0.09f * MM * std::sin(2 * PI * cyc + 0.4f) * mv;
    V3 org = O + n * (stemR(sb) + 0.62f * MM + 0.05f * MM * std::sin(4 * PI * cyc) * mv) + X * sway;
    M34 B = M34::fromBasis(X, n, T, org) * M34::rotZ(roll) * M34::rotX(pitch);
    P.body = B;
    static const float homeZ[3] = {1.75f + 1.2f, 0.55f + 0.15f, -0.65f - 1.4f};
    static const float alpha[3] = {1.22f, 1.5f, 1.32f};
    for (int k = 0; k < 3; k++) for (int s = 0; s < 2; s++) {
        float sg = s ? -1.f : 1.f; int group = (k + s) & 1; float off = group * 0.5f;
        float cy = cyc + off; float c = std::floor(cy); float p = cy - c;
        auto land = [&](float cc) { return std::min(S0 + (cc - off + DUTY * 0.5f) * STRIDE + homeZ[k] * MM, stemLen - (0.5f + 0.9f * (2 - k)) * MM); };
        float sf, lift = 0;
        if (p < DUTY) sf = land(c); else { float x = (p - DUTY) / (1 - DUTY); float e = x * x * (3 - 2 * x); sf = lerp(land(c), land(c + 1), e); lift = std::sin(PI * x); }
        V3 fo, ft, fn, fx; stemFrame(sf, fo, ft, fn, fx);
        float a = alpha[k];
        P.foot[k * 2 + s] = fo + (fn * std::cos(a) + fx * (std::sin(a) * sg)) * (stemR(sf) + 0.04f * MM) + (fn * 0.5f + fx * (0.8f * sg)) * (0.45f * MM * lift);
    }
    for (int s = 0; s < 2; s++) {
        float ph = s * 2.1f;
        P.antYaw[s] = 0.22f * std::sin(2 * PI * 6.3f * t + ph) * (0.5f + 0.5f * mv) + 0.25f * std::sin(t * 1.9f + ph * 2) + 0.3f * (1 - mv) * std::sin(t * 4.3f + ph);
        P.antPitch[s] = 0.18f * std::sin(2 * PI * 4.7f * t + ph * 1.7f) + 0.2f * (1 - mv) * std::sin(t * 3.1f + ph);
    }
    P.headYaw = 0.28f * (1 - mv) * std::sin(t * 2.3f) * smooth(2.4f, 3.0f, t) + 0.04f * std::sin(t * 5);
    P.headPitch = 0.08f * std::sin(t * 3.3f) * (1 - mv);
    return P;
}

static float backOut(float x) { x = sat(x); float c1 = 1.70158f, c3 = c1 + 1; return 1 + c3 * std::pow(x - 1, 3.0f) + c1 * std::pow(x - 1, 2.0f); }
// take-off, slow motion (film time t; about 1/25 real speed: an 85 Hz wingbeat becomes 3.4 Hz, so each frame holds a
// moderately blurred wing. At 8.5 Hz the wing was a faint smear in most frames and flashed sharp at the stroke ends.)
static const float WINGHZ = 3.4f;
static lb::Pose poseFly(float t) {
    lb::Pose P = poseClimb(7.5f + t * 0.1f);
    // freeze the legs/gait at the end of the climb but keep small motions slowed down
    float e1 = backOut((t - 0.45f) / 0.5f);
    P.elyYaw = 1.0f * e1; P.elyRoll = 0.78f * e1 + 0.03f * std::sin(t * 2 * PI * WINGHZ) * sat(t - 1.5f);
    P.wingOut = ease(0.75f, 1.55f, t);
    float tf = t - 1.45f; float A = 1.05f * ease(0.0f, 0.55f, tf);
    float ph = 2 * PI * WINGHZ * std::max(tf, 0.f);
    P.wingStroke = 0.30f * P.wingOut + A * std::sin(ph);
    P.wingPitch = 0.75f * std::cos(ph) * sat(A);
    P.wingSweep = -0.25f + 0.3f * std::sin(ph + 0.6f) * sat(A);
    float tl = t - 2.15f;                              // lift-off
    V3 X = P.body.X(), Y = P.body.Y(), Z = P.body.Z(), O = P.body.T();
    float crouch = 0.25f * MM * (ease(1.5f, 2.05f, t) - ease(2.05f, 2.2f, t) * 2.2f);
    O = O - Y * crouch;
    float pitchUp = -0.10f * ease(1.4f, 2.1f, t) - 0.75f * ease(0.0f, 0.9f, tl);
    if (tl > 0) {
        V3 up(0, 1, 0);
        O = O + up * (0.0085f * tl * tl + 0.004f * tl) + V3(0.8f, 0, 0.3f) * (0.0085f * tl * tl + 0.001f * tl) + up * (0.00012f * std::sin(ph + 1.2f));
    }
    P.body = M34::fromBasis(X, Y, Z, O) * M34::rotX(-pitchUp);
    if (tl > 0) { // legs let go: blend the feet to tucked positions in the body frame
        float k = ease(0.0f, 0.35f, tl);
        static const V3 rest[3] = {V3(1.5f, -0.9f, 2.6f) * MM, V3(2.0f, -1.0f, 0.3f) * MM, V3(1.7f, -0.9f, -2.3f) * MM};
        for (int kk = 0; kk < 3; kk++) for (int s = 0; s < 2; s++) { float sg = s ? -1.f : 1.f; V3 rp = P.body.pt(V3(rest[kk].x * sg, rest[kk].y, rest[kk].z)); P.foot[kk * 2 + s] = lerp(P.foot[kk * 2 + s], rp, k); }
    }
    for (int s = 0; s < 2; s++) { P.antYaw[s] += -0.3f * ease(0.3f, 1.2f, t); P.antPitch[s] += -0.25f * ease(0.3f, 1.2f, t); }
    return P;
}

static void frameCommon(Scene& sc, FrameCtx& fc, int mode) {
    float t = (float)fc.t; float tt = (float)fc.f / (fc.nf - 1);
    sc.addInst(stemB, M34::identity(), 0); sc.addInst(bgB, M34::identity(), 0);
    for (const DropPt& d : drops) sc.addInst(dropB, M34::translate(d.p) * M34::scale(d.r), 0);
    float sh = (mode == 0 ? 0.4f : 0.3f) / 24.0f;
    lb::Pose p0 = mode == 0 ? poseClimb(t) : poseFly(t), p1 = mode == 0 ? poseClimb(t + sh) : poseFly(t + sh);
    lb::emit(sc, M, p0, p1, mode == 1 && p0.wingOut > 0.02f);
    { // rig continuity check: a knee that jumps much further than its foot means the IK flipped
        static std::vector<V3> pk, pf; std::vector<V3> kn; lb::Parts tmp; lb::solve(M, p0, tmp, &kn);
        if (pk.size() == kn.size()) for (size_t i = 0; i < kn.size(); i++) { float dk = len(kn[i] - pk[i]), df = len(p0.foot[i] - pf[i]); if (dk > 2.5f * df + 0.8f * MM) fprintf(stderr, "  RIG WARNING f%d leg %zu knee moved %.2f mm, foot %.2f mm\n", fc.f, i, dk / MM, df / MM); }
        pk = kn; pf.assign(p0.foot, p0.foot + 6); }
    Camera& c = fc.cam; Env& e = fc.env;
    V3 body = p0.body.T();
    if (mode == 0) {
        // follow with a lag: the head tracks a smoothed version of the walk
        float Ds = 0; for (int k = 0; k < 9; k++) Ds += walked(t - 0.55f + 0.08f * k - 0.3f); Ds /= 9;
        V3 tg = stemAt(std::min(S0 + Ds, tipStop())) + V3(-0.002f, 0.004f, 0);
        c.target = tg; c.pos = tg + V3(0.012f, 0.006f, 0.150f);
        c.focal = 0.15f; c.fstop = 14.0f; c.focusDist = len(body + V3(0.0006f, 0, 0.0012f) - c.pos);
    } else {
        V3 tip = stemAt(stemLen);
        c.target = tip + V3(0.006f, 0.0035f, 0); c.pos = c.target + V3(0.012f, 0.004f, 0.165f);
        c.focal = 0.15f; c.fstop = 14.0f; c.focusDist = len(stemAt(tipStop()) + V3(0.001f, 0, 0.0015f) - c.pos);
    }
    e.sunDir = norm(V3(-0.42f, 0.66f, 0.62f));
    e.sunE = V3(6.0f, 5.2f, 4.0f); e.sunAngR = 0.03f;
    e.type = 2; e.bd0 = V3(0.04f, 0.06f, 0.02f); e.bd1 = V3(0.12f, 0.20f, 0.05f); e.bdScale = 3.5f; e.bdMix = 1; e.bdP0 = -0.05f; e.bdP1 = 0.8f; e.bdP2 = 13; e.bdP3 = 0.85f; e.bdGap = 0.08f;
    e.zen = V3(0.14f, 0.30f, 0.66f) * 1.3f; e.hor = V3(0.50f, 0.66f, 0.26f) * 1.2f; e.gnd = V3(0.02f, 0.035f, 0.014f); e.horExp = 0.5f;
    e.glow = 0.04f; e.glowExp = 20; e.eps = 1.5e-6f; e.clampL = 18;
    Rng r(33);
    for (int i = 0; i < 26; i++) {
        V3 p(r.r(-0.5f, 0.5f), r.r(-0.3f, 0.5f), r.r(-2.2f, -0.6f));
        float tw = 0.7f + 0.3f * std::sin(t * (mode ? 0.1f : 1.0f) * r.r(1, 3) + r.r(0, 6.28f));
        float br = r.f(); br = 0.08f + 0.92f * br * br * br; float cm = r.f();
        V3 col = (cm < 0.5f ? lerp(V3(1.0f, 0.9f, 0.62f), V3(1.0f, 0.97f, 0.85f), cm * 2) : lerp(V3(0.55f, 0.8f, 0.3f), V3(0.8f, 0.95f, 0.6f), cm * 2 - 1)) * (br * 0.5e-3f * tw);
        float dz = p.z - c.pos.z; sc.addGlint(p, col * (dz * dz), r.r(0.05f, 0.22f));
    }
    for (const DropPt& d : drops) { V3 v = norm(c.pos - d.p); V3 n = norm(v + e.sunDir); float F = 0.02f + 0.98f * std::pow(1 - std::max(dot(v, n), 0.f), 5.0f); sc.addGlint(d.p + n * d.r, e.sunE * (F * d.r * d.r * 0.25f), 0.1f); }
    (void)tt;
}
static void frameClimb(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 0); }
static void frameFly(Scene& sc, FrameCtx& fc) { frameCommon(sc, fc, 1); }
} // namespace lbs
