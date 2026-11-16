// main.mm — host: builds the world, drives the Metal compute path tracer, post-processes and writes frames
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <sys/stat.h>
#include "geom.h"
#include "world3.h"
#include "cat.h"

struct Uni {
  float camPos0[4], camPos1[4], camR0[4], camU0[4], camF0[4], camR1[4], camU1[4], camF1[4];
  float lens[4], sunDir[4], sunCol[4], timeP[4], fogLo[4], fogHi[4], water[4], mapRect[4], kr0[4], kr1[4];
  float ripple[16][4];
  u32 counts[4], dims[4];
};
static void set3(float* d, V3 v, float w = 0) { d[0] = v.x; d[1] = v.y; d[2] = v.z; d[3] = w; }
static double nowSec() { return [[NSDate date] timeIntervalSince1970]; }

// ---------------- post processing (CPU) ----------------
static void blurH(std::vector<float>& img, int W, int H, float sigma) {
  int R = (int)ceilf(sigma * 2.5f);
  std::vector<float> k(R + 1); float s = 0;
  for (int i = 0; i <= R; i++) { k[i] = expf(-0.5f * i * i / (sigma * sigma)); s += (i ? 2 : 1) * k[i]; }
  for (auto& x : k) x /= s;
  std::vector<float> tmpv(img.size());
  float* tmp = tmpv.data(); float* imgp = img.data(); const float* kk = k.data();
  dispatch_apply(H, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t y) {
    for (int x = 0; x < W; x++) {
      float a[3] = {0, 0, 0};
      for (int i = -R; i <= R; i++) { int xx = std::min(std::max(x + i, 0), W - 1); float w = kk[abs(i)]; const float* p = &imgp[(y * W + xx) * 3]; a[0] += p[0] * w; a[1] += p[1] * w; a[2] += p[2] * w; }
      float* o = &tmp[(y * W + x) * 3]; o[0] = a[0]; o[1] = a[1]; o[2] = a[2];
    }
  });
  dispatch_apply(W, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t x) {
    for (int y = 0; y < H; y++) {
      float a[3] = {0, 0, 0};
      for (int i = -R; i <= R; i++) { int yy = std::min(std::max(y + i, 0), H - 1); float w = kk[abs(i)]; const float* p = &tmp[(yy * W + x) * 3]; a[0] += p[0] * w; a[1] += p[1] * w; a[2] += p[2] * w; }
      float* o = &imgp[(y * W + x) * 3]; o[0] = a[0]; o[1] = a[1]; o[2] = a[2];
    }
  });
}
struct PostP { float exposure = 1, lensFog = 0, blink = 0, fade = 1; u32 seed = 0; };
static inline float tone(float x) { // filmic curve (ACES-like fit)
  x = fmaxf(x, 0.0f) * 0.62f;
  float y = (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
  y = clampf(y, 0, 1);
  return y <= 0.0031308f ? 12.92f * y : 1.055f * powf(y, 1 / 2.4f) - 0.055f;
}
static void postProcess(const float* acc, int W, int H, const PostP& P, std::vector<uint8_t>& out) {
  std::vector<float> imgv((size_t)W * H * 3);
  float* img = imgv.data();
  for (size_t i = 0; i < (size_t)W * H; i++) {
    float inv = P.exposure / fmaxf(acc[i * 4 + 3], 1.0f);
    img[i * 3] = acc[i * 4] * inv; img[i * 3 + 1] = acc[i * 4 + 1] * inv; img[i * 3 + 2] = acc[i * 4 + 2] * inv;
  }
  // bloom on quarter resolution
  int ds = std::max(1, W / 480); int w4 = W / ds, h4 = H / ds;
  std::vector<float> b1((size_t)w4 * h4 * 3);
  for (int y = 0; y < h4; y++)
    for (int x = 0; x < w4; x++) {
      float a[3] = {0, 0, 0};
      for (int j = 0; j < ds; j++)
        for (int i = 0; i < ds; i++) { const float* p = &img[((size_t)(y * ds + j) * W + x * ds + i) * 3]; a[0] += p[0]; a[1] += p[1]; a[2] += p[2]; }
      float n = 1.0f / (ds * ds);
      for (int c = 0; c < 3; c++) { float v = a[c] * n; b1[((size_t)y * w4 + x) * 3 + c] = fminf(v, 40.0f); }
    }
  std::vector<float> b2 = b1, b3 = b1;
  for (auto& v : b1) v = fmaxf(v - 0.9f, 0.0f);
  b2 = b1;
  blurH(b1, w4, h4, 2.2f);
  blurH(b2, w4, h4, 9.0f);
  blurH(b3, w4, h4, 22.0f); // wide veil (lens fog / haze)
  const float* B1 = b1.data(); const float* B2 = b2.data(); const float* B3 = b3.data();
  const PostP PP = P;
  out.resize((size_t)W * H * 3);
  float fog = P.lensFog;
  uint8_t* op = out.data();
  dispatch_apply(H, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(size_t y) {
    u32 rs = pcg((u32)y * 7919u + PP.seed * 104729u);
    for (int x = 0; x < W; x++) {
      float nx = (x + 0.5f) / W * 2 - 1, ny = ((y + 0.5f) / H * 2 - 1) * H / W;
      float r2 = nx * nx + ny * ny;
      // chromatic aberration: sample R and B radially shifted
      float c[3];
      for (int ch = 0; ch < 3; ch++) {
        float sc = 1.0f + (ch - 1) * 0.0016f * r2 * 1.8f;
        float fx = ((nx * sc) * 0.5f + 0.5f) * W - 0.5f, fy = ((ny * sc) * W / H * 0.5f + 0.5f) * H - 0.5f;
        fx = clampf(fx, 0, W - 1.001f); fy = clampf(fy, 0, H - 1.001f);
        int ix = (int)fx, iy = (int)fy; float ax = fx - ix, ay = fy - iy;
        const float* p = &img[((size_t)iy * W + ix) * 3 + ch];
        c[ch] = (p[0] * (1 - ax) + p[3] * ax) * (1 - ay) + (p[(size_t)W * 3] * (1 - ax) + p[(size_t)W * 3 + 3] * ax) * ay;
      }
      float bx = clampf((x + 0.5f) / ds - 0.5f, 0, w4 - 1.001f), by = clampf((y + 0.5f) / ds - 0.5f, 0, h4 - 1.001f);
      int ix = (int)bx, iy = (int)by; float ax = bx - ix, ay = by - iy;
      for (int ch = 0; ch < 3; ch++) {
        auto bil = [&](const float* B) {
          const float* p = &B[((size_t)iy * w4 + ix) * 3 + ch];
          return (p[0] * (1 - ax) + p[3] * ax) * (1 - ay) + (p[(size_t)w4 * 3] * (1 - ax) + p[(size_t)w4 * 3 + 3] * ax) * ay;
        };
        float v = c[ch] + 0.10f * bil(B1) + 0.07f * bil(B2);
        if (fog > 0) v = v * (1 - 0.55f * fog) + (bil(B3) * 0.9f + 0.10f) * fog * 0.75f;
        c[ch] = v;
      }
      float vig = 1.0f - 0.30f * r2 - 0.10f * r2 * r2;
      // eyelids (blink): curved dark lids closing from top and bottom
      float lid = 1.0f;
      if (PP.blink > 0) {
        float yy = (y + 0.5f) / H;
        float top = PP.blink * (0.60f - 0.10f * nx * nx), bot = 1.0f - PP.blink * (0.46f - 0.08f * nx * nx);
        lid = smooth(top - 0.02f, top + 0.10f, yy) * (1.0f - smooth(bot - 0.10f, bot + 0.02f, yy));
        lid *= 1.0f - 0.35f * PP.blink;
      }
      float g = vig * lid * PP.fade;
      float lumv = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
      rs = pcg(rs); float n1 = ((rs & 0xffff) + ((rs >> 16) & 0xffff)) * (1.0f / 65535.0f) - 1.0f;
      for (int ch = 0; ch < 3; ch++) {
        float v = lumv + (c[ch] - lumv) * 1.06f; // slight saturation
        v = tone(v * g);
        v += n1 * (0.004f + 0.006f * (1 - v));
        op[((size_t)y * W + x) * 3 + ch] = (uint8_t)clampf(v * 255.0f + 0.5f, 0, 255);
      }
    }
  });
}

int main(int argc, char** argv) {
  @autoreleasepool {
    int W = 960, H = 540, spp = 16, f0 = -1, f1 = -1, debug = 0, sppPass = 2, bands = 1, fstep = 1; const char* framesDir = nullptr;
    double tStill = 0; float fps = 30;
    const char* out = "out.ppm"; const char* seg = nullptr; const char* meter = nullptr; const char* expoFile = nullptr; const char* tlOut = nullptr;
    bool manual = false, top = false; V3 mc(1, 0.22f, 6), ml(3, 0.1f, 6.5f); float mfocus = 1.0f, topHalf = 10, exposure = 1.0f, aperture = -1;
    const char* shaderPath = "src/render.metal";
    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      auto nf = [&]() { return (float)atof(argv[++i]); };
      if (a == "--w") W = atoi(argv[++i]); else if (a == "--h") H = atoi(argv[++i]); else if (a == "--spp") spp = atoi(argv[++i]);
      else if (a == "--t") tStill = atof(argv[++i]); else if (a == "--out") out = argv[++i]; else if (a == "--seg") seg = argv[++i];
      else if (a == "--from") f0 = atoi(argv[++i]); else if (a == "--to") f1 = atoi(argv[++i]);
      else if (a == "--cam") { manual = true; mc.x = nf(); mc.y = nf(); mc.z = nf(); } else if (a == "--look") { ml.x = nf(); ml.y = nf(); ml.z = nf(); }
      else if (a == "--focus") mfocus = nf(); else if (a == "--top") { top = true; mc.x = nf(); mc.z = nf(); topHalf = nf(); }
      else if (a == "--debug") debug = atoi(argv[++i]); else if (a == "--meter") meter = argv[++i]; else if (a == "--expo") expoFile = argv[++i];
      else if (a == "--exp") exposure = nf(); else if (a == "--shader") shaderPath = argv[++i]; else if (a == "--pass") sppPass = atoi(argv[++i]);
      else if (a == "--bands") bands = atoi(argv[++i]); else if (a == "--step") fstep = atoi(argv[++i]); else if (a == "--frames") framesDir = argv[++i]; else if (a == "--timeline") tlOut = argv[++i]; else if (a == "--aperture") aperture = nf();
      else { fprintf(stderr, "unknown arg %s\n", argv[i]); return 1; }
    }
    double tb = nowSec();
    Scene sc; World wd;
    buildWorld(sc, wd);
    sc.finalize();
    Cat cat; cat.build(wd);
    fprintf(stderr, "world: %zu protos, %zu unique tris, %zu instances (%.1fM instanced tris), %zu blas nodes, %zu tlas nodes, build %.1fs\n", sc.protos.size(), sc.tris.size(), sc.ginst.size(), sc.instTris / 1e6, sc.nodes.size(), sc.tlas.size(), nowSec() - tb);
    if (tlOut) { cat.writeTimeline(tlOut, fps); if (f0 < 0 && !manual && !top && strcmp(out, "out.ppm") == 0) return 0; }

    id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
    NSError* err = nil;
    NSString* src = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:shaderPath] encoding:NSUTF8StringEncoding error:&err];
    if (!src) { fprintf(stderr, "cannot read shader %s\n", shaderPath); return 1; }
    MTLCompileOptions* opt = [MTLCompileOptions new];
    id<MTLLibrary> lib = [dev newLibraryWithSource:src options:opt error:&err];
    if (!lib) { fprintf(stderr, "shader compile error: %s\n", [[err description] UTF8String]); return 1; }
    id<MTLComputePipelineState> pso = [dev newComputePipelineStateWithFunction:[lib newFunctionWithName:@"render"] error:&err];
    if (!pso) { fprintf(stderr, "pipeline error: %s\n", [[err description] UTF8String]); return 1; }
    id<MTLCommandQueue> queue = [dev newCommandQueue];
    auto mkbuf = [&](const void* p, size_t n) { return [dev newBufferWithBytes:p length:std::max<size_t>(n, 64) options:MTLResourceStorageModeShared]; };
    id<MTLBuffer> bNodes = mkbuf(sc.nodes.data(), sc.nodes.size() * sizeof(BNode));
    id<MTLBuffer> bTris = mkbuf(sc.tris.data(), sc.tris.size() * sizeof(GTri));
    id<MTLBuffer> bAttr = mkbuf(sc.attrs.data(), sc.attrs.size() * sizeof(GAttr));
    id<MTLBuffer> bInst = mkbuf(sc.ginst.data(), sc.ginst.size() * sizeof(GInst));
    id<MTLBuffer> bTlas = mkbuf(sc.tlas.data(), sc.tlas.size() * sizeof(BNode));
    id<MTLBuffer> bMats = mkbuf(sc.mats.data(), sc.mats.size() * sizeof(GMat));
    const int DMAX = 1024;
    id<MTLBuffer> bDT = [dev newBufferWithLength:DMAX * 2 * sizeof(BNode) options:MTLResourceStorageModeShared];
    id<MTLBuffer> bD0 = [dev newBufferWithLength:DMAX * sizeof(GInst) options:MTLResourceStorageModeShared];
    id<MTLBuffer> bD1 = [dev newBufferWithLength:DMAX * sizeof(GInst) options:MTLResourceStorageModeShared];
    id<MTLBuffer> bUni = [dev newBufferWithLength:sizeof(Uni) options:MTLResourceStorageModeShared];
    MTLTextureDescriptor* td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:W height:H mipmapped:NO];
    td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite; td.storageMode = MTLStorageModeShared;
    id<MTLTexture> acc = [dev newTextureWithDescriptor:td];
    MTLTextureDescriptor* md = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:wd.mapW height:wd.mapH mipmapped:NO];
    md.usage = MTLTextureUsageShaderRead; md.storageMode = MTLStorageModeShared;
    id<MTLTexture> gmap = [dev newTextureWithDescriptor:md];
    [gmap replaceRegion:MTLRegionMake2D(0, 0, wd.mapW, wd.mapH) mipmapLevel:0 withBytes:wd.map.data() bytesPerRow:wd.mapW * 4];
    MTLTextureDescriptor* ld = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:wd.labW height:wd.labH mipmapped:NO];
    ld.usage = MTLTextureUsageShaderRead; ld.storageMode = MTLStorageModeShared;
    id<MTLTexture> labels = [dev newTextureWithDescriptor:ld];
    [labels replaceRegion:MTLRegionMake2D(0, 0, wd.labW, wd.labH) mipmapLevel:0 withBytes:wd.lab.data() bytesPerRow:wd.labW];

    // Kronecker alphas (R8 sequence)
    double phi = 1.0; for (int i = 0; i < 64; i++) phi = pow(1.0 + phi, 1.0 / 9.0);
    float kr[8]; for (int k = 0; k < 8; k++) kr[k] = (float)fmod(pow(1.0 / phi, k + 1), 1.0);

    std::vector<float> expo;
    if (expoFile) { FILE* f = fopen(expoFile, "r"); if (f) { float v; while (fscanf(f, "%f", &v) == 1) expo.push_back(v); fclose(f); } }
    FILE* pipe = nullptr; FILE* mf = meter ? fopen(meter, "w") : nullptr;
    if (seg) {
      char cmd[2048];
      snprintf(cmd, sizeof cmd, "ffmpeg -y -loglevel error -f rawvideo -pix_fmt rgb24 -s %dx%d -r %g -i - -c:v libx264 -preset slow -crf 15 -pix_fmt yuv420p -g 60 -keyint_min 60 -sc_threshold 0 -bf 2 -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv -vf scale=out_color_matrix=bt709:out_range=tv '%s.tmp.mp4'", W, H, fps, seg);
      pipe = popen(cmd, "w");
      if (!pipe) { fprintf(stderr, "cannot start ffmpeg\n"); return 1; }
    }
    bool still = (f0 < 0);
    if (still) { f0 = f1 = 0; }
    std::vector<float> accCPU((size_t)W * H * 4);
    std::vector<uint8_t> rgb;
    std::vector<InstCPU> d0, d1;
    for (int f = f0; f <= f1; f += fstep) {
      double t = still ? tStill : f / (double)fps;
      double shutter = 0.5 / fps;
      double tr0 = nowSec();
      Uni U; memset(&U, 0, sizeof U);
      CamPose c0 = cat.pose(t), c1 = cat.pose(t + shutter);
      if (manual || top) {
        V3 F = norm(ml - mc), R = norm(cross(F, V3(0, 1, 0))), Uv = cross(R, F);
        if (top) { mc.y = 30; F = V3(0, -1, 0); R = V3(1, 0, 0); Uv = V3(0, 0, 1); }
        c0.pos = mc; c0.R = R; c0.U = Uv; c0.F = F; c0.focus = mfocus; c1 = c0;
      }
      if (aperture >= 0) c0.aperture = c1.aperture = aperture;
      set3(U.camPos0, c0.pos, c0.focus); set3(U.camPos1, c1.pos, c1.focus);
      set3(U.camR0, c0.R); set3(U.camU0, c0.U); set3(U.camF0, c0.F); set3(U.camR1, c1.R); set3(U.camU1, c1.U); set3(U.camF1, c1.F);
      U.lens[0] = top ? topHalf : tanf(wd.hfov * 0.25f); U.lens[1] = top ? 0 : c0.aperture; U.lens[2] = (float)H / W; U.lens[3] = 0.72f;
      set3(U.sunDir, wd.sunDir, cosf(wd.sunRadius)); set3(U.sunCol, wd.sunCol, wd.skyGain);
      U.timeP[0] = (float)t; U.timeP[1] = (float)shutter; U.timeP[2] = wd.windAt(t); U.timeP[3] = wd.wet;
      set3(U.fogLo, wd.ghLo, wd.fogIn); set3(U.fogHi, wd.ghHi, wd.fogOut);
      U.water[0] = wd.waterY; U.water[1] = 1.0f; U.water[2] = G::GEAVE; U.water[3] = top ? 1 : 0;
      U.mapRect[0] = wd.mapX0; U.mapRect[1] = wd.mapZ0; U.mapRect[2] = 1.0f / (wd.mapW * wd.mapRes); U.mapRect[3] = 1.0f / (wd.mapH * wd.mapRes);
      memcpy(U.kr0, kr, 16); memcpy(U.kr1, kr + 4, 16);
      std::vector<Ripple> rp; cat.ripples(t, rp);
      int nr = std::min<int>((int)rp.size(), 16);
      for (int i = 0; i < nr; i++) { U.ripple[i][0] = rp[i].x; U.ripple[i][1] = rp[i].z; U.ripple[i][2] = rp[i].t0; U.ripple[i][3] = rp[i].amp; }
      U.counts[0] = (u32)f * 7919u + 13u; U.counts[3] = nr;
      U.dims[0] = W; U.dims[1] = H; U.dims[3] = debug;
      // dynamic instances
      d0.clear(); d1.clear();
      cat.dynamics(t, c0, sc, d0); cat.dynamics(t + shutter, c1, sc, d1);
      size_t nd = std::min(d0.size(), (size_t)DMAX);
      {
        std::vector<AABB> bx(nd);
        for (size_t i = 0; i < nd; i++) {
          float ex = (d0[i].flags & F_FLAP) ? 1.0f : 0.0f;
          AABB a = sc.instBox(d0[i]), b = sc.instBox(d1[i]);
          a.grow(b);
          if (ex > 0) { V3 c = (a.lo + a.hi) * 0.5f; float r = len(a.hi - a.lo) * 0.75f; a.lo = c - V3(r, r, r); a.hi = c + V3(r, r, r); }
          bx[i] = a;
        }
        std::vector<BNode> dn; std::vector<u32> ord;
        buildBVH(bx, dn, ord, 1, 30, false);
        GInst* g0 = (GInst*)[bD0 contents]; GInst* g1 = (GInst*)[bD1 contents];
        for (size_t i = 0; i < nd; i++) { g0[i] = sc.toG(d0[ord[i]]); g0[i].flags |= F_DYN; g1[i] = sc.toG(d1[ord[i]]); }
        memcpy([bDT contents], dn.data(), dn.size() * sizeof(BNode));
      }
      memset(accCPU.data(), 0, accCPU.size() * sizeof(float));
      [acc replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:accCPU.data() bytesPerRow:W * 16];
      int attempts = 0; bool ok = false;
      while (!ok && attempts < 4) {
        attempts++;
        bool gpuErr = false;
        int bandH = (H + bands - 1) / bands;
        for (int s = 0; s < spp && !gpuErr; s += sppPass) {
          for (int b = 0; b < bands && !gpuErr; b++) {
            U.counts[1] = s; U.counts[2] = std::min(sppPass, spp - s); U.dims[2] = b * bandH;
            // each dispatch gets its own uniform buffer snapshot
            id<MTLBuffer> ub = [dev newBufferWithBytes:&U length:sizeof(Uni) options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> cb = [queue commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            [enc setComputePipelineState:pso];
            [enc setTexture:acc atIndex:0]; [enc setTexture:gmap atIndex:1]; [enc setTexture:labels atIndex:2];
            [enc setBuffer:ub offset:0 atIndex:0]; [enc setBuffer:bNodes offset:0 atIndex:1]; [enc setBuffer:bTris offset:0 atIndex:2]; [enc setBuffer:bAttr offset:0 atIndex:3];
            [enc setBuffer:bInst offset:0 atIndex:4]; [enc setBuffer:bTlas offset:0 atIndex:5]; [enc setBuffer:bMats offset:0 atIndex:6];
            [enc setBuffer:bDT offset:0 atIndex:7]; [enc setBuffer:bD0 offset:0 atIndex:8]; [enc setBuffer:bD1 offset:0 atIndex:9];
            [enc dispatchThreadgroups:MTLSizeMake((W + 15) / 16, (std::min(bandH, H - b * bandH) + 15) / 16, 1) threadsPerThreadgroup:MTLSizeMake(16, 16, 1)];
            [enc endEncoding];
            [cb commit];
            [cb waitUntilCompleted];
            if (cb.error) { fprintf(stderr, "frame %d: GPU error %s\n", f, [[cb.error description] UTF8String]); gpuErr = true; }
          }
        }
        [acc getBytes:accCPU.data() bytesPerRow:W * 16 fromRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0];
        size_t bad = 0;
        if (!gpuErr) for (size_t i = 0; i < (size_t)W * H; i++) if (accCPU[i * 4 + 3] != (float)spp) bad++;
        if (gpuErr || bad) {
          fprintf(stderr, "frame %d: incomplete (%zu px), retry %d\n", f, bad, attempts);
          memset(accCPU.data(), 0, accCPU.size() * sizeof(float));
          [acc replaceRegion:MTLRegionMake2D(0, 0, W, H) mipmapLevel:0 withBytes:accCPU.data() bytesPerRow:W * 16];
          [NSThread sleepForTimeInterval:2.0];
        } else ok = true;
      }
      if (!ok) { fprintf(stderr, "frame %d FAILED\n", f); if (pipe) pclose(pipe); return 2; }
      PostP P; P.seed = f;
      P.exposure = exposure * ((expo.size() > (size_t)f && !still) ? expo[f] : 1.0f) * ((still && !expo.empty()) ? expo[std::min((size_t)(tStill * fps), expo.size() - 1)] : 1.0f);
      if (!manual && !top) { P.lensFog = cat.lensFog(t); P.blink = cat.blink(t); P.fade = cat.fade(t); }
      if (mf) {
        double s = 0, sw = 0;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
          const float* p = &accCPU[((size_t)y * W + x) * 4];
          float l = (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / spp;
          float nx = (x + 0.5f) / W * 2 - 1, ny = (y + 0.5f) / H * 2 - 1; float w = expf(-(nx * nx + ny * ny) * 1.2f);
          s += w * logf(fminf(l, 8.0f) + 0.02f); sw += w;
        }
        fprintf(mf, "%d %.5f\n", f, exp(s / sw)); fflush(mf);
      }
      postProcess(accCPU.data(), W, H, P, rgb);
      if (pipe) { fwrite(rgb.data(), 1, rgb.size(), pipe); }
      else if (framesDir) { char fn[1024]; snprintf(fn, sizeof fn, "%s/f_%05d.ppm", framesDir, f); FILE* fo = fopen(fn, "wb"); fprintf(fo, "P6\n%d %d\n255\n", W, H); fwrite(rgb.data(), 1, rgb.size(), fo); fclose(fo); }
      else if (!mf) { FILE* fo = fopen(out, "wb"); fprintf(fo, "P6\n%d %d\n255\n", W, H); fwrite(rgb.data(), 1, rgb.size(), fo); fclose(fo); }
      if (still || (f - f0) % 10 == 0) fprintf(stderr, "frame %d t=%.2f  %.2fs  (dyn %zu)\n", f, t, nowSec() - tr0, nd);
    }
    if (pipe) {
      int rc = pclose(pipe);
      if (rc != 0) { fprintf(stderr, "ffmpeg failed\n"); return 3; }
      std::string tmp = std::string(seg) + ".tmp.mp4";
      rename(tmp.c_str(), seg);
    }
    if (mf) fclose(mf);
  }
  return 0;
}
