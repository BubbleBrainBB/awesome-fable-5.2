// scene.h — routes, shots, and what stands where
#pragma once
#include "flora.h"
static float gFarTree = 1e9f;   // alpha-free far-tree LOD exists but gave no speed-up, so it is off

static int routeMain(Terrain& T, const std::string& work, std::map<std::string, std::string>& args) {
    // candidate summits: prominent local maxima above 2700 m. For each, find the cheapest trailhead
    // on a stream and keep the summit whose route has the gentlest worst section (a hike, not a climb).
    std::vector<std::pair<float, vec2>> peaks; const int W = 24;
    for (int z = W; z < T.NF - W; z += 2) for (int x = W; x < T.NF - W; x += 2) { float h = T.hF[(size_t)z * T.NF + x]; if (h < 2450) continue; bool mx = true;
        for (int dz = -W; dz <= W && mx; dz += 2) for (int dx = -W; dx <= W; dx += 2) if (T.hF[(size_t)(z + dz) * T.NF + x + dx] > h) { mx = false; break; }
        if (mx) peaks.push_back({h, v2(x * T.dxF, z * T.dxF)}); }
    std::sort(peaks.begin(), peaks.end(), [](auto& a, auto& b) { return a.first > b.first; }); if (peaks.size() > 44) peaks.resize(44);
    float bestScore = 1e30f; vec2 summit = peaks[0].second, start = summit; std::vector<vec2> bestPath;
    for (auto& pk : peaks) {
        T.dijkstraField(pk.second);
        float bc = 1e30f; vec2 head = pk.second;
        for (int z = 8; z < T.NF - 8; z += 2) for (int x = 8; x < T.NF - 8; x += 2) {
            size_t i = (size_t)z * T.NF + x; if (T.areaF[i] < 2.0e6f || T.areaF[i] > 2.5e7f || T.hF[i] < 1580 || T.hF[i] > 1680) continue;
            vec2 p = v2(x * T.dxF, z * T.dxF); float d = length(p - pk.second); if (d < 3000 || d > 9000) continue;
            float c = T.djDist[T.djIndex(p)]; if (c < bc) { bc = c; head = p; } }
        if (bc > 1e29f) continue;
        vec2 st = head; float bs = -1;
        for (int k = 0; k < 720; k++) { float a = k * 2 * PI / 720, r = 70 + 50 * ((k * 7) % 5) / 4.0f; vec2 p = head + v2(cosf(a), sinf(a)) * r;
            size_t i = (size_t)(p.y / T.dxF) * T.NF + (size_t)(p.x / T.dxF); if (T.areaF[i] > 5e4f) continue;
            auto path = T.pathFrom(p); bool cross = false; for (size_t q = 0; q < std::min<size_t>(path.size(), 14); q++) { size_t j = (size_t)(path[q].y / T.dxF) * T.NF + (size_t)(path[q].x / T.dxF); if (T.areaF[j] > 4e5f) cross = true; }
            if (!cross) continue; float sc = 100 - fabsf(T.hF[i] - T.baseH(head.x, head.y) - 6); if (sc > bs) { bs = sc; st = p; } }
        auto path = T.pathFrom(st); float len = 0, worst = 0, steepLen = 0;
        for (size_t i = 0; i + 2 < path.size(); i++) { float L = length(path[i + 2] - path[i]); float g = fabsf(T.baseH(path[i + 2].x, path[i + 2].y) - T.baseH(path[i].x, path[i].y)) / L; worst = std::max(worst, g); if (g > 0.38f) steepLen += L * 0.5f; len += length(path[i + 1] - path[i]); }
        float score = bc;
        fprintf(stderr, "peak %.0f m at %.0f %.0f: route %.0f m, worst grade %.2f, steep %.0f m, cost %.0f, score %.0f\n", pk.first, pk.second.x, pk.second.y, len, worst, steepLen, bc, score);
        if (score < bestScore) { bestScore = score; summit = pk.second; start = st; bestPath = path; }
    }
    fprintf(stderr, "chosen summit %.0f %.0f, start %.0f %.0f\n", summit.x, summit.y, start.x, start.y);
    double tr0 = now(); auto path = T.routeDirectional(summit, start); fprintf(stderr, "directional route: %zu nodes, %.1fs\n", path.size(), now() - tr0); if (path.size() < 4) path = bestPath; T.buildTrail(path); T.saveTrail((work + "/trail.bin").c_str());
    fprintf(stderr, "trail length %d m\n", T.trail.n());
    printf("#   s    x      z     alt  grade slope  forest larch rock scree snow area\n");
    for (int i = 0; i < T.trail.n(); i += 100) { vec2 p = T.trail.p[i]; vec4 g = T.geoAt(p.x, p.y);
        Eco e = sh_eco(T.trail.bed[i], g.w, g.y * 2 - 1, 0.5f, g.z, sh_vn2(p.x / 260, p.y / 260, 61U), sh_fbm2(p.x / 90, p.y / 90, 3, 62U));
        printf("%5d %6.0f %6.0f %5.0f %5.2f %5.2f  %4.2f %4.2f %4.2f %4.2f %4.2f %.0f\n", i, p.x, p.y, T.trail.bed[i], T.trail.grade[i], acosf(g.w) * 180 / PI, e.forest, e.larch, e.rock, e.scree, e.snow, T.areaF[(size_t)(p.y / T.dxF) * T.NF + (size_t)(p.x / T.dxF)]); }
    return 0;
}

