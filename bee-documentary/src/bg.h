// bg.h — 背景花园：别的波斯菊、花蕾、羽状细裂叶、草叶，真实地布置在主花周围，交给镜头去虚化。
// 每个背景植株是一个部件，可以各自随风摆动。
#pragma once
struct BgMats { int leaf, twig, blade; };
static BgMats gBG;
static void initBgMats(World& W) {
    GMat m = mkMat(M_TRANS, V3(0.55f), 0.34f, 0.045f, 1.0f, T_LEAF, V3(0.62f)); m.p0 = 1.0f; gBG.leaf = W.addMat(m);
    gBG.twig = W.addMat(mkMat(M_DIFF, V3(0.8f), 0.45f, 0.045f));
    m = mkMat(M_TRANS, V3(0.55f), 0.30f, 0.045f, 1.0f, T_LEAF, V3(0.55f)); m.p0 = 1.0f; gBG.blade = W.addMat(m);
}
struct BgItem { int part; V3 pivot; float phase, amp, freq; Xf place; };

struct Garden {
    std::vector<BgItem> items;
    // 一片弯曲的草叶/阔叶
    static void addBlade(Builder& B, RNG& rng, const V3& base, const V3& dir0, float L, float Wd, float droop, const V3& col, int seg = 7) {
        V3 d = norm(dir0), side = norm(cross(d, V3(0.3f, 1, 0.2f)));
        side = rotAxis(side, d, rng.f() * TAU);
        V3 p = base; uint32_t prev[2] = {0, 0};
        for (int i = 0; i <= seg; i++) {
            float t = (float)i / seg;
            float w = Wd * std::pow(std::sin(PI * std::min(1.f, 0.12f + t * 0.88f)), 0.6f) * (1 - t * t * 0.6f);
            V3 n = norm(cross(side, d));
            V3 c = col * (0.85f + 0.3f * t);
            uint32_t a = B.vert(p - side * w, n, t, 0, c), b = B.vert(p + side * w, n, t, 1, c);
            if (i > 0) { B.tri(prev[0], prev[1], b, gBG.blade); B.tri(prev[0], b, a, gBG.blade); }
            prev[0] = a; prev[1] = b;
            p = p + d * (L / seg);
            d = norm(d + V3(0, -droop, 0) * (0.25f + t) + rng.onSphere() * 0.05f);
        }
    }
    // 波斯菊的羽状细裂叶：中脉 + 两侧成对的线形裂片
    static void addFeatherLeaf(Builder& B, RNG& rng, const V3& base, const V3& dir0, float L, const V3& col) {
        V3 d = norm(dir0), t1, t2; basis(d, t1, t2);
        float a0 = rng.f() * TAU; V3 side = t1 * std::cos(a0) + t2 * std::sin(a0), nrm = cross(d, side);
        V3 p = base; int n = 7;
        for (int i = 0; i < n; i++) {
            float t = (float)i / n;
            V3 q = p + d * (L / n);
            float r = 0.45f * (1 - 0.6f * t);
            B.cyl(p, q, r, r * 0.85f, gBG.twig, col, 0, 1, rng.f());
            if (i >= 1) for (int s = -1; s <= 1; s += 2) {
                V3 ld = norm(d * 0.8f + side * (float)s + nrm * (0.3f * rng.gauss()));
                float ll = L * 0.42f * (1 - 0.5f * t) * (0.7f + 0.6f * rng.f());
                V3 a = q, b = a + ld * (ll * 0.5f), c = b + norm(ld + d * 0.5f + rng.onSphere() * 0.2f) * (ll * 0.5f);
                B.cyl(a, b, 0.30f, 0.26f, gBG.twig, col, 0, 1, rng.f()); B.cyl(b, c, 0.24f, 0.10f, gBG.twig, col, 0, 1, rng.f());
                if (rng.f() < 0.5f) { V3 e = b + norm(ld * 0.5f + nrm * (rng.f() - 0.5f) + side * (float)s) * (ll * 0.35f); B.cyl(b, e, 0.2f, 0.08f, gBG.twig, col, 0, 1, rng.f()); }
            }
            p = q; d = norm(d + rng.onSphere() * 0.08f + V3(0, -0.04f, 0));
        }
    }
    // 花蕾：萼片包着的小球 + 细梗
    static void addBud(Builder& B, RNG& rng, float scale) {
        V3 gc(0.30f, 0.46f, 0.12f), pc(0.80f, 0.40f, 0.58f);
        B.grid([&](float u, float v) { float th = u * PI, ph = v * TAU; float r = 4.2f * scale * (1 + 0.06f * std::cos(8 * ph) * std::sin(th));
                 V3 c = mix(gc, pc, smooth(0.25f, 0.05f, u)); return PV(V3(r * std::sin(th) * std::cos(ph), 5.0f * scale * std::cos(th), r * std::sin(th) * std::sin(ph)), u, v, c); }, 10, 16, gFM.stem, true, false);
        Spine sp{V3(0, -4.0f * scale, 0), V3(0.5f, -40, 0.4f), V3(3, -110, 2), V3(6, -200, 3)};
        B.tube(sp, V3(1, 0, 0.2f), [&](float, float, float& ra, float& rb) { ra = rb = 0.8f * scale; }, 12, 6, gFM.stem, [=](float, float) { return V3(0.28f, 0.45f, 0.12f); });
        (void)rng;
    }
    // camSector：机位大致的方位（弧度）；该方向近处不放东西，避免挡镜头。fgR：离主花这么近的邻花归入前景层（全分辨率）
    // leafClear：离主花这么近的叶丛整丛不放（俯拍时它们会横在镜头和主体之间）
    void build(Builder& B, uint64_t seed, float fgR, int nFlowers, int nLeaves, float camAz0, float camAz1, bool nearElems, float leafClear = 0) {
        RNG rng(seed, 91);
        V3 pink(0.90f, 0.42f, 0.66f), pale(0.95f, 0.70f, 0.82f), white(0.96f, 0.94f, 0.92f), mag(0.80f, 0.20f, 0.48f);
        auto inCamSector = [&](const V3& p, float margin) {
            float az = std::atan2(p.x, p.z);          // 与 camAz 的定义一致：0 = +z
            float lo = camAz0 - margin, hi = camAz1 + margin;
            for (int k = -1; k <= 1; k++) { float a = az + k * TAU; if (a > lo && a < hi) return true; }
            return false;
        };
        for (int i = 0; i < nFlowers; i++) {
            float r = 34 + 420 * std::pow(rng.f(), 1.4f), a = rng.f() * TAU;
            V3 p(r * std::sin(a), rng.range(-70, 45) * sat(r / 150 + 0.25f), r * std::cos(a));
            float pick = rng.f(), tm = rng.f(), sc = 0.85f + 0.3f * rng.f(), budP = rng.f(), budS = 0.7f + 0.5f * rng.f(), openK = 0.7f + 0.3f * rng.f();
            float tx = rng.range(-0.5f, 0.5f), tz = rng.range(-0.5f, 0.5f), ty = rng.f() * TAU;
            float phase = rng.f() * 20, amp = 0.6f + 0.8f * rng.f(), freq = 0.7f + 0.6f * rng.f();
            if (inCamSector(p, 0.35f) && r < 380) continue;
            V3 tint = pick < 0.5f ? mix(pink, pale, tm) : (pick < 0.75f ? white : (pick < 0.9f ? mag : pale));
            std::string nm = "bg_flower" + std::to_string(i);
            bool fg = r < fgR;
            if (budP < 0.22f) { B.beginPart(nm, fg ? L_FG : L_MID); addBud(B, rng, budS); B.endPart(); }
            else buildCosmos(B, nm, seed + 100 + i, fg ? 1 : 1, tint, nullptr, fg ? L_FG : L_MID, sc, openK);
            BgItem it; it.part = B.W.part(nm);
            Xf tilt = Xf::rot(V3(1, 0, 0), tx) * Xf::rot(V3(0, 0, 1), tz) * Xf::rot(V3(0, 1, 0), ty);
            it.place = Xf::trans(p) * tilt; it.pivot = p + V3(0, -220, 0); it.phase = phase; it.amp = amp; it.freq = freq;
            items.push_back(it);
        }
        // 羽状叶与草叶：成丛（每丛用自己的随机数流）
        for (int i = 0; i < nLeaves; i++) {
            float r = 30 + 380 * std::pow(rng.f(), 1.2f), a = rng.f() * TAU;
            V3 p(r * std::sin(a), rng.range(-170, -10), r * std::cos(a));
            float phase = rng.f() * 20, amp = 0.8f + 1.0f * rng.f(), freq = 0.8f + 0.8f * rng.f();
            if (inCamSector(p, 0.3f) && r < 300) continue;
            if (r < leafClear) continue;
            RNG lr(seed * 31 + i, 17);
            std::string nm = "bg_leaf" + std::to_string(i);
            B.beginPart(nm, r < fgR ? L_FG : L_MID);
            V3 g1(0.16f, 0.30f, 0.07f), g2(0.30f, 0.44f, 0.10f), g3(0.10f, 0.22f, 0.08f);
            int n = 2 + (int)(lr.f() * 4);
            for (int k = 0; k < n; k++) {
                V3 col = mix(mix(g1, g2, lr.f()), g3, lr.f() * 0.5f);
                V3 dir = norm(V3(lr.gauss() * 0.6f, 1.0f, lr.gauss() * 0.6f));
                V3 b0 = V3(lr.gauss() * 8, lr.gauss() * 10, lr.gauss() * 8);
                if (lr.f() < 0.6f || r < 150) addFeatherLeaf(B, lr, b0, dir, (50 + 60 * lr.f()) * (r < 150 ? 0.7f : 1.0f), col);   // 主花附近不放长草叶，免得挡镜头
                else addBlade(B, lr, b0, dir, 90 + 110 * lr.f(), 2.2f + 3 * lr.f(), 0.10f + 0.2f * lr.f(), col * 0.85f);
            }
            B.endPart();
            BgItem it; it.part = B.W.part(nm); it.place = Xf::trans(p); it.pivot = p + V3(0, -120, 0); it.phase = phase; it.amp = amp; it.freq = freq;
            items.push_back(it);
        }
        (void)nearElems;
    }
    void pose(FramePose& fp, int key, double t, float wind) const {
        for (const BgItem& it : items) {
            float sx = wind * it.amp * (0.016f * std::sin((float)t * 1.7f * it.freq + it.phase) + 0.012f * wob(t * 0.8 * it.freq + it.phase, 5.0f));
            float sz = wind * it.amp * (0.013f * std::sin((float)t * 1.3f * it.freq + it.phase * 1.7f) + 0.010f * wob(t * 1.0 * it.freq + it.phase, 6.0f));
            fp.keys[it.part][key] = Xf::rotAbout(it.pivot, V3(0, 0, 1), sx) * Xf::rotAbout(it.pivot, V3(1, 0, 0), sz) * it.place;
        }
    }
};
