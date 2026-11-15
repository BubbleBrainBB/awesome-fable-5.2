// sim.h — everything that moves and is simulated on the CPU, stepped deterministically from t = -PREROLL at 240 Hz:
//   the diver (camera path + breathing buoyancy + fin-kick sway + head inertia + torch hand),
//   a school of fusiliers (boids: separation / alignment / cohesion, terrain, diver and manta avoidance),
//   clouds of anthias hovering over coral heads, a manta ray on a banked glide path, and the diver's own bubbles.
#pragma once
#include "world.h"

static const float FILM_LEN = 78.0f;
static const float SIM_DT = 1.0f / 240.0f;
static const float PREROLL = 8.0f;

struct Key { float t, x, y, z, yaw, pitch; };
// the dive: surface -> reef flat along the sand channel -> over the lip -> down the wall -> out into the blue
static const Key KEYS[] = {
    {-9.0f, -22.6f, -1.5f, 0.6f, 6, 64},
    {0.0f, -22.2f, -1.6f, 0.6f, 6, 64},
    {3.5f, -21.8f, -1.9f, 0.6f, 5, 54},
    {8.0f, -20.4f, -3.2f, 0.6f, 2, -26},
    {13.0f, -17.2f, -5.4f, 0.35f, 0, -22},
    {20.0f, -12.2f, -7.0f, 0.2f, -14, -12},
    {26.0f, -7.6f, -6.2f, 0.35f, 12, -8},
    {31.0f, -3.6f, -4.2f, 0.8f, 0, -15},
    {34.5f, -0.3f, -3.7f, 1.0f, 16, -30},
    {38.0f, 3.7f, -5.2f, 1.2f, 92, -40},
    {43.0f, 5.9f, -10.6f, 3.0f, 120, -34},
    {48.0f, 6.5f, -15.0f, 5.0f, 126, -16},
    {52.5f, 6.6f, -18.0f, 7.6f, 124, -9},
    {58.0f, 6.3f, -21.0f, 11.6f, 126, -6},
    {62.5f, 7.9f, -22.3f, 14.2f, 46, 6},
    {68.0f, 8.4f, -22.6f, 15.3f, 20, 30},
    {74.0f, 8.6f, -22.3f, 15.8f, -40, 52},
    {78.0f, 8.7f, -22.0f, 16.0f, -62, 50},
    {86.0f, 8.8f, -21.8f, 16.1f, -66, 48},
};
static const int NKEYS = sizeof(KEYS) / sizeof(KEYS[0]);
struct PKey { float t, x, y, z; };
static const PKey SCHOOL[] = {   // where the fusilier school wants to be
    {-9, -9, -5.6f, -13}, {8, -10, -5.6f, -12}, {14, -11, -6.0f, -7}, {19.5f, -8.8f, -6.3f, 0.2f}, {25, -6, -5.8f, 6.5f}, {31, -1, -5.2f, 8},
    {37, 5.5f, -7.5f, 7}, {43, 8.5f, -11.5f, 9.5f}, {50, 9.8f, -16.5f, 14}, {57, 10.6f, -20.0f, 19.5f}, {63, 13.5f, -17.5f, 22}, {70, 12.5f, -12.5f, 18.5f},
    {78, 10.5f, -10.5f, 14.5f}, {88, 10, -10, 12},
};
static const PKey MANTA[] = {
    {40, 60, -12, 52}, {50, 45, -13, 41}, {58, 33.5f, -15.5f, 33}, {63, 23.5f, -17.4f, 25.6f}, {67, 15.4f, -18.7f, 19.8f}, {70, 11.4f, -18.5f, 16.6f},
    {73, 9.9f, -16.3f, 11.6f}, {76, 11.8f, -12.2f, 5.2f}, {80, 15.5f, -7.8f, -3}, {88, 22, -4.5f, -14},
};
template <int N> static V3 pathAt(const PKey (&K)[N], float t) {
    int i = 0; while (i < N - 2 && t > K[i + 1].t) i++;
    const PKey& a = K[i]; const PKey& b = K[i + 1]; const PKey& p = K[std::max(i - 1, 0)]; const PKey& n = K[std::min(i + 2, N - 1)];
    float h = b.t - a.t, u = clampf((t - a.t) / h, 0, 1);
    float h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
    auto herm = [&](float va, float vb, float vp, float vn) {
        float ma = (vb - vp) / (b.t - p.t), mb = (vn - va) / (n.t - a.t);
        if (i == 0) ma = (vb - va) / h; if (i + 2 > N - 1) mb = (vb - va) / h;
        return h00 * va + h10 * h * ma + h01 * vb + h11 * h * mb;
    };
    return V3(herm(a.x, b.x, p.x, n.x), herm(a.y, b.y, p.y, n.y), herm(a.z, b.z, p.z, n.z));
}
static void keyAt(float t, V3& pos, float& yaw, float& pitch) {
    int i = 0; while (i < NKEYS - 2 && t > KEYS[i + 1].t) i++;
    const Key& a = KEYS[i]; const Key& b = KEYS[i + 1];
    const Key& p = KEYS[std::max(i - 1, 0)]; const Key& n = KEYS[std::min(i + 2, NKEYS - 1)];
    float h = b.t - a.t, u = clampf((t - a.t) / h, 0, 1);
    float h00 = 2 * u * u * u - 3 * u * u + 1, h10 = u * u * u - 2 * u * u + u, h01 = -2 * u * u * u + 3 * u * u, h11 = u * u * u - u * u;
    auto herm = [&](float va, float vb, float vp, float vn) {
        float ma = (vb - vp) / (b.t - p.t), mb = (vn - va) / (n.t - a.t);
        if (i == 0) ma = (vb - va) / h; if (i + 2 > NKEYS - 1) mb = (vb - va) / h;
        return h00 * va + h10 * h * ma + h01 * vb + h11 * h * mb;
    };
    pos = V3(herm(a.x, b.x, p.x, n.x), herm(a.y, b.y, p.y, n.y), herm(a.z, b.z, p.z, n.z));
    yaw = herm(a.yaw, b.yaw, p.yaw, n.yaw); pitch = herm(a.pitch, b.pitch, p.pitch, n.pitch);
}

