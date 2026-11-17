// flora.h — procedural meshes: conifers (spruce, larch), grass, shrubs, ferns, rocks, dead wood, props.
// Everything is grown from a seed; object space is metres, y up, origin at the ground contact.
#pragma once
#include "bvh.h"

static inline vec3 rotY(vec3 p, float a) { float c = cosf(a), s = sinf(a); return v3(c * p.x + s * p.z, p.y, -s * p.x + c * p.z); }
static inline vec3 anyPerp(vec3 d) { return normalize(cross(d, fabsf(d.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))); }

// A ribbon card following a centre line, 'half' wide each side along 'side' vectors. uv.x along (0..1), uv.y across.
static void addCard(Mesh& m, const std::vector<vec3>& c, const std::vector<vec3>& side, float half0, float half1, uint32_t mat) {
    int n = (int)c.size();
    for (int i = 0; i + 1 < n; i++) {
        float u0 = (float)i / (n - 1), u1 = (float)(i + 1) / (n - 1); float h0 = lerp(half0, half1, u0), h1 = lerp(half0, half1, u1);
        vec3 a = c[i] - side[i] * h0, b = c[i] + side[i] * h0, d = c[i + 1] - side[i + 1] * h1, e = c[i + 1] + side[i + 1] * h1;
        vec3 n0 = normalize(cross(c[i + 1] - c[i], side[i])), n1 = normalize(cross(c[i + 1] - c[i], side[i + 1]));
        m.tri(a, b, e, n0, n0, n1, v2(u0, 0), v2(u0, 1), v2(u1, 1), mat); m.tri(a, e, d, n0, n1, n1, v2(u0, 0), v2(u1, 1), v2(u1, 0), mat);
    }
}
// Needle spray card: constant width (the alpha pattern tapers it), uv.x in metres along the shoot, aux = half width in metres
static void addSpray(Mesh& m, const std::vector<vec3>& c, const std::vector<vec3>& side, float half, uint32_t mat, float uOff) {
    int n = (int)c.size(); float acc = uOff;
    for (int i = 0; i + 1 < n; i++) {
        float L = length(c[i + 1] - c[i]); float u0 = acc, u1 = acc + L; acc = u1;
        vec3 a = c[i] - side[i] * half, b = c[i] + side[i] * half, d = c[i + 1] - side[i + 1] * half, e = c[i + 1] + side[i + 1] * half;
        vec3 n0 = normalize(cross(c[i + 1] - c[i], side[i])), n1 = normalize(cross(c[i + 1] - c[i], side[i + 1]));
        m.tri(a, b, e, n0, n0, n1, v2(u0, 0), v2(u0, 1), v2(u1, 1), mat, half); m.tri(a, e, d, n0, n1, n1, v2(u0, 0), v2(u1, 1), v2(u1, 0), mat, half);
    }
}

struct ConiferParams {
    float H = 24, r0 = 0.22f, crownBase = 0.45f, Lmax = 3.6f, whorl = 0.5f, droop = 0.5f, upturn = 0.5f, lateralLen = 0.55f, lateralDroop = 0.8f, cardW = 0.085f;
    int perWhorl = 4; uint32_t needle = M_SPRUCE, bark = M_BARK; float density = 1.0f; float deadBelow = 0.6f; bool sapling = false; bool solid = false;
};

