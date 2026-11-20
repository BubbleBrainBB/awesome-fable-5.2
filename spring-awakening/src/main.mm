// main.mm — host: builds scenes, drives the Metal compute path tracer, writes linear float frames.
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <string>
#include <chrono>
#include "scene.h"
#include "geom.h"
#include "shots.h"

struct Uni {
    float camPos[4], camR[4], camU[4], camF[4];
    float lens[4], lens2[4], lens3[4];
    float sunDir[4], sunE[4], skyZen[4], skyHor[4], skyGnd[4], skyP[4];
    float bd0[4], bd1[4], bdP[4];
    float fog[4], fogCol[4], cloud[4], misc[4], misc2[4];
    uint32_t dims[4], counts[4], tiles[4];
};

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct GBuf {
    id<MTLBuffer> buf = nil; size_t cap = 0;
    void ensure(id<MTLDevice> dev, size_t bytes, bool keep, size_t keepBytes) {
        if (bytes <= cap && buf) return;
        size_t nc = std::max<size_t>(bytes * 3 / 2, 4096);
        id<MTLBuffer> nb = [dev newBufferWithLength:nc options:MTLResourceStorageModeShared];
        if (keep && buf && keepBytes) memcpy(nb.contents, buf.contents, std::min(keepBytes, cap));
        buf = nb; cap = nc;
    }
};