static inline float surgeVX(float t) { return 1.18f * cosf(t * 1.18f) + 0.35f * 2.07f * cosf(t * 2.07f + 1.3f); }
static inline float surgeVZ(float t) { return 0.45f * 0.83f * cosf(t * 0.83f + 0.7f) + 0.2f * 1.71f * cosf(t * 1.71f + 2.9f); }
struct Breath { float tin, din, tout, dout; };
struct CamState { V3 pos, f, r, u; float yaw, pitch, roll; V3 torchP, torchD; float torchI; };
struct FishS { V3 pos, vel, fwd, acc, home, off; float len, phase, species, seed, turn, hop; };
struct BubS { V3 pos, vel; float r, age, seed, tb; };
struct GFish { F4 pos, fwd, up, par, vel; };
struct GBub { F4 pos, par, vel; };
struct MantaState { V3 pos, f, u; float phase, amp, scale; };

struct Sim {
    std::vector<Breath> breaths;
    float t = -PREROLL; int stepN = 0;
    float syaw = 0, spitch = 0, vyaw = 0, vpitch = 0, bob = 0, vbob = 0, kick = 0;
    V3 tdir = V3(1, 0, 0), prevCamPos, camVel;
    CamState cam; MantaState manta;
    std::vector<FishS> fish; std::vector<BubS> bubs;
    float emitAcc = 0; uint32_t bubN = 0;
    int nFus = 0;