static void buildScene(const Terrain& T, Scene& S, vec2 centre, int shot, const Look& L);

static int stillMain(Terrain& T, Gpu& G, std::map<std::string, std::string>& args) {
    int W = (int)argf(args, "w", 960), H = (int)argf(args, "h", 540), spp = (int)argf(args, "spp", 16);
    Look L; L.sunAz = argf(args, "sunaz", 120); L.sunEl = argf(args, "sunel", 30); L.mie = argf(args, "mie", 1); L.exposure = argf(args, "exp", 1); L.hfov = argf(args, "fov", 72);
    L.treeRadius = argf(args, "treer", 0); G.U.misc.y = argf(args, "debug", 0); L.mist = v4(argf(args, "mist", 0), argf(args, "misty", 1600), argf(args, "misth", 60), argf(args, "mistn", 0.5f)); L.ambOcc = argf(args, "amb", 0.5f);
    vec2 xz; float yaw = argf(args, "yaw", 0);
    if (args.count("s")) { float s = argf(args, "s", 0); xz = T.trail.pos(s); vec2 d = T.trail.dir(s); vec2 n = v2(-d.y, d.x); xz += n * argf(args, "off", 0); yaw += atan2f(d.y, d.x) * 180 / PI; }
    else xz = v2(argf(args, "x", 16192), argf(args, "z", 14220));
    double t0 = now(); T.bakeClip(xz.x, xz.y); G.uploadClip(T); fprintf(stderr, "clip bake %.1fs\n", now() - t0);
    float eye = argf(args, "eye", 1.65f); vec3 pos = v3(xz.x, T.surfaceH(xz.x, xz.y) + eye, xz.y);
    t0 = now(); G.setLook(L, pos.y); G.U.shot = v4(xz.x - T.O.x, xz.y - T.O.z, L.treeRadius, pos.y * 0.001f); pos -= T.O; fprintf(stderr, "sky %.1fs  cam %.1f %.1f %.1f\n", now() - t0, pos.x, pos.y, pos.z);
    Scene S; t0 = now(); buildScene(T, S, xz, (int)argf(args, "shot", -1), L); S.buildTLAS(); G.uploadScene(S);
    fprintf(stderr, "scene: %zu instances, %zu tris, %.1fs\n", S.insts.size(), S.tris.size(), now() - t0);
    G.setSize(W, H); Cam c = camLook(pos, yaw, argf(args, "pitch", 0)); t0 = now();
    if (!G.render(c, c, 0, 0, spp, 4)) return 2;
    fprintf(stderr, "render %.2fs\n", now() - t0);
    PostParams P; P.exposure = L.exposure; std::vector<uint8_t> img; float ml = postProcess((const vec4*)[G.bAccum contents], W, H, P, img);
    std::string out = args.count("out") ? args["out"] : "work/still.png"; writePNG(out.c_str(), img.data(), W, H); fprintf(stderr, "mean log2 lum %.2f -> %s\n", ml, out.c_str());
    return 0;
}

