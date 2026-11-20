// ladybird.h — seven-spot ladybird (Coccinella septempunctata) built from rigid parts, with a simple rig.
// Anatomy from memory (not checked against references): 6 legs on the thorax underside, 3 visible tarsal segments,
// short clubbed antennae, 7 spots (1 shared at the scutellum + 3 per elytron), pale marks beside the scutellum,
// on the front corners of the pronotum and beside the eyes; membranous hind wings longer than the elytra.
#pragma once
#include "plants.h"

namespace lb {
static const float MM = 0.001f;
struct Model {
    BlasH body, ely[2], pron, head, femur[3], tibia[3], tars, ant, wing[2];
    int mBlack, mEly, mPron, mHead, mWing, mLeg, mEye, mAnt;
    float femL[3] = {1.55f * MM, 1.8f * MM, 2.0f * MM}, tibL[3] = {1.35f * MM, 1.55f * MM, 1.8f * MM}, tarL = 0.85f * MM;
};
static const V3 HIP[3] = {V3(0.80f, 0.12f, 1.75f) * MM, V3(0.95f, 0.10f, 0.55f) * MM, V3(0.95f, 0.10f, -0.65f) * MM};
static const V3 ELY_HINGE = V3(0.22f, 2.42f, 1.85f) * MM;
static const V3 WING_HINGE = V3(0.95f, 2.0f, 1.35f) * MM;
static const V3 ANT_BASE = V3(0.50f, 0.78f, 3.78f) * MM;

struct Pose {
    M34 body = M34::identity();
    float elyYaw = 0, elyRoll = 0;        // elytra opening (rad)
    float wingOut = 0;                    // 0 folded .. 1 extended
    float wingStroke = 0, wingPitch = 0, wingSweep = 0;
    V3 foot[6];                           // world targets, order L1 R1 L2 R2 L3 R3 (L = +x)
    float antYaw[2] = {0, 0}, antPitch[2] = {0, 0};
    float headYaw = 0, headPitch = 0;
};

static void build(Scene& sc, Model& M) {
    M.mBlack = sc.addMat([] { Mat m = mkMat(V3(0.012f, 0.012f, 0.013f), 0.22f, 1.0f, 1); set4(m.base2, V3(0.02f, 0.018f, 0.016f)); m.scale = 9000; m.bump = 1; set4(m.p0, 0.000012f, 0.000004f, 0, 0.6f); return m; }());
    M.mEly = sc.addMat([] { Mat m = mkMat(V3(0.70f, 0.070f, 0.016f), 0.085f, 1.6f, 9); set4(m.base2, V3(0.010f, 0.010f, 0.011f)); m.trans = 0.06f; m.scale = 14000; m.bump = 1; set4(m.p0, 0.000004f, 0, 0, 0); set4(m.p2, V3(0.86f, 0.80f, 0.62f)); return m; }());
    M.mPron = sc.addMat([] { Mat m = mkMat(V3(0.010f, 0.010f, 0.011f), 0.09f, 1.0f, 10); set4(m.base2, V3(0.86f, 0.82f, 0.66f)); m.scale = 14000; m.bump = 1; set4(m.p0, 1.52f, 2.92f, 0.36f, 1.35f); set4(m.p1, 0.000004f, 0, 0, 0); return m; }());
    M.mHead = sc.addMat([] { Mat m = mkMat(V3(0.010f, 0.010f, 0.011f), 0.12f, 1.0f, 10); set4(m.base2, V3(0.86f, 0.82f, 0.66f)); m.scale = 14000; m.bump = 1; set4(m.p0, 0.42f, 3.60f, 0.12f, 1.0f); set4(m.p1, 0.000004f, 0, 0, 0); return m; }());
    M.mWing = sc.addMat([] { Mat m = mkMat(V3(0.20f, 0.14f, 0.08f), 0.34f, 0.8f, 8); m.type = MT_THIN; m.ior = 1.5f; set4(m.base2, V3(0.10f, 0.06f, 0.03f)); m.trans = 0.22f; set4(m.p2, V3(0.86f, 0.80f, 0.68f)); set4(m.sh, V3(0.75f, 0.7f, 0.6f)); return m; }());
    M.mLeg = sc.addMat([] { Mat m = mkMat(V3(0.014f, 0.013f, 0.013f), 0.18f, 1.0f, 0); return m; }());
    M.mEye = sc.addMat([] { Mat m = mkMat(V3(0.006f), 0.05f, 1.0f, 0); return m; }());
    M.mAnt = sc.addMat([] { Mat m = mkMat(V3(0.16f, 0.09f, 0.04f), 0.3f, 0.8f, 0); return m; }());
    Mesh m;
    // ventral body + abdomen top (seen when the elytra open)
    addBlob(m, V3(0, 0.62f, -0.45f) * MM, V3(2.50f, 0.85f, 3.05f) * MM, 28, 40, M.mBlack);
    addBlob(m, V3(0, 1.05f, -0.75f) * MM, V3(2.15f, 1.35f, 2.65f) * MM, 24, 36, M.mBlack, 0, [](V3 d) { return 1 + 0.03f * std::sin(d.z * 0 + std::acos(clampf(d.y, -1, 1)) * 9); });
    addBlob(m, V3(0, 0.95f, 1.75f) * MM, V3(1.75f, 1.05f, 0.95f) * MM, 16, 24, M.mBlack);      // prothorax core
    addBlob(m, V3(0, 2.30f, 1.84f) * MM, V3(0.30f, 0.07f, 0.36f) * MM, 8, 12, M.mBlack);       // scutellum
    M.body = sc.addBlas(m); m.clear();
    // elytra: part of an ellipsoid; alpha from the suture (0) to the margin, beta from front to rear tip
    for (int side = 0; side < 2; side++) {
        int na = 36, nb = 64; std::vector<V3> P(na * nb); std::vector<V2> UV(na * nb);
        V3 C = V3(0, 0.30f, -0.50f) * MM; V3 R = V3(2.72f, 2.42f, 3.30f) * MM;
        for (int i = 0; i < na; i++) for (int j = 0; j < nb; j++) {
            float al = (0.012f + 1.72f * i / (na - 1.f)); float be = 0.72f + (PI - 0.72f) * j / (nb - 1.f);
            // the margin dips a little below the equator and flares out slightly
            float fl = 1 + 0.035f * smooth(1.35f, 1.72f, al);
            V3 q(R.x * std::sin(be) * std::sin(al) * fl, R.y * std::sin(be) * std::cos(al), R.z * std::cos(be));
            q = q + C; float sx = side ? -1.f : 1.f;
            P[i * nb + j] = V3(q.x * sx, q.y, q.z); UV[i * nb + j] = V2(q.x / MM, q.z / MM);
        }
        addGrid(m, na, nb, P, &UV, nullptr, M.mEly, false, false, 0.3f * side, side == 1);
        M.ely[side] = sc.addBlas(m); m.clear();
    }
    // pronotum
    { int na = 40, nb = 22; std::vector<V3> P(na * nb); std::vector<V2> UV(na * nb);
      V3 C = V3(0, 0.42f, 2.05f) * MM; V3 R = V3(2.10f, 1.72f, 1.25f) * MM;
      for (int i = 0; i < na; i++) for (int j = 0; j < nb; j++) {
          float al = -1.62f + 3.24f * i / (na - 1.f); float be = 0.25f + 1.72f * j / (nb - 1.f);     // be: 0 = front pole
          float notch = 1 - 0.16f * std::exp(-al * al / 0.18f) * smooth(0.9f, 0.25f, be);            // front emargination around the head
          V3 q(R.x * std::sin(be) * std::sin(al), R.y * std::sin(be) * std::cos(al), R.z * std::cos(be) * notch);
          q = q + C; P[i * nb + j] = q; UV[i * nb + j] = V2(std::fabs(q.x) / MM, q.z / MM);
      }
      addGrid(m, na, nb, P, &UV, nullptr, M.mPron, false, false, 0, true);
      M.pron = sc.addBlas(m); m.clear(); }
    // head (origin at the neck so it can turn): local coords relative to NECK
    { V3 nk = V3(0, 0.80f, 2.95f) * MM;
      { int nu = 20, nv = 30; std::vector<V3> P(nu * nv); std::vector<V2> UV(nu * nv);
        for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) { float th = PI * i / (nu - 1.f), ph = 2 * PI * j / nv;
            V3 d(std::sin(th) * std::cos(ph), std::cos(th), std::sin(th) * std::sin(ph));
            V3 q = V3(0, 0.0f, 0.42f) * MM + d * V3(1.08f, 0.72f, 0.62f) * MM; P[i * nv + j] = q; UV[i * nv + j] = V2(std::fabs(q.x) / MM, (q.z + nk.z) / MM); }
        addGrid(m, nu, nv, P, &UV, nullptr, M.mHead, false, true, 0, true); }
      for (int s = -1; s <= 1; s += 2) {
          addBlob(m, V3(0.78f * s, 0.10f, 0.50f) * MM, V3(0.34f, 0.40f, 0.36f) * MM, 12, 18, M.mEye);
          // maxillary palp: two joints ending in a hatchet-shaped club
          std::vector<V3> p = {V3(0.35f * s, -0.35f, 0.75f) * MM, V3(0.48f * s, -0.62f, 1.02f) * MM, V3(0.52f * s, -0.80f, 1.18f) * MM};
          addTube(m, p, std::vector<float>{0.07f * MM, 0.08f * MM, 0.16f * MM}, 6, M.mAnt);
          addBlob(m, V3(0.53f * s, -0.86f, 1.24f) * MM, V3(0.17f, 0.10f, 0.14f) * MM, 6, 8, M.mAnt);
      }
      addBlob(m, V3(0, -0.22f, 0.92f) * MM, V3(0.55f, 0.22f, 0.25f) * MM, 8, 12, M.mAnt);    // labrum / mouth
      M.head = sc.addBlas(m); m.clear(); }
    // legs
    for (int k = 0; k < 3; k++) {
        float L = M.femL[k];
        addLathe(m, 14, 10, [L](float u) { return V2((0.10f + 0.13f * std::pow(std::sin(PI * std::pow(u, 0.8f)), 0.7f)) * MM, u * L); }, M34::identity(), M.mLeg);
        addBlob(m, V3(0), V3(0.2f * MM), 6, 8, M.mLeg);
        M.femur[k] = sc.addBlas(m); m.clear();
        L = M.tibL[k];
        addLathe(m, 12, 8, [L](float u) { return V2((0.085f + 0.035f * std::sin(PI * u) + 0.02f * u) * MM, u * L); }, M34::identity(), M.mLeg);
        addBlob(m, V3(0), V3(0.12f * MM), 6, 8, M.mLeg);
        M.tibia[k] = sc.addBlas(m); m.clear();
    }
    { // tarsus: 3 segments with pads + 2 claws, along +Y, sole towards -Z
      float y = 0; for (int i = 0; i < 3; i++) { float l = (i == 2 ? 0.34f : 0.24f) * MM; float w = (i == 1 ? 0.13f : 0.10f) * MM;
          addBlob(m, V3(0, y + l * 0.5f, 0), V3(w, l * 0.56f, w * 0.75f), 8, 10, M.mLeg); y += l; }
      for (int s = -1; s <= 1; s += 2) { std::vector<V3> p = {V3(0, y - 0.03f * MM, 0), V3(0.06f * s * MM, y + 0.08f * MM, 0.0f), V3(0.08f * s * MM, y + 0.10f * MM, -0.08f * MM)}; addTube(m, p, std::vector<float>{0.03f * MM, 0.022f * MM, 0.006f * MM}, 5, M.mLeg); }
      M.tars = sc.addBlas(m); m.clear(); }
    { // antenna along +Y: scape, pedicel, 6 funicle beads, 3-jointed club
      float y = 0; const float len[11] = {0.20f, 0.10f, 0.11f, 0.10f, 0.10f, 0.09f, 0.09f, 0.09f, 0.11f, 0.12f, 0.13f}; const float rad[11] = {0.075f, 0.055f, 0.045f, 0.043f, 0.043f, 0.045f, 0.048f, 0.055f, 0.075f, 0.092f, 0.085f};
      for (int i = 0; i < 11; i++) { float bend = 0.02f * i * i * MM * 0.1f; addBlob(m, V3(bend, y + len[i] * 0.5f * MM, 0), V3(rad[i] * MM, len[i] * 0.56f * MM, rad[i] * MM), 7, 9, M.mAnt); y += len[i] * MM; }
      M.ant = sc.addBlas(m); m.clear(); }
    for (int side = 0; side < 2; side++) { // hind wing: +Y span, +Z up; leading edge towards +X for the left (+x) wing
        Blade b; b.len = 9.4f * MM; b.width = 1.9f * MM; b.maxAt = 0.6f; b.q = 0.55f; b.baseW = 0.35f; b.fold = 0.0f; b.cup = 0.12f; b.pleat = 0.08f * MM; b.pleatN = 3; b.pleatSlope = -0.4f; b.nu = 36; b.nv = 6;
        addBlade(m, b, M34::identity(), M.mWing, 0.5f * side, 0);
        M.wing[side] = sc.addBlas(m); m.clear();
    }
}