int main(int argc, const char** argv) {
    @autoreleasepool {
        if (argc < 8) { fprintf(stderr, "usage: render <shot> <f0> <f1> <W> <H> <spp> <out|-> [step] [srcdir]\n"); return 1; }
        std::string shotName = argv[1];
        int f0 = atoi(argv[2]), f1 = atoi(argv[3]), W = atoi(argv[4]), H = atoi(argv[5]), spp = atoi(argv[6]);
        std::string outPath = argv[7];
        int step = argc > 8 ? atoi(argv[8]) : 1;
        std::string srcDir = argc > 9 ? argv[9] : "src";
        const ShotDef* shot = findShot(shotName.c_str());
        if (!shot) { fprintf(stderr, "unknown shot %s\n", shotName.c_str()); return 1; }
        int nf = (int)std::lround(shot->dur * 24.0);
        if (f1 < 0) f1 = nf - 1;

        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = [dev newCommandQueue];
        // load kernel source (+ patterns include)
        auto readFile = [&](const std::string& p) { NSString* s = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:p.c_str()] encoding:NSUTF8StringEncoding error:nil]; return s; };
        NSString* ksrc = readFile(srcDir + "/kernel.metal"); NSString* psrc = readFile(srcDir + "/patterns.metal");
        if (!ksrc || !psrc) { fprintf(stderr, "cannot read kernel sources in %s\n", srcDir.c_str()); return 1; }
        ksrc = [ksrc stringByReplacingOccurrencesOfString:@"#include \"patterns.metal\"" withString:psrc];
        NSError* err = nil;
        MTLCompileOptions* opt = [MTLCompileOptions new];
        opt.mathMode = MTLMathModeSafe;
        id<MTLLibrary> lib = [dev newLibraryWithSource:ksrc options:opt error:&err];
        if (!lib) { fprintf(stderr, "metal compile error: %s\n", err.localizedDescription.UTF8String); return 1; }
        id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"render"] error:&err];
        if (!pso) { fprintf(stderr, "pipeline error: %s\n", err.localizedDescription.UTF8String); return 1; }

        Scene sc;
        double tI = now();
        shot->init(sc);
        sc.freezeStatic();
        fprintf(stderr, "[%s] init %.2fs: %zu tris %zu segs %zu nodes %zu static insts\n", shot->name, now() - tI, sc.tris.size(), sc.segs.size(), sc.nodes.size(), sc.insts.size());

        GBuf bU, bTlas, bInst, bNodes, bTris, bTriA, bSegs, bMats, bGl, bTile, bAcc, bFog;
        bU.ensure(dev, sizeof(Uni), false, 0);
        bAcc.ensure(dev, (size_t)W * H * 16, false, 0);
        bFog.ensure(dev, (size_t)W * H * 16, false, 0);
        FILE* out = outPath == "-" ? stdout : fopen(outPath.c_str(), "wb");
        if (!out) { fprintf(stderr, "cannot open output\n"); return 1; }
        std::vector<float> rgb((size_t)W * H * 3);
        const int TX = 32, TY = 18, TMAX = 63;

        for (int f = f0; f <= f1; f += step) {
            double tF = now();
            sc.resetDynamic();
            FrameCtx fc; fc.f = f; fc.nf = nf; fc.t = f / 24.0; fc.W = W; fc.H = H;
            shot->frame(sc, fc);
            sc.buildTLAS();
            double tB = now();
            // ---- upload ----
            auto up = [&](GBuf& b, const void* data, size_t bytes, size_t staticBytes) {
                bool re = bytes > b.cap || !b.buf;
                b.ensure(dev, std::max<size_t>(bytes, 64), false, 0);
                size_t from = (re || sc.staticDirty) ? 0 : std::min(staticBytes, bytes);
                if (bytes > from) memcpy((char*)b.buf.contents + from, (const char*)data + from, bytes - from);
            };
            up(bNodes, sc.nodes.data(), sc.nodes.size() * sizeof(Node), sc.sNodes * sizeof(Node));
            up(bTris, sc.tris.data(), sc.tris.size() * sizeof(Tri), sc.sTris * sizeof(Tri));
            up(bTriA, sc.triAs.data(), sc.triAs.size() * sizeof(TriA), sc.sTris * sizeof(TriA));
            up(bSegs, sc.segs.data(), sc.segs.size() * sizeof(Seg), sc.sSegs * sizeof(Seg));
            sc.staticDirty = false;
            up(bTlas, sc.tlas.data(), sc.tlas.size() * sizeof(Node), 0);
            up(bInst, sc.insts.data(), sc.insts.size() * sizeof(Inst), 0);
            up(bMats, sc.mats.data(), sc.mats.size() * sizeof(Mat), 0);
            up(bGl, sc.glints.data(), sc.glints.size() * sizeof(Glint), 0);

            // ---- camera ----
            Camera& c = fc.cam; Env& e = fc.env;
            V3 fw = norm(c.target - c.pos); V3 rt = norm(cross(fw, c.up)); V3 up3 = cross(rt, fw);
            if (c.roll != 0) { float cr = std::cos(c.roll), sr = std::sin(c.roll); V3 r2 = rt * cr + up3 * sr, u2 = up3 * cr - rt * sr; rt = r2; up3 = u2; }
            float fd = c.focusDist > 0 ? c.focusDist : len(c.target - c.pos);
            float tanW = c.sensorW * 0.5f / c.focal, tanH = tanW * H / W;
            float lensR = c.fstop > 0 ? c.focal / (2 * c.fstop) : 0;
            Uni U; memset(&U, 0, sizeof(U));
            set4(U.camPos, c.pos); set4(U.camR, rt); set4(U.camU, up3); set4(U.camF, fw);
            set4(U.lens, tanW, tanH, lensR, fd); set4(U.lens2, c.blades, c.bladeRot, c.roundness, c.catEye); set4(U.lens3, c.rim, 0, 1, e.eps);
            set4(U.sunDir, norm(e.sunDir), e.sunAngR); set4(U.sunE, e.sunE, e.clampL);
            set4(U.skyZen, e.zen); set4(U.skyHor, e.hor); set4(U.skyGnd, e.gnd); set4(U.skyP, e.horExp, e.glow, e.glowExp, e.type);
            set4(U.bd0, e.bd0, e.bdScale); set4(U.bd1, e.bd1, e.bdMix); set4(U.bdP, e.bdP0, e.bdP1, e.bdP2, e.bdP3);
            set4(U.fog, e.fogDen, e.fogFall, e.fogY0, e.fogG); set4(U.fogCol, e.fogAmb, e.fogMax);
            set4(U.cloud, e.cloudStr, e.cloudScale, e.cloudOfs.x, e.cloudOfs.y); set4(U.misc, e.time, e.skyGain, e.bdGap, e.sunDisc); set4(U.misc2, (float)e.fogN, e.fogSun, e.fogClamp, (float)e.maxDiff);
            U.dims[0] = W; U.dims[1] = H; U.counts[0] = (uint32_t)sc.insts.size(); U.counts[1] = (uint32_t)sc.glints.size(); U.counts[2] = (uint32_t)(f * 7919 + 13);
            U.tiles[0] = TX; U.tiles[1] = TY; U.tiles[2] = TMAX;
            // ---- glint tile binning ----
            std::vector<uint32_t> tile((size_t)TX * TY * (TMAX + 1), 0);
            int dropped = 0;
            for (size_t gi = 0; gi < sc.glints.size(); gi++) {
                const Glint& g = sc.glints[gi]; V3 rel = V3(g.pos[0], g.pos[1], g.pos[2]) - c.pos;
                float Z = dot(rel, fw); if (Z <= 1e-6f) continue;
                float sx = dot(rel, rt) / Z, sy = dot(rel, up3) / Z;
                float pt = 2 * tanW / W;
                float cc = std::max(lensR * std::fabs(fd - Z) / (Z * fd), 1.3f * pt) + 3 * pt;
                float x0 = ((sx - cc) / tanW * 0.5f + 0.5f) * TX, x1 = ((sx + cc) / tanW * 0.5f + 0.5f) * TX;
                float y0 = (0.5f - (sy + cc) / tanH * 0.5f) * TY, y1 = (0.5f - (sy - cc) / tanH * 0.5f) * TY;
                int ix0 = std::max(0, (int)std::floor(x0)), ix1 = std::min(TX - 1, (int)std::floor(x1)), iy0 = std::max(0, (int)std::floor(y0)), iy1 = std::min(TY - 1, (int)std::floor(y1));
                for (int ty = iy0; ty <= iy1; ty++) for (int tx = ix0; tx <= ix1; tx++) {
                    uint32_t* t = &tile[(size_t)(ty * TX + tx) * (TMAX + 1)];
                    if (t[0] < (uint32_t)TMAX) { t[1 + t[0]] = (uint32_t)gi; t[0]++; } else dropped++;
                }
            }
            up(bTile, tile.data(), tile.size() * 4, 0);

            // ---- render passes ----
            const int perPass = spp <= 8 ? spp : 8;
            bool ok = false; int tries = 0;
            while (!ok && tries < 3) {
                tries++;
                bool failed = false;
                for (int s = 0; s < spp; s += perPass) {
                    U.dims[2] = s; U.dims[3] = std::min(perPass, spp - s);
                    // uniforms change per pass: use a fresh small buffer each time
                    id<MTLBuffer> ub = [dev newBufferWithBytes:&U length:sizeof(U) options:MTLResourceStorageModeShared];
                    id<MTLCommandBuffer> cb = [queue commandBuffer];
                    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
                    [enc setComputePipelineState:pso];
                    [enc setBuffer:ub offset:0 atIndex:0];
                    [enc setBuffer:bTlas.buf offset:0 atIndex:1]; [enc setBuffer:bInst.buf offset:0 atIndex:2]; [enc setBuffer:bNodes.buf offset:0 atIndex:3];
                    [enc setBuffer:bTris.buf offset:0 atIndex:4]; [enc setBuffer:bTriA.buf offset:0 atIndex:5]; [enc setBuffer:bSegs.buf offset:0 atIndex:6];
                    [enc setBuffer:bMats.buf offset:0 atIndex:7]; [enc setBuffer:bGl.buf offset:0 atIndex:8]; [enc setBuffer:bTile.buf offset:0 atIndex:9];
                    [enc setBuffer:bAcc.buf offset:0 atIndex:10]; [enc setBuffer:bFog.buf offset:0 atIndex:11];
                    MTLSize tg = MTLSizeMake(16, 16, 1);
                    [enc dispatchThreads:MTLSizeMake(W, H, 1) threadsPerThreadgroup:tg];
                    [enc endEncoding];
                    [cb commit]; [cb waitUntilCompleted];
                    if (cb.error) { fprintf(stderr, "GPU error frame %d: %s\n", f, cb.error.localizedDescription.UTF8String); failed = true; break; }
                }
                if (failed) continue;
                const float* acc = (const float*)bAcc.buf.contents;
                size_t bad = 0;
                for (size_t i = 0; i < (size_t)W * H; i++) {
                    float n = acc[i * 4 + 3];
                    if (n != (float)spp) bad++;
                    float inv = 1.0f / spp;
                    rgb[i * 3] = acc[i * 4] * inv; rgb[i * 3 + 1] = acc[i * 4 + 1] * inv; rgb[i * 3 + 2] = acc[i * 4 + 2] * inv;
                }
                if (bad) { fprintf(stderr, "frame %d: %zu pixels with wrong sample count, retry\n", f, bad); continue; }
                if (e.fogDen > 0) { // fog in-scatter is smooth: blur its (noisy) buffer a little, then add it back
                    const float* fg = (const float*)bFog.buf.contents; float sig = 2.6f * W / 1920.0f; int R = std::max(1, (int)std::ceil(sig * 2.5f));
                    std::vector<float> k(2 * R + 1); float ks = 0; for (int i = -R; i <= R; i++) { k[i + R] = std::exp(-0.5f * i * i / (sig * sig)); ks += k[i + R]; } for (float& v : k) v /= ks;
                    std::vector<float> a((size_t)W * H * 3), b((size_t)W * H * 3); float inv = 1.0f / spp;
                    for (size_t i = 0; i < (size_t)W * H; i++) for (int c2 = 0; c2 < 3; c2++) a[i * 3 + c2] = fg[i * 4 + c2] * inv;
                    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float s0 = 0, s1 = 0, s2 = 0; for (int i = -R; i <= R; i++) { int xx = std::min(W - 1, std::max(0, x + i)); const float* p = &a[((size_t)y * W + xx) * 3]; float w = k[i + R]; s0 += w * p[0]; s1 += w * p[1]; s2 += w * p[2]; } float* q = &b[((size_t)y * W + x) * 3]; q[0] = s0; q[1] = s1; q[2] = s2; }
                    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { float s0 = 0, s1 = 0, s2 = 0; for (int i = -R; i <= R; i++) { int yy = std::min(H - 1, std::max(0, y + i)); const float* p = &b[((size_t)yy * W + x) * 3]; float w = k[i + R]; s0 += w * p[0]; s1 += w * p[1]; s2 += w * p[2]; } float* q = &rgb[((size_t)y * W + x) * 3]; q[0] += s0; q[1] += s1; q[2] += s2; }
                }
                ok = true;
            }
            if (!ok) { fprintf(stderr, "FAILED frame %d\n", f); return 2; }
            fwrite(rgb.data(), sizeof(float), rgb.size(), out); fflush(out);
            fprintf(stderr, "[%s] f %d/%d build %.2fs render %.2fs insts %zu tris %zu segs %zu glints %zu%s%s\n", shot->name, f, nf, tB - tF, now() - tB, sc.insts.size(), sc.tris.size(), sc.segs.size(), sc.glints.size(), dropped ? " (glints dropped)" : "", tries > 1 ? " RETRIED" : "");
        }
        if (out != stdout) fclose(out);
    }
    return 0;
}