static void addBranch(Mesh& m, Rng& R, vec3 base, float az, float L, float slope0, float curve, const ConiferParams& P, float rBase) {
    // main axis: quadratic in the vertical plane of azimuth az
    vec3 dh = v3(cosf(az), 0, sinf(az)); const int NS = 7; std::vector<vec3> pts(NS); std::vector<float> rad(NS);
    float wig = R.r(-0.12f, 0.12f);
    for (int i = 0; i < NS; i++) { float u = (float)i / (NS - 1); vec3 p = base + dh * (L * u) + v3(0, 1, 0) * (L * (slope0 * u + curve * u * u)) + cross(dh, v3(0, 1, 0)) * (wig * L * u * u); pts[i] = p; rad[i] = std::max(0.004f, rBase * (1 - 0.92f * u)); }
    m.tube(pts, rad, 4, P.bark, false);
    // laterals with needle cards
    float sp = 0.13f / P.density; int nl = std::max(3, (int)(L / sp));
    for (int k = 0; k < nl; k++) {
        float u = 0.10f + 0.90f * (k + R.f() * 0.6f) / nl; if (u > 1) u = 1; float fi = u * (NS - 1); int i = std::min(NS - 2, (int)fi); vec3 p = lerp(pts[i], pts[i + 1], fi - i); vec3 ax = normalize(pts[i + 1] - pts[i]);
        float sideSign = (k & 1) ? 1.f : -1.f; vec3 lat = normalize(cross(ax, v3(0, 1, 0))) * sideSign;
        float ll = P.lateralLen * (0.35f + 0.65f * sinf(PI * std::min(1.f, u * 1.15f))) * R.r(0.7f, 1.15f) * std::min(1.f, L / 2.2f + 0.35f);
        if (u > 0.97f) { lat = ax; ll *= 0.8f; }     // terminal shoot
        vec3 d0 = normalize(lat * R.r(0.55f, 0.9f) + ax * R.r(0.45f, 0.8f) - v3(0, 1, 0) * (P.lateralDroop * R.r(0.15f, 0.55f)));
        const int CS = 3; std::vector<vec3> c(CS), s1(CS), s2(CS);
        for (int j = 0; j < CS; j++) { float t = (float)j / (CS - 1); c[j] = p + d0 * (ll * t) - v3(0, 1, 0) * (P.lateralDroop * ll * 0.55f * t * t); }
        for (int j = 0; j < CS; j++) { vec3 t = normalize(c[std::min(j + 1, CS - 1)] - c[std::max(j - 1, 0)]); vec3 flat = normalize(cross(t, v3(0, 1, 0)) + v3(0, 0.25f * R.r(-1, 1), 0)); s1[j] = flat; s2[j] = normalize(cross(t, flat)); }
        float w = P.cardW * R.r(0.85f, 1.2f) * (0.6f + 0.4f * std::min(1.f, ll / 0.4f));
        float uo = R.r(0, 3);
        if (P.solid) { addCard(m, c, s1, w * 0.50f, w * 0.12f, P.needle & 0xffu); addCard(m, c, s2, w * 0.36f, w * 0.10f, P.needle & 0xffu); }
        else { addSpray(m, c, s1, w, P.needle, uo); addSpray(m, c, s2, w * 0.7f, P.needle, uo + 1.7f); }
    }
}

