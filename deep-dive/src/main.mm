// main.mm — "Deep Dive" frame renderer. Builds the reef, simulates the diver, the sea surface and the animals,
// and renders every frame with the Metal compute kernel in render.metal. One PNG per frame, resumable.
//   dive --out frames --size 1920x1080 --spp 16 --frames 0:2339
#import <Metal/Metal.h>
#include "water.h"
#include "sim.h"

struct Uniforms {
    F4 camPos, camR, camU, camF, res, sunW, sunA, caus, torchP, torchD, cnt, gridO, gridN, surge, dbg, mantaP, mantaF, mantaU, fade, camVel;
};

static std::string readFile(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb"); if (!f) { fprintf(stderr, "cannot read %s\n", p.c_str()); exit(1); }
    std::string s; char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n); fclose(f); return s;
}
static bool fileExists(const std::string& p) { FILE* f = fopen(p.c_str(), "rb"); if (f) { fclose(f); return true; } return false; }

// ------------------------------------------------------------ darkroom: glare, exposure, tone curve, vignette, grain
static void gaussBlur(std::vector<float>& img, int w, int h, float sigma) {
    int r = std::max(1, (int)ceilf(2.5f * sigma)); std::vector<float> ker(2 * r + 1); float ks = 0;
    for (int i = -r; i <= r; i++) { ker[i + r] = expf(-0.5f * i * i / (sigma * sigma)); ks += ker[i + r]; }
    for (auto& v : ker) v /= ks;
    std::vector<float> tmp(img.size());
    float* ip = img.data(); float* tp = tmp.data(); const float* kp = ker.data();
    parallelFor(h, [=](size_t y) {
        for (int x = 0; x < w; x++) { float a[3] = {0, 0, 0}; for (int d = -r; d <= r; d++) { int xx = std::min(std::max(x + d, 0), w - 1); const float* p = ip + (y * w + xx) * 3; float k = kp[d + r]; a[0] += k * p[0]; a[1] += k * p[1]; a[2] += k * p[2]; } float* o = tp + (y * w + x) * 3; o[0] = a[0]; o[1] = a[1]; o[2] = a[2]; }
    });
    parallelFor(h, [=](size_t y) {
        for (int x = 0; x < w; x++) { float a[3] = {0, 0, 0}; for (int d = -r; d <= r; d++) { int yy = std::min(std::max((int)y + d, 0), h - 1); const float* p = tp + ((size_t)yy * w + x) * 3; float k = kp[d + r]; a[0] += k * p[0]; a[1] += k * p[1]; a[2] += k * p[2]; } float* o = ip + (y * w + x) * 3; o[0] = a[0]; o[1] = a[1]; o[2] = a[2]; }
    });
}
struct FrameStats { double logLum = 0, mr = 0, mg = 0, mb = 0; };
static FrameStats develop(const std::vector<float>& acc, int W, int H, float exposure, V3 wb, float fadeMul, int frame, std::vector<uint8_t>& out) {
    std::vector<float> hdr((size_t)W * H * 3);
    FrameStats st; double sl = 0, sr = 0, sg = 0, sb = 0, sw = 0;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const float* a = &acc[((size_t)y * W + x) * 4]; float inv = 1.0f / std::max(a[3], 1.0f);
        float r = a[0] * inv, g = a[1] * inv, b = a[2] * inv;
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        float cx = (x + 0.5f) / W - 0.5f, cy = (y + 0.5f) / H - 0.5f; float wgt = expf(-(cx * cx + cy * cy) * 4.0f);
        sl += wgt * logf(std::max(lum, 1e-4f)); sr += wgt * r; sg += wgt * g; sb += wgt * b; sw += wgt;
        float* o = &hdr[((size_t)y * W + x) * 3]; o[0] = r * exposure * wb.x; o[1] = g * exposure * wb.y; o[2] = b * exposure * wb.z;
    }
    st.logLum = sl / sw; st.mr = sr / sw; st.mg = sg / sw; st.mb = sb / sw;
    // glare: three Gaussian lobes built at quarter resolution
    int ds = 4, w4 = W / ds, h4 = H / ds;
    std::vector<float> q((size_t)w4 * h4 * 3);
    for (int y = 0; y < h4; y++) for (int x = 0; x < w4; x++) {
        float a[3] = {0, 0, 0};
        for (int j = 0; j < ds; j++) for (int i = 0; i < ds; i++) { const float* p = &hdr[((size_t)(y * ds + j) * W + x * ds + i) * 3]; a[0] += std::min(p[0], 30.0f); a[1] += std::min(p[1], 30.0f); a[2] += std::min(p[2], 30.0f); }
        float* o = &q[((size_t)y * w4 + x) * 3]; o[0] = a[0] / (ds * ds); o[1] = a[1] / (ds * ds); o[2] = a[2] / (ds * ds);
    }
    float sc = W / 1920.0f;
    std::vector<float> b1 = q, b2, b3;
    gaussBlur(b1, w4, h4, 1.6f * sc); b2 = b1; gaussBlur(b2, w4, h4, 5.5f * sc); b3 = b2; gaussBlur(b3, w4, h4, 20.0f * sc);
    out.resize((size_t)W * H * 3);
    const float w1 = 0.055f, w2 = 0.05f, w3 = 0.05f;
    parallelFor(H, [&](size_t y) {
        for (int x = 0; x < W; x++) {
            float fx = (x + 0.5f) / ds - 0.5f, fy = (y + 0.5f) / ds - 0.5f;
            int x0 = std::min(std::max((int)floorf(fx), 0), w4 - 1), y0 = std::min(std::max((int)floorf(fy), 0), h4 - 1);
            int x1 = std::min(x0 + 1, w4 - 1), y1 = std::min(y0 + 1, h4 - 1); float ax = clampf(fx - x0, 0, 1), ay = clampf(fy - y0, 0, 1);
            float w00 = (1 - ax) * (1 - ay), w10 = ax * (1 - ay), w01 = (1 - ax) * ay, w11 = ax * ay;
            const float* p = &hdr[(y * W + x) * 3];
            float nx = ((x + 0.5f) / W - 0.5f) * 2, ny = ((y + 0.5f) / H - 0.5f) * 2 * H / W;
            float rr = sqrtf(nx * nx + ny * ny) / 1.147f;
            float vig = 1.0f - 0.30f * powf(rr, 2.4f) - 0.25f * sstep(0.90f, 1.06f, rr);
            uint32_t hs = pcg((uint32_t)x + 5501u * (uint32_t)y + 7919u * 104729u * (uint32_t)(frame + 1));
            float g1 = (hs & 0xffff) / 65535.0f + ((hs >> 16) & 0xffff) / 65535.0f - 1.0f;      // triangular
            uint32_t hs2 = pcg(hs);
            for (int c = 0; c < 3; c++) {
                size_t i00 = ((size_t)y0 * w4 + x0) * 3 + c, i10 = ((size_t)y0 * w4 + x1) * 3 + c, i01 = ((size_t)y1 * w4 + x0) * 3 + c, i11 = ((size_t)y1 * w4 + x1) * 3 + c;
                float g = w1 * (b1[i00] * w00 + b1[i10] * w10 + b1[i01] * w01 + b1[i11] * w11) + w2 * (b2[i00] * w00 + b2[i10] * w10 + b2[i01] * w01 + b2[i11] * w11) + w3 * (b3[i00] * w00 + b3[i10] * w10 + b3[i01] * w01 + b3[i11] * w11);
                float v = (p[c] * (1 - w1 - w2 - w3) + g) * vig * fadeMul;
                v = std::max(v, 0.0f);
                v = (v * (2.51f * v + 0.03f)) / (v * (2.43f * v + 0.59f) + 0.14f);          // filmic shoulder
                v = clampf(v, 0, 1);
                v = v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1 / 2.4f) - 0.055f;
                v += g1 * 0.011f * (1.0f - 0.5f * v);                                          // grain
                float dth = (((hs2 >> (8 * c)) & 0xff) + 0.5f) / 256.0f - 0.5f;
                out[(y * W + x) * 3 + c] = (uint8_t)clampf(v * 255.0f + 0.5f + dth, 0, 255);
            }
        }
    });
    return st;
}