    void init(const World& world) {
        float tb = -PREROLL + 0.3f; int k = 0;
        while (tb < FILM_LEN + 6) {
            Breath b; b.tin = tb; b.din = 1.75f + 0.3f * hashf(k, 1); b.tout = b.tin + b.din + 0.35f + 0.25f * hashf(k, 2);
            b.dout = 2.5f + 0.5f * hashf(k, 3); tb = b.tout + b.dout + 0.55f + 0.5f * hashf(k, 4); breaths.push_back(b); k++;
        }
        V3 p; keyAt(t, p, syaw, spitch); prevCamPos = p;
        RNG r(777);
        // fusiliers
        V3 a0 = pathAt(SCHOOL, t);
        nFus = 280;
        for (int i = 0; i < nFus; i++) {
            FishS f; f.species = 0; f.len = r.range(0.20f, 0.30f); f.seed = r.f(); f.phase = r.range(0, 6.28f);
            f.off = V3(r.gauss() * 1.9f, r.gauss() * 0.55f, r.gauss() * 1.0f);
            f.pos = a0 + f.off; f.vel = V3(0.3f, 0, 0.8f); f.fwd = norm(f.vel); f.turn = 0; f.hop = 0; f.home = a0;
            fish.push_back(f);
        }
        // anthias: clouds over coral heads near the route
        std::vector<V3> homes;
        for (const Inst& I : world.insts) {
            int type = (int)I.ax.w; if (type != T_TABLE && type != T_BUSH && type != T_DOME && type != T_BARREL) continue;
            V3 top(I.bnd.x, I.bnd.y + I.bnd.w * 0.55f, I.bnd.z);
            float sc = I.pos.w;
            bool flat = top.x < edgeX(top.z) + 0.5f && top.y > -9.5f;
            float want = 0;
            if (flat && top.x > -26 && top.x < 3 && fabsf(top.z - 0.8f) < 7.5f && sc > 0.55f) want = 0.55f;
            if (!flat && top.y > -26 && top.y < -8 && top.z > -4 && top.z < 22) want = 0.22f;
            if (hashf((uint32_t)homes.size() * 7 + 3, (uint32_t)(I.ay.w * 1e6f)) < want) homes.push_back(top);
        }
        for (size_t h = 0; h < homes.size(); h++) {
            int n = 6 + (int)(hashf((uint32_t)h, 91) * 16);
            for (int i = 0; i < n; i++) {
                FishS f; f.species = 1; f.len = r.range(0.065f, 0.105f); f.seed = r.f(); f.phase = r.range(0, 6.28f);
                f.home = homes[h]; f.off = V3(r.gauss() * 0.55f, 0.25f + fabsf(r.gauss()) * 0.5f, r.gauss() * 0.55f);
                f.pos = f.home + f.off; f.vel = V3(0, 0, 0); f.fwd = V3(0.2f, 0, -1); f.turn = 0; f.hop = r.range(0.2f, 3.0f);
                fish.push_back(f);
            }
        }
        fprintf(stderr, "fish: %d fusiliers, %zu anthias in %zu clouds\n", nFus, fish.size() - nFus, homes.size());
        step(0);
    }
    float lung(float tt) const {
        for (const Breath& b : breaths) {
            if (tt < b.tin) return 0;
            if (tt < b.tin + b.din) return smooth01((tt - b.tin) / b.din);
            if (tt < b.tout) return 1;
            if (tt < b.tout + b.dout) return 1 - smooth01((tt - b.tout) / b.dout);
        }
        return 0;
    }
    float exhale(float tt) const {   // exhaust flow 0..1 (strong start, tapering)
        for (const Breath& b : breaths) if (tt >= b.tout && tt < b.tout + b.dout) { float u = (tt - b.tout) / b.dout; return sstep(0, 0.06f, u) * (1 - u * 0.7f) * (1 - sstep(0.85f, 1.0f, u)); }
        return 0;
    }
    void stepManta() {
        V3 p = pathAt(MANTA, t), p1 = pathAt(MANTA, t + 0.15f), p0 = pathAt(MANTA, t - 0.15f);
        V3 v = (p1 - p0) / 0.3f, a = (p1 + p0 - p * 2.0f) / (0.15f * 0.15f);
        V3 f = norm(v); V3 alat = a - f * dot(a, f);
        V3 up = norm(V3(0, 1, 0) + V3(alat.x, 0, alat.z) * 0.9f);
        up = norm(up - f * dot(up, f));
        manta.phase = 2 * PI * t / 3.6f; manta.amp = 0.82f; manta.scale = 3.9f;
        // the body heaves a little against the wing beat
        manta.pos = p + up * (0.10f * sinf(manta.phase + 2.6f)); manta.f = norm(f + up * (0.03f * cosf(manta.phase + 2.6f))); manta.u = norm(up - manta.f * dot(up, manta.f));
    }
    void stepCamera(float dt) {
        V3 kp; float kyaw, kpitch; keyAt(t, kp, kyaw, kpitch);
        V3 kp2; float y2, p2; keyAt(t + 0.05f, kp2, y2, p2);
        float speed = len(kp2 - kp) / 0.05f;
        float effort = clampf(speed / 0.55f, 0.0f, 1.2f);
        // the diver's gaze locks onto the manta when it comes out of the blue
        float wm = sstep(63.2f, 66.4f, t);
        if (wm > 0) {
            V3 d = manta.pos + manta.f * 0.6f + V3(0, 0.5f, 0) - kp;
            float my = atan2f(d.z, d.x) * 180 / PI, mp = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z)) * 180 / PI;
            while (my - kyaw > 180) my -= 360; while (my - kyaw < -180) my += 360;
            kyaw = lerpf(kyaw, my, wm); kpitch = lerpf(kpitch, mp, wm);
        }
        const float om = 3.0f;
        vyaw += (om * om * (kyaw - syaw) - 2 * om * vyaw) * dt; syaw += vyaw * dt;
        vpitch += (om * om * (kpitch - spitch) - 2 * om * vpitch) * dt; spitch += vpitch * dt;
        float lv = lung(t);
        vbob += (0.26f * (lv - 0.45f) - 1.2f * vbob - 0.6f * bob) * dt; bob += vbob * dt;
        kick += 2 * PI * 0.72f * (0.35f + 0.65f * effort) * dt;
        float ke = 0.25f + 0.75f * effort;
        float yaw = syaw + ke * 0.75f * sinf(kick + 0.6f) + 1.3f * (0.5f * sinf(0.31f * t + 1.0f) + 0.32f * sinf(0.73f * t + 2.2f) + 0.18f * sinf(1.7f * t + 0.4f));
        float pitch = spitch + ke * 0.35f * sinf(2 * kick) + 0.9f * (0.5f * sinf(0.27f * t + 3.0f) + 0.3f * sinf(0.61f * t + 0.7f) + 0.2f * sinf(1.43f * t + 1.9f)) - 6.0f * vbob;
        float roll = ke * 1.2f * sinf(kick) + 0.8f * (0.6f * sinf(0.23f * t + 0.5f) + 0.4f * sinf(0.57f * t + 2.6f)) + 0.05f * vyaw;
        float ya = yaw * PI / 180, pa = pitch * PI / 180, ra = roll * PI / 180;
        V3 f(cosf(ya) * cosf(pa), sinf(pa), sinf(ya) * cosf(pa));
        V3 r0 = norm(cross(f, V3(0, 1, 0))), u0 = cross(r0, f);
        V3 r = r0 * cosf(ra) + u0 * sinf(ra), u = u0 * cosf(ra) - r0 * sinf(ra);
        V3 pos = kp + V3(0, bob, 0) + r0 * (0.022f * ke * sinf(kick)) + f * (0.02f * effort * sinf(2 * kick + 0.5f));
        float sa = surgeAmp(pos.y) * 0.6f;
        pos += V3(surgeX(t) * sa, 0, surgeZ(t) * sa);
        prevCamPos = cam.pos;
        if (dt > 0) camVel = (pos - cam.pos) / dt;
        cam.pos = pos; cam.f = f; cam.r = r; cam.u = u; cam.yaw = yaw; cam.pitch = pitch; cam.roll = roll;
        // the torch hand aims at what the diver is looking at (about 2.4 m ahead), with a little lag and wobble
        cam.torchP = pos + r * 0.24f - u * 0.26f + f * 0.12f;
        V3 want = norm(pos + f * 2.4f + r * (0.06f * sinf(0.9f * t)) + u * (0.05f * sinf(1.3f * t + 1.0f)) - cam.torchP);
        tdir = norm(tdir + (want - tdir) * std::min(1.0f, 3.0f * dt));
        cam.torchD = tdir;
        cam.torchI = 4.0f * sstep(45.6f, 45.75f, t) * (1 - sstep(62.0f, 64.5f, t));
    }
    void fishForces() {
        V3 anchor = pathAt(SCHOOL, t), a1 = pathAt(SCHOOL, t + 0.5f);
        V3 av = (a1 - anchor) / 0.5f; float aspd = len(av);
        V3 adir = aspd > 0.05f ? av / aspd : V3(0, 0, 1);
        V3 alat = norm(cross(adir, V3(0, 1, 0)));
        const float dtF = SIM_DT * 4;
        for (int i = 0; i < nFus; i++) {
            FishS& f = fish[i];
            V3 sep, avgV, cen; int nn = 0;
            for (int j = 0; j < nFus; j++) {
                if (j == i) continue;
                V3 d = fish[j].pos - f.pos; float d2 = dot(d, d);
                if (d2 > 1.3f * 1.3f) continue;
                float dd = sqrtf(d2) + 1e-4f;
                if (dd < 0.42f) sep -= d * ((0.42f - dd) / (0.42f * dd));
                avgV += fish[j].vel; cen += d; nn++;
            }
            V3 F;
            if (nn) { F += sep * 7.0f + (avgV / (float)nn - f.vel) * 1.3f + (cen / (float)nn) * 0.5f; }
            // stay in formation around the travelling anchor (an elongated stream)
            V3 slot = anchor + adir * f.off.x + V3(0, f.off.y, 0) + alat * f.off.z;
            V3 toS = slot - f.pos; float ls = len(toS);
            F += toS * (0.9f * std::min(1.0f, 2.5f / (ls + 1e-3f))) + (av - f.vel) * 0.5f;
            // reef, diver, manta
            float d = terr(f.pos);
            if (d < 1.1f) F += terrN(f.pos) * ((1.1f - d) * 9.0f);
            V3 dc = f.pos - cam.pos; float lc = len(dc);
            if (lc < 2.1f) F += dc * ((2.1f - lc) * 5.0f / (lc + 1e-3f));
            V3 dm = f.pos - manta.pos; float lm = len(dm);
            if (lm < 4.0f) F += dm * ((4.0f - lm) * 2.0f / (lm + 1e-3f));
            if (f.pos.y > -1.5f) F.y -= (f.pos.y + 1.5f) * 6.0f;
            // restless wandering
            float n1 = sinf(t * (0.7f + f.seed) + f.seed * 40.0f), n2 = sinf(t * (1.1f + 0.5f * f.seed) + f.seed * 71.0f);
            F += V3(n1 * 0.5f, n2 * 0.15f, n2 * 0.5f);
            F.y *= 0.8f;
            f.acc = F;
        }
        // anthias: hover at a slot above the coral, face the current, hop now and then, shrink back when the diver comes close
        for (size_t i = nFus; i < fish.size(); i++) {
            FishS& f = fish[i];
            f.hop -= dtF;
            if (f.hop <= 0) {
                uint32_t k = (uint32_t)(t * 60.0f) + (uint32_t)i * 977u;
                f.off = V3((hashf(k, 1) * 2 - 1) * 0.75f, 0.2f + hashf(k, 2) * 0.85f, (hashf(k, 3) * 2 - 1) * 0.75f);
                f.hop = 0.8f + 3.5f * hashf(k, 4);
            }
            float lc = len(f.pos - cam.pos);
            float shy = 1.0f - 0.7f * (1 - sstep(1.2f, 2.6f, lc));
            V3 target = f.home + V3(f.off.x * shy, f.off.y * shy * shy, f.off.z * shy);
            V3 F = (target - f.pos) * 7.0f - f.vel * 3.2f;
            float n1 = sinf(t * (2.3f + f.seed) + f.seed * 40.0f), n2 = sinf(t * (3.1f + f.seed) + f.seed * 71.0f), n3 = sinf(t * (1.7f + f.seed) + f.seed * 13.0f);
            F += V3(n1, n2 * 0.6f, n3) * 0.5f;
            f.acc = F;
        }
    }
    void stepFish(float dt) {
        for (size_t i = 0; i < fish.size(); i++) {
            FishS& f = fish[i];
            f.vel += f.acc * dt;
            float sp = len(f.vel);
            if (f.species < 0.5f) {
                float lo = 0.55f, hi = 2.6f;
                if (sp < lo) f.vel = f.vel * (lo / (sp + 1e-5f)); else if (sp > hi) f.vel = f.vel * (hi / sp);
                float vy = f.vel.y, vh = sqrtf(f.vel.x * f.vel.x + f.vel.z * f.vel.z);
                if (fabsf(vy) > 0.6f * vh) f.vel.y = 0.6f * vh * (vy > 0 ? 1 : -1);
                sp = len(f.vel);
            }
            f.pos += f.vel * dt;
            V3 want;
            if (f.species < 0.5f) want = norm(f.vel);
            else { V3 cur(0.25f + 0.3f * sinf(f.seed * 20.0f), 0, -1.0f); want = norm(cur * 0.35f + f.vel * 2.2f + V3(0.3f * sinf(t * 0.8f + f.seed * 9.0f), 0, 0)); }
            V3 nf = norm(f.fwd + (want - f.fwd) * std::min(1.0f, (f.species < 0.5f ? 9.0f : 5.0f) * dt));
            V3 dfw = nf - f.fwd;
            V3 side = norm(cross(f.fwd, V3(0, 1, 0)));
            float turnRate = dot(dfw, side) / dt;
            f.turn += (clampf(turnRate * 0.35f, -1.0f, 1.0f) - f.turn) * std::min(1.0f, 6.0f * dt);
            f.fwd = nf;
            f.phase += 2 * PI * (f.species < 0.5f ? (1.6f + 1.9f * sp) : (3.0f + 6.0f * sp)) * dt;
        }
    }
    void spawnBubble(V3 p, V3 v, float r) { BubS b; b.pos = p; b.vel = v; b.r = r; b.age = 0; b.seed = hashf(bubN, 555); b.tb = t; bubN++; bubs.push_back(b); }
    void stepBubbles(float dt) {
        // exhaust: both sides of the regulator, in pulses
        float flow = exhale(t);
        float pulse = 0.55f + 0.45f * sinf(t * 31.0f) * sinf(t * 12.7f + 1.0f);
        emitAcc += 62.0f * flow * pulse * dt;
        V3 camV = (cam.pos - prevCamPos) / std::max(dt, 1e-5f);
        while (emitAcc >= 1.0f) {
            emitAcc -= 1.0f;
            uint32_t k = bubN; float sgn = (k & 1) ? 1.0f : -1.0f;
            float g = hashf(k, 1) + hashf(k, 2) + hashf(k, 3) - 1.5f;
            float r = clampf(0.0034f * expf(1.5f * g), 0.0016f, 0.024f);
            V3 p = cam.pos - cam.u * 0.105f + cam.f * 0.075f + cam.r * (sgn * 0.06f) + V3(hashf(k, 4) - 0.5f, hashf(k, 5) - 0.5f, hashf(k, 6) - 0.5f) * 0.03f;
            V3 v = camV * 0.6f + cam.r * (sgn * (0.25f + 0.3f * hashf(k, 7))) + cam.f * (0.15f * hashf(k, 8)) + V3(0, 0.15f, 0) + V3(hashf(k, 9) - 0.5f, 0, hashf(k, 10) - 0.5f) * 0.2f;
            spawnBubble(p, v, r);
        }
        // the splash of the entry, still rising when the film opens
        if (t > -3.0f && t < 0.6f) {
            static float acc2 = 0; acc2 += 70.0f * (1 - sstep(-1.0f, 0.6f, t)) * dt;
            while (acc2 >= 1.0f) {
                acc2 -= 1.0f; uint32_t k = bubN;
                V3 p = V3(-21.2f, -2.6f, 0.6f) + V3(hashf(k, 11) * 2.6f - 0.4f, hashf(k, 12) * 2.0f - 0.6f, hashf(k, 13) * 2.4f - 1.2f);
                float g = hashf(k, 1) + hashf(k, 2) + hashf(k, 3) - 1.5f;
                spawnBubble(p, V3(0, 0.1f, 0), clampf(0.003f * expf(1.3f * g), 0.0015f, 0.016f));
            }
        }
        for (BubS& b : bubs) {
            b.age += dt;
            float vt = clampf(0.19f + 9.5f * b.r, 0.2f, 0.42f);
            float wf = 3.5f + 3.0f * b.seed, wa = b.r > 0.0025f ? 0.07f : 0.03f;
            V3 want(wa * sinf(b.age * wf * 2 + b.seed * 50.0f), vt, wa * cosf(b.age * wf * 1.7f + b.seed * 21.0f));
            b.vel += (want - b.vel) * std::min(1.0f, dt / 0.14f);
            b.pos += b.vel * dt;
        }
        size_t w = 0;
        for (size_t i = 0; i < bubs.size(); i++) { const BubS& b = bubs[i]; if (b.pos.y < -0.06f && b.age < 60 && len(b.pos - cam.pos) < 32) bubs[w++] = b; }
        bubs.resize(w);
    }
    void step(float dt) {
        t += dt;
        stepManta();
        stepCamera(dt);
        if (dt > 0) {
            if ((stepN & 3) == 0) fishForces();
            stepFish(dt); stepBubbles(dt);
            stepN++;
        } else fishForces();
    }
    void advanceTo(float target) { while (t + SIM_DT * 0.5f < target) step(SIM_DT); }

    // ---- export for the GPU
    void exportFish(std::vector<GFish>& out) const {
        out.clear();
        for (const FishS& f : fish) {
            if (len(f.pos - cam.pos) > 42) continue;
            float sa = surgeAmp(f.pos.y) * (f.species < 0.5f ? 0.5f : 1.0f);
            V3 p = f.pos + V3(surgeX(t) * sa, 0, surgeZ(t) * sa);
            V3 fw = f.fwd; V3 side = norm(cross(fw, V3(0, 1, 0))); V3 up = cross(side, fw);
            GFish g; g.pos = f4(p, f.len); g.fwd = f4(fw, f.phase); g.up = f4(up, f.species); g.par = F4{0, f.turn, f.seed, 0}; g.vel = f4(f.vel + V3(surgeVX(t) * sa, 0, surgeVZ(t) * sa), 0);
            out.push_back(g);
        }
    }
    void exportBubbles(std::vector<GBub>& out) const {
        out.clear();
        for (const BubS& b : bubs) {
            float sa = surgeAmp(b.pos.y);
            V3 p = b.pos + V3((surgeX(t) - surgeX(b.tb)) * sa, 0, (surgeZ(t) - surgeZ(b.tb)) * sa);
            float big = sstep(0.004f, 0.014f, b.r);
            float sq = 1.0f - 0.42f * big + 0.10f * big * sinf(b.age * 19.0f + b.seed * 30.0f);
            GBub g; g.pos = f4(p, b.r * (1 + 0.25f * big) * sstep(0.0f, 0.06f, b.age));
            g.par = F4{sq, 0.25f * big * sinf(b.age * 7.0f + b.seed * 11.0f), 0.25f * big * cosf(b.age * 6.1f + b.seed * 5.0f), b.seed};
            g.vel = f4(b.vel + V3(surgeVX(t) * sa, 0, surgeVZ(t) * sa), 0);
            out.push_back(g);
        }
    }
};