// ------------------------------------------------------------------ the film: seven clips along the trail
static std::vector<Shot> getShots() {
    std::vector<Shot> v; float t = 0;
    auto add = [&](Shot s) { s.t0Film = t; t += s.dur; s.seed = (uint32_t)v.size() * 97 + 13; v.push_back(s); };
    { Shot s; s.name = "forest"; s.look.mistSamples = 1; s.spp = 16; s.look.clampL = 6.0f; s.look.nr = 0.5f; s.s0 = 262; s.dur = 12; s.speed = 1.12f; s.look.sunAz = 154; s.look.sunEl = 27; s.look.mie = 2.0f; s.look.exposure = 0.9f; s.look.mist = v4(14, 1610, 70, 0.6f); s.look.treeRadius = 320; s.look.ambOcc = 0.25f;
      s.gaze = {{0, 0, 0}, {4, 0, 0}, {7.0f, -30, 6}, {9, -22, 4}, {12, 0, 0}}; add(s); }
    { Shot s; s.name = "stream"; s.spp = 16; s.look.clampL = 6.0f; s.look.nr = 0.5f; s.s0 = 1096.5f; s.dur = 11; s.speed = 1.0f; s.look.sunAz = 160; s.look.sunEl = 30; s.look.mie = 1.8f; s.look.exposure = 1.0f; s.look.mist = v4(8, 1700, 60, 0.6f); s.look.treeRadius = 320; s.look.ambOcc = 0.3f;
      s.gaze = {{0, 0, -2}, {3.5f, 0, -5}, {6.2f, -58, -14}, {8.4f, -46, -10}, {11, 0, -2}}; add(s); }
    { Shot s; s.name = "shafts"; s.spp = 20; s.look.clampL = 6.0f; s.look.nr = 0.8f; s.s0 = 1405; s.dur = 11; s.speed = 1.05f; s.look.sunAz = 166; s.look.sunEl = 33; s.look.mie = 1.8f; s.look.exposure = 0.8f; s.look.mist = v4(26, 1765, 80, 0.7f); s.look.treeRadius = 320; s.look.ambOcc = 0.25f;
      s.gaze = {{0, 0, 2}, {3, 0, 4}, {6.0f, -18, 20}, {8.5f, -14, 14}, {11, 0, 2}}; add(s); }
    { Shot s; s.name = "treeline"; s.spp = 16; s.s0 = 3118; s.dur = 11; s.speed = 1.05f; s.look.sunAz = 198; s.look.sunEl = 38; s.look.mie = 1.4f; s.look.exposure = 0.26f; s.look.treeRadius = 900; s.look.windAmt = 0.5f; s.look.ambOcc = 0.5f;
      s.gaze = {{0, 0, 0}, {5, 0, 2}, {8, 25, 6}, {11, 10, 4}}; add(s); }
    { Shot s; s.name = "meadow"; s.spp = 20; s.s0 = 3880; s.dur = 9; s.speed = 1.1f; s.look.sunAz = 228; s.look.sunEl = 42; s.look.mie = 1.2f; s.look.exposure = 0.2f; s.look.treeRadius = 1300; s.look.windAmt = 0.9f;
      s.gaze = {{0, 0, 0}, {2, 0, 0}, {5, 40, 1}, {7, 50, 0}, {9, 25, 0}}; add(s); }
    { Shot s; s.name = "saddle"; s.spp = 20; s.s0 = 4975; s.dur = 11; s.speed = 1.1f; s.look.sunAz = 258; s.look.sunEl = 35; s.look.mie = 1.1f; s.look.exposure = 0.2f; s.look.treeRadius = 1300; s.look.windAmt = 1.0f;
      s.gaze = {{0, 0, 2}, {4, 0, 4}, {8, -30, 6}, {11, -10, 4}}; add(s); }
    { Shot s; s.name = "ridge"; s.spp = 24; s.s0 = 6225; s.dur = 10; s.speed = 0.9f; s.look.sunAz = 294; s.look.sunEl = 21; s.look.mie = 1.0f; s.look.exposure = 0.6f; s.look.treeRadius = 2200; s.look.windAmt = 1.2f;
      s.gaze = {{0, 0, 0}, {3.5f, 0, 0}, {6.5f, -55, -4}, {8.5f, -48, -2}, {10, -10, 0}}; add(s); }
    { Shot s; s.name = "summit"; s.spp = 24; s.s0 = 6426.6f; s.dur = 20; s.speed = 0.95f; s.look.sunAz = 318; s.look.sunEl = 5.5f; s.look.mie = 2.6f; s.look.exposure = 0.55f; s.look.treeRadius = 2600; s.look.windAmt = 1.4f;
      s.speedKeys = {{0, 1}, {6.5f, 1}, {9.0f, 0}, {20, 0}};
      s.gaze = {{0, 0, 4}, {5, 0, 6}, {8, 18, 2}, {13, 105, -2}, {18, 190, -2}, {20, 204, -1}}; add(s); }
    return v;
}

// exposure: scripted key value per shot times a slow auto-exposure derived from a metering pass
static std::vector<float> loadMeter(const std::string& path, int nframes, float fps) {
    std::vector<float> ev(nframes, 0.f); FILE* f = fopen(path.c_str(), "r"); if (!f) return ev;
    std::vector<float> raw(nframes, NAN); int fr; float ml; while (fscanf(f, "%d %f", &fr, &ml) == 2) if (fr >= 0 && fr < nframes) raw[fr] = ml; fclose(f);
    float last = NAN; for (int i = 0; i < nframes; i++) { if (!std::isnan(raw[i])) last = raw[i]; else raw[i] = last; } for (int i = nframes - 1; i >= 0; i--) { if (!std::isnan(raw[i])) last = raw[i]; else raw[i] = last; }
    if (std::isnan(raw[0])) return ev;
    // camera AE: first-order lag (0.9 s), partial compensation
    float mean = 0; for (float v : raw) mean += v; mean /= nframes; float st = raw[0]; float a = 1 - expf(-1 / (0.9f * fps));
    for (int i = 0; i < nframes; i++) { st += (raw[i] - st) * a; ev[i] = -0.75f * (st - mean) - 0.35f * (mean + 3.3f); }
    return ev;
}

