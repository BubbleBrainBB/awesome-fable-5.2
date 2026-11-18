// shots.h — 镜头表：每个镜头的机位、对焦、蜜蜂与花的动作。
// 所有动作都是时间的确定性函数（步态模拟在建场景时一次算完），所以任何一帧都能单独重渲。
#pragma once
#include <array>

struct Cam {
    V3 pos, fwd, right, up; float focus = 200, fieldW = 30, apR = 2.4f;
    void lookAt(const V3& eye, const V3& tgt, float roll = 0) {
        pos = eye; fwd = norm(tgt - eye); right = norm(cross(fwd, V3(0, 1, 0))); up = cross(right, fwd);
        if (roll != 0) { right = rotAxis(right, fwd, roll); up = rotAxis(up, fwd, roll); }
    }
};
struct FrameSetup {
    Cam c0, c1; FramePose pose;
    V3 sunDir = norm(V3(-0.6f, 0.5f, -0.6f)), sunE = V3(9.0f, 7.4f, 5.4f);
    V3 zen = V3(0.55f, 0.85f, 1.5f), hor = V3(1.5f, 1.45f, 1.35f), gnd = V3(0.10f, 0.16f, 0.05f);
    V3 fillDir = V3(0, 0.3f, 1), wind = V3(0); float fillK = 0.6f;
    float catD = 40, catM = 1.9f, rimK = 0.18f, bladeRot = 0.2f, timeSec = 0, hairBoost = 1.7f, sunCos = std::cos(0.8f * PI / 180);
    float farGain = 1, farSeed = 0, twPhase = 0, farBlur = 0.012f;
    V3 farA = V3(0.035f, 0.075f, 0.022f), farB = V3(0.16f, 0.30f, 0.07f), farC = V3(0.62f, 0.30f, 0.42f);
    std::vector<float> highlights;
    void fillU(float* U, const World& W, float aspect) {
        auto set = [&](int i, const V3& v, float w = 0) { U[i * 4] = v.x; U[i * 4 + 1] = v.y; U[i * 4 + 2] = v.z; U[i * 4 + 3] = w; };
        set(U_CAM_POS0, c0.pos); set(U_CAM_POS1, c1.pos); set(U_CAM_R0, c0.right); set(U_CAM_R1, c1.right);
        set(U_CAM_U0, c0.up); set(U_CAM_U1, c1.up); set(U_CAM_F0, c0.fwd); set(U_CAM_F1, c1.fwd);
        set(U_LENS, V3(c0.focus, c1.focus, c0.fieldW), c0.apR); set(U_LENS2, V3(catD, catM, rimK), bladeRot);
        set(U_SUN_DIR, norm(sunDir), sunCos); set(U_SUN_E, sunE); set(U_SKY_ZEN, zen); set(U_SKY_HOR, hor); set(U_GROUND, gnd);
        set(U_MISC, V3(timeSec, 0, P("debug", 0)), hairBoost); set(U_FILL, norm(fillDir), fillK);
        set(U_FAR0, V3(farBlur, farGain, farSeed), 0); set(U_FAR1, farA); set(U_FAR2, farB); set(U_FAR3, farC); set(U_WIND, wind, twPhase);
        (void)W; (void)aspect;
    }
};

struct ShotCfg { int frames; float slow; int K; float fgR; float camAz0, camAz1; float hairK, hairW, pollenK, pellet; bool bee; float shutter; float wind; };
static const int NSHOT = 11;
static const ShotCfg CFG[NSHOT] = {
    /* 0 调试静帧 */ {1,   1,        1,  0,   -0.6f, 0.6f,  1.0f, 1.0f, 1.0f, 1.0f,  true,  0.0f, 1.0f},
    /* 1 建立镜头 */ {192, 1,        20, 70,  -0.1f, 0.8f,  0.35f, 1.7f, 0.5f, 0.55f, true,  0.5f, 1.0f},
    /* 2 慢动作飞来 */ {168, 1 / 60.f, 4,  0,   -0.4f, 0.7f,  1.0f, 1.0f, 0.5f, 0.55f, true,  0.14f, 0.8f},
    /* 3 降落 */     {150, 1 / 20.f, 6,  0,   -0.2f, 1.0f,  0.8f, 1.1f, 0.5f, 0.55f, true,  0.22f, 0.8f},
    /* 4 行走探蜜 */ {216, 1,        2,  0,   -0.8f, 0.4f,  1.0f, 1.0f, 0.8f, 0.62f, true,  0.5f, 0.8f},
    /* 5 口器特写 */ {216, 1,        2,  0,   -1.4f, -0.3f, 1.0f, 1.0f, 1.0f, 0.68f, true,  0.5f, 0.6f},
    /* 6 绒毛花粉 */ {192, 1,        2,  0,   0.1f, 1.3f,   1.2f, 1.0f, 3.2f, 0.78f, true,  0.5f, 0.6f},
    /* 7 梳理花粉篮 */ {192, 1,      2,  0,   -0.1f, 1.1f,  1.0f, 1.0f, 1.6f, 0.90f, true,  0.5f, 0.6f},
    /* 8 俯拍绕行 */ {168, 1,        2,  60,  -1.1f, 0.1f,  0.8f, 1.1f, 1.6f, 0.96f, true,  0.5f, 1.5f},
    /* 9 起飞 */     {144, 1 / 14.f, 6,  0,   -0.4f, 0.8f,  0.7f, 1.2f, 1.6f, 1.0f,  true,  0.16f, 0.8f},
    /* 10 收尾 */    {168, 1,        2,  100, -0.3f, 0.9f,  0.1f, 1.0f, 0.0f, 1.0f,  false, 0.5f, 1.2f},
};

static V3 pathCR(double t, std::initializer_list<std::pair<double, V3>> ks) {
    const std::pair<double, V3>* k = ks.begin(); int n = (int)ks.size();
    if (t <= k[0].first) return k[0].second;
    if (t >= k[n - 1].first) return k[n - 1].second;
    int i = 0; while (i < n - 2 && t > k[i + 1].first) i++;
    float s = (float)((t - k[i].first) / (k[i + 1].first - k[i].first));
    V3 p1 = k[i].second, p2 = k[i + 1].second, p0 = i > 0 ? k[i - 1].second : p1 * 2 - p2, p3 = i + 2 < n ? k[i + 2].second : p2 * 2 - p1;
    float s2 = s * s, s3 = s2 * s;
    return (p1 * 2 + (p2 - p0) * s + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * s2 + (p1 * 3 - p0 - p2 * 3 + p3) * s3) * 0.5f;
}
static Xf flyFrame(const V3& pos, float yaw, float pitch, float roll) {
    V3 f0(-std::cos(yaw), 0, std::sin(yaw));
    V3 fwd = f0 * std::cos(pitch) + V3(0, 1, 0) * std::sin(pitch), up = V3(0, 1, 0) * std::cos(pitch) - f0 * std::sin(pitch);
    V3 right = cross(fwd, up);
    up = rotAxis(up, fwd, roll); right = rotAxis(right, fwd, roll);
    return Xf(pos, fwd, up, right);
}
static Xf blendXf(const Xf& a, const Xf& b, float t) {
    Xf r; r.o = mix(a.o, b.o, t); r.x = norm(mix(a.x, b.x, t)); V3 y = mix(a.y, b.y, t); r.z = norm(cross(r.x, y)); r.y = cross(r.z, r.x); return r;
}

