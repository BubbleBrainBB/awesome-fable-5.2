// world.h — the reef on the CPU side: the same terrain function the shader marches, plus the placement of
// coral / sponge instances on it and the uniform grid the shader uses to find them.
#pragma once
#include "base.h"
using std::floor; using std::sin; using std::cos; using std::exp; using std::pow; using std::log; using std::sqrt; using std::fmax; using std::fmin;
#define SH_INLINE static inline
#define CTXD
#define CTX
#define NOISE4(x, y, z) noise4(x, y, z)
#include "shared.h"

static inline float terr(V3 p) { return terrainSDF(p.x, p.y, p.z); }
static inline V3 terrN(V3 p) {
    const float e = 0.02f;
    return norm(V3(terr(p + V3(e, 0, 0)) - terr(p - V3(e, 0, 0)), terr(p + V3(0, e, 0)) - terr(p - V3(0, e, 0)), terr(p + V3(0, 0, e)) - terr(p - V3(0, 0, e))));
}
static bool marchTerrain(V3 ro, V3 rd, float tmax, V3& hit) {
    float t = 0;
    for (int i = 0; i < 400; i++) { V3 p = ro + rd * t; float d = terr(p); if (d < 0.004f) { hit = p; return true; } t += d; if (t > tmax) break; }
    return false;
}

struct Inst { F4 pos, ax, ay, az, bnd; };
struct Cell { uint32_t off, cnt; float empty, pad; };
enum { T_TABLE = 0, T_BUSH, T_FAN, T_WHIP, T_BARREL, T_DOME };

struct World {
    std::vector<Inst> insts; std::vector<Cell> cells; std::vector<uint32_t> cellIdx;
    V3 gridO = V3(-36, -48, -30); float cs = 1.0f; int nx = 54, ny = 46, nz = 66;

