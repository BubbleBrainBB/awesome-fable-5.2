// main.mm — 主机程序：构建世界、建立 Metal 计算管线、渲染帧
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <simd/simd.h>
#include <sys/stat.h>
#include "world.h"
#include "flight.h"

static std::string SRC = "src/";
static std::string readFile(const std::string& p){ FILE* f=fopen(p.c_str(),"rb"); if(!f){ fprintf(stderr,"cannot read %s\n",p.c_str()); exit(1);} std::string s; char buf[65536]; size_t n; while((n=fread(buf,1,sizeof buf,f))>0) s.append(buf,n); fclose(f); return s; }
static bool writePNG(const char* path,const uint8_t* rgba,int w,int h){
    CGColorSpaceRef cs=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGDataProviderRef dp=CGDataProviderCreateWithData(NULL,rgba,(size_t)w*h*4,NULL);
    CGImageRef img=CGImageCreate(w,h,8,32,w*4,cs,kCGImageAlphaNoneSkipLast|kCGBitmapByteOrderDefault,dp,NULL,false,kCGRenderingIntentDefault);
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(NULL,(const UInt8*)path,strlen(path),false);
    CGImageDestinationRef dst=CGImageDestinationCreateWithURL(url,CFSTR("public.png"),1,NULL);
    bool ok=false; if(dst){ CGImageDestinationAddImage(dst,img,NULL); ok=CGImageDestinationFinalize(dst); CFRelease(dst); }
    CFRelease(url); CGImageRelease(img); CGDataProviderRelease(dp); CGColorSpaceRelease(cs); return ok;
}

struct Renderer {
    id<MTLDevice> dev; id<MTLCommandQueue> q; id<MTLLibrary> lib;
    std::map<std::string,id<MTLComputePipelineState>> ps;
    id<MTLTexture> tH,tTreeA,tTreeCol,tTreeBase,tMaxTree,tAux,tSky,tShadow,tSunT,tCloud,tAcc,tOut,tB1a,tB1b,tB2a,tB2b;
    std::vector<id<MTLBuffer>> ubuf; int W=0,H=0;
    World* world=nullptr; Uni probeUni;