static int shotMain(Terrain& T, Gpu& G, int si, std::map<std::string, std::string>& args) {
    auto shots = getShots(); if (si < 0 || si >= (int)shots.size()) { fprintf(stderr, "no such shot\n"); return 1; } Shot sh = shots[si];
    const float fps = 30; int nf = (int)(sh.dur * fps + 0.5f); int f0 = 0, f1 = nf, step = (int)argf(args, "step", 1);
    if (args.count("frames")) { sscanf(args["frames"].c_str(), "%d:%d", &f0, &f1); f1 = std::min(f1, nf); }
    int W = (int)argf(args, "w", 1920), H = (int)argf(args, "h", 1080), spp = (int)argf(args, "spp", sh.spp); bool meter = args.count("meter");
    if (meter) { W = 192; H = 108; spp = 4; }
    std::string work = args.count("work") ? args["work"] : "work"; char buf[512];
    snprintf(buf, sizeof buf, "%s/frames/shot%d", work.c_str(), si); std::string outDir = args.count("out") ? args["out"] : buf; mkdir((work + "/frames").c_str(), 0755); mkdir(outDir.c_str(), 0755);
    if (args.count("exp")) sh.look.exposure = argf(args, "exp", 1);
    if (std::string(sh.name) == "stream") T.buildStream(1100);
    Walker Wk; Wk.init(T, sh); gFarTree = argf(args, "fartree", 1e9f);
    float sMid = 0.5f * (Wk.station(0) + Wk.station(sh.dur)); vec2 centre = Wk.pathXZ(sMid);
    double t0 = now(); T.bakeClip(centre.x, centre.y); Wk.init(T, sh); G.uploadClip(T);
    if (args.count("camdump")) {      // debug: print the camera of every frame (shutter open / close) and stop
        for (int f = 0; f < nf; f++) for (int k = 0; k < 2; k++) { float t = f / fps + k * 0.5f / fps; Cam c = Wk.at(t);
            printf("%d %d %.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f\n", f, k, c.pos.x - T.O.x, c.pos.y, c.pos.z - T.O.z, atan2f(c.F.z, c.F.x) * 180 / PI, asinf(c.F.y) * 180 / PI, asinf(c.R.y) * 180 / PI, Wk.station(t), Wk.speedAt(t)); }
        return 0; }
    Cam c0 = Wk.at(0); G.setLook(sh.look, c0.pos.y); G.U.shot = v4(centre.x - T.O.x, centre.y - T.O.z, sh.look.treeRadius, c0.pos.y * 0.001f); G.U.misc.y = argf(args, "debug", 0); G.U.misc.w = sh.look.clampL;
    Scene S; buildScene(T, S, centre, si, sh.look); S.buildTLAS(); G.uploadScene(S);
    fprintf(stderr, "shot %d '%s': stations %.1f..%.1f, %zu instances, %zu tris, setup %.1fs\n", si, sh.name, Wk.station(0), Wk.station(sh.dur), S.insts.size(), S.tris.size(), now() - t0);
    G.setSize(W, H); snprintf(buf, sizeof buf, "%s/meter_shot%d.txt", work.c_str(), si); std::string meterPath = buf;
    std::vector<float> ev = loadMeter(meterPath, nf, fps); FILE* mf = meter ? fopen(meterPath.c_str(), "w") : nullptr;
    FILE* sf = nullptr; if (!meter && f0 == 0 && f1 == nf) { snprintf(buf, sizeof buf, "%s/steps_shot%d.txt", work.c_str(), si); sf = fopen(buf, "w"); }
    if (sf) { float lastPh = Wk.phase(0); for (int i = 1; i < (int)(sh.dur * 240); i++) { float t = i / 240.f, ph = Wk.phase(t); if (floorf(ph) != floorf(lastPh)) fprintf(sf, "%.4f %.3f %.3f %d\n", t, Wk.speedAt(t), T.trail.gradeAt(Wk.station(t)), T.deckY(Wk.station(t)) > -1e8f ? 1 : 0); lastPh = ph; } fclose(sf); }
    if (args.count("stepsonly")) { snprintf(buf, sizeof buf, "%s/steps_shot%d.txt", work.c_str(), si); FILE* q = fopen(buf, "w"); float lastPh = Wk.phase(0); for (int i = 1; i < (int)(sh.dur * 240); i++) { float t = i / 240.f, ph = Wk.phase(t); if (floorf(ph) != floorf(lastPh)) fprintf(q, "%.4f %.3f %.3f %d\n", t, Wk.speedAt(t), T.trail.gradeAt(Wk.station(t)), T.deckY(Wk.station(t)) > -1e8f ? 1 : 0); lastPh = ph; } fclose(q); return 0; }
    int done = 0; double tr0 = now();
    std::vector<char> only; if (args.count("only")) { only.assign(nf, 0); std::string q = args["only"]; size_t pos = 0; while (pos < q.size()) { int v = atoi(q.c_str() + pos); if (v >= 0 && v < nf) only[v] = 1; pos = q.find(',', pos); if (pos == std::string::npos) break; pos++; } }
    for (int f = f0; f < f1; f += step) {
        if (!only.empty() && !only[f]) continue;
        snprintf(buf, sizeof buf, "%s/f_%04d.png", outDir.c_str(), f); std::string path = buf;
        if (!meter && !args.count("force")) { struct stat stt; if (stat(path.c_str(), &stt) == 0 && stt.st_size > 1000) continue; }
        float t = f / fps; Cam a = Wk.at(t), b = Wk.at(t + 0.5f / fps); if (args.count("freeze")) { a = Wk.at(2.0f); b = a; }   // debug: hold the camera, let time run (to look at wind)
        a.pos -= T.O; b.pos -= T.O;
        uint32_t gf = (uint32_t)((sh.t0Film + t) * fps + 0.5f); G.U.cloud.z = sh.t0Film + t;
        bool ok = false; for (int attempt = 0; attempt < 3 && !ok; attempt++) ok = G.render(a, b, sh.t0Film + t, gf, spp, 4);
        if (!ok) { fprintf(stderr, "frame %d failed\n", f); return 3; }
        PostParams P; P.exposure = sh.look.exposure * powf(2.f, ev[f]); P.seed = gf; P.localTM = sh.look.localTM; P.nr = sh.look.nr;
        { vec3 sd = v3(G.U.sunDir.x, G.U.sunDir.y, G.U.sunDir.z); float z = dot(sd, a.F); if (z > 0.05f) { float th = G.U.lens.x; P.sunUV = v2(dot(sd, a.R) / z / th, dot(sd, a.U) / z / (th * H / W)); P.sunVis = smooth(0.05f, 0.3f, z); } }
        float filmT = sh.t0Film + t, filmEnd = shots.back().t0Film + shots.back().dur; P.fade = smooth(0, 0.8f, filmT) * smooth(0, 1.6f, filmEnd - filmT);
        std::vector<uint8_t> img; float ml = postProcess((const vec4*)[G.bAccum contents], W, H, P, img);
        if (meter) fprintf(mf, "%d %.4f\n", f, ml); else writePNG(path.c_str(), img.data(), W, H);
        done++; if (done % 10 == 0 || meter == false) fprintf(stderr, "shot %d frame %d/%d  %.2fs/frame\n", si, f, nf, (now() - tr0) / done);
    }
    if (mf) fclose(mf);
    return 0;
}


