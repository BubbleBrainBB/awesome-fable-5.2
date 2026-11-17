// engine.mm — host program of the hiking renderer (headless Metal compute).
//   engine route                         find the trail, write work/trail.bin
//   engine still  [options]              one frame from an explicit camera
//   engine shot N [options]              frames of shot N (see shots.h)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include "common.h"
#include "atmos.h"
#include "terrain.h"
#include "bvh.h"
#include "post.h"
#include <sys/stat.h>
#include <chrono>
#include <map>

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static bool writePNG(const char* path, const uint8_t* rgb, int W, int H) {
    CFDataRef data = CFDataCreate(NULL, rgb, (CFIndex)W * H * 3); CGDataProviderRef prov = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGImageRef img = CGImageCreate(W, H, 8, 24, W * 3, cs, kCGImageAlphaNone | kCGBitmapByteOrderDefault, prov, NULL, false, kCGRenderingIntentDefault);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)path, strlen(path), false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL); bool ok = false;
    if (dst) { CGImageDestinationAddImage(dst, img, NULL); ok = CGImageDestinationFinalize(dst); CFRelease(dst); }
    CFRelease(url); CGImageRelease(img); CGColorSpaceRelease(cs); CGDataProviderRelease(prov); CFRelease(data); return ok;
}

struct Cam { vec3 pos, R, U, F; };
static Cam camLook(vec3 pos, float yawDeg, float pitchDeg, float rollDeg = 0) {
    float y = yawDeg * PI / 180, p = pitchDeg * PI / 180, r = rollDeg * PI / 180; Cam c; c.pos = pos;
    c.F = v3(cosf(p) * cosf(y), sinf(p), cosf(p) * sinf(y)); vec3 R0 = normalize(cross(c.F, v3(0, 1, 0))), U0 = cross(R0, c.F);
    c.R = R0 * cosf(r) + U0 * sinf(r); c.U = U0 * cosf(r) - R0 * sinf(r); return c;
}

struct Look {           // everything about a shot that is not geometry
    float sunAz = 120, sunEl = 30; float mie = 1.0f; float exposure = 1.0f; float hfov = 72; float distort = 0.05f;
    vec4 mist = v4(0, 0, 100, 0); float cloudTop = ECO_CLOUDTOP; float treeRadius = 0; vec2 wind = v2(1, 0); float windAmt = 0.0f; float ambOcc = 0.5f; float cirrus = 0.45f; float localTM = 0.30f; float clampL = 4000.0f; float nr = 0.0f; int mistSamples = 2;
};