    id<MTLTexture> tex2d(MTLPixelFormat f,int w,int h,bool write=false){
        MTLTextureDescriptor* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:f width:w height:h mipmapped:NO];
        d.usage=MTLTextureUsageShaderRead|(write?MTLTextureUsageShaderWrite:0); d.storageMode=MTLStorageModeShared; return [dev newTextureWithDescriptor:d];
    }
    void upload(id<MTLTexture> t,const void* data,int w,int h,int bpp){ [t replaceRegion:MTLRegionMake2D(0,0,w,h) mipmapLevel:0 withBytes:data bytesPerRow:(NSUInteger)w*bpp]; }
    id<MTLComputePipelineState> pipe(const char* name){
        NSError* e=nil; id<MTLFunction> fn=[lib newFunctionWithName:[NSString stringWithUTF8String:name]];
        if(!fn){ fprintf(stderr,"no function %s\n",name); exit(1); }
        id<MTLComputePipelineState> p=[dev newComputePipelineStateWithFunction:fn error:&e];
        if(!p){ fprintf(stderr,"pipeline %s: %s\n",name,[[e localizedDescription] UTF8String]); exit(1);} return p;
    }
    bool init(World* w){
        world=w; dev=MTLCreateSystemDefaultDevice(); if(!dev){ fprintf(stderr,"no Metal device\n"); return false; } q=[dev newCommandQueue];
        std::string src="#include <metal_stdlib>\nusing namespace metal;\n";
        for(const char* f:{"shared.h","layout.h","common.metal","atmos.metal","scene.metal","hero.metal","render.metal"}) src+=readFile(SRC+f)+"\n";
        MTLCompileOptions* opt=[MTLCompileOptions new]; NSError* e=nil;
        lib=[dev newLibraryWithSource:[NSString stringWithUTF8String:src.c_str()] options:opt error:&e];
        if(!lib){ fprintf(stderr,"shader compile failed:\n%s\n",[[e localizedDescription] UTF8String]); return false; }
        for(const char* n:{"skyK","sunTK","shadowK","renderK","bloomDownK","blurK","postK","probeK"}) ps[n]=pipe(n);
        tH=tex2d(MTLPixelFormatR32Float,TH_N,TH_N); upload(tH,w->H.data(),TH_N,TH_N,4);
        tTreeA=tex2d(MTLPixelFormatRGBA8Unorm,TT_N,TT_N); upload(tTreeA,w->treeA.data(),TT_N,TT_N,4);
        tTreeCol=tex2d(MTLPixelFormatRGBA8Unorm,TT_N,TT_N); upload(tTreeCol,w->treeCol.data(),TT_N,TT_N,4);
        tTreeBase=tex2d(MTLPixelFormatR32Float,TT_N,TT_N); upload(tTreeBase,w->treeBase.data(),TT_N,TT_N,4);
        tMaxTree=tex2d(MTLPixelFormatR8Unorm,1024,1024); upload(tMaxTree,w->maxTree.data(),1024,1024,1);
        tAux=tex2d(MTLPixelFormatRGBA8Unorm,w->NL,w->NL); upload(tAux,w->aux.data(),w->NL,w->NL,4);
        tSky=tex2d(MTLPixelFormatRGBA32Float,256,128,true); tShadow=tex2d(MTLPixelFormatR32Float,1024,1024,true); tSunT=tex2d(MTLPixelFormatRGBA32Float,64,2,true);
        { MTLTextureDescriptor* d=[MTLTextureDescriptor new]; d.textureType=MTLTextureType3D; d.pixelFormat=MTLPixelFormatRGBA8Unorm; d.width=128; d.height=128; d.depth=128; d.usage=MTLTextureUsageShaderRead; d.storageMode=MTLStorageModeShared;
          tCloud=[dev newTextureWithDescriptor:d]; [tCloud replaceRegion:MTLRegionMake3D(0,0,0,128,128,128) mipmapLevel:0 slice:0 withBytes:w->cloud.data() bytesPerRow:128*4 bytesPerImage:128*128*4]; }
        for(int i=0;i<80;i++) ubuf.push_back([dev newBufferWithLength:sizeof(Uni) options:MTLResourceStorageModeShared]);
        return true;
    }
    void setSize(int w,int h){ if(w==W&&h==H) return; W=w; H=h;
        tAcc=tex2d(MTLPixelFormatRGBA32Float,w,h,true); tOut=tex2d(MTLPixelFormatRGBA8Unorm,w,h,true);
        int w1=(w+3)/4,h1=(h+3)/4,w2=(w+15)/16,h2=(h+15)/16;
        tB1a=tex2d(MTLPixelFormatRGBA16Float,w1,h1,true); tB1b=tex2d(MTLPixelFormatRGBA16Float,w1,h1,true);
        tB2a=tex2d(MTLPixelFormatRGBA16Float,w2,h2,true); tB2b=tex2d(MTLPixelFormatRGBA16Float,w2,h2,true); }
    void dispatch(id<MTLComputeCommandEncoder> enc,id<MTLComputePipelineState> p,int w,int h){
        NSUInteger tw=p.threadExecutionWidth, th=p.maxTotalThreadsPerThreadgroup/tw;
        [enc dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(tw,th,1)];
    }
    void setSceneTex(id<MTLComputeCommandEncoder> enc,int base){
        id<MTLTexture> t[]={tH,tTreeA,tTreeCol,tTreeBase,tMaxTree,tAux,tSky,tShadow,tSunT,tCloud};
        for(int i=0;i<10;i++) [enc setTexture:t[i] atIndex:base+i];
    }
    bool run(id<MTLCommandBuffer> cb,const char* what){ [cb commit]; [cb waitUntilCompleted]; if(cb.error){ fprintf(stderr,"GPU error in %s: %s\n",what,[[cb.error localizedDescription] UTF8String]); return false;} return true; }

    // 渲染一帧到 rgba（W*H*4）。返回 false 表示本帧不可信（GPU 报错或采样数不齐）。
    bool renderFrame(Uni u,int spp,uint8_t* rgba,double* meanLogLum=nullptr){
        u.res=simd_make_float4((float)W,(float)H,u.res.z,u.res.w); u.tp.w=(float)spp;
        memcpy(ubuf[0].contents,&u,sizeof(Uni));
        @autoreleasepool {
            id<MTLCommandBuffer> cb=[q commandBuffer]; id<MTLComputeCommandEncoder> enc=[cb computeCommandEncoder];
            [enc setComputePipelineState:ps["skyK"]]; [enc setTexture:tSky atIndex:0]; [enc setBuffer:ubuf[0] offset:0 atIndex:0]; dispatch(enc,ps["skyK"],256,128);
            [enc setComputePipelineState:ps["sunTK"]]; [enc setTexture:tSunT atIndex:0]; [enc setTexture:tSky atIndex:1]; [enc setBuffer:ubuf[0] offset:0 atIndex:0]; dispatch(enc,ps["sunTK"],64,2);
            [enc setComputePipelineState:ps["shadowK"]]; [enc setTexture:tShadow atIndex:0]; [enc setTexture:tH atIndex:1]; [enc setTexture:tTreeA atIndex:2]; [enc setBuffer:ubuf[0] offset:0 atIndex:0]; dispatch(enc,ps["shadowK"],1024,1024);
            [enc endEncoding]; if(!run(cb,"prepass")) return false;
            float wexp=0;
            for(int s=0;s<spp;s++){
                Uni us=u; us.tp.z=(float)s; memcpy(ubuf[1+s].contents,&us,sizeof(Uni));
                float r2x=fmodf(0.7548776662f*(s+1),1.0f), r2y=fmodf(0.5698402910f*(s+1),1.0f); float ox=(r2x-0.5f)*1.5f, oy=(r2y-0.5f)*1.5f; wexp+=expf(-2.2f*(ox*ox+oy*oy));
                id<MTLCommandBuffer> cb2=[q commandBuffer]; id<MTLComputeCommandEncoder> e2=[cb2 computeCommandEncoder];
                [e2 setComputePipelineState:ps["renderK"]]; [e2 setTexture:tAcc atIndex:0]; setSceneTex(e2,1); [e2 setBuffer:ubuf[1+s] offset:0 atIndex:0]; dispatch(e2,ps["renderK"],W,H);
                [e2 endEncoding]; if(!run(cb2,"render")) return false;
            }
            // 采样数校验：每个像素的权重和必须等于期望值
            { std::vector<float> acc((size_t)W*H*4); [tAcc getBytes:acc.data() bytesPerRow:(NSUInteger)W*16 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
              double sl=0; size_t bad=0; for(size_t i=0;i<(size_t)W*H;i++){ float a=acc[i*4+3]; if(fabsf(a-wexp)>1e-3f*wexp) bad++; float l=(0.2126f*acc[i*4]+0.7152f*acc[i*4+1]+0.0722f*acc[i*4+2])/std::max(a,1e-6f); sl+=log(std::max(l,1e-4f)); }
              if(meanLogLum) *meanLogLum=sl/((double)W*H);
              if(bad){ fprintf(stderr,"sample-count check failed on %zu pixels\n",bad); return false; } }
            id<MTLCommandBuffer> cb3=[q commandBuffer]; id<MTLComputeCommandEncoder> e3=[cb3 computeCommandEncoder];
            Uni ub=u; ub.dbg=simd_make_float4(0,0,0,0); memcpy(ubuf[70].contents,&ub,sizeof(Uni)); ub.dbg.y=1; memcpy(ubuf[71].contents,&ub,sizeof(Uni)); ub.dbg=simd_make_float4(1,0,0,0); memcpy(ubuf[72].contents,&ub,sizeof(Uni));
            auto down=[&](id<MTLTexture> s,id<MTLTexture> d,int ui){ [e3 setComputePipelineState:ps["bloomDownK"]]; [e3 setTexture:s atIndex:0]; [e3 setTexture:d atIndex:1]; [e3 setBuffer:ubuf[ui] offset:0 atIndex:0]; dispatch(e3,ps["bloomDownK"],(int)d.width,(int)d.height); };
            auto blur=[&](id<MTLTexture> s,id<MTLTexture> d,int ui){ [e3 setComputePipelineState:ps["blurK"]]; [e3 setTexture:s atIndex:0]; [e3 setTexture:d atIndex:1]; [e3 setBuffer:ubuf[ui] offset:0 atIndex:0]; dispatch(e3,ps["blurK"],(int)d.width,(int)d.height); };
            down(tAcc,tB1a,70); blur(tB1a,tB1b,70); blur(tB1b,tB1a,71);
            down(tB1a,tB2a,72); blur(tB2a,tB2b,70); blur(tB2b,tB2a,71); blur(tB2a,tB2b,70); blur(tB2b,tB2a,71);
            [e3 setComputePipelineState:ps["postK"]]; [e3 setTexture:tAcc atIndex:0]; [e3 setTexture:tB1a atIndex:1]; [e3 setTexture:tB2a atIndex:2]; [e3 setTexture:tOut atIndex:3]; [e3 setBuffer:ubuf[0] offset:0 atIndex:0]; dispatch(e3,ps["postK"],W,H);
            [e3 endEncoding]; if(!run(cb3,"post")) return false;
            [tOut getBytes:rgba bytesPerRow:(NSUInteger)W*4 fromRegion:MTLRegionMake2D(0,0,W,H) mipmapLevel:0];
        }
        return true;
    }
    // 探针：对一批点求 (含细节地形高, 场景距离, 水位, 材质)
    std::vector<simd_float4> probe(const std::vector<simd_float4>& pts){
        std::vector<simd_float4> out(pts.size()); if(pts.empty()) return out;
        Uni u=probeUni; memcpy(ubuf[0].contents,&u,sizeof u);
        id<MTLBuffer> bi=[dev newBufferWithBytes:pts.data() length:pts.size()*16 options:MTLResourceStorageModeShared];
        id<MTLBuffer> bo=[dev newBufferWithLength:pts.size()*16 options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> cb=[q commandBuffer]; id<MTLComputeCommandEncoder> enc=[cb computeCommandEncoder];
        [enc setComputePipelineState:ps["probeK"]]; setSceneTex(enc,0); [enc setBuffer:ubuf[0] offset:0 atIndex:0]; [enc setBuffer:bi offset:0 atIndex:1]; [enc setBuffer:bo offset:0 atIndex:2];
        [enc dispatchThreads:MTLSizeMake(pts.size(),1,1) threadsPerThreadgroup:MTLSizeMake(ps["probeK"].threadExecutionWidth,1,1)];
        [enc endEncoding]; run(cb,"probe"); memcpy(out.data(),bo.contents,pts.size()*16); return out;
    }
};

void Flight::check(Renderer& R){
    // 沿航线每 1/30 s 取一点，用 GPU 探针求场景距离（含地形细节、树、水），报告过近的地方
    std::vector<simd_float4> pts; std::vector<double> ts;
    for(double t=0;t<T_END;t+=1.0/30.0){ FState s; state(t,s); pts.push_back(simd_make_float4((float)s.pos.x,(float)s.pos.y,(float)s.pos.z,0)); ts.push_back(t); }
    auto r=R.probe(pts); int sec=-1; float mn=1e9; double worstT=0; float worst=1e9;
    for(size_t i=0;i<pts.size();i++){ int sc=(int)ts[i]; if(sc!=sec){ if(sec>=0) fprintf(stderr,"t=%2d s  min clearance %.2f m  (y=%.1f)\n",sec,mn,pts[i-1].y); sec=sc; mn=1e9; }
        float d=r[i].y; float aw=pts[i].y-std::max(r[i].x,r[i].z); d=std::min(d,aw); mn=std::min(mn,d); if(ts[i]<T_LAND-0.5 && d<worst){ worst=d; worstT=ts[i]; } }
    fprintf(stderr,"worst clearance before landing: %.2f m at t=%.2f\n",worst,worstT);
}
static double nowSec(){ return [[NSDate date] timeIntervalSince1970]; }

int main(int argc,char** argv){ @autoreleasepool {
    std::vector<std::string> a(argv+1,argv+argc);
    auto has=[&](const char* k){ for(auto& s:a) if(s==k) return true; return false; };
    auto getf=[&](const char* k,int idx,double def){ for(size_t i=0;i<a.size();i++) if(a[i]==k && i+1+idx<a.size()) return atof(a[i+1+idx].c_str()); return def; };
    auto gets=[&](const char* k,const char* def){ for(size_t i=0;i<a.size();i++) if(a[i]==k && i+1<a.size()) return a[i+1]; return std::string(def); };
    if(a.empty()){ fprintf(stderr,"usage: bird still|seq|path ...\n"); return 1; }
    std::string mode=a[0];
    World world; double t0=nowSec();
    if(!world.build("work/terrain.bin","work/world.bin")) return 1;
    fprintf(stderr,"world ready (%.1fs)\n",nowSec()-t0);
    Flight fl; fl.build(world);
    if(mode=="path"){ fl.dump(gets("--out","work/flight.csv").c_str()); if(!has("--check")) return 0; }
    Renderer R; t0=nowSec(); if(!R.init(&world)) return 1; fprintf(stderr,"gpu ready (%.1fs)\n",nowSec()-t0);
    int W=(int)getf("--res",0,960), H=(int)getf("--res",1,540); int spp=(int)getf("--spp",0,4); R.setSize(W,H);
    double fps=getf("--fps",0,60.0); double shutter=getf("--shutter",0,0.5);
    int quality=(int)getf("--q",0,1);
    { memset(&R.probeUni,0,sizeof(Uni)); fl.defaults(R.probeUni,0); R.probeUni.flags.z=1; }
    if(mode=="path" && has("--check")){ fl.check(R); return 0; }
    std::vector<uint8_t> img((size_t)W*H*4);
    if(mode=="still"){
        Uni u; memset(&u,0,sizeof u);
        std::string out=gets("--out","work/dbg/still.png");
        if(has("--t")){ double t=getf("--t",0,0); fl.frameUni(u,t,t+shutter/fps); u.tp.y=(t<fl.T_LAND)?(float)llround(t*fps):7.0f; }
        else {
            float x=getf("--pos",0,0),y=getf("--pos",1,100),z=getf("--pos",2,0); float yaw=getf("--look",0,0)*M_PI/180, pit=getf("--look",1,0)*M_PI/180; float fov=getf("--fov",0,80)*M_PI/180;
            if(has("--rv")){ x=getf("--rv",0,1500); z=riverZ(x)+getf("--rv",2,0); y=riverBed(x)+riverDepth(x)+getf("--rv",1,2); fprintf(stderr,"river cam at %.1f %.1f %.1f\n",x,y,z); }
            Cam c=makeCam(simd_make_float3(x,y,z),yaw,pit,getf("--roll",0,0)*M_PI/180,fov); u.cam0=c; u.cam1=c;
            fl.defaults(u,getf("--time",0,0)); u.lens=simd_make_float4(50,0,0.25f,1);
        }
        if(has("--sun")){ float el=getf("--sun",0,7)*M_PI/180, az=getf("--sun",1,0)*M_PI/180; u.sunDir=simd_make_float4(-cosf(el)*cosf(az),sinf(el),cosf(el)*sinf(az),0); }
        if(has("--exp")) u.res.z=getf("--exp",0,1);
        u.flags.x=(float)quality; u.flags.w=getf("--dbg",0,0);
        double tr=nowSec(); double ll; bool ok=R.renderFrame(u,spp,img.data(),&ll); double dt=nowSec()-tr;
        fprintf(stderr,"frame %s in %.2fs  meanLogLum=%.3f\n",ok?"ok":"FAILED",dt,ll);
        writePNG(out.c_str(),img.data(),W,H); return ok?0:2;
    }
    if(mode=="seq"){
        // 渲染帧区间 [f0,f1) → 通过管道交给 ffmpeg 编码成一个分段；每 keep 帧另存一张 PNG
        int f0=(int)getf("--frames",0,0), f1=(int)getf("--frames",1,60); std::string out=gets("--out","work/seg/seg.mp4"); int keep=(int)getf("--keep",0,0);
        std::string keepDir=gets("--keepdir","work/keys"); int crf=(int)getf("--crf",0,12);
        std::string lumPath=gets("--lum","");
        char cmd[2048]; snprintf(cmd,sizeof cmd,"ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgba -s %dx%d -r %g -i - -c:v libx264 -preset medium -crf %d -pix_fmt yuv420p -colorspace bt709 -color_primaries bt709 -color_trc bt709 '%s.tmp.mp4'",W,H,fps,crf,out.c_str());
        FILE* pp=popen(cmd,"w"); if(!pp){ fprintf(stderr,"cannot start ffmpeg\n"); return 1; }
        FILE* lf=lumPath.empty()?nullptr:fopen(lumPath.c_str(),"a");
        double tr=nowSec();
        for(int f=f0;f<f1;f++){
            Uni u; memset(&u,0,sizeof u); double t=f/fps; fl.frameUni(u,t,t+shutter/fps); u.flags.x=(float)quality; u.tp.y=(t<fl.T_LAND)?(float)f:7.0f;   // 停栖后抖动图案固定，静止画面不沸腾
            if(has("--exp")) u.res.z=getf("--exp",0,1);
            bool ok=false; double ll=0; for(int attempt=0;attempt<3 && !ok;attempt++){ ok=R.renderFrame(u,spp,img.data(),&ll); if(!ok) fprintf(stderr,"frame %d retry %d\n",f,attempt+1); }
            if(!ok){ fprintf(stderr,"frame %d failed permanently\n",f); pclose(pp); return 2; }
            fwrite(img.data(),1,img.size(),pp);
            if(lf) fprintf(lf,"%d %.5f\n",f,ll);
            if(keep>0 && f%keep==0){ char kp[1024]; snprintf(kp,sizeof kp,"%s/f%05d.png",keepDir.c_str(),f); writePNG(kp,img.data(),W,H); }
        }
        if(lf) fclose(lf);
        int rc=pclose(pp); if(rc!=0){ fprintf(stderr,"ffmpeg failed\n"); return 3; }
        std::string tmp=out+".tmp.mp4"; rename(tmp.c_str(),out.c_str());
        fprintf(stderr,"seg %d-%d done in %.1fs (%.2fs/frame)\n",f0,f1,nowSec()-tr,(nowSec()-tr)/(f1-f0));
        return 0;
    }
    return 0;
} }
