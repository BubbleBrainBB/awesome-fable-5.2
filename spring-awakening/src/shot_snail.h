// shot_snail.h — after the rain: a banded snail glides along a mossy branch, tentacles probing.
// Anatomy from memory: coiled shell on the back with its mouth sunk into the body and filled by the mantle, muscular foot,
// a neck that slopes down to the head, two long upper tentacles carrying the eyes, two short lower ones.
#pragma once
#include "plants.h"

namespace snl {
static BlasH branchB, shellB, dropB, collarB; static Moss moss;
static int mBark, mShell, mSkin, mTent, mEye, mWater, mMantle;
struct MossPt { V3 p, n; float h; uint32_t seed; }; static std::vector<MossPt> mossPts;
struct DropPt { V3 p; float r; }; static std::vector<DropPt> drops;
static const float BR = 0.03f;
static V3 branchC(float x) { return V3(x, 0.006f * std::sin(x * 14 + 0.5f) + 0.004f * std::sin(x * 37) - 0.16f * x - 0.25f * x * x, 0.006f * std::sin(x * 6 + 1)); }
static float branchR(float x) { return BR * (1 + 0.06f * fbm(V3(x * 10, 0.3f, 0), 2)); }
static float topY(float x) { return branchC(x).y + branchR(x); }

static void init(Scene& sc) {
    mBark = sc.addMat([] { Mat m = mkMat(V3(0.11f, 0.085f, 0.06f), 0.6f, 0.9f, 7); set4(m.base2, V3(0.03f, 0.024f, 0.018f)); m.scale = 90; m.bump = 1; set4(m.p0, 9, 14, 0.0012f, 0.6f); set4(m.p2, V3(0.26f, 0.34f, 0.22f)); return m; }());
    mShell = sc.addMat([] { Mat m = mkMat(V3(0.60f, 0.47f, 0.20f), 0.22f, 0.9f, 12); set4(m.base2, V3(0.10f, 0.055f, 0.03f)); m.trans = 0.12f; m.bump = 1; set4(m.p0, 2600, 0.0000022f, 0, 0); set4(m.p2, V3(0.72f, 0.66f, 0.5f)); return m; }());
    mSkin = sc.addMat([] { Mat m = mkMat(V3(0.47f, 0.36f, 0.20f), 0.14f, 1.5f, 13); set4(m.base2, V3(0.47f, 0.36f, 0.20f) * 0.78f); m.trans = 0.3f; set4(m.sh, V3(0.12f, 0.09f, 0.05f)); m.bump = 1; set4(m.p0, 2.4f, 76, 0.00004f, 0); set4(m.p2, V3(0.42f, 0.36f, 0.26f), 0.7f); return m; }());
    mTent = sc.addMat([] { Mat m = mkMat(V3(0.45f, 0.36f, 0.22f), 0.16f, 1.4f, 13); set4(m.base2, V3(0.36f, 0.28f, 0.17f)); m.trans = 0.22f; set4(m.sh, V3(0.22f, 0.18f, 0.13f)); m.bump = 1; set4(m.p0, 2.4f, 18, 0.00002f, 0); set4(m.p2, V3(0.4f, 0.34f, 0.25f), 0.0f); return m; }());
    mMantle = sc.addMat([] { Mat m = mkMat(V3(0.44f, 0.34f, 0.19f), 0.16f, 1.5f, 0); m.trans = 0.3f; set4(m.sh, V3(0.12f, 0.09f, 0.05f)); return m; }());
    mEye = sc.addMat([] { Mat m = mkMat(V3(0.008f), 0.1f, 1.0f, 0); return m; }());
    mWater = sc.addMat([] { Mat m = mkMat(V3(1), 0.0f, 1.0f, 0); m.type = MT_DIEL; m.ior = 1.33f; set4(m.sh, V3(0.9f)); return m; }());
    mossInit(sc, moss, V3(0.035f, 0.09f, 0.015f), V3(0.20f, 0.36f, 0.045f), 6);
    Mesh m; Rng r(77);
    { int n = 80; std::vector<V3> p(n); std::vector<float> rr(n); for (int i = 0; i < n; i++) { float x = -0.35f + 0.7f * i / (n - 1.f); p[i] = branchC(x); rr[i] = branchR(x); }
      Frames f = ptFrames(p, V3(0, 1, 0));
      addTube(m, p, rr, 72, mBark, 0.3f, 8, &f, [](float u, float v) { return 1 + 0.05f * ridged(V3(std::cos(v * 2 * PI) * 2.5f, std::sin(v * 2 * PI) * 2.5f, u * 30), 3) - 0.035f; });
      branchB = sc.addBlas(m); m.clear(); }
    mossPts.clear();
    for (int k = 0; k < 90000; k++) {
        float x = r.r(-0.3f, 0.3f); float a = r.r(-1.9f, 2.4f);      // angle from the top towards the camera side
        float dn = fbm(V3(x * 12, a * 2, 4), 3);
        float path = a > 0 ? smooth(0.55f, 1.25f, a) : smooth(0.30f, 0.75f, -a);     // keep the snail's track (and the view of its foot) clear
        if (dn < 0.12f - 0.35f * path + 0.1f * hash1(k)) continue;
        V3 c = branchC(x); V3 n(0, std::cos(a), std::sin(a));
        mossPts.push_back({c + n * (branchR(x) * 0.985f), n, (0.0025f + 0.004f * sat(dn * 2)) * (0.45f + 0.55f * path), (uint32_t)k});
    }
    // shell: log-spiral tube. local X = spiral axis (apex +X), spiral in the YZ plane
    { int nt = 260, nv = 30; std::vector<V3> P(nt * nv); std::vector<V2> UV(nt * nv);
      float R1 = 0.0074f, th1 = 2 * PI * 3.6f, b = std::log(2.3f) / (2 * PI), psi0 = 2.9f;
      for (int i = 0; i < nt; i++) for (int j = 0; j < nv; j++) {
          float u = i / (nt - 1.f); float th = th1 * (0.05f + 0.95f * u); float R = R1 * std::exp(b * (th - th1)); float rho = 0.95f * R;
          float psi = psi0 - (th1 - th); float x = 1.25f * (R1 - R);
          V3 er(0, std::cos(psi), std::sin(psi)); V3 C = V3(x, 0, 0) + er * R;
          float v = 2 * PI * j / nv; float flare = 1 + 0.06f * smooth(0.985f, 1.0f, u);
          P[i * nv + j] = C + (er * std::cos(v) + V3(1, 0, 0) * std::sin(v)) * (rho * flare); UV[i * nv + j] = V2(u, (float)j / nv);
      }
      addGrid(m, nt, nv, P, &UV, nullptr, mShell, false, true, 0, false);
      shellB = sc.addBlas(m); m.clear();
      // mantle collar: the animal fills the mouth of the shell
      V3 er(0, std::cos(psi0), std::sin(psi0)), tg(0, -std::sin(psi0), std::cos(psi0)); float rho = 0.95f * R1 * 1.06f;
      M34 X = M34::fromBasis(er, V3(1, 0, 0), tg, er * R1 - tg * 0.0002f);
      addBlob(m, V3(0), V3(rho * 0.985f, rho * 0.985f, rho * 0.6f), 20, 28, mMantle, 0, nullptr, &X); collarB = sc.addBlas(m); m.clear(); }
    addBlob(m, V3(0), V3(1), 16, 24, mWater); dropB = sc.addBlas(m); m.clear();
    drops.clear();
    for (int k = 0; k < 140; k++) { float x = r.r(-0.25f, 0.25f); float a = r.r(-1.3f, 1.9f); if (std::fabs(a) < 0.3f) continue; V3 c = branchC(x); V3 n(0, std::cos(a), std::sin(a)); float rr = r.r(0.0004f, 0.0013f); drops.push_back({c + n * (branchR(x) + 0.004f + rr * 0.3f), rr}); }
    for (int k = 0; k < 9; k++) { float x = r.r(-0.2f, 0.2f); V3 c = branchC(x); float rr = r.r(0.0014f, 0.0024f); drops.push_back({c + V3(0, -branchR(x) - rr * 0.8f, r.r(0.0f, 0.012f)), rr}); }
}

static void frame(Scene& sc, FrameCtx& fc) {
    float t = (float)fc.t; float tt = (float)fc.f / (fc.nf - 1);
    sc.addInst(branchB, M34::identity(), 0);
    for (const MossPt& mp : mossPts) { Rng r2(mp.seed * 7 + 1); mossAdd(sc, moss, mp.p, mp.n, mp.h, r2); }
    for (const DropPt& d : drops) sc.addInst(dropB, M34::translate(d.p) * M34::scale(V3(d.r, d.r * 0.85f, d.r)), 0);
    // ---- snail ----
    float Lb = 0.046f * (1 + 0.025f * std::sin(t * 2.2f));
    float xTail = -0.040f + 0.0014f * t + 0.0003f * std::sin(t * 2.2f);
    float headYaw = 0.35f * std::sin(t * 0.55f + 0.4f) + 0.1f * std::sin(t * 1.7f);
    float headLift = 0.6f + 0.4f * std::sin(t * 0.8f + 1);
    Mesh m; int ns = 110, na = 40; std::vector<V3> P(ns * na); std::vector<V2> UV(ns * na);
    auto center = [&](float s) { float x = xTail + s * Lb; float hd = smooth(0.72f, 1.0f, s);
        float z = 0.006f * hd * hd * headYaw + 0.0008f * std::sin(s * 5 + t * 0.7f) * (1 - s);
        return V3(x - 0.0012f * hd * std::fabs(headYaw), topY(x) + 0.0035f * hd * hd * headLift, z); };
    auto bodyH = [&](float s) { float e = sat((s - 0.9f) / 0.1f); return 0.0078f * (0.16f + 0.84f * smooth(0.0f, 0.42f, s)) * (1 - 0.36f * smooth(0.60f, 1.0f, s)) * std::pow(std::max(0.f, std::sin(PI * std::pow(s, 0.5f) * 0.9f)), 0.38f) * std::sqrt(std::max(0.f, 1 - e * e * e * e)); };
    for (int i = 0; i < ns; i++) for (int j = 0; j < na; j++) {
        float s = i / (ns - 1.f); float a = 2 * PI * j / na;
        float e9 = sat((s - 0.9f) / 0.1f); float w = 0.0062f * std::pow(std::max(0.f, std::sin(PI * std::pow(s, 0.6f) * 0.9f)), 0.5f) * (1 - 0.2f * smooth(0.78f, 1.0f, s)) * std::sqrt(std::max(0.f, 1 - e9 * e9 * e9 * e9));
        float h = bodyH(s);
        float ca = std::cos(a), sa = std::sin(a);
        float y = sa > 0 ? h * std::pow(sa, 0.7f) : -0.0002f;
        float fr = 1 + 0.20f * std::exp(-sa * sa / 0.015f) * (1 - 0.6f * smooth(0.85f, 1.0f, s));   // foot fringe
        float wn = (1 - 0.35f * smooth(0.25f, 0.8f, sa > 0 ? sa : 0.f) * smooth(0.0f, 0.3f, s));        // body narrows above the foot
        V3 c = center(s); V3 c2 = center(std::min(1.f, s + 0.02f)); V3 fw = norm(c2 - c + V3(1e-5f, 0, 0)); V3 side = norm(cross(V3(0, 1, 0), fw));
        P[i * na + j] = c + side * (w * ca * fr * wn) + V3(0, y, 0); UV[i * na + j] = V2(s * Lb * 1000, (float)j / na);
    }
    addGrid(m, ns, na, P, &UV, nullptr, mSkin, false, true, 0, true);
    // tentacles
    V3 hc = center(0.95f), hc2 = center(1.0f); V3 fw = norm(hc2 - hc), side = norm(cross(V3(0, 1, 0), fw));
    for (int k = 0; k < 4; k++) {
        bool upper = k < 2; float sg = (k & 1) ? -1.f : 1.f;
        float ext = 1;   // a touch-and-retract event on each upper tentacle
        if (upper) { float ph = t - (sg > 0 ? 2.6f : 5.4f); if (ph > 0) ext = 1 - 0.72f * std::exp(-ph * 1.1f) * smooth(0.0f, 0.18f, ph); ext *= 0.92f + 0.08f * std::sin(t * 1.3f + k); }
        float L = (upper ? 0.0135f : 0.0034f) * ext; float r0 = upper ? 0.00046f : 0.00034f;
        float sb = upper ? 0.925f : 0.975f;
        V3 base = center(sb) + V3(0, bodyH(sb) * (upper ? 0.82f : 0.3f), 0) + side * (sg * (upper ? 0.0017f : 0.0018f));
        float sw = 0.22f * std::sin(t * 0.9f + k * 1.9f) + 0.1f * std::sin(t * 2.3f + k);
        V3 d0 = norm(fw * (upper ? 0.55f : 1.0f) + V3(0, upper ? 0.9f : -0.2f, 0) + side * (sg * (upper ? 0.62f : 0.6f) + sw * 0.5f));
        V3 d1 = norm(d0 + fw * (0.75f + 0.3f * std::sin(t * 0.7f + k)) + V3(0, -0.45f + 0.2f * sw, 0));
        int n = 16; std::vector<V3> p(n); std::vector<float> rr(n);
        for (int i = 0; i < n; i++) { float u = i / (n - 1.f); p[i] = base - d0 * 0.0006f + lerp(d0, d1, u * 0.7f) * ((L + 0.0006f) * u); rr[i] = r0 * (1.35f - 0.85f * u) * (1 + (upper ? 1.3f : 0.5f) * std::exp(-(1 - u) * (1 - u) / 0.004f)) * (0.8f + 0.2f / std::max(ext, 0.3f)); }
        addTube(m, p, rr, 10, mTent, 0.2f * k, 1);
        addBlob(m, p[n - 1], V3(rr[n - 1] * 0.98f), 8, 10, mTent);
        if (upper) addBlob(m, p[n - 1] + d1 * (r0 * 0.5f) + V3(0, r0 * 0.25f, 0), V3(r0 * 0.6f), 8, 10, mEye);
    }
    sc.addInst(sc.addBlas(m), M34::identity(), 0);
    // shell rides on the back, rocking a little
    { V3 c = center(0.50f); float rock = 0.05f * std::sin(t * 2.2f + 1) + 0.03f * std::sin(t * 0.9f);
      M34 S = M34::translate(c + V3(-0.005f, bodyH(0.5f) * 0.2f + 0.0074f * 0.95f + -0.003f, -0.004f)) * M34::rotZ(rock) * M34::rotY(0.30f) * M34::rotX(-0.55f) * M34::fromBasis(V3(0, 0, 1), V3(0, 1, 0), V3(1, 0, 0), V3(0)) * M34::rotX(-0.2f);
      sc.addInst(shellB, S, 0); sc.addInst(collarB, S, 0); }
    Camera& c = fc.cam; Env& e = fc.env;
    V3 look = V3(xTail + 0.028f + 0.003f * tt, topY(0) + 0.008f, 0);
    c.target = look; c.pos = look + V3(0.022f - 0.012f * tt, 0.034f, 0.30f);
    c.focal = 0.15f; c.fstop = 11.0f; c.focusDist = len(center(0.9f) + V3(0, 0.004f, 0.002f) - c.pos);
    e.sunDir = norm(V3(-0.55f, 0.42f, -0.72f)); e.sunE = V3(6.0f, 5.0f, 3.6f) * (0.55f + 0.45f * ease(0.0f, 0.5f, tt)); e.sunAngR = 0.03f;
    e.type = 2; e.bd0 = V3(0.04f, 0.05f, 0.02f); e.bd1 = V3(0.11f, 0.17f, 0.045f); e.bdScale = 4; e.bdMix = 1; e.bdP0 = -0.08f; e.bdP1 = 0.85f; e.bdP2 = 15; e.bdP3 = 0.92f; e.bdGap = 0.03f;
    e.zen = V3(0.20f, 0.33f, 0.62f) * 1.25f; e.hor = V3(0.52f, 0.64f, 0.26f) * 1.2f; e.gnd = V3(0.015f, 0.026f, 0.012f); e.horExp = 0.5f;
    e.glow = 0.06f; e.glowExp = 22; e.eps = 4e-6f; e.clampL = 18; e.sunDisc = 5.0f;
    for (const DropPt& d : drops) { V3 v = norm(c.pos - d.p); V3 n = norm(v + e.sunDir); float F = 0.02f + 0.98f * std::pow(1 - std::max(dot(v, n), 0.f), 5.0f); sc.addGlint(d.p + n * d.r, e.sunE * (F * d.r * d.r * 0.25f), 0.1f); }
    Rng r(41);
    for (int i = 0; i < 30; i++) {
        V3 p(r.r(-0.6f, 0.6f), r.r(-0.3f, 0.5f), r.r(-2.4f, -0.6f));
        float tw = 0.7f + 0.3f * std::sin(t * r.r(1, 3) + r.r(0, 6.28f));
        float br = r.f(); br = 0.08f + 0.92f * br * br * br; float cm = r.f();
        V3 col = (cm < 0.5f ? lerp(V3(1.0f, 0.9f, 0.62f), V3(1.0f, 0.97f, 0.85f), cm * 2) : lerp(V3(0.55f, 0.8f, 0.3f), V3(0.8f, 0.95f, 0.6f), cm * 2 - 1)) * (br * 0.8e-3f * tw);
        float dz = p.z - c.pos.z; sc.addGlint(p, col * (dz * dz), r.r(0.05f, 0.22f));
    }
}
} // namespace snl