struct Shots {
    FlowerInfo fi;
    BeeRig rig;
    int shot = 0;
    ShotCfg cfg;
    bool hasBee = true;
    std::vector<int> petalPart; int pBase = -1, pFlorets = -1;
    Garden garden;
    static constexpr float DIP = 0.030f;       // 蜜蜂的体重让花头下沉的角度（弧度）
    const float xA = -1.0f, zA = 0.3f, yawA = 0.21f;

    // ---- 花：花头整体随花梗摆动（风 + 蜜蜂的重量），花瓣各自轻微颤动 ----
    Xf flowerXf(double t, float dip) const {
        Xf tilt = Xf::rot(V3(1, 0, 0), 3 * PI / 180) * Xf::rot(V3(0, 0, 1), -6 * PI / 180);
        float wk = cfg.wind * P("wind", 1.0f);
        float sx = wk * (0.012f * std::sin((float)t * 1.9f) + 0.010f * wob(t * 0.9, 1.0f) + 0.004f * wob(t * 3.1, 2.0f));
        float sz = wk * (0.010f * std::sin((float)t * 1.5f + 1.0f) + 0.008f * wob(t * 1.1, 3.0f));
        V3 piv(0.5f, -70, 0.4f);
        return Xf::rotAbout(piv, V3(0, 0, 1), sx + dip) * Xf::rotAbout(piv, V3(1, 0, 0), sz + 0.3f * dip) * tilt;
    }
    void poseFlower(FramePose& fp, int key, double t, float dip) const {
        Xf fx = flowerXf(t, dip);
        fp.keys[pBase][key] = fx; fp.keys[pFlorets][key] = fx;
        float wk = cfg.wind * P("wind", 1.0f);
        for (int k = 0; k < fi.nPetal; k++) {
            V3 tang = norm(cross(V3(0, 1, 0), fi.petalDir[k]));
            float a = wk * (0.020f * wob(t * 1.7 + k * 3.3, 10.0f + k) + 0.012f * std::sin((float)t * 4.3f + k * 2.1f)) + 0.6f * dip * (0.3f + 0.7f * std::max(0.f, dot(fi.petalDir[k], V3(-1, 0, 0))));
            fp.keys[petalPart[k]][key] = fx * Xf::rotAbout(fi.petalHinge[k], tang, a);
        }
    }
    // 花盘上某处的落脚点（花的局部坐标）
    V3 footOn(const V3& q) const {
        float r = std::sqrt(q.x * q.x + q.z * q.z);
        if (r > fi.Rd * 1.25f) return V3(q.x, -1.55f + 0.30f * (r - 4.7f), q.z);
        float best = 1e9f; int bi = 0;
        for (size_t i = 0; i < fi.top.size(); i++) { float d = sqr(fi.top[i].x - q.x) + sqr(fi.top[i].z - q.z); if (d < best) { best = d; bi = (int)i; } }
        V3 tp = fi.top[bi];
        V3 off(q.x - tp.x, 0, q.z - tp.z); float ol = len(off);
        if (ol > 0.22f) off = off * (0.22f / ol);
        return tp + off - V3(0, 0.10f, 0);
    }
    struct Stand { Xf X; V3 foot[6]; float y, pitch; };
    void idealFeet(float x, float z, float yaw, V3* foot) const {
        V3 fwd(-std::cos(yaw), 0, std::sin(yaw)), right = cross(fwd, V3(0, 1, 0));
        for (int idx = 0; idx < 6; idx++) {
            int li = idx / 2; float sg = idx % 2 == 0 ? -1.f : 1.f;
            V3 id = LEGS[li].ideal;
            float jx = 0.5f * (hashF(li * 7 + (idx % 2) * 3 + 11) - 0.5f), jz = 0.4f * (hashF(li * 5 + (idx % 2) + 3) - 0.5f);
            V3 q = V3(x, 0, z) + fwd * (id.x + jx) + right * ((id.z + jz) * sg);
            float qr = std::sqrt(q.x * q.x + q.z * q.z), rmax = fi.Rd * 0.93f;
            if (qr > rmax && qr < fi.Rd * 1.25f) { q.x *= rmax / qr; q.z *= rmax / qr; }
            foot[idx] = footOn(q);
        }
    }
    static void heightPitch(const V3* foot, float& y, float& pitch) {
        float hf = (foot[0].y + foot[1].y) / 2, hh = (foot[4].y + foot[5].y) / 2, hall = 0;
        for (int i = 0; i < 6; i++) hall += foot[i].y / 6;
        float hmax = -1e9f; for (int i = 0; i < 6; i++) hmax = std::max(hmax, foot[i].y);
        float y0 = hall + 3.3f, y1 = hmax + 2.75f;
        y = y0 + std::max(0.f, y1 - y0) * 0.8f; pitch = std::atan2(hf - hh, 8.7f) * 0.6f;
    }
    static Xf bodyXf(float x, float y, float z, float yaw, float pitch) {
        V3 fwd(-std::cos(yaw), 0, std::sin(yaw));
        V3 f2 = fwd * std::cos(pitch) + V3(0, 1, 0) * std::sin(pitch), up = V3(0, 1, 0) * std::cos(pitch) - fwd * std::sin(pitch);
        return Xf(V3(x, y, z), f2, up, cross(f2, up));
    }
    Stand standAt(float x, float z, float yaw, float extraPitch = 0, float lower = 0) const {
        Stand s; idealFeet(x, z, yaw, s.foot);
        heightPitch(s.foot, s.y, s.pitch);
        s.X = bodyXf(x, s.y - lower, z, yaw, s.pitch + extraPitch);
        return s;
    }