static Mesh makeConifer(uint32_t seed, const ConiferParams& P) {
    Rng R(seed); Mesh m; float H = P.H;
    // trunk
    int NT = std::max(8, (int)(H / 0.8f)); std::vector<vec3> tp(NT + 1); std::vector<float> tr(NT + 1);
    float leanA = R.r(0, 2 * PI), lean = R.r(0.0f, 0.03f), bowA = R.r(0, 2 * PI), bow = R.r(0, 0.012f) * H;
    auto axis = [&](float h) { float u = h / H; return v3(cosf(leanA) * lean * h + cosf(bowA) * bow * sinf(PI * u), h, sinf(leanA) * lean * h + sinf(bowA) * bow * sinf(PI * u)); };
    auto radius = [&](float h) { float u = h / H; return P.r0 * powf(std::max(0.f, 1 - u), 0.85f) + 0.30f * P.r0 * expf(-h / 0.45f) + 0.012f * (1 - u); };
    for (int i = 0; i <= NT; i++) { float u = (float)i / NT; float h = -0.5f + (H + 0.5f) * u * (0.35f + 0.65f * u); tp[i] = axis(std::max(h, -0.5f)); tp[i].y = h; tr[i] = std::max(0.006f, radius(std::max(0.f, h))); }
    m.tube(tp, tr, P.sapling ? 6 : 12, P.bark, true);
    // root flares
    if (!P.sapling) for (int k = 0; k < 5; k++) { float a = R.r(0, 2 * PI); float len = P.r0 * R.r(2.5f, 5.0f); std::vector<vec3> rp; std::vector<float> rr;
        for (int j = 0; j < 5; j++) { float t = j / 4.f; rp.push_back(v3(cosf(a + 0.3f * t) * (P.r0 * 0.6f + len * t), 0.45f * P.r0 * (1 - t) * (1 - t) * 3 - 0.06f - 0.10f * t, sinf(a + 0.3f * t) * (P.r0 * 0.6f + len * t))); rr.push_back(P.r0 * (0.55f - 0.42f * t)); }
        m.tube(rp, rr, 6, P.bark, true); }
    // whorls of branches
    float hb = P.crownBase * H; float h = P.sapling ? 0.15f * H : hb * P.deadBelow;
    while (h < H - 0.15f) {
        float x = (h - hb) / (H - hb);    // <0 below live crown
        int nb = P.perWhorl + (int)(R.f() * 2) - (x > 0.8f ? 1 : 0); float a0 = R.r(0, 2 * PI);
        for (int b = 0; b < nb; b++) {
            float az = a0 + 2 * PI * b / nb + R.r(-0.35f, 0.35f); vec3 base = axis(h + R.r(-0.08f, 0.08f)); base.y = h + R.r(-0.08f, 0.08f);
            if (x < 0) {     // dead, bare, broken branch stubs under the crown
                if (R.f() < 0.55f) continue; float L = R.r(0.4f, 1.8f); std::vector<vec3> pts; std::vector<float> rr; vec3 dh = v3(cosf(az), 0, sinf(az));
                for (int i = 0; i < 4; i++) { float u = i / 3.f; pts.push_back(base + dh * (L * u) + v3(0, 1, 0) * (L * (-0.25f * u - 0.25f * u * u)) + cross(dh, v3(0, 1, 0)) * (R.r(-0.05f, 0.05f) * L)); rr.push_back(0.022f * (1 - 0.8f * u)); }
                m.tube(pts, rr, 3, M_TWIG, false); continue;
            }
            float prof = powf(std::max(0.f, 1 - x), 0.85f) * smooth(-0.02f, 0.10f, x) + 0.04f;
            float L = P.Lmax * prof * R.r(0.75f, 1.15f); if (L < 0.12f) continue;
            float slope0 = lerp(-0.45f * P.droop - 0.1f, 0.45f, smooth(0.2f, 1.0f, x)) + R.r(-0.08f, 0.08f); float curve = P.upturn * lerp(0.42f, 0.05f, x) + R.r(-0.04f, 0.04f);
            addBranch(m, R, base, az, L, slope0, curve, P, std::max(0.006f, radius(h) * 0.28f));
        }
        h += P.whorl * R.r(0.8f, 1.25f) * lerp(1.0f, 0.55f, sat(x));
    }
    // leader
    { vec3 top = axis(H); top.y = H; std::vector<vec3> c = {top - v3(0, 0.5f, 0), top - v3(0, 0.2f, 0), top + v3(0, 0.15f, 0)}; std::vector<vec3> s1(3, v3(1, 0, 0)), s2(3, v3(0, 0, 1)); addSpray(m, c, s1, P.cardW * 0.7f, P.needle, 0); addSpray(m, c, s2, P.cardW * 0.7f, P.needle, 1.3f); }
    return m;
}