    void add(int type, V3 base, V3 up, V3 fwdHint, float scale, float seed, float hue, float twist) {
        V3 ay = norm(up);
        V3 az = fwdHint - ay * dot(fwdHint, ay); if (len(az) < 1e-3f) az = cross(ay, V3(1, 0, 0)); az = norm(az);
        V3 ax = cross(ay, az);
        float c = cosf(twist), s = sinf(twist);
        V3 ax2 = ax * c + az * s, az2 = az * c - ax * s;
        static const float cy[6] = {0.32f, 0.42f, 0.62f, 0.85f, 0.5f, 0.2f}, rb[6] = {1.22f, 0.80f, 1.05f, 1.15f, 0.78f, 0.98f};
        Inst I; I.pos = f4(base, scale); I.ax = f4(ax2, (float)type); I.ay = f4(ay, seed); I.az = f4(az2, hue);
        I.bnd = f4(base + ay * (cy[type] * scale), rb[type] * scale + 0.06f);
        insts.push_back(I);
    }
    struct RayHero { V3 o, d; int type; float scale, hue; };
    void build(uint32_t seed, const std::vector<RayHero>& rayHeroes) {
        RNG r(seed);
        // ---- hand-placed pieces along the dive route
        auto ground = [&](float x, float z, V3& p, V3& n) { bool ok = marchTerrain(V3(x, -2.0f, z), V3(0, -1, 0), 30, p); if (ok) n = terrN(p); return ok; };
        auto wallAt = [&](float y, float z, V3& p, V3& n) { bool ok = marchTerrain(V3(edgeX(z) + 16, y, z), V3(-1, 0, 0), 40, p); if (ok) n = terrN(p); return ok; };
        struct Hero { int type; float a, b, scale, hue, twist; bool wall; };
        const Hero heroes[] = {
            {T_TABLE, -13.2f, 3.6f, 1.35f, 0.2f, 0.3f, false}, {T_TABLE, -12.0f, -2.6f, 1.0f, 0.6f, 1.3f, false},
            {T_BUSH, -15.5f, -2.3f, 0.75f, 0.2f, 0.0f, false}, {T_DOME, -9.6f, -3.0f, 1.25f, 0.3f, 0.0f, false},
            {T_BARREL, -8.2f, 3.4f, 0.9f, 0.5f, 0.0f, false}, {T_FAN, -5.6f, -2.2f, 1.1f, 0.6f, 0.2f, false},
            {T_TABLE, -4.4f, 3.0f, 1.2f, 0.2f, 2.0f, false}, {T_BUSH, -2.6f, -1.5f, 0.8f, 0.5f, 0.0f, false},
            {T_FAN, -20.5f, 12.6f, 1.9f, 0.2f, 0.75f, true}, {T_FAN, -17.2f, 9.3f, 1.4f, 0.6f, 0.6f, true},
            {T_WHIP, -19.0f, 10.8f, 1.5f, 0.3f, 0.0f, true}, {T_BARREL, -22.8f, 14.8f, 1.1f, 0.3f, 0.0f, true},
            {T_FAN, -13.0f, 4.5f, 1.5f, 0.9f, 0.7f, true}, {T_FAN, -23.6f, 17.5f, 1.6f, 0.5f, 0.8f, true},
        };
        V3 zc(0, 0, 1);
        for (const Hero& h : heroes) {
            V3 p, n; bool ok = h.wall ? wallAt(h.a, h.b, p, n) : ground(h.a, h.b, p, n);
            if (!ok) continue;
            V3 up = h.wall ? norm(n * 0.8f + V3(0, 0.62f, 0)) : norm(n * 0.4f + V3(0, 1, 0));
            if (h.type == T_BARREL || h.type == T_TABLE) up = norm(n * 0.25f + V3(0, 1, 0));
            add(h.type, p - up * (0.06f * h.scale), up, zc, h.scale, r.f(), h.hue, h.twist);
        }
        size_t nHand = insts.size();
        for (const RayHero& h : rayHeroes) {
            V3 p; if (!marchTerrain(h.o, h.d, 14, p)) continue;
            V3 n = terrN(p);
            V3 up = (h.type == T_BARREL) ? norm(n * 0.3f + V3(0, 1, 0)) : norm(n * 0.75f + V3(0, 0.66f, 0));
            add(h.type, p - up * (0.06f * h.scale), up, h.d * -1.0f, h.scale, r.f(), h.hue, 0.0f);
        }
        size_t nHero = insts.size();
        // ---- scattered over the reef flat
        for (float gx = -34; gx < 6; gx += 1.15f) for (float gz = -26; gz < 26; gz += 1.15f) {
            float x = gx + r.range(0, 1.15f), z = gz + r.range(0, 1.15f);
            float u = r.f(), sc = r.f(), sd = r.f(), hue = r.f(), tw = r.range(0, 6.28f);
            if (x - edgeX(z) > 0.3f) continue;
            if (chanMask(x, z) > 0.18f) continue;
            V3 p, n; if (!ground(x, z, p, n) || n.y < 0.45f) continue;
            bool nearHero = false; for (size_t i = 0; i < 8 && i < insts.size(); i++) { V3 d = V3(insts[i].pos.x, insts[i].pos.y, insts[i].pos.z) - p; if (len(d) < 1.6f) nearHero = true; }
            if (nearHero) continue;
            V3 up = norm(n * 0.35f + V3(0, 1, 0));
            int type; float scale;
            if (u < 0.20f) { type = T_TABLE; scale = lerpf(0.45f, 1.25f, sc * sc); }
            else if (u < 0.50f) { type = T_BUSH; scale = lerpf(0.35f, 0.8f, sc); }
            else if (u < 0.63f) { type = T_DOME; scale = lerpf(0.4f, 1.0f, sc * sc); }
            else if (u < 0.69f) { type = T_WHIP; scale = lerpf(0.6f, 1.1f, sc); }
            else if (u < 0.72f) { type = T_BARREL; scale = lerpf(0.5f, 0.9f, sc); }
            else if (u < 0.78f) { type = T_FAN; scale = lerpf(0.5f, 1.0f, sc); }
            else continue;
            add(type, p - up * (0.07f * scale), up, zc, scale, sd, hue, type == T_FAN ? r.range(-0.4f, 0.4f) : tw);
        }
        // ---- on the wall
        for (float gy = -44; gy < -7.5f; gy += 1.25f) for (float gz = -26; gz < 34; gz += 1.25f) {
            float y = gy + r.range(0, 1.25f), z = gz + r.range(0, 1.25f);
            float u = r.f(), sc = r.f(), sd = r.f(), hue = r.f(), tw = r.range(0, 6.28f);
            V3 p, n; if (!wallAt(y, z, p, n)) continue;
            bool nearHero = false; for (size_t i = 8; i < nHero; i++) { V3 d = V3(insts[i].pos.x, insts[i].pos.y, insts[i].pos.z) - p; if (len(d) < (i >= nHand ? 1.7f : 1.3f)) nearHero = true; }
            if (nearHero) continue;
            int type; float scale; V3 up;
            if (u < 0.30f) { type = T_FAN; scale = lerpf(0.6f, 1.7f, sc * sc); up = norm(n * 0.8f + V3(0, 0.62f, 0)); tw = r.range(-0.35f, 0.35f); }
            else if (u < 0.44f) { type = T_WHIP; scale = lerpf(0.7f, 1.6f, sc); up = norm(n + V3(0, 0.5f, 0)); }
            else if (u < 0.56f && n.y > 0.15f) { type = T_TABLE; scale = lerpf(0.5f, 1.2f, sc); up = norm(n * 0.3f + V3(0, 1, 0)); }
            else if (u < 0.62f && n.y > 0.45f) { type = T_BARREL; scale = lerpf(0.6f, 1.2f, sc); up = norm(n * 0.2f + V3(0, 1, 0)); }
            else if (u < 0.72f && n.y > 0.25f) { type = T_BUSH; scale = lerpf(0.35f, 0.7f, sc); up = norm(n * 0.5f + V3(0, 1, 0)); }
            else continue;
            add(type, p - up * (0.07f * scale), up, zc, scale, sd, hue, tw);
        }
        buildGrid();
    }
    void buildGrid() {
        size_t nc = (size_t)nx * ny * nz;
        std::vector<std::vector<uint32_t>> lists(nc);
        for (size_t id = 0; id < insts.size(); id++) {
            const Inst& I = insts[id]; V3 c(I.bnd.x, I.bnd.y, I.bnd.z); float rr = I.bnd.w + 0.21f;
            int x0 = (int)floorf((c.x - rr - gridO.x) / cs), x1 = (int)floorf((c.x + rr - gridO.x) / cs);
            int y0 = (int)floorf((c.y - rr - gridO.y) / cs), y1 = (int)floorf((c.y + rr - gridO.y) / cs);
            int z0 = (int)floorf((c.z - rr - gridO.z) / cs), z1 = (int)floorf((c.z + rr - gridO.z) / cs);
            for (int z = std::max(z0, 0); z <= std::min(z1, nz - 1); z++) for (int y = std::max(y0, 0); y <= std::min(y1, ny - 1); y++) for (int x = std::max(x0, 0); x <= std::min(x1, nx - 1); x++) {
                V3 lo = gridO + V3((float)x, (float)y, (float)z) * cs, hi = lo + V3(cs, cs, cs);
                V3 q(std::max(std::max(lo.x - c.x, c.x - hi.x), 0.f), std::max(std::max(lo.y - c.y, c.y - hi.y), 0.f), std::max(std::max(lo.z - c.z, c.z - hi.z), 0.f));
                if (len(q) <= rr) lists[(size_t)x + nx * ((size_t)y + (size_t)ny * z)].push_back((uint32_t)id);
            }
        }
        cells.resize(nc); cellIdx.clear();
        for (size_t c = 0; c < nc; c++) { cells[c].off = (uint32_t)cellIdx.size(); cells[c].cnt = (uint32_t)lists[c].size(); cells[c].pad = 0; for (uint32_t v : lists[c]) cellIdx.push_back(v); }
        if (cellIdx.empty()) cellIdx.push_back(0);
        const float halfDiag = 0.8661f * cs;
        parallelFor(nz, [&](size_t z) {
            for (int y = 0; y < ny; y++) for (int x = 0; x < nx; x++) {
                V3 cc = gridO + V3(x + 0.5f, y + 0.5f, (float)z + 0.5f) * cs; float best = 30.0f;
                for (const Inst& I : insts) { float d = len(V3(I.bnd.x, I.bnd.y, I.bnd.z) - cc) - I.bnd.w - 0.21f; if (d < best) best = d; }
                cells[(size_t)x + nx * ((size_t)y + (size_t)ny * z)].empty = std::max(best - halfDiag, 0.0f);
            }
        });
    }
};