// ------------------------------------------------------------ screen-tile lists for fish and bubbles
struct Lens { float T, A; int W, H, tile, tx, ty; };
static void binSphere(const CamState& cam, const Lens& L, V3 c, float R, uint32_t id, std::vector<std::vector<uint32_t>>& lists) {
    V3 d = c - cam.pos; float dist = len(d);
    int x0, x1, y0, y1;
    if (dist < R * 1.25f + 0.02f) { x0 = 0; y0 = 0; x1 = L.tx - 1; y1 = L.ty - 1; }
    else {
        float zc = dot(d, cam.f), xc = dot(d, cam.r), yc = dot(d, cam.u);
        float th = acosf(clampf(zc / dist, -1, 1)), al = asinf(R / dist);
        if (th - al > L.A * atanf(1.16f * L.T)) return;
        float lim = 0.97f * L.A * PI * 0.5f;
        float rr = tanf(std::min(th, lim) / L.A) / L.T;
        float sxy = sqrtf(xc * xc + yc * yc) + 1e-9f;
        float px = (xc / sxy * rr + 1) * 0.5f * L.W, py = (1 - (yc / sxy * rr) / ((float)L.H / L.W)) * 0.5f * L.H;
        float cE = cosf(std::min(th + al, lim) / L.A);
        float pr = al / (L.A * L.T * cE * cE) * 0.5f * L.W * 1.2f + 3.0f;
        x0 = (int)floorf((px - pr) / L.tile); x1 = (int)floorf((px + pr) / L.tile); y0 = (int)floorf((py - pr) / L.tile); y1 = (int)floorf((py + pr) / L.tile);
        if (x1 < 0 || y1 < 0 || x0 >= L.tx || y0 >= L.ty) return;
        x0 = std::max(x0, 0); y0 = std::max(y0, 0); x1 = std::min(x1, L.tx - 1); y1 = std::min(y1, L.ty - 1);
    }
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) lists[(size_t)y * L.tx + x].push_back(id);
}