// ---------- rocks: random convex polytope + noise, creased normals ----------
static Mesh makeRock(uint32_t seed, float angular = 1.0f, int subdiv = 4) {
    Rng R(seed); std::vector<vec3> v; std::vector<uint32_t> idx;
    const float t = (1 + sqrtf(5.f)) / 2; vec3 iv[12] = {v3(-1, t, 0), v3(1, t, 0), v3(-1, -t, 0), v3(1, -t, 0), v3(0, -1, t), v3(0, 1, t), v3(0, -1, -t), v3(0, 1, -t), v3(t, 0, -1), v3(t, 0, 1), v3(-t, 0, -1), v3(-t, 0, 1)};
    int it[60] = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};
    for (auto& p : iv) v.push_back(normalize(p)); for (int i : it) idx.push_back(i);
    for (int s = 0; s < subdiv; s++) { std::map<uint64_t, uint32_t> mid; std::vector<uint32_t> ni;
        auto mp = [&](uint32_t a, uint32_t b) { uint64_t k = a < b ? ((uint64_t)a << 32) | b : ((uint64_t)b << 32) | a; auto f = mid.find(k); if (f != mid.end()) return f->second; v.push_back(normalize(v[a] + v[b])); return mid[k] = (uint32_t)v.size() - 1; };
        for (size_t i = 0; i < idx.size(); i += 3) { uint32_t a = idx[i], b = idx[i + 1], c = idx[i + 2], ab = mp(a, b), bc = mp(b, c), ca = mp(c, a); uint32_t q[12] = {a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca}; ni.insert(ni.end(), q, q + 12); }
        idx.swap(ni); }
    int NP = 9 + (int)(R.f() * 8); std::vector<vec3> pn(NP); std::vector<float> ph(NP);
    for (int k = 0; k < NP; k++) { pn[k] = normalize(v3(R.g(), R.g() * 0.8f, R.g())); ph[k] = R.r(0.55f, 1.0f); }
    vec3 sc = v3(R.r(0.7f, 1.3f), R.r(0.45f, 0.9f), R.r(0.7f, 1.3f)); uint32_t ns = seed * 7 + 3;
    for (auto& p : v) { vec3 d = p; float r = 1.25f; for (int k = 0; k < NP; k++) { float c = dot(d, pn[k]); if (c > 1e-3f) r = std::min(r, ph[k] / c); }
        r = lerp(1.0f, r, angular);
        float nz = sh_fbm3(d.x * 2.1f + 5, d.y * 2.1f, d.z * 2.1f, 4, ns) - 0.5f, nz2 = sh_fbm3(d.x * 7 + 1, d.y * 7, d.z * 7, 3, ns + 9) - 0.5f;
        r *= 1 + 0.22f * nz + 0.05f * nz2; p = d * r * sc; }
    // creased normals
    size_t nt = idx.size() / 3; std::vector<vec3> fn(nt); std::vector<std::vector<uint32_t>> adj(v.size());
    for (size_t i = 0; i < nt; i++) { fn[i] = normalize(cross(v[idx[i * 3 + 1]] - v[idx[i * 3]], v[idx[i * 3 + 2]] - v[idx[i * 3]])); for (int k = 0; k < 3; k++) adj[idx[i * 3 + k]].push_back((uint32_t)i); }
    Mesh m; for (size_t i = 0; i < nt; i++) { vec3 nn[3]; for (int k = 0; k < 3; k++) { vec3 a = v3(0, 0, 0); for (uint32_t f : adj[idx[i * 3 + k]]) if (dot(fn[f], fn[i]) > 0.80f) a += fn[f]; nn[k] = normalize(a); }
        m.tri(v[idx[i * 3]], v[idx[i * 3 + 1]], v[idx[i * 3 + 2]], nn[0], nn[1], nn[2], v2(0, 0), v2(1, 0), v2(0, 1), M_ROCK); }
    return m;
}

// ---------- grass: a patch of tussocks, blades as tapered bent ribbons ----------
static void addBlade(Mesh& m, Rng& R, vec3 root, float az, float len, float lean, float width, uint32_t mat) {
    vec3 dh = v3(cosf(az), 0, sinf(az)), side = v3(-sinf(az), 0, cosf(az)); const int NS = 4; vec3 prev = root; float tw = R.r(-0.5f, 0.5f);
    for (int i = 0; i < NS; i++) { float u0 = (float)i / NS, u1 = (float)(i + 1) / NS; float a1 = lean * u1 * (0.6f + 0.9f * u1);
        vec3 nxt = root + (v3(0, 1, 0) * cosf(a1) + dh * sinf(a1)) * (len * u1) * (1.0f - 0.12f * u1 * lean);
        float w0 = width * (1 - u0 * u0 * 0.85f), w1 = width * (1 - u1 * u1 * 0.85f); if (i == NS - 1) w1 = width * 0.05f;
        vec3 s0 = normalize(side + dh * (tw * u0)), s1 = normalize(side + dh * (tw * u1));
        vec3 a = prev - s0 * w0, b = prev + s0 * w0, c = nxt + s1 * w1, d = nxt - s1 * w1; vec3 n = normalize(cross(b - a, d - a)); if (n.y < 0) n = -n; n = normalize(n + v3(0, 0.6f, 0));
        m.tri(a, b, c, n, n, n, v2(0, u0), v2(1, u0), v2(1, u1), mat); m.tri(a, c, d, n, n, n, v2(0, u0), v2(1, u1), v2(0, u1), mat); prev = nxt; }
}
// size: patch edge (m); tufts per patch; blade length range
static Mesh makeGrassPatch(uint32_t seed, float size, int tufts, int blades, float len0, float len1, float lean) {
    Rng R(seed); Mesh m;
    for (int t = 0; t < tufts; t++) { vec3 c = v3(R.r(-0.5f, 0.5f) * size, 0, R.r(-0.5f, 0.5f) * size); float tl = R.r(len0, len1), tr = R.r(0.02f, 0.07f); float pref = R.r(0, 2 * PI), prefAmt = R.r(0, 0.5f);
        int nb = (int)(blades * R.r(0.6f, 1.3f));
        for (int b = 0; b < nb; b++) { float a = R.r(0, 2 * PI), rr = tr * sqrtf(R.f()); vec3 root = c + v3(cosf(a) * rr, -0.01f, sinf(a) * rr);
            float az = lerp(a, pref, prefAmt) + R.r(-0.5f, 0.5f); addBlade(m, R, root, az, tl * R.r(0.55f, 1.15f), lean * R.r(0.3f, 1.4f) + rr / tr * 0.25f, R.r(0.0022f, 0.0045f) * (1 + tl * 2), M_GRASS); } }
    return m;
}