struct Gpu {
    id<MTLDevice> dev; id<MTLCommandQueue> queue; id<MTLComputePipelineState> pso;
    id<MTLTexture> tH[4], tM0, tM1, tG2, tSky, tW; id<MTLBuffer> bU, bAccum, bAtmo, bTlas, bNodes, bTris, bAttrs, bInsts;
    int W = 0, H = 0; Uniforms U;
    bool init(const std::string& srcDir) {
        dev = MTLCreateSystemDefaultDevice(); if (!dev) { fprintf(stderr, "no Metal device\n"); return false; }
        queue = [dev newCommandQueue];
        auto slurp = [](const std::string& p) { FILE* f = fopen(p.c_str(), "rb"); if (!f) { fprintf(stderr, "cannot read %s\n", p.c_str()); exit(1); } std::string s; char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n); fclose(f); return s; };
        std::string src = "#include <metal_stdlib>\nusing namespace metal;\n" + slurp(srcDir + "/shared.h") + slurp(srcDir + "/shader.metal");
        NSError* err = nil; MTLCompileOptions* opt = [MTLCompileOptions new]; opt.mathMode = MTLMathModeFast;
        id<MTLLibrary> lib = [dev newLibraryWithSource:[NSString stringWithUTF8String:src.c_str()] options:opt error:&err];
        if (!lib) { fprintf(stderr, "shader compile failed:\n%s\n", [[err localizedDescription] UTF8String]); return false; }
        pso = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"render"] error:&err];
        if (!pso) { fprintf(stderr, "pipeline failed: %s\n", [[err localizedDescription] UTF8String]); return false; }
        if (!dev.supports32BitFloatFiltering) { fprintf(stderr, "device cannot filter 32-bit float textures\n"); return false; }
        bU = [dev newBufferWithLength:sizeof(Uniforms) options:MTLResourceStorageModeShared];
        bAtmo = [dev newBufferWithLength:sizeof(vec4) * ATMO_N * 2 options:MTLResourceStorageModeShared];
        memset(&U, 0, sizeof U); return true;
    }
    id<MTLTexture> makeTex(MTLPixelFormat fmt, int w, int h, const void* data, int bpp) {
        MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt width:w height:h mipmapped:NO]; d.usage = MTLTextureUsageShaderRead; d.storageMode = MTLStorageModeShared;
        id<MTLTexture> t = [dev newTextureWithDescriptor:d]; [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:data bytesPerRow:(NSUInteger)w * bpp]; return t;
    }
    void setSize(int w, int h) { if (w == W && h == H) return; W = w; H = h; bAccum = [dev newBufferWithLength:sizeof(vec4) * (size_t)W * H options:MTLResourceStorageModeShared]; }
    template <class T> id<MTLBuffer> makeBuf(const std::vector<T>& v) { size_t n = std::max<size_t>(1, v.size()) * sizeof(T); id<MTLBuffer> b = [dev newBufferWithLength:n options:MTLResourceStorageModeShared]; if (!v.empty()) memcpy([b contents], v.data(), v.size() * sizeof(T)); return b; }
    void uploadGlobal(const Terrain& T) {
        tH[2] = makeTex(MTLPixelFormatR32Float, T.NF, T.NF, T.hF.data(), 4); tH[3] = makeTex(MTLPixelFormatR32Float, T.NR, T.NR, T.hR.data(), 4);
        tG2 = makeTex(MTLPixelFormatRGBA8Unorm, T.NF, T.NF, T.geoF.data(), 4);
        // textures hold texel centres at integer coords: corner = origin - half texel
    }
    void uploadClip(const Terrain& T) {
        int N = Terrain::NC; tH[0] = makeTex(MTLPixelFormatR32Float, N, N, T.h0.data(), 4); tH[1] = makeTex(MTLPixelFormatR32Float, N, N, T.h1.data(), 4);
        tM0 = makeTex(MTLPixelFormatRGBA8Unorm, N, N, T.m0.data(), 4); tM1 = makeTex(MTLPixelFormatRGBA8Unorm, N, N, T.m1.data(), 4);
        tW = makeTex(MTLPixelFormatR32Float, N / 4, N / 4, T.w0.data(), 4); U.misc.z = T.stream.on ? 1.0f : 0.0f;
        U.origin = v4(T.O, 0);
        U.clip0 = v4(T.c0x - 0.5f * T.d0 - T.O.x, T.c0z - 0.5f * T.d0 - T.O.z, 1.0f / (N * T.d0), T.d0); U.clip1 = v4(T.c1x - 0.5f * T.d1 - T.O.x, T.c1z - 0.5f * T.d1 - T.O.z, 1.0f / (N * T.d1), T.d1);
        U.clip2 = v4(-0.5f * T.dxF - T.O.x, -0.5f * T.dxF - T.O.z, 1.0f / (T.NF * T.dxF), T.dxF); U.clip3 = v4(T.offR - 0.5f * T.dxR - T.O.x, T.offR - 0.5f * T.dxR - T.O.z, 1.0f / (T.NR * T.dxR), T.dxR);
    }
    void uploadScene(const Scene& S) {
        bTlas = makeBuf(S.tlasNodes); bNodes = makeBuf(S.blasNodes); bTris = makeBuf(S.tris); bAttrs = makeBuf(S.attrs); bInsts = makeBuf(S.insts); U.tlasCount = (uint32_t)S.insts.size();
    }
    void setLook(const Look& L, float camAltM) {
        Atmos A; A.mieScale = L.mie; A.build();
        float az = L.sunAz * PI / 180, el = L.sunEl * PI / 180; vec3 sun = v3(cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az));
        SkyMap sky; sky.build(A, camAltM * 0.001f, sun);
        tSky = makeTex(MTLPixelFormatRGBA32Float, SkyMap::W, SkyMap::H, sky.px.data(), 16);
        vec4* at = (vec4*)[bAtmo contents];
        for (int i = 0; i < ATMO_N; i++) { float h = ATMO_HMAX * i / (ATMO_N - 1); at[i] = v4(A.transToSun(Atmos::Rg + h, sun.y), 0); at[ATMO_N + i] = v4(A.psi(h, sun.y), 0); }
        const float SUN = 20.0f;
        fprintf(stderr, "sky irradiance / sun = %.3f %.3f %.3f, sun transmittance %.3f %.3f %.3f\n", sky.irradianceUp.x, sky.irradianceUp.y, sky.irradianceUp.z, at[(int)(camAltM * 0.001f / ATMO_HMAX * (ATMO_N - 1))].x, at[(int)(camAltM * 0.001f / ATMO_HMAX * (ATMO_N - 1))].y, at[(int)(camAltM * 0.001f / ATMO_HMAX * (ATMO_N - 1))].z);
        U.sunDir = v4(sun, 0.00465f); U.sunE = v4(SUN, SUN, SUN, L.mie); U.skyAmb = v4(sky.irradianceUp * SUN, L.ambOcc);
        U.lens = v4(tanf(L.hfov * PI / 360), 0, L.distort, 0); U.mist = L.mist; U.cloud = v4(L.cloudTop, 1, 0, (float)L.mistSamples); U.wind = v4(L.wind.x, L.wind.y, L.windAmt, 1.3f);
        U.shot.z = L.treeRadius; U.shot.w = camAltM * 0.001f; U.misc.x = L.cirrus; U.misc.w = L.clampL;
    }
    // render one frame into the accumulation buffer; returns false on GPU error / incomplete samples
    bool render(const Cam& c0, const Cam& c1, float time, uint32_t frame, int spp, int sppPass) {
        U.camPos0 = v4(c0.pos, 0); U.camPos1 = v4(c1.pos, 0); U.camR0 = v4(c0.R, 0); U.camU0 = v4(c0.U, 0); U.camF0 = v4(c0.F, 0); U.camR1 = v4(c1.R, 0); U.camU1 = v4(c1.U, 0); U.camF1 = v4(c1.F, 0);
        U.lens.y = (float)H / W; U.lens.w = time; U.width = W; U.height = H; U.frame = frame; U.sppTotal = spp;
        for (int s0 = 0; s0 < spp; s0 += sppPass) {
            U.spp0 = s0; U.sppN = std::min(sppPass, spp - s0); memcpy([bU contents], &U, sizeof U);
            id<MTLCommandBuffer> cb = [queue commandBuffer]; id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            [enc setComputePipelineState:pso];
            for (int i = 0; i < 4; i++) [enc setTexture:tH[i] atIndex:i];
            [enc setTexture:tM0 atIndex:4]; [enc setTexture:tM1 atIndex:5]; [enc setTexture:tG2 atIndex:6]; [enc setTexture:tSky atIndex:7]; [enc setTexture:tW atIndex:8];
            [enc setBuffer:bU offset:0 atIndex:0]; [enc setBuffer:bAccum offset:0 atIndex:1]; [enc setBuffer:bAtmo offset:0 atIndex:2];
            [enc setBuffer:bTlas offset:0 atIndex:3]; [enc setBuffer:bNodes offset:0 atIndex:4]; [enc setBuffer:bTris offset:0 atIndex:5]; [enc setBuffer:bAttrs offset:0 atIndex:6]; [enc setBuffer:bInsts offset:0 atIndex:7];
            MTLSize tg = MTLSizeMake(8, 8, 1), groups = MTLSizeMake((W + 7) / 8, (H + 7) / 8, 1);
            [enc dispatchThreadgroups:groups threadsPerThreadgroup:tg]; [enc endEncoding]; [cb commit]; [cb waitUntilCompleted];
            if (cb.status != MTLCommandBufferStatusCompleted || cb.error) { fprintf(stderr, "GPU error: %s\n", cb.error ? [[cb.error localizedDescription] UTF8String] : "status"); return false; }
        }
        const vec4* a = (const vec4*)[bAccum contents]; size_t bad = 0; for (size_t i = 0; i < (size_t)W * H; i++) if (a[i].w != (float)spp) bad++;
        if (bad) { fprintf(stderr, "frame %u: %zu pixels with incomplete samples\n", frame, bad); return false; }
        return true;
    }
};