    // ---- 步态模拟：交替三角步。身体沿给定路径走，足的落点粘在小花上，超出舒适范围就换步 ----
    struct Gait {
        double dt = 1.0 / 240; int n = 0;
        std::vector<std::array<V3, 6>> foot;
        std::vector<float> by, bp;
        std::function<void(double, float&, float&, float&)> path;
        void run(const Shots& sh, double dur) {
            n = (int)(dur / dt) + 2; foot.resize(n); by.resize(n); bp.resize(n);
            float x, z, yaw; path(0, x, z, yaw);
            V3 anchor[6], from[6], to[6]; double t0[6]; bool step[6] = {0, 0, 0, 0, 0, 0};
            sh.idealFeet(x, z, yaw, anchor);
            float y, pitch; heightPitch(anchor, y, pitch);
            const int grp[2][3] = {{0, 3, 4}, {1, 2, 5}};
            const double T = 0.085; double lastStep = -1; int lastGrp = 1;
            for (int i = 0; i < n; i++) {
                double t = i * dt;
                path(t, x, z, yaw);
                V3 ideal[6]; float xa, za, ya; path(t + 0.06, xa, za, ya); sh.idealFeet(xa, za, ya, ideal);
                bool any = false; for (int k = 0; k < 6; k++) any |= step[k];
                if (!any && t - lastStep > 0.03) {
                    float err[2] = {0, 0};
                    for (int g = 0; g < 2; g++) for (int k = 0; k < 3; k++) err[g] = std::max(err[g], len(anchor[grp[g][k]] - ideal[grp[g][k]]));
                    int g = err[0] > err[1] ? 0 : 1;
                    if (err[g] > 0.85f) {
                        if (err[1 - g] > 0.85f && g == lastGrp) g = 1 - g;   // 两组都落后时严格交替
                        for (int k = 0; k < 3; k++) { int f = grp[g][k]; if (len(anchor[f] - ideal[f]) > 0.25f) { step[f] = true; t0[f] = t + 0.012 * k; from[f] = anchor[f]; to[f] = ideal[f]; } }
                        lastGrp = g;
                    }
                }
                V3 cur[6];
                for (int k = 0; k < 6; k++) {
                    if (step[k]) {
                        float s = (float)((t - t0[k]) / T);
                        if (s >= 1) { step[k] = false; anchor[k] = to[k]; lastStep = t; cur[k] = anchor[k]; }
                        else if (s <= 0) cur[k] = from[k];
                        else { float e = s * s * (3 - 2 * s); cur[k] = mix(from[k], to[k], e) + V3(0, 0.75f * std::sin(PI * s), 0); }
                    } else cur[k] = anchor[k];
                    foot[i][k] = cur[k];
                }
                float ty, tp; heightPitch(anchor, ty, tp);
                float a = (float)(dt / 0.10);
                y += (ty - y) * a; pitch += (tp - pitch) * a;
                by[i] = y; bp[i] = pitch;
            }
        }
        void at(double t, V3* f, float& y, float& pitch) const {
            double fi = clampf((float)(t / dt), 0, (float)(n - 2)); int i = (int)fi; float w = (float)(fi - i);
            for (int k = 0; k < 6; k++) f[k] = mix(foot[i][k], foot[i + 1][k], w);
            y = mix(by[i], by[i + 1], w); pitch = mix(bp[i], bp[i + 1], w);
        }
    } gait;

    void build(World& W, int shot_) {
        shot = shot_; cfg = CFG[shot];
        initFlowerMats(W); initBeeMats(W); initBgMats(W);
        Builder B(W, 20261117);
        gHairWidthK = P("hairW", cfg.hairW);
        buildCosmos(B, "hero", 20261118, 0, V3(0.90f, 0.42f, 0.66f), &fi, L_FG);
        for (int k = 0; k < fi.nPetal; k++) petalPart.push_back(W.part("hero_petal" + std::to_string(k)));
        pBase = W.part("hero_base"); pFlorets = W.part("hero_florets");
        hasBee = cfg.bee && P("bee", 1) > 0;
        if (hasBee) rig.build(B, L_FG, P("hairK", cfg.hairK), P("pollenK", cfg.pollenK), P("headK", 0.5f), P("legK", 0.45f));
        if (hasBee) rig.calibrate();
        fprintf(stderr, "hairs: %zu\n", B.hairCount);
        gHairWidthK = 1;
        if (P("bg", 1) > 0) garden.build(B, 777, P("fgR", cfg.fgR), (int)P("bgFlowers", 22), (int)P("bgLeaves", 90), P("camAz0", cfg.camAz0), P("camAz1", cfg.camAz1), false, P("leafClear", shot == 8 ? 140.0f : 0.0f));
        if (shot == 4) {
            gait.path = [](double t, float& x, float& z, float& yaw) {
                x = kf(t, {{0.2, 1.9f}, {3.3, -1.0f}}); z = kf(t, {{0.2, 1.0f}, {3.3, 0.3f}}); yaw = kf(t, {{0.0, -0.16f}, {1.6, 0.02f}, {3.7, 0.21f}}); };
            gait.run(*this, 10);
        }
        if (shot == 8) {
            gait.path = [](double t, float& x, float& z, float& yaw) {
                x = kf(t, {{1.3, -1.0f}, {3.7, 0.5f}}); z = kf(t, {{1.3, 0.3f}, {3.7, -1.1f}}); yaw = kf(t, {{1.2, 0.21f}, {3.8, 1.15f}}); };
            gait.run(*this, 8);
        }
    }