// ---------- dwarf shrub (bilberry / alpenrose scale): wiry stems with small leaves ----------
static Mesh makeShrub(uint32_t seed, float height, float radius, int stems, int leavesPer, float leaf) {
    Rng R(seed); Mesh m;
    for (int s = 0; s < stems; s++) { float a = R.r(0, 2 * PI), out = R.r(0.1f, 1.0f); vec3 root = v3(cosf(a) * radius * 0.25f * out, -0.01f, sinf(a) * radius * 0.25f * out);
        vec3 tip = v3(cosf(a) * radius * out, height * R.r(0.5f, 1.0f) * (1 - 0.35f * out), sinf(a) * radius * out); vec3 mid = lerp(root, tip, 0.5f) + v3(0, height * 0.25f, 0);
        std::vector<vec3> p = {root, mid, tip}; std::vector<float> r = {0.004f, 0.003f, 0.0015f}; m.tube(p, r, 3, M_TWIG, false);
        for (int l = 0; l < leavesPer; l++) { float t = R.r(0.25f, 1.0f); vec3 q = t < 0.5f ? lerp(root, mid, t * 2) : lerp(mid, tip, t * 2 - 1); q += v3(R.r(-1, 1), R.r(-0.5f, 1), R.r(-1, 1)) * (leaf * 1.2f);
            vec3 d = normalize(v3(R.g(), R.g() * 0.4f + 0.3f, R.g())), sd = anyPerp(d); vec3 n = normalize(cross(d, sd)); if (n.y < 0) n = -n; float ls = leaf * R.r(0.7f, 1.3f);
            m.quad(q - d * ls - sd * ls * 0.6f, q + d * ls - sd * ls * 0.6f, q + d * ls + sd * ls * 0.6f, q - d * ls + sd * ls * 0.6f, M_LEAF, n); } }
    return m;
}

// ---------- fern: rosette of arching fronds ----------
static Mesh makeFern(uint32_t seed, float len, int fronds) {
    Rng R(seed); Mesh m;
    for (int f = 0; f < fronds; f++) { float az = 2 * PI * (f + R.r(-0.3f, 0.3f)) / fronds; float L = len * R.r(0.65f, 1.1f), arch = R.r(0.5f, 1.2f); vec3 dh = v3(cosf(az), 0, sinf(az)), sd = v3(-sinf(az), 0, cosf(az));
        const int NS = 6; std::vector<vec3> c(NS), s(NS); for (int i = 0; i < NS; i++) { float u = (float)i / (NS - 1); float ang = 1.15f - arch * 1.5f * u; c[i] = v3(0, 0.02f, 0) + dh * (L * u * (0.55f + 0.3f * u)) + v3(0, 1, 0) * (L * (sinf(1.15f) * u - 0.5f * arch * u * u)); (void)ang;
            s[i] = normalize(sd + v3(0, R.r(-0.15f, 0.15f), 0)); }
        addCard(m, c, s, L * 0.17f, L * 0.03f, M_FERN); }
    return m;
}

