// main.mm — 宿主：建场景、每帧摆姿态与机位、驱动自写的 Metal compute 光线追踪内核、输出线性帧。
// 编译：clang++ -O2 -std=c++17 -fobjc-arc main.mm -framework Metal -framework Foundation -framework CoreGraphics -o render
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <string>
#include <map>
#include <chrono>
#include <sys/stat.h>

static std::map<std::string, float> gParams;
static float P(const char* name, float def) { auto it = gParams.find(name); return it == gParams.end() ? def : it->second; }

#include "flower.h"
#include "bee.h"
#include "anim.h"
#include "bg.h"
#include "shots.h"

static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static uint16_t f2h(float f) {   // float32 -> float16
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t sign = (x >> 16) & 0x8000u; int e = (int)((x >> 23) & 0xff) - 127 + 15; uint32_t m = x & 0x7fffffu;
    if (e <= 0) { if (e < -10) return (uint16_t)sign; m = (m | 0x800000u) >> (1 - e); return (uint16_t)(sign | ((m + 0x1000u) >> 13)); }
    if (e >= 31) return (uint16_t)(sign | 0x7c00u);
    uint32_t r = sign | ((uint32_t)e << 10) | ((m + 0x1000u) >> 13);
    return (uint16_t)r;
}

int main(int argc, char** argv) {
    @autoreleasepool {
        int Wd = 1920, Hd = 1080, spp = 48, sppLow = 192, lowDiv = 4, shot = 0, f0 = 0, f1 = 1, spd = 4, fstep = 1;
        std::string outDir = "work/test", srcDir = "src";
        double fps = 24;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            auto nx = [&]() { return std::string(argv[++i]); };
            if (a == "-w") Wd = atoi(nx().c_str()); else if (a == "-h") Hd = atoi(nx().c_str());
            else if (a == "-spp") spp = atoi(nx().c_str()); else if (a == "-spplow") sppLow = atoi(nx().c_str());
            else if (a == "-lowdiv") lowDiv = atoi(nx().c_str()); else if (a == "-shot") shot = atoi(nx().c_str());
            else if (a == "-f0") f0 = atoi(nx().c_str()); else if (a == "-f1") f1 = atoi(nx().c_str()); else if (a == "-fstep") fstep = atoi(nx().c_str());
            else if (a == "-o") outDir = nx(); else if (a == "-src") srcDir = nx(); else if (a == "-spd") spd = atoi(nx().c_str());
            else if (a == "-p") { std::string kv = nx(); size_t e = kv.find('='); gParams[kv.substr(0, e)] = (float)atof(kv.substr(e + 1).c_str()); }
        }
        mkdir(outDir.c_str(), 0755);
        double tBuild = nowSec();
        World Wo;
        Shots SH;
        SH.build(Wo, shot);
        Wo.nodes.resize(Wo.nodeCount);
        fprintf(stderr, "scene: %zu tris, %zu hair segs, %zu spheres, %zu parts, %zu nodes, build %.2fs\n", Wo.tris.size(), Wo.cyls.size(), Wo.sphs.size(), Wo.parts.size(), Wo.nodes.size(), nowSec() - tBuild);

        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = [dev newCommandQueue];
        NSError* err = nil;
        NSString* sh = [NSString stringWithContentsOfFile:[NSString stringWithFormat:@"%s/shared.h", srcDir.c_str()] encoding:NSUTF8StringEncoding error:&err];
        NSString* kn = [NSString stringWithContentsOfFile:[NSString stringWithFormat:@"%s/kernel.metal", srcDir.c_str()] encoding:NSUTF8StringEncoding error:&err];
        if (!sh || !kn) { fprintf(stderr, "cannot read kernel source\n"); return 1; }
        MTLCompileOptions* opt = [MTLCompileOptions new];
        opt.fastMathEnabled = YES;
        id<MTLLibrary> lib = [dev newLibraryWithSource:[sh stringByAppendingString:kn] options:opt error:&err];
        if (!lib) { fprintf(stderr, "MSL compile error: %s\n", [[err description] UTF8String]); return 1; }
        id<MTLComputePipelineState> pRender = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"render"] error:&err];
        id<MTLComputePipelineState> pResolve = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"resolve"] error:&err];
        if (!pRender || !pResolve) { fprintf(stderr, "pipeline error: %s\n", [[err description] UTF8String]); return 1; }

        auto mkBuf = [&](const void* data, size_t bytes) -> id<MTLBuffer> {
            size_t n = std::max<size_t>(bytes, 64);
            id<MTLBuffer> b = [dev newBufferWithLength:n options:MTLResourceStorageModeShared];
            if (data && bytes) memcpy([b contents], data, bytes);
            return b;
        };
        id<MTLBuffer> bNodes = mkBuf(Wo.nodes.data(), Wo.nodes.size() * sizeof(GNode));
        id<MTLBuffer> bTris = mkBuf(Wo.tris.data(), Wo.tris.size() * sizeof(GTri));
        id<MTLBuffer> bTidx = mkBuf(Wo.tidx.data(), Wo.tidx.size() * sizeof(GTriIdx));
        id<MTLBuffer> bVerts = mkBuf(Wo.verts.data(), Wo.verts.size() * sizeof(GVert));
        id<MTLBuffer> bCyls = mkBuf(Wo.cyls.data(), Wo.cyls.size() * sizeof(GCyl));
        id<MTLBuffer> bCattr = mkBuf(Wo.cattr.data(), Wo.cattr.size() * sizeof(GCylAttr));
        id<MTLBuffer> bSphs = mkBuf(Wo.sphs.data(), Wo.sphs.size() * sizeof(GSph));
        id<MTLBuffer> bSattr = mkBuf(Wo.sattr.data(), Wo.sattr.size() * sizeof(GSphAttr));
        id<MTLBuffer> bMats = mkBuf(Wo.mats.data(), Wo.mats.size() * sizeof(GMat));
        id<MTLBuffer> bVeins = mkBuf(Wo.veins.data(), Wo.veins.size() * sizeof(float));
        int LW = (Wd + lowDiv - 1) / lowDiv, LH = (Hd + lowDiv - 1) / lowDiv;
        id<MTLBuffer> bFg0 = mkBuf(nullptr, (size_t)Wd * Hd * 16), bFg1 = mkBuf(nullptr, (size_t)Wd * Hd * 16), bOut = mkBuf(nullptr, (size_t)Wd * Hd * 16);
        id<MTLBuffer> bMid0 = mkBuf(nullptr, (size_t)LW * LH * 16), bMid1 = mkBuf(nullptr, (size_t)LW * LH * 16), bNear0 = mkBuf(nullptr, (size_t)LW * LH * 16), bNear1 = mkBuf(nullptr, (size_t)LW * LH * 16);
        id<MTLBuffer> bU = mkBuf(nullptr, U_N * 16), bHl = mkBuf(nullptr, 2 * 16 * 512);
        id<MTLBuffer> bTlas = mkBuf(nullptr, 4096 * sizeof(GNode)), bInst = mkBuf(nullptr, 2048 * sizeof(GInst)), bKeys = mkBuf(nullptr, 65536 * sizeof(GKey));
        float* U = (float*)[bU contents];

        NSUInteger tw = pRender.threadExecutionWidth, th = pRender.maxTotalThreadsPerThreadgroup / tw;
        auto runLayer = [&](id<MTLBuffer> a0, id<MTLBuffer> a1, int w, int h, int n, uint32_t mask, uint32_t frameSeed) -> bool {
            bool ok = true;
            for (int s = 0; s < n; s += spd) {
                int k = std::min(spd, n - s);
                // 每次调度用自己的一份参数块（调度是异步执行的）
                id<MTLBuffer> bu = [dev newBufferWithBytes:U length:U_N * 16 options:MTLResourceStorageModeShared];
                float* u = (float*)[bu contents];
                u[U_RES * 4 + 0] = (float)w; u[U_RES * 4 + 1] = (float)h; u[U_RES * 4 + 2] = 1.f / w; u[U_RES * 4 + 3] = 1.f / h;
                u[U_FRAME * 4 + 0] = (float)frameSeed; u[U_FRAME * 4 + 1] = (float)s; u[U_FRAME * 4 + 2] = (float)k; u[U_FRAME * 4 + 3] = (float)mask;
                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
                [enc setComputePipelineState:pRender];
                id<MTLBuffer> bufs[16] = {a0, a1, bu, bTlas, bInst, bKeys, bNodes, bTris, bTidx, bVerts, bCyls, bCattr, bSphs, bSattr, bMats, bVeins};
                for (int i = 0; i < 16; i++) [enc setBuffer:bufs[i] offset:0 atIndex:i];
                [enc dispatchThreads:MTLSizeMake(w, h, 1) threadsPerThreadgroup:MTLSizeMake(tw, th, 1)];
                [enc endEncoding];
                [cb commit];
                [cb waitUntilCompleted];
                if (cb.error) { fprintf(stderr, "GPU error: %s\n", [[cb.error description] UTF8String]); ok = false; }
            }
            return ok;
        };
        // 每个像素都必须拿到全部采样，否则这一帧作废重渲（防 GPU 被别的进程挤占时的静默坏块）
        auto checkCount = [&](id<MTLBuffer> a1, int w, int h, int n) {
            const float* p = (const float*)[a1 contents]; size_t bad = 0;
            for (size_t i = 0; i < (size_t)w * h; i++) if (p[i * 4 + 3] != (float)n) bad++;
            return bad;
        };

        TLASBuild TB;
        std::vector<uint16_t> half((size_t)Wd * Hd * 4);
        for (int fr = f0; fr < f1; fr += fstep) {
            char path[1024]; snprintf(path, sizeof(path), "%s/f_%05d.bin", outDir.c_str(), fr);
            struct stat st; if (stat(path, &st) == 0 && st.st_size == (off_t)half.size() * 2 && P("force", 0) == 0) continue;
            double t0 = nowSec();
            FrameSetup FS;
            SH.frame(Wo, fr, fps, FS);
            for (int attempt = 0; attempt < 4; attempt++) {
                TB.build(Wo, FS.pose);
                if (TB.inst.size() > 2048 || TB.nodes.size() > 4096 || TB.keys.size() > 65536) { fprintf(stderr, "TLAS too big\n"); return 1; }
                memcpy([bTlas contents], TB.nodes.data(), TB.nodes.size() * sizeof(GNode));
                memcpy([bInst contents], TB.inst.data(), TB.inst.size() * sizeof(GInst));
                memcpy([bKeys contents], TB.keys.data(), TB.keys.size() * sizeof(GKey));
                memset(U, 0, U_N * 16);
                FS.fillU(U, Wo, (float)Wd / Hd);
                U[U_LOWRES * 4 + 0] = (float)LW; U[U_LOWRES * 4 + 1] = (float)LH;
                U[U_COUNTS * 4 + 1] = (float)TB.inst.size(); U[U_COUNTS * 4 + 2] = (float)Wo.nVeinFore; U[U_COUNTS * 4 + 3] = (float)Wo.nVeinHind;
                int nh = std::min((int)FS.highlights.size() / 8, 512);
                if (nh) memcpy([bHl contents], FS.highlights.data(), (size_t)nh * 32);
                U[U_FAR0 * 4 + 3] = (float)nh;
                uint32_t seed = (uint32_t)(shot * 100003 + fr * 7 + 20261117 % 65521) & 0xffffff;
                bool ok = true;
                bool hasMid = false, hasNear = false;
                for (size_t p = 0; p < Wo.parts.size(); p++) if (FS.pose.visible[p]) { if (Wo.parts[p].flags & L_MID) hasMid = true; if (Wo.parts[p].flags & L_NEAR) hasNear = true; }
                ok &= runLayer(bFg0, bFg1, Wd, Hd, spp, L_FG, seed);
                if (hasMid) ok &= runLayer(bMid0, bMid1, LW, LH, sppLow, L_MID, seed + 1); else { memset([bMid0 contents], 0, (size_t)LW * LH * 16); float* q = (float*)[bMid1 contents]; for (size_t i = 0; i < (size_t)LW * LH; i++) { q[i * 4] = 1; q[i * 4 + 1] = 1e9f; q[i * 4 + 2] = 0; q[i * 4 + 3] = 1; } }
                if (hasNear) ok &= runLayer(bNear0, bNear1, LW, LH, sppLow, L_NEAR, seed + 2); else { memset([bNear0 contents], 0, (size_t)LW * LH * 16); float* q = (float*)[bNear1 contents]; for (size_t i = 0; i < (size_t)LW * LH; i++) { q[i * 4] = 1; q[i * 4 + 1] = 1e9f; q[i * 4 + 2] = 0; q[i * 4 + 3] = 1; } }
                size_t bad = checkCount(bFg1, Wd, Hd, spp) + (hasMid ? checkCount(bMid1, LW, LH, sppLow) : 0) + (hasNear ? checkCount(bNear1, LW, LH, sppLow) : 0);
                // 合成
                U[U_RES * 4 + 0] = (float)Wd; U[U_RES * 4 + 1] = (float)Hd;
                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
                [enc setComputePipelineState:pResolve];
                id<MTLBuffer> bufs[9] = {bOut, bFg0, bFg1, bMid0, bMid1, bNear0, bNear1, bU, bHl};
                for (int i = 0; i < 9; i++) [enc setBuffer:bufs[i] offset:0 atIndex:i];
                [enc dispatchThreads:MTLSizeMake(Wd, Hd, 1) threadsPerThreadgroup:MTLSizeMake(pResolve.threadExecutionWidth, pResolve.maxTotalThreadsPerThreadgroup / pResolve.threadExecutionWidth, 1)];
                [enc endEncoding]; [cb commit]; [cb waitUntilCompleted];
                if (cb.error) ok = false;
                const float* o = (const float*)[bOut contents];
                size_t nan = 0;
                for (size_t i = 0; i < (size_t)Wd * Hd * 4; i++) { float v = o[i]; if (!(v == v) || v > 1e30f || v < -1e30f) { nan++; v = 0; } half[i] = f2h(v); }
                if (ok && bad == 0 && nan < 64) break;
                fprintf(stderr, "frame %d attempt %d: gpuOk=%d, pixels with wrong sample count=%zu, nan=%zu -> retry\n", fr, attempt, (int)ok, bad, nan);
                if (attempt == 3) { fprintf(stderr, "frame %d FAILED\n", fr); return 2; }
            }
            char tmp[1100]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
            FILE* fp = fopen(tmp, "wb"); fwrite(half.data(), 2, half.size(), fp); fclose(fp); rename(tmp, path);
            fprintf(stderr, "shot %d frame %d  %.2fs  inst=%zu\n", shot, fr, nowSec() - t0, TB.inst.size());
        }
    }
    return 0;
}