static std::map<std::string, std::string> parseArgs(int argc, char** argv, int first) {
    std::map<std::string, std::string> m; for (int i = first; i < argc; i++) { std::string a = argv[i]; if (a.rfind("--", 0) == 0) { std::string k = a.substr(2); if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) m[k] = argv[++i]; else m[k] = "1"; } }
    return m;
}
static float argf(std::map<std::string, std::string>& m, const char* k, float d) { return m.count(k) ? (float)atof(m[k].c_str()) : d; }

#include "camera.h"
#include "scene.h"

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "usage: engine route|still|shot ...\n"); return 1; }
        std::string mode = argv[1]; auto args = parseArgs(argc, argv, 2);
        std::string work = args.count("work") ? args["work"] : "work", srcDir = args.count("src") ? args["src"] : "src";
        Terrain T; double t0 = now();
        if (!T.load((work + "/fine.bin").c_str(), (work + "/far.bin").c_str())) { fprintf(stderr, "terrain maps missing; run terrain_gen first\n"); return 1; }
        fprintf(stderr, "terrain loaded %.1fs\n", now() - t0);
        if (mode == "rebed") { if (!T.loadTrail((work + "/trail.bin").c_str())) return 1; T.finishTrail(); T.saveTrail((work + "/trail.bin").c_str()); fprintf(stderr, "trail bed recomputed for %d points\n", T.trail.n()); return 0; }   // keep the line, refit heights
        if (mode == "route") return routeMain(T, work, args);
        if (mode == "horizon") {      // terrain horizon elevation around trail stations (to place the sun)
            T.loadTrail((work + "/trail.bin").c_str());
            for (int i = 2; i < argc; i++) { float st = atof(argv[i]); vec2 p = T.trail.pos(st); float y0 = T.macroH(p.x, p.y) + 1.7f; vec2 d = T.trail.dir(st);
                printf("s=%.0f heading %.0f deg alt %.0f:", st, atan2f(d.y, d.x) * 180 / PI, y0);
                for (int az = 0; az < 360; az += 15) { float a = az * PI / 180, mx = -90; for (float r = 30; r < 30000; r *= 1.04f) { float h = T.baseH(p.x + cosf(a) * r, p.y + sinf(a) * r); mx = std::max(mx, atan2f(h - y0, r) * 180 / PI); } printf(" %d:%.0f", az, mx); }
                printf("\n"); }
            return 0; }
        if (!T.loadTrail((work + "/trail.bin").c_str())) fprintf(stderr, "warning: no trail yet\n");
        Gpu G; if (!G.init(srcDir)) return 1; G.uploadGlobal(T);
        if (mode == "still") return stillMain(T, G, args);
        if (mode == "shot") return shotMain(T, G, argc > 2 ? atoi(argv[2]) : 0, args);
        fprintf(stderr, "unknown mode\n"); return 1;
    }
}