// ---------- dead wood ----------
static Mesh makeLog(uint32_t seed, float len, float r) {
    Rng R(seed); Mesh m; const int NS = 9; std::vector<vec3> p(NS); std::vector<float> rr(NS);
    for (int i = 0; i < NS; i++) { float u = (float)i / (NS - 1); p[i] = v3((u - 0.5f) * len, r * 0.75f + R.r(-0.02f, 0.02f), sinf(u * 2.5f + seed) * 0.05f * len * 0.1f); rr[i] = r * (1 - 0.35f * u) * R.r(0.95f, 1.05f); }
    m.tube(p, rr, 10, M_BARK, true); { std::vector<vec3> q = {p[0] + v3(0.001f, 0, 0), p[0]}; std::vector<float> qr = {rr[0], rr[0]}; m.tube(q, qr, 10, M_WOOD, true); }
    for (int k = 0; k < 5; k++) { float u = R.r(0.15f, 0.9f); int i = (int)(u * (NS - 1)); float a = R.r(0, PI); vec3 d = normalize(v3(R.r(-0.3f, 0.3f), sinf(a) + 0.2f, cosf(a))); float L = R.r(0.2f, 0.8f); std::vector<vec3> q = {p[i], p[i] + d * L}; std::vector<float> qr = {0.025f, 0.008f}; m.tube(q, qr, 4, M_TWIG, true); }
    return m;
}
static void addBox(Mesh& m, vec3 c, vec3 hx, vec3 hy, vec3 hz, uint32_t mat) {
    vec3 p[8]; for (int k = 0; k < 8; k++) p[k] = c + hx * ((k & 1) ? 1.f : -1.f) + hy * ((k & 2) ? 1.f : -1.f) + hz * ((k & 4) ? 1.f : -1.f);
    int f[6][4] = {{0, 2, 3, 1}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 4, 6, 2}, {1, 3, 7, 5}};
    for (auto& q : f) { vec3 a = p[q[0]], b = p[q[1]], cc = p[q[2]], d = p[q[3]]; vec3 n = normalize(cross(b - a, d - a)); float lu = length(b - a), lv = length(d - a);
        m.tri(a, b, cc, n, n, n, v2(0, 0), v2(0, lu), v2(lv, lu), mat); m.tri(a, cc, d, n, n, n, v2(0, 0), v2(lv, lu), v2(lv, 0), mat); }
}

// ---------- footbridge: two log stringers, split-log deck, one handrail. Object space: x along the trail from
// abutment A (x=0,y=0) to B (x=L, y=rise), z across. Deck top is at y = 0 .. rise.
static Mesh makeBridge(uint32_t seed, float L, float rise) {
    Rng R(seed); Mesh m; vec3 ax = normalize(v3(L, rise, 0)); float len = sqrtf(L * L + rise * rise);
    for (int k = -1; k <= 1; k += 2) { std::vector<vec3> p; std::vector<float> r; for (int i = 0; i <= 6; i++) { float u = i / 6.f; p.push_back(ax * (len * (u * 1.16f - 0.08f)) + v3(0, -0.17f - 0.03f * sinf(PI * u), k * 0.30f)); r.push_back(0.115f * (1 + 0.06f * R.r(-1, 1))); } m.tube(p, r, 10, M_BARK, true); }
    float x = -0.1f; int i = 0;
    while (x < len + 0.05f) { float w = R.r(0.13f, 0.19f); vec3 c = ax * (x + w * 0.5f) + v3(0, -0.022f + R.r(-0.006f, 0.006f), R.r(-0.02f, 0.02f)); float sk = R.r(-0.03f, 0.03f);
        addBox(m, c, ax * (w * 0.5f - 0.006f) + v3(0, 0, sk), v3(0, 0.022f, 0), v3(-sk * 0.3f, 0, 0.50f + R.r(-0.03f, 0.04f)), M_WOOD); x += w; i++; }
    // handrail on the +z side
    for (int k = 0; k < 3; k++) { float u = 0.06f + 0.44f * k; vec3 b = ax * (len * u) + v3(0, -0.25f, 0.56f); std::vector<vec3> p = {b, b + v3(R.r(-0.02f, 0.02f), 1.22f, 0.02f)}; std::vector<float> r = {0.055f, 0.045f}; m.tube(p, r, 8, M_WOOD, true); }
    { std::vector<vec3> p; std::vector<float> r; for (int i2 = 0; i2 <= 5; i2++) { float u = i2 / 5.f; p.push_back(ax * (len * (u * 1.08f - 0.04f)) + v3(0, 0.93f + 0.015f * sinf(u * 9), 0.60f)); r.push_back(0.038f); } m.tube(p, r, 8, M_WOOD, true); }
    return m;
}