// ------------------------------------------------------------------ asset library
struct Assets { std::vector<int> spruceFar, larchFar, spruce, larch, sapling, rock, grassShort, grassLong, grassForest, shrub, fern, log; std::vector<float> spruceH, larchH; };
static Assets buildAssets(Scene& S) {
    Assets A; double t0 = now();
    std::vector<Mesh> meshes; std::vector<std::vector<int>*> dest; std::vector<std::function<Mesh()>> jobs;
    auto add = [&](std::vector<int>& d, std::function<Mesh()> f) { dest.push_back(&d); jobs.push_back(f); };
    float sh[] = {13, 17, 21, 25, 29, 33};
    for (int i = 0; i < 6; i++) { ConiferParams P; P.H = sh[i]; P.r0 = 0.011f * P.H + 0.03f; P.crownBase = 0.30f + 0.04f * i; P.Lmax = 1.9f + 0.075f * P.H; P.whorl = 0.42f + 0.012f * P.H; P.lateralLen = 0.62f; P.cardW = 0.10f;
        A.spruceH.push_back(P.H); add(A.spruce, [=] { return makeConifer(100 + i, P); }); ConiferParams Q = P; Q.solid = true; Q.density = 0.8f; add(A.spruceFar, [=] { return makeConifer(100 + i, Q); }); }
    float lh[] = {10, 14, 18, 23};
    for (int i = 0; i < 4; i++) { ConiferParams P; P.H = lh[i]; P.r0 = 0.012f * P.H + 0.03f; P.crownBase = 0.18f; P.Lmax = 2.0f + 0.12f * P.H; P.whorl = 0.36f; P.droop = 0.25f; P.upturn = 0.30f; P.lateralLen = 0.60f; P.lateralDroop = 0.9f; P.cardW = 0.11f; P.perWhorl = 4; P.needle = M_LARCH; P.bark = M_BARK_LARCH; P.density = 1.15f; P.deadBelow = 0.85f;
        A.larchH.push_back(P.H); add(A.larch, [=] { return makeConifer(200 + i, P); }); ConiferParams Q = P; Q.solid = true; Q.density = 0.9f; add(A.larchFar, [=] { return makeConifer(200 + i, Q); }); }
    for (int i = 0; i < 3; i++) { ConiferParams P; P.H = 1.2f + 1.3f * i; P.r0 = 0.02f + 0.012f * i; P.crownBase = 0.08f; P.Lmax = 0.55f + 0.35f * i; P.whorl = 0.22f; P.lateralLen = 0.28f; P.cardW = 0.07f; P.sapling = true; P.droop = 0.2f; add(A.sapling, [=] { return makeConifer(300 + i, P); }); }
    for (int i = 0; i < 10; i++) add(A.rock, [=] { return makeRock(400 + i, i < 7 ? 1.0f : 0.5f, 4); });
    for (int i = 0; i < 4; i++) add(A.grassShort, [=] { return makeGrassPatch(500 + i, 0.7f, 16, 22, 0.06f, 0.16f, 0.9f); });
    for (int i = 0; i < 4; i++) add(A.grassLong, [=] { return makeGrassPatch(520 + i, 0.7f, 9, 34, 0.18f, 0.42f, 1.1f); });
    for (int i = 0; i < 3; i++) add(A.grassForest, [=] { return makeGrassPatch(540 + i, 0.8f, 5, 14, 0.10f, 0.28f, 1.0f); });
    for (int i = 0; i < 4; i++) add(A.shrub, [=] { return makeShrub(600 + i, 0.22f + 0.05f * i, 0.35f + 0.08f * i, 16, 14, 0.012f); });
    for (int i = 0; i < 3; i++) add(A.fern, [=] { return makeFern(700 + i, 0.55f + 0.12f * i, 9 + i); });
    for (int i = 0; i < 3; i++) add(A.log, [=] { return makeLog(800 + i, 5.0f + 2.5f * i, 0.16f + 0.04f * i); });
    meshes.resize(jobs.size()); parallel_for(jobs.size(), [&](size_t i) { meshes[i] = jobs[i](); });
    for (size_t i = 0; i < jobs.size(); i++) dest[i]->push_back(S.addMesh(meshes[i]));
    fprintf(stderr, "assets: %zu meshes, %zu tris, %.1fs\n", jobs.size(), S.tris.size(), now() - t0); return A;
}

