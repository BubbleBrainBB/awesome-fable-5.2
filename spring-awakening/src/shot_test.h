// shot_test.h — engine validation scene.
#pragma once
static BlasH tb_ground, tb_ball, tb_glass, tb_leaf, tb_hair;
static void test_init(Scene& sc) {
    sc.addMat(mkMat(V3(0.35f, 0.25f, 0.18f), 0.6f, 1.0f, 1));   // 0 ground
    sc.mats[0].base2[0] = 0.12f; sc.mats[0].base2[1] = 0.09f; sc.mats[0].base2[2] = 0.06f; sc.mats[0].scale = 40; sc.mats[0].bump = 1; sc.mats[0].p0[0] = 0.004f; sc.mats[0].p0[1] = 0.0005f;
    sc.addMat(mkMat(V3(0.8f, 0.1f, 0.08f), 0.15f, 1.0f, 0));    // 1 red glossy
    Mat g = mkMat(V3(1), 0.0f); g.type = MT_DIEL; g.ior = 1.33f; set4(g.sh, V3(0.9f)); sc.addMat(g);   // 2 water
    Mat lf = mkMat(V3(0.15f, 0.4f, 0.05f), 0.3f); lf.trans = 0.5f; set4(lf.base2, V3(0.35f, 0.6f, 0.05f)); sc.addMat(lf); // 3 leaf
    Mat hr = mkMat(V3(0.8f, 0.8f, 0.7f), 0.4f, 0.5f); hr.trans = 0.5f; set4(hr.base2, V3(0.9f, 0.9f, 0.8f)); set4(hr.sh, V3(0.6f)); sc.addMat(hr); // 4 hair
    Mesh m;
    { int n = 64; std::vector<V3> P(n * n); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { float x = (i / (n - 1.f) - 0.5f) * 0.6f, z = (j / (n - 1.f) - 0.5f) * 0.6f; P[i * n + j] = V3(x, 0.004f * fbm(V3(x * 30, 0, z * 30)), z); }
      addGrid(m, n, n, P, nullptr, nullptr, 0, false, false, 0, true); tb_ground = sc.addBlas(m); m.clear(); }
    addBlob(m, V3(0), V3(1), 48, 96, 0); tb_ball = sc.addBlas(m); m.clear();
    { int n = 24; std::vector<V3> P(n * n); for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { float u = i / (n - 1.f), v = j / (n - 1.f) * 2 - 1; float w = std::sin(PI * u) * 0.35f; P[i * n + j] = V3(v * w, u, 0.15f * v * v * w + 0.2f * u * u); }
      addGrid(m, n, n, P, nullptr, nullptr, 3); tb_leaf = sc.addBlas(m); m.clear(); }
    { Rng r(5); for (int i = 0; i < 4000; i++) { V3 d = r.dir(); if (d.y < 0) d.y = -d.y; V3 p0 = d, p1 = d * 1.25f + V3(0, -0.05f, 0); m.seg(p0, 0.006f, p1, 0.001f, 4, 0, 1, r.f()); }
      tb_hair = sc.addBlas(m); m.clear(); }
}
static void test_frame(Scene& sc, FrameCtx& fc) {
    sc.addInst(tb_ground, M34::identity(), 0);
    sc.addInst(tb_ball, M34::translate(V3(-0.03f, 0.02f, 0)) * M34::scale(0.02f), 1);
    sc.addInst(tb_ball, M34::translate(V3(0.02f, 0.015f, 0.02f)) * M34::scale(0.015f), 2);
    sc.addInst(tb_leaf, M34::translate(V3(0.05f, 0.0f, -0.03f)) * M34::rotY(0.5f) * M34::scale(0.07f), 0);
    sc.addInst(tb_ball, M34::translate(V3(0.0f, 0.012f, -0.05f)) * M34::scale(0.012f), 1);
    sc.addInst(tb_hair, M34::translate(V3(0.0f, 0.012f, -0.05f)) * M34::scale(0.012f), 0);
    Rng r(3);
    for (int i = 0; i < 40; i++) sc.addGlint(V3(r.r(-0.3f, 0.3f), r.r(0.0f, 0.15f), r.r(-0.6f, -0.25f)), V3(1.0f, 0.95f, 0.8f) * r.r(0.3e-3f, 2e-3f), 0.0f);
    fc.cam.pos = V3(0.02f, 0.05f, 0.16f); fc.cam.target = V3(0.01f, 0.015f, 0.0f); fc.cam.focal = 0.06f; fc.cam.fstop = 4;
    fc.env.sunDir = V3(-0.5f, 0.5f, -0.6f); fc.env.sunE = V3(4.0f, 3.6f, 3.0f); fc.env.eps = 2e-6f;
}