    // 第 nth 近的可探小花（花局部坐标的喙目标）
    V3 probFloret(const Xf& Xl, int nth) const {
        V3 ideal = Xl.pt(V3(3.0f, -3.6f, 0.0f));
        std::vector<std::pair<float, int>> c;
        for (size_t i = 0; i < fi.top.size(); i++) if (fi.kind[i] != 2) c.push_back({len(fi.top[i] - ideal), (int)i});
        std::sort(c.begin(), c.end());
        int bi = c[std::min(nth, (int)c.size() - 1)].second;
        return fi.top[bi] - fi.axis[bi] * 0.9f;
    }
    // 触角、呼吸、头部的常驻小动作
    void idle(BeeState& S, double tau, float tap, float seed) const {
        for (int e = 0; e < 2; e++) {
            float s = seed + e * 7.7f;
            S.antYaw[e] = 0.16f * wob(tau * 2.3, s) + 0.06f * wob(tau * 7.1, s + 1);
            S.antPitch[e] = 0.14f * wob(tau * 1.9, s + 2);
            float tp = std::max(0.f, wob(tau * 1.3, s + 3)) * tap;                       // 一阵一阵地点触
            S.antElbow[e] = 0.22f * wob(tau * 3.1, s + 4) - 0.30f * tp * (0.5f + 0.5f * std::sin((float)tau * TAU * 7.5f + e * 2.0f));
        }
        float pump = 0.5f + 0.5f * std::sin((float)tau * TAU * 3.1f + 0.6f * std::sin((float)tau * 1.3f));
        float burst = smooth(-0.2f, 0.3f, wob(tau * 0.45, seed + 9));                    // 腹部一阵一阵地泵动
        S.breath = 0.5f + (pump - 0.5f) * (0.25f + 0.75f * burst);
        S.abdCurl = 0.10f + 0.05f * wob(tau * 0.7, seed + 11);
        S.abdYaw = 0.05f * wob(tau * 0.5, seed + 12);
        S.wingOpen = 0.02f * wob(tau * 5, seed + 13) + 0.10f * smooth(0.75f, 0.95f, wob(tau * 0.6, seed + 14));
    }
    // 足的微调：隔一阵抬起一只脚换个抓握点
    V3 regrip(const V3& anchor, int idx, double tau, float seed) const {
        double period = 2.3 + 0.9 * hashF(idx * 31 + (int)(seed * 10));
        double ph = tau / period + hashF(idx * 17 + 5 + (int)(seed * 10));
        int n = (int)std::floor(ph); float f = (float)(ph - n);
        auto offs = [&](int k) { uint32_t h = hashU((uint32_t)(k * 131 + idx * 977 + (int)(seed * 100))); return V3(hashF(h) - 0.5f, 0, hashF(h + 1) - 0.5f) * 0.55f; };
        float T = (float)(0.11 / period);
        V3 a = offs(n - 1), b = offs(n);
        if (f < T) { float s = f / T, e = s * s * (3 - 2 * s); return anchor + mix(a, b, e) + V3(0, 0.45f * std::sin(PI * s), 0); }
        return anchor + b;
    }
    // 飞行时足的悬垂姿态（世界坐标）；reach>0 时向前下方伸出准备着陆
    void flightFeet(BeeState& S, double tau, float reach) const {
        static const V3 tgt[3] = {V3(3.6f, -3.0f, 1.3f), V3(0.2f, -4.6f, 2.4f), V3(-5.3f, -4.9f, 1.9f)};
        static const V3 rch[3] = {V3(4.0f, -3.6f, 2.5f), V3(0.4f, -4.3f, 4.6f), V3(-5.4f, -4.3f, 3.4f)};
        for (int idx = 0; idx < 6; idx++) {
            int li = idx / 2; float sg = idx % 2 == 0 ? -1.f : 1.f;
            V3 q = mix(tgt[li], rch[li], reach);
            q.z *= sg;
            q += V3(0.18f * wob(tau * 31 + idx, 40.f + idx), 0.15f * wob(tau * 27 + idx, 50.f + idx), 0.12f * wob(tau * 23, 60.f + idx));
            S.foot[idx] = S.X.pt(q); S.footN[idx] = S.X.dir(norm(V3(li == 2 ? 0.45f : 0.1f, 1, 0)));
        }
    }
    // 站在花上的蜜蜂：Xl、foot 都在花的局部坐标
    void onFlower(BeeState& S, const Xf& fx, const Xf& Xl, const V3* foot) const {
        S.X = fx * Xl;
        for (int i = 0; i < 6; i++) { S.foot[i] = fx.pt(foot[i]); S.footN[i] = fx.dir(V3(0, 1, 0)); }
    }

    struct Eval { BeeState S; bool bee = false; Cam cam; float dip = 0; double tw = 0; };
    static Cam mkCam(const V3& tgt, float azDeg, float elDeg, float D, float fieldW, float apR, const V3& focusPt, double tf, float focusOff = 0) {
        float az = azDeg * PI / 180, el = elDeg * PI / 180;
        V3 eye = tgt + V3(std::sin(az) * std::cos(el), std::sin(el), std::cos(az) * std::cos(el)) * D;
        // 三脚架上的微小晃动：慢漂 + 细抖
        float a = fieldW * 0.0011f, b = fieldW * 0.00035f;
        V3 jt(a * wob(tf * 0.9, 71.f) + b * wob(tf * 6.3, 72.f), a * wob(tf * 1.1, 73.f) + b * wob(tf * 7.1, 74.f), 0);
        Cam c; c.lookAt(eye, tgt);
        V3 t2 = tgt + c.right * jt.x + c.up * jt.y;
        c.lookAt(eye, t2, 0.0015f * wob(tf * 0.7, 75.f));
        c.focus = dot(focusPt - eye, c.fwd) + focusOff;
        // 对焦呼吸：对焦距离变化时取景略有伸缩
        c.fieldW = fieldW * (1 + 0.25f * (c.focus - D) / D); c.apR = apR;
        return c;
    }