static vec3 gOrigin = v3(0, 0, 0);
static inline Mat34 placeMat(vec3 pos, float yaw, vec3 up, vec3 scale) {
    pos -= gOrigin; vec3 x0 = v3(cosf(yaw), 0, sinf(yaw)); vec3 z = normalize(cross(x0, up)); vec3 x = cross(up, z); return mat34(x * scale.x, up * scale.y, z * scale.z, pos);
}
static Eco ecoAt(const Terrain& T, float x, float z, float* altOut = nullptr) {
    vec4 g = T.geoAt(x, z); float alt = T.baseH(x, z); if (altOut) *altOut = alt;
    return sh_eco(alt, g.w, g.y * 2 - 1, sh_fbm2(x / 900.f, z / 900.f, 3, 51U), g.z, sh_vn2(x / 260, z / 260, 61U), sh_fbm2(x / 90, z / 90, 3, 62U));
}

static void buildScene(const Terrain& T, Scene& S, vec2 centre, int shot, const Look& L) {
    Assets A = buildAssets(S); double t0 = now(); gOrigin = T.O;
    // ---- trees: one candidate per shared tree cell
    float R = L.treeRadius; int c0x = (int)floorf((centre.x - R) / TREE_CELL), c1x = (int)floorf((centre.x + R) / TREE_CELL), c0z = (int)floorf((centre.y - R) / TREE_CELL), c1z = (int)floorf((centre.y + R) / TREE_CELL);
    size_t ntree = 0;
    if (R > 0) for (int cz = c0z; cz <= c1z; cz++) for (int cx = c0x; cx <= c1x; cx++) {
        TreeCell tc = sh_treeCell(cx, cz); float dx = tc.px - centre.x, dz = tc.pz - centre.y; if (dx * dx + dz * dz > R * R) continue;
        Eco e = ecoAt(T, tc.px, tc.pz); uint32_t h = sh_hc2(cx, cz, 9100U); if (sh_u2f(h) > e.forest) continue;
        TrailHit th; if (T.trailQuery(tc.px, tc.pz, th) && th.d < 1.7f) continue;
        { Terrain::StreamHit sq; if (T.streamQuery(tc.px, tc.pz, sq) && sq.d < 4.5f) continue; }
        bool larch = tc.kind < e.larch; bool farT = dx * dx + dz * dz > gFarTree * gFarTree; const auto& ids = larch ? (farT ? A.larchFar : A.larch) : (farT ? A.spruceFar : A.spruce); const auto& hs = larch ? A.larchH : A.spruceH;
        float want = tc.height * (larch ? 0.72f : 1.0f); int best = 0; for (size_t i = 0; i < hs.size(); i++) if (fabsf(hs[i] - want) < fabsf(hs[best] - want)) best = (int)i;
        float sc = want / hs[best]; float y = T.surfaceH(tc.px, tc.pz) - 0.08f; float v = 0.8f + 0.4f * sh_u2f(sh_hu(h + 5));
        vec3 tint = larch ? v3(v, v * (0.85f + 0.3f * sh_u2f(sh_hu(h + 6))), v) : v3(v, v, v * (0.9f + 0.3f * sh_u2f(sh_hu(h + 6))));
        S.addInstance(ids[best], placeMat(v3(tc.px, y, tc.pz), sh_u2f(sh_hu(h + 7)) * 6.283f, v3(0, 1, 0), v3(sc, sc, sc)), tint, 0, h); ntree++;
    }
    if (T.stream.on) {
        vec2 a = T.trail.pos(T.stream.deckA), b = T.trail.pos(T.stream.deckB); float L = length(b - a), rise = T.stream.deckYB - T.stream.deckYA; vec2 d = (b - a) / L;
        int bi = S.addMesh(makeBridge(77, L, rise)); vec3 X = v3(d.x, 0, d.y), Z = v3(-d.y, 0, d.x);
        S.addInstance(bi, mat34(X, v3(0, 1, 0), Z, v3(a.x, T.stream.deckYA, a.y) - gOrigin), v3(1, 1, 1), 0, 4242);
    }
    // ---- ground cover on a jittered grid around the camera corridor
    auto scatter = [&](float radius, float cell, uint32_t salt, const std::function<void(float, float, float, uint32_t)>& fn) {
        int a0 = (int)floorf((centre.x - radius) / cell), a1 = (int)floorf((centre.x + radius) / cell), b0 = (int)floorf((centre.y - radius) / cell), b1 = (int)floorf((centre.y + radius) / cell);
        for (int b = b0; b <= b1; b++) for (int a = a0; a <= a1; a++) { uint32_t h = sh_hc2(a, b, salt); float x = (a + sh_u2f(h)) * cell, z = (b + sh_u2f(sh_hu(h + 1))) * cell;
            float d = sqrtf((x - centre.x) * (x - centre.x) + (z - centre.y) * (z - centre.y)); if (d > radius) continue; fn(x, z, d, sh_hu(h + 2)); }
    };
    size_t ngrass = 0, nrock = 0, nshrub = 0;
    // grass
    scatter(55, 0.42f, 7001U, [&](float x, float z, float d, uint32_t h) {
        float keep = 1 - smooth(14, 55, d); if (sh_u2f(h) > keep * keep + 0.02f) return;
        { Terrain::StreamHit sq; if (T.streamQuery(x, z, sq) && sq.d < 3.4f) return; }
        Eco e = ecoAt(T, x, z); vec4 m; float y = T.groundH(x, z, true, &m); if (m.x > 0.3f) return; y += 2 * m.y * sh_detail(x - T.O.x, z - T.O.z, 0);
        vec3 n = T.surfaceN(x, z, 0.25f); if (n.y < 0.72f) return; float cover = e.grass * (1 - e.snow);
        float clump = sh_fbm2(x / 3.1f, z / 3.1f, 3, 7002U); 
        if (e.forest > 0.4f) { if (sh_u2f(sh_hu(h + 3)) > 0.5f * clump * 2) return; int id = A.grassForest[h % A.grassForest.size()];
            S.addInstance(id, placeMat(v3(x, y, z), sh_u2f(sh_hu(h + 4)) * 6.283f, normalize(n + v3(0, 1.5f, 0)), v3(1, 1, 1) * (0.8f + 0.5f * sh_u2f(sh_hu(h + 5)))), v3(0.50f, 0.62f, 0.38f) * (0.7f + 0.6f * sh_u2f(sh_hu(h + 6))), 0.10f, h); ngrass++; return; }
        if (sh_u2f(sh_hu(h + 3)) > cover * (0.45f + 0.9f * clump)) return;
        bool lng = sh_fbm2(x / 6.0f + 3, z / 6.0f, 2, 7003U) + 0.25f * (clump - 0.5f) > 0.55f; const auto& ids = lng ? A.grassLong : A.grassShort; float s = 0.85f + 0.5f * sh_u2f(sh_hu(h + 5));
        float gold = sh_fbm2(x / 9.0f, z / 9.0f, 2, 7004U); vec3 tint = lerp(v3(0.75f, 1.05f, 0.7f), v3(1.25f, 1.05f, 0.8f), gold) * (0.8f + 0.4f * sh_u2f(sh_hu(h + 6)));
        S.addInstance(ids[h % ids.size()], placeMat(v3(x, y, z), sh_u2f(sh_hu(h + 4)) * 6.283f, normalize(n + v3(0, 1.0f, 0)), v3(s, s * (0.8f + 0.5f * sh_u2f(sh_hu(h + 7))), s)), tint, lng ? 0.22f : 0.12f, h); ngrass++;
    });
    // dwarf shrubs
    scatter(70, 0.8f, 7101U, [&](float x, float z, float d, uint32_t h) {
        { Terrain::StreamHit sq; if (T.streamQuery(x, z, sq) && sq.d < 3.4f) return; }
        float alt; Eco e = ecoAt(T, x, z, &alt); float p = smooth(0.52f, 0.70f, sh_fbm2(x / 4.3f + 9.0f, z / 4.3f, 3, 66U)) * smooth(1900.0f, 2150.0f, alt) * e.grass * (1 - e.forest * 0.7f);
        bool inForest = e.forest > 0.4f; p = std::max(p, e.forest * 0.30f * smooth(0.45f, 0.6f, sh_fbm2(x / 6.0f, z / 6.0f + 3, 3, 67U))); if (sh_u2f(h) > p * (1 - smooth(35, 70, d))) return;
        vec4 m; float y = T.groundH(x, z, true, &m); if (m.x > 0.15f) return; y += 2 * m.y * sh_detail(x - T.O.x, z - T.O.z, 0); vec3 n = T.surfaceN(x, z, 0.3f); if (n.y < 0.7f) return;
        float s = 0.8f + 0.7f * sh_u2f(sh_hu(h + 1)); float v = 0.7f + 0.6f * sh_u2f(sh_hu(h + 2));
        vec3 tint = inForest ? v3(0.35f * v, 1.9f * v, 0.9f * v) * (sh_u2f(sh_hu(h + 4)) < 0.25f ? v3(2.2f, 0.45f, 0.8f) : v3(1, 1, 1)) : v3(v, v * (0.6f + 0.8f * sh_u2f(sh_hu(h + 4))), v);
        S.addInstance(A.shrub[h % A.shrub.size()], placeMat(v3(x, y, z), sh_u2f(sh_hu(h + 3)) * 6.283f, normalize(n + v3(0, 2, 0)), v3(s, s, s)), tint, 0.05f, h); nshrub++;
    });
    // forest floor: ferns, saplings, dead wood
    scatter(45, 1.3f, 7201U, [&](float x, float z, float d, uint32_t h) {
        { Terrain::StreamHit sq; if (T.streamQuery(x, z, sq) && sq.d < 3.4f) return; }
        Eco e = ecoAt(T, x, z); if (e.forest < 0.3f) return; vec4 m; float y = T.groundH(x, z, true, &m); if (m.x > 0.1f) return; y += 2 * m.y * sh_detail(x - T.O.x, z - T.O.z, 0);
        float r = sh_u2f(h), cl = sh_fbm2(x / 5.0f, z / 5.0f, 3, 7202U);
        if (r < 0.45f * smooth(0.35f, 0.6f, cl)) { float s = 0.7f + 0.6f * sh_u2f(sh_hu(h + 1)); float au = sh_u2f(sh_hu(h + 2));
            S.addInstance(A.fern[h % A.fern.size()], placeMat(v3(x, y, z), au * 6.283f, v3(0, 1, 0), v3(s, s, s)), lerp(v3(0.8f, 1.0f, 0.7f), v3(1.6f, 1.0f, 0.5f), au * au), 0.06f, h); }
        else if (r < 0.34f) { float s = 0.6f + 0.8f * sh_u2f(sh_hu(h + 1)); S.addInstance(A.sapling[h % A.sapling.size()], placeMat(v3(x, y - 0.03f, z), r * 99, v3(0, 1, 0), v3(s, s, s)), v3(1, 1.1f, 0.9f), 0, h); }
        else if (r < 0.352f) { vec3 n = T.surfaceN(x, z, 0.6f); S.addInstance(A.log[h % A.log.size()], placeMat(v3(x, y, z), r * 777, n, v3(1, 1, 1)), v3(0.9f, 0.9f, 0.85f), 0, h); }
    });
    // rocks: power-law sizes; more on scree / near rock, a few everywhere
    scatter(160, 1.1f, 7301U, [&](float x, float z, float d, uint32_t h) {
        float alt; Eco e = ecoAt(T, x, z, &alt); vec3 nrm = T.surfaceN(x, z, 0.5f); float steep = 1 - nrm.y;
        float nearBrook = 0; { Terrain::StreamHit sq; if (T.streamQuery(x, z, sq)) nearBrook = 1 - smooth(1.0f, 5.0f, sq.d); }
        float field = sh_fbm2(x / 14.0f, z / 14.0f, 3, 7302U); float p = 0.012f + 0.75f * nearBrook + 0.9f * e.scree + 0.25f * e.rock + 0.25f * smooth(0.62f, 0.78f, field) * smooth(1900, 2300, alt) + 0.05f * e.forest * smooth(0.55f, 0.7f, field);
        p *= (1 - smooth(60, 160, d) * 0.97f); if (sh_u2f(h) > p) return;
        float u = sh_u2f(sh_hu(h + 1)); float size = 0.10f * powf(1 - u * 0.985f, -0.62f); size = std::min(size, 2.2f); if (d > 60 && size < 0.3f) return;
        { TrailHit tq; if (T.trailQuery(x, z, tq) && tq.d < 0.9f + size * 1.4f && size > 0.09f) return; }
        float y = T.surfaceH(x, z);
        vec3 up = normalize(nrm + v3(sh_u2f(sh_hu(h + 2)) - 0.5f, 1.2f, sh_u2f(sh_hu(h + 3)) - 0.5f)); float v = 0.8f + 0.4f * sh_u2f(sh_hu(h + 4));
        S.addInstance(A.rock[h % A.rock.size()], placeMat(v3(x, y + size * 0.12f, z), u * 50, up, v3(size, size, size)), v3(v, v, v * 0.98f), 0, h); nrock++; (void)steep;
    });
    fprintf(stderr, "placed: %zu trees, %zu grass, %zu shrubs, %zu rocks (%.1fs)\n", ntree, ngrass, nshrub, nrock, now() - t0);
}