int main(int argc, char** argv) {
    @autoreleasepool {
        int W = 1920, H = 1080, spp = 16, f0 = 0, f1 = 0, fstep = 1, quality = 1, sub = 4; float fps = 30, dbg = 0, exposure = 1.6f;
        std::string outDir = "frames", srcDir = "src", expFile, statsFile, scanFile, onlyFile; bool force = false;
        float camOv[5]; bool haveCamOv = false;
        float rmsSlope = 0.09f, blurGain = 1.0f, hideType = 0;
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i]; auto nxt = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
            if (a == "--size") { sscanf(nxt().c_str(), "%dx%d", &W, &H); }
            else if (a == "--spp") spp = atoi(nxt().c_str());
            else if (a == "--sub") sub = atoi(nxt().c_str());
            else if (a == "--frames") { std::string s = nxt(); int n = sscanf(s.c_str(), "%d:%d:%d", &f0, &f1, &fstep); if (n == 1) f1 = f0; if (n < 3) fstep = 1; }
            else if (a == "--out") outDir = nxt();
            else if (a == "--src") srcDir = nxt();
            else if (a == "--fps") fps = atof(nxt().c_str());
            else if (a == "--quality") quality = atoi(nxt().c_str());
            else if (a == "--dbg") dbg = atof(nxt().c_str());
            else if (a == "--exposure") exposure = atof(nxt().c_str());
            else if (a == "--expfile") expFile = nxt();
            else if (a == "--stats") statsFile = nxt();
            else if (a == "--force") force = true;
            else if (a == "--scan") scanFile = nxt();      // no rendering: list the frames in which a bubble passes within 15 cm of the lens
            else if (a == "--only") onlyFile = nxt();      // render only the frames listed in this file
            else if (a == "--hide") hideType = atof(nxt().c_str());
            else if (a == "--slope") rmsSlope = atof(nxt().c_str());
            else if (a == "--blur") blurGain = atof(nxt().c_str());
            else if (a == "--cam") { sscanf(nxt().c_str(), "%f,%f,%f,%f,%f", camOv, camOv + 1, camOv + 2, camOv + 3, camOv + 4); haveCamOv = true; }
            else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 1; }
        }
        sub = std::max(1, std::min(sub, spp));
        double tStart = nowSec();
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) { fprintf(stderr, "no Metal device\n"); return 1; }
        std::string pre = "#include <metal_stdlib>\nusing namespace metal;\n#define SH_INLINE static inline\n#define CTXD , texture3d<float> nzT\n#define CTX , nzT\n"
                          "static inline float4 nz4m(texture3d<float> nzT, float3 p) {\n"
                          "  float3 t = p * 8.0f - 0.5f; float3 fl = floor(t); float3 f = t - fl; f = f*f*f*(f*(f*6.0f-15.0f)+10.0f);\n"
                          "  int3 a = int3(fl) & 127; int3 b = (a + 1) & 127;\n"
                          "  float4 c000 = nzT.read(uint3(a.x,a.y,a.z)), c100 = nzT.read(uint3(b.x,a.y,a.z)), c010 = nzT.read(uint3(a.x,b.y,a.z)), c110 = nzT.read(uint3(b.x,b.y,a.z));\n"
                          "  float4 c001 = nzT.read(uint3(a.x,a.y,b.z)), c101 = nzT.read(uint3(b.x,a.y,b.z)), c011 = nzT.read(uint3(a.x,b.y,b.z)), c111 = nzT.read(uint3(b.x,b.y,b.z));\n"
                          "  return mix(mix(mix(c000,c100,f.x), mix(c010,c110,f.x), f.y), mix(mix(c001,c101,f.x), mix(c011,c111,f.x), f.y), f.z); }\n"
                          "#define NOISE4(x,y,z) nz4m(nzT, float3(x,y,z))\ntypedef unsigned int uint_unused;\n";
        std::string src = pre + readFile(srcDir + "/shared.h") + readFile(srcDir + "/render.metal");
        NSError* err = nil;
        id<MTLLibrary> lib = [dev newLibraryWithSource:@(src.c_str()) options:[MTLCompileOptions new] error:&err];
        if (!lib) { fprintf(stderr, "shader compile failed:\n%s\n", err.description.UTF8String); return 1; }
        id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"renderK"] error:&err];
        if (!pso) { fprintf(stderr, "pipeline failed: %s\n", err.description.UTF8String); return 1; }
        id<MTLCommandQueue> queue = [dev newCommandQueue];

        // ---- static data
        buildNoise();
        std::vector<World::RayHero> rayHeroes;
        {   // what the torch will find: a big red fan straight ahead at 57 s, a sponge and a violet fan beside it
            struct RH { float t, dyaw, dpitch; int type; float scale, hue; };
            const RH rh[] = {{57.0f, 0, 2, T_FAN, 1.7f, 0.2f}, {55.6f, 24, -14, T_BARREL, 1.0f, 0.3f}, {58.6f, -20, 8, T_FAN, 1.3f, 0.9f}, {53.0f, 6, -4, T_FAN, 1.2f, 0.6f}};
            for (const RH& h : rh) {
                V3 p; float yaw, pitch; keyAt(h.t, p, yaw, pitch);
                float ya = (yaw + h.dyaw) * PI / 180, pa = (pitch + h.dpitch) * PI / 180;
                rayHeroes.push_back(World::RayHero{p, V3(cosf(ya) * cosf(pa), sinf(pa), sinf(ya) * cosf(pa)), h.type, h.scale, h.hue});
            }
        }
        World world; world.build(20261114, rayHeroes);
        Waves waves; waves.init(4242, rmsSlope, 38.0f);
        float sunEl = 47.0f * PI / 180, sunAz = 16.0f * PI / 180;
        V3 sunA(cosf(sunEl) * cosf(sunAz), sinf(sunEl), cosf(sunEl) * sinf(sunAz));
        Caustics caus; caus.init(sunA);
        Sim sim; sim.init(world);
        [[NSFileManager defaultManager] createDirectoryAtPath:@(outDir.c_str()) withIntermediateDirectories:YES attributes:nil error:nil];
        fprintf(stderr, "setup %.2fs: %zu instances, %zu cell refs\n", nowSec() - tStart, world.insts.size(), world.cellIdx.size());

        // ---- GPU resources
        auto mkTex3 = [&](MTLPixelFormat pf, int w, int h, int d) { MTLTextureDescriptor* td = [MTLTextureDescriptor new]; td.textureType = MTLTextureType3D; td.pixelFormat = pf; td.width = w; td.height = h; td.depth = d; td.usage = MTLTextureUsageShaderRead; return [dev newTextureWithDescriptor:td]; };
        id<MTLTexture> nzTex = mkTex3(MTLPixelFormatRGBA16Float, NZ_N, NZ_N, NZ_N);
        {
            std::vector<__fp16> hf(g_nz.size()); for (size_t i = 0; i < g_nz.size(); i++) hf[i] = (__fp16)g_nz[i];
            [nzTex replaceRegion:MTLRegionMake3D(0, 0, 0, NZ_N, NZ_N, NZ_N) mipmapLevel:0 slice:0 withBytes:hf.data() bytesPerRow:NZ_N * 8 bytesPerImage:NZ_N * NZ_N * 8];
        }
        id<MTLTexture> cauTex = mkTex3(MTLPixelFormatR16Float, caus.N, caus.N, caus.K);
        id<MTLTexture> cauLoTex = mkTex3(MTLPixelFormatR16Float, caus.NL, caus.NL, caus.K);
        MTLTextureDescriptor* sd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:waves.N height:waves.N mipmapped:NO];
        sd.usage = MTLTextureUsageShaderRead; id<MTLTexture> surfTex = [dev newTextureWithDescriptor:sd];
        MTLTextureDescriptor* ad = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:W height:H mipmapped:NO];
        ad.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite; id<MTLTexture> accTex = [dev newTextureWithDescriptor:ad];
        id<MTLBuffer> uBuf = [dev newBufferWithLength:sizeof(Uniforms) options:MTLResourceStorageModeShared];
        id<MTLBuffer> instBuf = [dev newBufferWithBytes:world.insts.data() length:world.insts.size() * sizeof(Inst) options:MTLResourceStorageModeShared];
        id<MTLBuffer> cellBuf = [dev newBufferWithBytes:world.cells.data() length:world.cells.size() * sizeof(Cell) options:MTLResourceStorageModeShared];
        id<MTLBuffer> idxBuf = [dev newBufferWithBytes:world.cellIdx.data() length:world.cellIdx.size() * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        const float hfov = 104.0f * PI / 180, lensT = 0.75f; float lensA = (hfov * 0.5f) / atanf(lensT);
        Lens lens; lens.T = lensT; lens.A = lensA; lens.W = W; lens.H = H; lens.tile = std::max(8, W / 60); lens.tx = (W + lens.tile - 1) / lens.tile; lens.ty = (H + lens.tile - 1) / lens.tile;
        const size_t MAXF = 2048, MAXB = 4096, MAXTI = 1 << 21;
        id<MTLBuffer> fishBuf = [dev newBufferWithLength:MAXF * sizeof(GFish) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bubBuf = [dev newBufferWithLength:MAXB * sizeof(GBub) options:MTLResourceStorageModeShared];
        id<MTLBuffer> tileBuf = [dev newBufferWithLength:(size_t)lens.tx * lens.ty * 2 * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> tileIdxBuf = [dev newBufferWithLength:MAXTI * sizeof(uint32_t) options:MTLResourceStorageModeShared];

        std::vector<float> expCurve;
        if (!expFile.empty()) { FILE* f = fopen(expFile.c_str(), "r"); float v; if (f) { while (fscanf(f, "%f", &v) == 1) expCurve.push_back(v); fclose(f); } if (expCurve.empty()) { fprintf(stderr, "exposure file %s missing/empty\n", expFile.c_str()); return 1; } }
        FILE* statsF = statsFile.empty() ? nullptr : fopen(statsFile.c_str(), "w");
        FILE* scanF = scanFile.empty() ? nullptr : fopen(scanFile.c_str(), "w");
        std::vector<char> only;
        if (!onlyFile.empty()) { only.assign(100000, 0); FILE* f = fopen(onlyFile.c_str(), "r"); int v; if (f) { while (fscanf(f, "%d", &v) == 1) if (v >= 0 && v < 100000) only[v] = 1; fclose(f); } }

        dispatch_group_t wgroup = dispatch_group_create();
        dispatch_semaphore_t wsem = dispatch_semaphore_create(6);
        std::vector<float> acc((size_t)W * H * 4);
        std::vector<GFish> gfish; std::vector<GBub> gbub;
        std::vector<std::vector<uint32_t>> lists((size_t)lens.tx * lens.ty);
        int rendered = 0, failed = 0; double tRender = 0;
        int sppPass = std::max(1, spp / sub);
        for (int f = f0; f <= f1; f += fstep) {
            char name[64]; snprintf(name, sizeof name, "/f_%05d.png", f);
            std::string path = outDir + name;
            if (!only.empty() && !only[f]) continue;
            if (!force && fileExists(path)) continue;
            double tf0 = nowSec(), tPrep = 0; bool nearBubble = false;
            float t = f / fps; float clear = 0; CamState cam0;
            bool gpuOK = true;
            for (int k = 0; k < sub && gpuOK; k++) {
                double tp0 = nowSec();
                const float dtSub = (0.5f / fps) / sub;
                float tk = t + k * dtSub;                               // 180 degree shutter
                sim.advanceTo(tk);
                CamState cam = sim.cam;
                if (haveCamOv) {
                    float ya = camOv[3] * PI / 180, pa = camOv[4] * PI / 180;
                    cam.pos = V3(camOv[0], camOv[1], camOv[2]); cam.f = V3(cosf(ya) * cosf(pa), sinf(pa), sinf(ya) * cosf(pa));
                    cam.r = norm(cross(cam.f, V3(0, 1, 0))); cam.u = cross(cam.r, cam.f);
                    cam.torchP = cam.pos + cam.r * 0.24f - cam.u * 0.26f; cam.torchD = cam.f;
                }
                if (k == 0) { clear = terr(cam.pos); cam0 = cam; }
                if (scanF) {
                    for (const BubS& b : sim.bubs) {
                        V3 d = b.pos - cam.pos; float dist = len(d); if (dist - b.r * 1.3f > 0.16f) continue;
                        float th = acosf(clampf(dot(d, cam.f) / std::max(dist, 1e-4f), -1, 1)), al = dist > b.r * 1.3f ? asinf(b.r * 1.3f / dist) : 3.2f;
                        if (th - al < lens.A * atanf(1.16f * lens.T)) nearBubble = true;
                    }
                    continue;
                }
                waves.eval(tk);
                caus.build(waves, sunA, blurGain);
                sim.exportFish(gfish); sim.exportBubbles(gbub);
                if (gfish.size() > MAXF) gfish.resize(MAXF); if (gbub.size() > MAXB) gbub.resize(MAXB);
                for (auto& l : lists) l.clear();
                for (size_t i = 0; i < gfish.size(); i++) binSphere(cam, lens, V3(gfish[i].pos.x, gfish[i].pos.y, gfish[i].pos.z), 0.60f * gfish[i].pos.w + dtSub * (len(V3(gfish[i].vel.x, gfish[i].vel.y, gfish[i].vel.z)) + len(sim.camVel)), (uint32_t)i, lists);
                for (size_t i = 0; i < gbub.size(); i++) binSphere(cam, lens, V3(gbub[i].pos.x, gbub[i].pos.y, gbub[i].pos.z), gbub[i].pos.w * 1.05f + dtSub * (len(V3(gbub[i].vel.x, gbub[i].vel.y, gbub[i].vel.z)) + len(sim.camVel)), (uint32_t)i | 0x80000000u, lists);
                {
                    uint32_t* tp = (uint32_t*)tileBuf.contents; uint32_t* ip = (uint32_t*)tileIdxBuf.contents; uint32_t off = 0;
                    for (size_t c = 0; c < lists.size(); c++) {
                        uint32_t n = (uint32_t)lists[c].size(); if (off + n > MAXTI) n = (uint32_t)(MAXTI - off);
                        tp[c * 2] = off; tp[c * 2 + 1] = n; if (n) memcpy(ip + off, lists[c].data(), n * sizeof(uint32_t)); off += n;
                    }
                }
                if (!gfish.empty()) memcpy(fishBuf.contents, gfish.data(), gfish.size() * sizeof(GFish));
                if (!gbub.empty()) memcpy(bubBuf.contents, gbub.data(), gbub.size() * sizeof(GBub));
                [surfTex replaceRegion:MTLRegionMake2D(0, 0, waves.N, waves.N) mipmapLevel:0 withBytes:waves.surf.data() bytesPerRow:waves.N * 16];
                [cauTex replaceRegion:MTLRegionMake3D(0, 0, 0, caus.N, caus.N, caus.K) mipmapLevel:0 slice:0 withBytes:caus.hi.data() bytesPerRow:caus.N * 2 bytesPerImage:(size_t)caus.N * caus.N * 2];
                [cauLoTex replaceRegion:MTLRegionMake3D(0, 0, 0, caus.NL, caus.NL, caus.K) mipmapLevel:0 slice:0 withBytes:caus.lo.data() bytesPerRow:caus.NL * 2 bytesPerImage:(size_t)caus.NL * caus.NL * 2];
                Uniforms U; memset(&U, 0, sizeof U);
                U.camPos = f4(cam.pos, tk); U.camR = f4(cam.r, lensT); U.camU = f4(cam.u, lensA); U.camF = f4(cam.f, (float)H / W);
                U.res = F4{(float)W, (float)H, (float)k, (float)sppPass};
                U.sunW = f4(caus.sunW, (float)(sub * sppPass)); U.sunA = f4(sunA, 0);
                U.caus = F4{CAU_L, caus.shx, caus.shz, (float)caus.K};
                U.torchP = f4(cam.torchP, cam.torchI); U.torchD = f4(cam.torchD, cosf(17.0f * PI / 180));
                U.cnt = F4{(float)world.insts.size(), (float)gfish.size(), (float)gbub.size(), (float)f};
                U.gridO = f4(world.gridO, world.cs); U.gridN = F4{(float)world.nx, (float)world.ny, (float)world.nz, (float)lens.tile};
                U.surge = F4{surgeX(tk), surgeZ(tk), (float)lens.tx, (float)lens.ty};
                U.dbg = F4{dbg, (float)quality, hideType, 0};
                const MantaState& m = sim.manta;
                U.fade = F4{0, dtSub, 0, 0}; U.camVel = f4(haveCamOv ? V3(0, 0, 0) : sim.camVel, 0);
                U.mantaP = f4(m.pos, len(m.pos - cam.pos) < 60 ? m.scale : 0.0f); U.mantaF = f4(m.f, m.phase); U.mantaU = f4(m.u, m.amp);
                memcpy(uBuf.contents, &U, sizeof U);
                tPrep += nowSec() - tp0;
                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> en = [cb computeCommandEncoder];
                [en setComputePipelineState:pso];
                [en setTexture:accTex atIndex:0]; [en setTexture:nzTex atIndex:1]; [en setTexture:cauTex atIndex:2]; [en setTexture:cauLoTex atIndex:3]; [en setTexture:surfTex atIndex:4];
                [en setBuffer:uBuf offset:0 atIndex:0]; [en setBuffer:instBuf offset:0 atIndex:1]; [en setBuffer:cellBuf offset:0 atIndex:2]; [en setBuffer:idxBuf offset:0 atIndex:3];
                [en setBuffer:fishBuf offset:0 atIndex:4]; [en setBuffer:bubBuf offset:0 atIndex:5]; [en setBuffer:tileBuf offset:0 atIndex:6]; [en setBuffer:tileIdxBuf offset:0 atIndex:7];
                [en dispatchThreadgroups:MTLSizeMake((W + 15) / 16, (H + 15) / 16, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
                [en endEncoding]; [cb commit]; [cb waitUntilCompleted];
                if (cb.status != MTLCommandBufferStatusCompleted || cb.error) { fprintf(stderr, "frame %d pass %d GPU error: %s\n", f, k, cb.error ? cb.error.description.UTF8String : "status"); gpuOK = false; }
            }
            if (scanF) { if (nearBubble) fprintf(scanF, "%d\n", f); continue; }
            if (!gpuOK) { fprintf(stderr, "frame %d FAILED (not written)\n", f); failed++; continue; }
            [accTex getBytes:acc.data() bytesPerRow:W * 16 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
            double tf2 = nowSec();
            // sanity: every pixel must have received every sample (guards against silently dropped GPU work)
            size_t bad = 0; float want = (float)(sub * sppPass);
            for (size_t q = 0; q < (size_t)W * H; q++) if (acc[q * 4 + 3] != want) bad++;
            if (bad) { fprintf(stderr, "frame %d: %zu pixels with missing samples -> FAILED (not written)\n", f, bad); failed++; continue; }
            float ex = exposure; if (!expCurve.empty()) ex = expCurve[std::min((size_t)f, expCurve.size() - 1)];
            float fadeMul = sstep(0.0f, 1.3f, t) * (1 - sstep(FILM_LEN - 2.4f, FILM_LEN - 0.15f, t));
            if (haveCamOv || dbg > 0) fadeMul = 1;
            auto out = std::make_shared<std::vector<uint8_t>>();
            FrameStats st = develop(acc, W, H, ex, V3(1.55f, 1.0f, 0.86f), fadeMul, f, *out);
            if (statsF) { fprintf(statsF, "%d %.5f %.5f %.5f %.5f %.3f %.2f %.2f %.2f %.1f %.1f\n", f, st.logLum, st.mr, st.mg, st.mb, clear, cam0.pos.x, cam0.pos.y, cam0.pos.z, cam0.yaw, cam0.pitch); fflush(statsF); }
            dispatch_semaphore_wait(wsem, DISPATCH_TIME_FOREVER);
            dispatch_group_async(wgroup, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                if (!writePNG(path, out->data(), W, H)) fprintf(stderr, "write failed %s\n", path.c_str());
                dispatch_semaphore_signal(wsem);
            });
            double tf3 = nowSec(); rendered++; tRender += tf3 - tf0;
            fprintf(stderr, "frame %5d t=%6.2f  prep %.2fs gpu %.2fs post %.2fs  cam(%.1f %.1f %.1f) clear %.2f fish %zu bub %zu  avg %.2fs\n", f, t, tPrep, tf2 - tf0 - tPrep, tf3 - tf2, cam0.pos.x, cam0.pos.y, cam0.pos.z, clear, gfish.size(), gbub.size(), tRender / rendered);
        }
        dispatch_group_wait(wgroup, DISPATCH_TIME_FOREVER);
        if (statsF) fclose(statsF);
        if (scanF) fclose(scanF);
        if (!haveCamOv && dbg == 0 && !scanF) {   // breathing timeline for the soundtrack
            FILE* tf = fopen((outDir + "/timeline.txt").c_str(), "w");
            if (tf) { for (auto& b : sim.breaths) fprintf(tf, "breath %.4f %.4f %.4f %.4f\n", b.tin, b.din, b.tout, b.dout); fclose(tf); }
        }
        fprintf(stderr, "done: %d frames rendered, %d failed, %.1fs\n", rendered, failed, nowSec() - tStart);
        return failed ? 2 : 0;
    }
}