    void eval(double tf, Eval& E) const {
        BeeState& S = E.S;
        double tw = tf * cfg.slow; E.tw = tw;
        S.pellet = P("pellet", cfg.pellet);
        Stand stA = standAt(xA, zA, yawA, -0.04f, 0.2f);
        const double W230 = TAU * 230.0;
        switch (shot) {
        case 0: {
            E.bee = true; E.dip = 0;
            Xf fx = flowerXf(tw, 0);
            Stand st = standAt(P("beeX", xA), P("beeZ", zA), P("beeYaw", 12) * PI / 180, P("beePitch", -2.3f) * PI / 180, P("beeLower", 0.2f));
            onFlower(S, fx, st.X, st.foot);
            idle(S, P("tau", 3.0f), 1, 1);
            S.headPitch = P("headPitch", 0.80f); S.headYaw = P("headYaw", 0.12f);
            S.probExt = P("probExt", 1); S.lick = P("lick", 0.5f);
            S.probTarget = fx.pt(probFloret(st.X, (int)P("floret", 0)));
            S.wingFlight = P("wingFlight", 0); S.wingPhase = P("wingPhase", 0); if (P("wingOpen", -1) >= 0) S.wingOpen = P("wingOpen", 0);
            V3 tgt(P("tx", 0.8f), P("ty", 4.0f), P("tz", 0));
            E.cam = mkCam(tgt, P("camAz", 0), P("camEl", 6), P("camDist", 210), P("fieldW", 26), P("apR", 2.4f), tgt, 0, P("focusOff", -2));
            break; }
        case 1: {   // 建立镜头：清晨的花丛，滑轨缓移；末尾蜜蜂飞入
            E.dip = 0;
            float u = (float)(tf / 8.0);
            V3 tgt = mix(V3(10, -1, 0), V3(-5, 1, 0), u * u * (3 - 2 * u) * 0.9f + 0.1f * u);
            Xf fx = flowerXf(tw, 0);
            E.cam = mkCam(tgt, 22 - 5 * u, 6.5f, 600, 128 - 8 * u, 9.0f, fx.pt(V3(0, 1, 0)), tf, 0);
            E.bee = tf > 5.3;
            V3 p = pathCR(tf, {{5.3, V3(-150, 62, -90)}, {6.1, V3(-78, 34, -40)}, {6.8, V3(-36, 20, -10)}, {7.4, V3(-17, 13, 3)}, {8.3, V3(-12, 11.5f, 5)}});
            V3 p2 = pathCR(tf + 0.02, {{5.3, V3(-150, 62, -90)}, {6.1, V3(-78, 34, -40)}, {6.8, V3(-36, 20, -10)}, {7.4, V3(-17, 13, 3)}, {8.3, V3(-12, 11.5f, 5)}});
            V3 v = p2 - p; float yaw = std::atan2(v.z, -v.x);
            p += V3(0.8f * wob(tw * 6, 81), 0.6f * wob(tw * 7, 82), 0.6f * wob(tw * 5, 83));
            S.X = flyFrame(p, yaw, 0.35f, 0.15f * wob(tw * 3, 84));
            idle(S, tw, 0, 2); S.wingFlight = 1; S.wingPhase = (float)std::fmod(W230 * tw, (double)TAU); S.wingAmp = 1; S.headPitch = 0.5f; S.probExt = 0;
            flightFeet(S, tw, 0);
            break; }
        case 2: {   // 高速摄影：蜜蜂贴着花瓣飞向花盘
            E.dip = 0;
            float u = (float)(tf / 7.0);
            float s = 1 - std::pow(1 - u, 1.35f);
            V3 p = mix(V3(41, 10.5f, 3.2f), V3(11.0f, 8.0f, 1.8f), s);
            float wph = (float)std::fmod(W230 * tw, (double)TAU);
            p += V3(0, 0.30f * wob(tw * 22, 91) + 0.07f * std::sin(wph), 0.25f * wob(tw * 17, 92));
            S.X = flyFrame(p, 0.14f + 0.05f * wob(tw * 9, 93), 0.40f + 0.05f * wob(tw * 14, 94), 0.06f * wob(tw * 12, 95));
            E.bee = true;
            idle(S, tw * 8 + 3, 0, 3); S.wingFlight = 1; S.wingPhase = wph; S.wingAmp = 1.0f; S.headPitch = 0.42f; S.probExt = 0;
            S.abdCurl = 0.22f + 0.05f * wob(tw * 10, 96);
            flightFeet(S, tw, smooth(0.6f, 1.0f, u) * 0.5f);
            // 摄影师跟拍：略微滞后，偶尔追焦不准再拉回
            float lag = (float)(tf - 0.22);
            float ul = sat(lag / 7.0f), sl = 1 - std::pow(1 - ul, 1.35f);
            V3 tgt = mix(V3(39.5f, 8.6f, 0), V3(13.5f, 7.0f, 0), sl) + V3(0.5f * wob(tf * 0.6, 97), 0.3f * wob(tf * 0.8, 98), 0);
            float miss = 3.2f * std::exp(-sqr((float)(tf - 2.6) / 0.45f)) - 2.0f * std::exp(-sqr((float)(tf - 5.2) / 0.35f)) + 0.5f * wob(tf * 0.9, 99);
            E.cam = mkCam(tgt, 7, 3.5f, 260, 31, 2.6f, S.X.pt(V3(1.2f, 0, -1.2f)), tf, miss);
            break; }
        case 3: {   // 降落：足前伸、触花、花头一沉再回弹、收翅
            float tc = 0.146f;
            float dtc = (float)tw - tc;
            const float om = TAU * 9.0f, ze = 0.22f;
            E.dip = dtc > 0 ? DIP * (1 - std::exp(-ze * om * dtc) * std::cos(om * dtc)) : 0;
            Xf fx = flowerXf(tw, E.dip);
            Xf H0 = flyFrame(V3(11.0f, 8.0f, 1.8f), 0.14f, 0.40f, 0);
            float a = smooth(0.000f, 0.167f, (float)tw);
            float settle = dtc > 0 ? 0.55f * std::exp(-dtc * 14) * std::sin(dtc * 46) : 0;
            Stand st = standAt(xA, zA, yawA, -0.02f + 0.10f * settle, 0.1f + 0.5f * settle);
            Xf Xw = fx * st.X;
            Xf Xb = blendXf(H0, Xw, a);
            Xb.o = mix(H0.o, Xw.o, a) + V3(0, 1.4f * std::sin(PI * a) * (1 - a), 0) + V3(0, 0.18f * wob(tw * 24, 101) * (1 - a), 0);
            S.X = Xb; E.bee = true;
            idle(S, tw * 4 + 5, 0.5f, 4);
            S.wingFlight = 1 - smooth(0.192f, 0.248f, (float)tw); S.wingPhase = (float)std::fmod(W230 * tw, (double)TAU); S.wingAmp = 1.0f - 0.35f * smooth(0.136f, 0.198f, (float)tw);
            S.wingOpen = 0.6f * (1 - smooth(0.236f, 0.298f, (float)tw));
            S.headPitch = 0.42f + 0.2f * a; S.probExt = 0; S.abdCurl = 0.25f - 0.12f * a;
            BeeState Sf = S; flightFeet(Sf, tw, smooth(0.000f, 0.099f, (float)tw));
            const float c0[3] = {0.118f, 0.133f, 0.149f}, c1[3] = {0.152f, 0.167f, 0.186f};
            for (int i = 0; i < 6; i++) {
                float w = smooth(c0[i / 2], c1[i / 2], (float)tw + 0.0025f * (i % 2));
                S.foot[i] = mix(Sf.foot[i], fx.pt(st.foot[i]), w); S.footN[i] = nlerp(Sf.footN[i], fx.dir(V3(0, 1, 0)), w);
            }
            E.cam = mkCam(V3(2.5f, 3.6f, 0), 24, 8.5f, 300, 47, 3.0f, fx.pt(V3(0, 3.5f, 1.5f)), tf, 0.4f * wob(tf * 0.7, 102));
            break; }
        case 4: {   // 在花盘上行走，触角探路，找到小花后伸出喙
            E.dip = DIP; E.bee = true;
            Xf fx = flowerXf(tw, E.dip);
            float x, z, yaw; gait.path(tw, x, z, yaw);
            V3 foot[6]; float y, pitch; gait.at(tw, foot, y, pitch);
            float feed = smooth(3.7f, 4.5f, (float)tw);
            float walk = 1 - smooth(3.2f, 3.8f, (float)tw);
            y += walk * 0.07f * std::sin((float)tw * TAU * 5.5f) - 0.2f * feed;
            Xf Xl = bodyXf(x, y, z, yaw + walk * 0.03f * std::sin((float)tw * TAU * 2.75f), pitch - 0.04f * feed);
            if (walk < 0.01f) for (int i = 0; i < 6; i++) foot[i] = regrip(foot[i], i, tw, 4.0f);
            if (P("dbg", 0) > 0) fprintf(stderr, "t=%.2f x=%.2f z=%.2f yaw=%.2f y=%.2f pitch=%.3f foot0=(%.2f %.2f %.2f)\n", tw, x, z, yaw, y, pitch, foot[0].x, foot[0].y, foot[0].z);
            onFlower(S, fx, Xl, foot);
            idle(S, tw, 1.0f, 5);
            S.headPitch = mix(0.48f, 0.82f, feed) + 0.03f * wob(tw * 2.1, 111) + 0.012f * std::sin((float)tw * TAU * 4.6f) * feed;
            S.headYaw = 0.18f * wob(tw * 1.2, 112) * (1 - feed) + 0.10f * feed;
            S.probExt = smooth(4.1f, 4.9f, (float)tw); S.lick = 0.5f + 0.5f * std::sin((float)tw * TAU * 4.6f);
            S.probTarget = fx.pt(probFloret(stA.X, 0));
            float u = (float)(tf / 9.0);
            V3 tgt = V3(0.45f * x - 1.6f, 3.9f, 0);
            float hunt = 1.8f * std::exp(-sqr((float)(tf - 3.9) / 0.4f)) + 0.3f * wob(tf * 0.8, 113);
            E.cam = mkCam(tgt, -16 + 6 * u, 11, 240, 28 - 4.0f * u * u * (3 - 2 * u), 2.3f, S.X.pt(V3(2.4f, -0.3f, -1.5f)), tf, hunt);
            break; }
        case 5: {   // 口器特写：喙探进小花，舌一伸一缩；中途换一朵
            E.dip = DIP; E.bee = true;
            double tau = tw + 20;
            Xf fx = flowerXf(tau, E.dip);
            float sway = 0.05f * wob(tau * 1.1, 121);
            Stand st = standAt(xA, zA, yawA, -0.04f, 0.2f + 0.05f * wob(tau * 0.9, 122));
            V3 foot[6]; for (int i = 0; i < 6; i++) foot[i] = regrip(st.foot[i], i, tau, 5.0f);
            // 这个机位下左前足如果按默认落点，整条腿正对镜头（只看到一个毛茸茸的端面）：把它的落点往前外侧挪一朵小花
            { V3 q = stA.X.pt(V3(P("s5fx", 3.2f), -3.5f, P("s5fz", -4.6f))); foot[0] = regrip(footOn(V3(q.x, 0, q.z)), 0, tau, 5.0f); }
            Xf Xl = st.X; Xl.o += V3(sway, 0, 0.04f * wob(tau * 0.8, 123));
            onFlower(S, fx, Xl, foot);
            idle(S, tau, 1.2f, 6);
            float ext = kf(tw, {{3.3, 1.0f}, {3.8, 0.42f}, {4.5, 0.42f}, {5.1, 1.0f}});
            float sw = smooth(3.9f, 4.5f, (float)tw);
            S.probExt = ext; S.lick = 0.5f + 0.5f * std::sin((float)tau * TAU * 4.6f);
            S.probTarget = fx.pt(mix(probFloret(stA.X, 0), probFloret(stA.X, 2), sw));
            S.headPitch = 0.82f - 0.17f * (1 - ext) + 0.015f * std::sin((float)tau * TAU * 4.6f) * (ext > 0.9f ? 1 : 0) + 0.02f * wob(tau * 2, 124);
            S.headYaw = mix(0.12f, -0.10f, sw) + 0.02f * wob(tau * 1.5, 125);
            float u = (float)(tf / 9.0);
            V3 headC = S.X.pt(V3(3.1f, -1.2f, -0.5f));
            V3 tgt = fx.pt(stA.X.pt(V3(3.0f, -0.75f, -0.4f)));
            float rack = kf(tf, {{5.4, 0.0f}, {6.3, 1.0f}});
            V3 eyePt = S.X.pt(V3(2.85f, -0.35f, -1.75f)), probPt = mix(headC, S.probTarget, 0.55f);
            E.cam = mkCam(tgt, -50 + 5 * u, 5, 165, 10.0f - 0.8f * u, 2.0f, mix(eyePt, probPt, rack), tf, 0.25f * wob(tf * 0.7, 126) + 0.9f * std::exp(-sqr((float)(tf - 6.4) / 0.25f)));
            break; }
        case 6: {   // 绒毛与花粉的极近特写：逆光毛边，滑轨沿身体缓移
            E.dip = DIP; E.bee = true;
            double tau = tw + 40;
            Xf fx = flowerXf(tau, E.dip);
            Stand st = standAt(xA, zA, yawA, -0.04f, 0.2f + 0.05f * wob(tau * 0.9, 131));
            V3 foot[6]; for (int i = 0; i < 6; i++) foot[i] = regrip(st.foot[i], i, tau, 6.0f);
            onFlower(S, fx, st.X, foot);
            idle(S, tau, 1.0f, 7);
            S.probExt = 1; S.lick = 0.5f + 0.5f * std::sin((float)tau * TAU * 4.6f); S.probTarget = fx.pt(probFloret(stA.X, 0));
            S.headPitch = 0.82f + 0.015f * std::sin((float)tau * TAU * 4.6f); S.headYaw = 0.12f;
            float u = (float)(tf / 8.0); float e = u * u * (3 - 2 * u);
            V3 tb = mix(V3(-1.3f, 1.25f, -1.0f), V3(1.5f, 1.0f, -1.2f), e);
            V3 tgt = fx.pt(stA.X.pt(tb));
            E.cam = mkCam(tgt, 40 + 6 * u, 13, 150, 8.2f, 1.5f, S.X.pt(tb + V3(0, 0.45f, -0.75f)), tf, 0.18f * wob(tf * 0.8, 132));
            break; }
        case 7: {   // 梳理：中足刮胸侧的花粉，后足互搓把花粉压进花粉篮
            E.dip = DIP; E.bee = true;
            double tau = tw + 60;
            Xf fx = flowerXf(tau, E.dip);
            Stand st = standAt(-0.6f, 0.2f, 0.34f, 0.03f, 0.0f);
            V3 foot[6]; for (int i = 0; i < 6; i++) foot[i] = regrip(st.foot[i], i, tau, 7.0f);
            Xf Xl = st.X;
            float rub = smooth(2.9f, 3.3f, (float)tw) * (1 - smooth(5.6f, 6.0f, (float)tw));
            Xl.o += V3(0, 0.25f * rub, 0);
            onFlower(S, fx, Xl, foot);
            idle(S, tau, 0.6f, 8);
            S.probExt = 0; S.headPitch = 0.50f + 0.04f * wob(tau * 1.7, 141); S.headYaw = 0.1f * wob(tau * 1.1, 142);
            S.abdCurl += 0.10f * rub;
            for (int rep = 0; rep < 2; rep++) {   // 左中足：两次沿胸侧向后下方刮
                float t0 = 0.8f + rep * 0.95f, s = (float)((tw - t0) / 0.8);
                if (s > 0 && s < 1) {
                    V3 a = Xl.inv().pt(foot[2]);
                    V3 q = pathCR(s, {{0.0, a}, {0.28, V3(0.9f, -0.5f, -2.25f)}, {0.62, V3(-0.6f, -1.5f, -2.35f)}, {0.8, V3(-1.6f, -2.3f, -2.3f)}, {1.0, a}});
                    float wn = smooth(0.0f, 0.18f, s) * smooth(1.0f, 0.82f, s);
                    S.foot[2] = S.X.pt(q); S.footN[2] = nlerp(S.footN[2], S.X.dir(norm(V3(0, 0.4f, -1))), wn);
                }
            }
            if (rub > 0) {   // 后足互搓
                float ph = (float)tw * TAU * 4.2f;
                for (int e = 0; e < 2; e++) {
                    float sg = e == 0 ? -1.f : 1.f;
                    V3 q(-4.1f + 0.55f * std::sin(ph + e * PI), -3.55f + 0.22f * std::cos(ph + e * PI), sg * 0.80f);
                    S.foot[4 + e] = mix(S.foot[4 + e], S.X.pt(q), rub); S.footN[4 + e] = nlerp(S.footN[4 + e], S.X.dir(norm(V3(0.3f, 1, -sg * 0.5f))), rub);
                }
            }
            float u = (float)(tf / 8.0);
            V3 tgt = fx.pt(st.X.pt(V3(-2.6f, -1.0f, -2.2f)));
            // 焦点跟着左后足花粉篮里的花粉团（跟焦略滞后）
            V3 fpt;
            { BeeState Sl = S; Xf Xi = S.X.inv(); BeeRig::LegSolve ls = rig.solveLeg(4, Xi.pt(S.foot[4]), Xi.dir(S.footN[4]));
              fpt = S.X.pt(ls.K + (ls.A - ls.K) * 0.6f + ls.planeN * 0.45f); (void)Sl; }
            V3 fstand = S.X.pt(V3(-3.2f, -2.0f, -2.9f));
            E.cam = mkCam(tgt, 30 - 5 * u, 7, 190, 15.5f - 1.5f * u, 2.0f, mix(fstand, fpt, 0.8f), tf, 0.3f * wob(tf * 0.8, 143));
            break; }
        case 8: {   // 俯拍：绕着花盘换位置继续采，花在风里摇
            E.dip = DIP; E.bee = true;
            double tau = tw + 80;
            Xf fx = flowerXf(tau, E.dip);
            float x, z, yaw; gait.path(tw, x, z, yaw);
            V3 foot[6]; float y, pitch; gait.at(tw, foot, y, pitch);
            float feed = (1 - smooth(0.7f, 1.3f, (float)tw)) + smooth(3.8f, 4.6f, (float)tw);
            float walk = smooth(1.2f, 1.5f, (float)tw) * (1 - smooth(3.6f, 3.9f, (float)tw));
            y += walk * 0.07f * std::sin((float)tw * TAU * 5.5f) - 0.2f * feed;
            Xf Xl = bodyXf(x, y, z, yaw + walk * 0.03f * std::sin((float)tw * TAU * 2.75f), pitch - 0.04f * feed);
            onFlower(S, fx, Xl, foot);
            idle(S, tau, 1.0f, 9);
            S.headPitch = mix(0.48f, 0.82f, feed) + 0.03f * wob(tau * 2.1, 151);
            S.headYaw = 0.15f * wob(tau * 1.2, 152) * (1 - feed);
            S.probExt = (1 - smooth(0.6f, 1.1f, (float)tw)) + smooth(4.0f, 4.7f, (float)tw); S.lick = 0.5f + 0.5f * std::sin((float)tau * TAU * 4.6f);
            Xf Xend = standAt(0.5f, -1.1f, 1.15f, -0.04f, 0.2f).X;
            S.probTarget = fx.pt(tw < 2.5 ? probFloret(stA.X, 0) : probFloret(Xend, 0));
            float u = (float)(tf / 7.0);
            E.cam = mkCam(V3(0.5f, 0.5f, 0.5f), -32 + 4 * u, 37, 330, 50 - 3 * u, 3.0f, fx.pt(V3(0, 3.0f, 0)), tf, 0.5f * wob(tf * 0.6, 153));
            break; }
        case 9: {   // 起飞：振翅、蹬离，花头回弹
            float tr = 0.135f, dtr = (float)tw - tr;
            const float om = TAU * 9.0f, ze = 0.20f;
            E.dip = dtr > 0 ? DIP * std::exp(-ze * om * dtr) * std::cos(om * dtr) : DIP;
            E.bee = true;
            Xf fx = flowerXf(tw + 100, E.dip);
            Stand st = standAt(xA, zA, yawA, 0.0f, 0.0f);
            Xf Xw = fx * st.X;
            float lift = std::pow(sat(((float)tw - 0.115f) / 0.32f), 1.7f);
            float crouch = smooth(0.02f, 0.09f, (float)tw) * (1 - smooth(0.10f, 0.14f, (float)tw));
            Xf Hf = flyFrame(V3(-19, 27, 8), 0.05f, 0.55f, 0.05f);
            Xf Xb = blendXf(Xw, Hf, smooth(0.0f, 0.5f, lift));
            Xb.o = Xw.o + V3(0, -0.35f * crouch, 0) + (Hf.o - Xw.o) * lift + V3(0, 3.0f * std::sin(PI * std::min(1.f, lift * 1.6f)), 0);
            S.X = Xb;
            idle(S, tw * 4 + 9, 0.3f, 10);
            S.wingFlight = smooth(0.025f, 0.075f, (float)tw); S.wingPhase = (float)std::fmod(W230 * tw, (double)TAU); S.wingAmp = 0.5f + 0.6f * smooth(0.03f, 0.12f, (float)tw);
            S.wingOpen = 0.8f * smooth(0.0f, 0.04f, (float)tw);
            S.headPitch = mix(0.60f, 0.42f, smooth(0.0f, 0.08f, (float)tw)); S.probExt = 0; S.abdCurl = 0.10f + 0.15f * lift;
            BeeState Sf = S; flightFeet(Sf, tw, 1 - smooth(0.16f, 0.30f, (float)tw));
            const float c0[3] = {0.10f, 0.115f, 0.13f}, c1[3] = {0.135f, 0.155f, 0.18f};
            for (int i = 0; i < 6; i++) {
                float w = smooth(c0[i / 2], c1[i / 2], (float)tw + 0.004f * (i % 2));
                S.foot[i] = mix(fx.pt(st.foot[i]), Sf.foot[i], w); S.footN[i] = nlerp(fx.dir(V3(0, 1, 0)), Sf.footN[i], w);
            }
            float u = (float)(tf / 6.0);
            V3 tgt = V3(-1.5f, 5.0f + 5.5f * smooth(0.45f, 1.0f, u), 0);
            E.cam = mkCam(tgt, 12, 5, 320, 56, 3.2f, fx.pt(V3(0, 4.0f, 1.0f)), tf, 0.4f * wob(tf * 0.7, 161));
            break; }
        default: {  // 10 收尾：空了的花在风里轻摇，焦点慢慢移向远处
            E.dip = 0; E.bee = false;
            Xf fx = flowerXf(tw + 120, 0);
            float u = (float)(tf / 7.0);
            float rack = kf(tf, {{3.2, 0.0f}, {6.6, 1.0f}});
            E.cam = mkCam(V3(1, 0.5f, 0), 16 + 3 * u, 8, 350, 66 + 6 * u, 3.6f, fx.pt(V3(0, 1, 0)), tf, 150 * rack * rack);
            break; }
        }
        if (shot >= 5 && shot <= 8) E.tw = tw + (shot - 4) * 20;
        if (shot == 9) E.tw = tw + 100;
        if (shot == 10) E.tw = tw + 120;
    }