// world matrices of all parts for a pose; fixed order so two poses can be paired for motion blur
struct Parts { std::vector<M34> xf; std::vector<int> part; };
static void solve(const Model& M, const Pose& P, Parts& out, std::vector<V3>* knees = nullptr) {
    out.xf.clear(); out.part.clear();
    const M34& B = P.body;
    auto add = [&](int part, const M34& x) { out.xf.push_back(x); out.part.push_back(part); };
    add(0, B);                                              // body
    for (int s = 0; s < 2; s++) {                           // elytra
        float sg = s ? -1.f : 1.f; V3 h(ELY_HINGE.x * sg, ELY_HINGE.y, ELY_HINGE.z);
        M34 r = M34::translate(h) * M34::rotY(-sg * P.elyYaw) * M34::rotZ(sg * P.elyRoll) * M34::translate(-h);
        add(1 + s, B * r);
    }
    add(3, B);                                              // pronotum
    V3 nk = V3(0, 0.80f, 2.95f) * MM;
    add(4, B * M34::translate(nk) * M34::rotY(P.headYaw) * M34::rotX(P.headPitch));
    M34 headM = out.xf.back();
    for (int s = 0; s < 2; s++) {                           // antennae
        float sg = s ? -1.f : 1.f; V3 ab = V3(ANT_BASE.x * sg, ANT_BASE.y - nk.y, ANT_BASE.z - nk.z);
        add(5, headM * M34::translate(ab) * M34::rotY(sg * (0.75f + P.antYaw[s])) * M34::rotX(1.15f + P.antPitch[s]));
    }
    for (int s = 0; s < 2; s++) {                           // hind wings
        float sg = s ? -1.f : 1.f; V3 h(WING_HINGE.x * sg, WING_HINGE.y, WING_HINGE.z);
        float o = P.wingOut;
        // folded: lying back over the abdomen; extended: span along +-X, stroke lifts the tip, sweep moves it fore/aft
        V3 spanF(0.12f * sg, 0.0f, -1.0f);
        V3 spanE(sg * std::cos(P.wingStroke) * std::cos(P.wingSweep), std::sin(P.wingStroke), std::sin(P.wingSweep) * std::cos(P.wingStroke));
        V3 Y = norm(lerp(spanF, spanE, smoother(o)));
        V3 upRef = norm(V3(0, 1, 0) + V3(0, 0, 1) * std::sin(P.wingPitch) * o);
        V3 Z = norm(upRef - Y * dot(upRef, Y)); V3 X = cross(Y, Z);
        float sc = 0.42f + 0.58f * smoother(o);
        add(6 + s, B * M34::fromBasis(X * (0.6f + 0.4f * o), Y * sc, Z, h));
    }
    // legs
    for (int k = 0; k < 3; k++) for (int s = 0; s < 2; s++) {
        float sg = s ? -1.f : 1.f; V3 hipL(HIP[k].x * sg, HIP[k].y, HIP[k].z);
        V3 H = B.pt(hipL); V3 F = P.foot[k * 2 + s];
        float L1 = M.femL[k], L2 = M.tibL[k] + M.tarL * 0.72f;
        V3 hf = F - H; float d = len(hf); float dm = (L1 + L2) * 0.985f;
        if (d > dm) { F = H + hf * (dm / d); hf = F - H; d = dm; }
        if (d < std::fabs(L1 - L2) * 1.05f + 1e-6f) { d = std::fabs(L1 - L2) * 1.05f + 1e-6f; }
        V3 dir = hf / std::max(len(hf), 1e-9f);
        V3 pole = B.dir(norm(V3(sg * 1.0f, 0.75f, k == 0 ? 0.35f : (k == 2 ? -0.35f : 0.0f))));
        V3 pp = pole - dir * dot(pole, dir); pp = norm(pp);
        float a = (L1 * L1 - L2 * L2 + d * d) / (2 * d); float h = std::sqrt(std::max(0.f, L1 * L1 - a * a));
        V3 K = H + dir * a + pp * h;
        V3 kd = norm(F - K); V3 A = K + kd * M.tibL[k];
        if (knees) knees->push_back(K);
        add(10 + k, frameY(H, K - H, 1, 0));
        add(13 + k, frameY(K, A - K, 1, 0));
        // tarsus: from the ankle to the foot, sole facing the body's ventral side
        V3 ty = norm(F - A + kd * 1e-5f); V3 down = B.dir(V3(0, -1, 0)); V3 tz = norm(down - ty * dot(down, ty)); V3 tx = cross(ty, tz);
        add(16, M34::fromBasis(tx, ty, tz, A));
    }
}
static void emit(Scene& sc, const Model& M, const Pose& p0, const Pose& p1, bool showWings) {
    Parts a, b; solve(M, p0, a); solve(M, p1, b);
    for (size_t i = 0; i < a.xf.size(); i++) {
        int pt = a.part[i]; BlasH h;
        if (pt == 0) h = M.body; else if (pt == 1) h = M.ely[0]; else if (pt == 2) h = M.ely[1]; else if (pt == 3) h = M.pron; else if (pt == 4) h = M.head; else if (pt == 5) h = M.ant;
        else if (pt == 6) h = M.wing[0]; else if (pt == 7) h = M.wing[1]; else if (pt >= 10 && pt < 13) h = M.femur[pt - 10]; else if (pt >= 13 && pt < 16) h = M.tibia[pt - 13]; else h = M.tars;
        if ((pt == 6 || pt == 7) && !showWings) continue;
        sc.addInst(h, a.xf[i], 0, V3(1), 0.1f * i, &b.xf[i]);
    }
}
} // namespace lb