    void frame(World& W, int fr, double fps, FrameSetup& F) {
        F.pose.init(W.parts.size());
        double tf = fr / fps;
        F.timeSec = (float)tf;
        float sa = P("sunAz", 232) * PI / 180, se = P("sunEl", 30) * PI / 180;
        F.sunDir = V3(std::cos(se) * std::cos(sa), std::sin(se), std::cos(se) * std::sin(sa));
        F.hairBoost = P("hairBoost", 1.7f); F.fillK = P("fill", 0.6f);
        int K = std::max(1, cfg.K);
        double shut = cfg.shutter / fps;
        std::vector<Eval> ev(K);
        for (int k = 0; k < K; k++) eval(tf + (K > 1 ? shut * k / (K - 1) : 0.0), ev[k]);
        // 花与背景：2 个关键帧（快门开、关）
        int K2 = K > 1 ? 2 : 1;
        for (int p : petalPart) F.pose.keys[p].resize(K2);
        F.pose.keys[pBase].resize(K2); F.pose.keys[pFlorets].resize(K2);
        for (const BgItem& it : garden.items) F.pose.keys[it.part].resize(K2);
        for (int k = 0; k < K2; k++) {
            const Eval& E = ev[k == 0 ? 0 : K - 1];
            poseFlower(F.pose, k, E.tw, E.dip);
            garden.pose(F.pose, k, E.tw, cfg.wind * P("wind", 1.0f));
        }
        if (hasBee) {
            std::vector<Xf> out(W.parts.size());
            bool vis = ev[0].bee;
            for (int id : rig.all) { F.pose.keys[id].resize(K); F.pose.visible[id] = vis; }
            if (vis) for (int k = 0; k < K; k++) { rig.apply(ev[k].S, out); for (int id : rig.all) F.pose.keys[id][k] = out[id]; }
        }
        if (P("dumpRig", 0) > 0 && hasBee && ev[0].bee) {   // 调试：打印各足膝关节（蜂体坐标），用来查反向运动学有没有跳变
            const BeeState& S0 = ev[0].S; Xf Xi = S0.X.inv();
            fprintf(stderr, "RIG %d", fr);
            for (int i = 0; i < 6; i++) { BeeRig::LegSolve ls = rig.solveLeg(i, Xi.pt(S0.foot[i]), Xi.dir(S0.footN[i])); fprintf(stderr, " %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f", ls.K.x, ls.K.y, ls.K.z, ls.F.x, ls.F.y, ls.F.z, ls.planeN.x, ls.planeN.y, ls.planeN.z); }
            fprintf(stderr, " %.3f %.3f %.3f\n", S0.X.o.x, S0.X.o.y, S0.X.o.z);
        }
        F.c0 = ev[0].cam; F.c1 = ev[K - 1].cam; F.c1.fieldW = F.c0.fieldW;
        // 口径蚀：第二光阑的大小随取景范围定，让画面四角的光斑被切成猫眼而不是整片变黑
        float k = F.catD / F.c0.focus;
        F.catM = 0.574f * F.c0.fieldW * k * 0.86f + 0.45f * F.c0.apR;
        // 远景：光斑半径（弧度）取镜头的名义值（整个镜头固定，避免跟焦时色块乱游）
        Eval e0; eval(cfg.frames * 0.5 / fps, e0);
        F.farBlur = e0.cam.apR / e0.cam.focus;
        F.farSeed = (float)(shot * 3.7);
        F.farGain = P("farGain", 1.0f);
        F.wind = V3(0.0015f * (float)std::sin(ev[0].tw * 0.7), 0.001f * (float)std::sin(ev[0].tw * 0.9 + 1), 0);
        F.twPhase = (float)(ev[0].tw * 2.2);
        // 光斑：叶隙天光与叶面反光，分布在镜头名义朝向周围
        RNG r(900 + shot, 5);
        float halfW = 0.5f * e0.cam.fieldW / e0.cam.focus;
        int nh = (int)P("nHl", 46);
        for (int i = 0; i < nh; i++) {
            V3 d = norm(e0.cam.fwd + e0.cam.right * (r.range(-2.2f, 2.2f) * halfW) + e0.cam.up * (r.range(-0.7f, 1.5f) * halfW * 0.5625f));
            float kk = 0.45f + 0.6f * r.f();
            float I = P("hlGain", 1.0f) * (0.010f + 0.10f * std::pow(r.f(), 3.0f)) * F.farBlur * F.farBlur * 6000.f;
            V3 tint = mix(V3(1.0f, 1.03f, 0.86f), V3(0.80f, 1.0f, 0.62f), r.f());
            float h[8] = {d.x, d.y, d.z, r.f(), tint.x * I, tint.y * I, tint.z * I, kk};
            F.highlights.insert(F.highlights.end(), h, h + 8);
        }
    }
};
