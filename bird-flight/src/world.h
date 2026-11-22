// world.h — 主机端世界构建：高分辨率高度图、AO、种树、云噪声
#pragma once
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <dispatch/dispatch.h>
#include "shared.h"
#include "layout.h"

static const uint32_t WSEED = 20261117u;
static inline uint32_t w_hu(uint32_t x){ x^=x>>16; x*=0x7feb352dU; x^=x>>15; x*=0x846ca68bU; x^=x>>16; return x; }
static inline uint32_t w_h2(int x,int y,uint32_t s){ return w_hu((uint32_t)x*0x9E3779B1u ^ w_hu((uint32_t)y*0x85EBCA77u ^ s)); }
static inline uint32_t w_h3(int x,int y,int z,uint32_t s){ return w_hu((uint32_t)x*0x9E3779B1u ^ w_hu((uint32_t)y*0x85EBCA77u ^ w_hu((uint32_t)z*0xC2B2AE3Du ^ s))); }
static inline float w_u(uint32_t h){ return (h>>8)*(1.0f/16777216.0f); }
static float w_vn(float x,float y,uint32_t s){
    float fx0=floorf(x), fy0=floorf(y); int ix=(int)fx0, iy=(int)fy0; float fx=x-fx0, fy=y-fy0;
    fx=fx*fx*fx*(fx*(fx*6-15)+10); fy=fy*fy*fy*(fy*(fy*6-15)+10);
    float a=w_u(w_h2(ix,iy,s)), b=w_u(w_h2(ix+1,iy,s)), c=w_u(w_h2(ix,iy+1,s)), d=w_u(w_h2(ix+1,iy+1,s));
    return a+(b-a)*fx+((c+(d-c)*fx)-(a+(b-a)*fx))*fy;
}
static float w_fbm(float x,float y,int oct,uint32_t s){ float a=0.5f,sum=0,f=1; for(int i=0;i<oct;i++){ sum+=a*w_vn(x*f,y*f,s+i*101u); a*=0.5f; f*=2.03f; } return sum/(1.0f-a*2.0f); }
static float w_ridged(float x,float y,int oct,uint32_t s){ float a=0.5f,sum=0,f=1,n=0; for(int i=0;i<oct;i++){ float v=1.0f-fabsf(w_vn(x*f,y*f,s+i*57u)*2.0f-1.0f); sum+=a*v*v; n+=a; a*=0.5f; f*=2.1f; } return sum/n; }

template<class F> static void parallel_rows(int n, F f){
    dispatch_apply((size_t)n, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^(size_t j){ f((int)j); });
}

struct World {
    int NL=0;                       // 低分辨率（侵蚀输出）
    std::vector<float> hl, flow;    // NL²
    std::vector<float> H;           // TH_N²
    std::vector<uint8_t> aux;       // NL² RGBA: flow, AO, 0, 0
    std::vector<uint8_t> treeA, treeCol; // TT_N² RGBA
    std::vector<float> treeBase;    // TT_N²
    std::vector<uint8_t> maxTree;   // 1024²
    std::vector<uint8_t> cloud;     // 128³ RGBA

    float lowAt(int i,int j) const { i=std::min(std::max(i,0),NL-1); j=std::min(std::max(j,0),NL-1); return hl[(size_t)j*NL+i]; }
    float lowBicubic(float gx,float gy) const {   // Catmull-Rom
        float fx0=floorf(gx), fy0=floorf(gy); int ix=(int)fx0, iy=(int)fy0; float fx=gx-fx0, fy=gy-fy0;
        float wx[4]={ -0.5f*fx+fx*fx-0.5f*fx*fx*fx, 1.0f-2.5f*fx*fx+1.5f*fx*fx*fx, 0.5f*fx+2.0f*fx*fx-1.5f*fx*fx*fx, -0.5f*fx*fx+0.5f*fx*fx*fx };
        float wy[4]={ -0.5f*fy+fy*fy-0.5f*fy*fy*fy, 1.0f-2.5f*fy*fy+1.5f*fy*fy*fy, 0.5f*fy+2.0f*fy*fy-1.5f*fy*fy*fy, -0.5f*fy*fy+0.5f*fy*fy*fy };
        float s=0; for(int b=0;b<4;b++){ float r=0; for(int a=0;a<4;a++) r+=wx[a]*lowAt(ix-1+a,iy-1+b); s+=wy[b]*r; } return s;
    }
    // 高分辨率双线性（不含河道刻蚀）
    float Hraw(float x,float z) const {
        float gx=(x-WORLD_X0)/WORLD_SIZE*TH_N-0.5f, gy=(z-WORLD_Z0)/WORLD_SIZE*TH_N-0.5f;
        int ix=(int)floorf(gx), iy=(int)floorf(gy); ix=std::min(std::max(ix,0),TH_N-2); iy=std::min(std::max(iy,0),TH_N-2);
        float fx=std::min(std::max(gx-ix,0.0f),1.0f), fy=std::min(std::max(gy-iy,0.0f),1.0f);
        const float* p=&H[(size_t)iy*TH_N+ix];
        return (p[0]+(p[1]-p[0])*fx)*(1-fy)+(p[TH_N]+(p[TH_N+1]-p[TH_N])*fx)*fy;
    }
    float Hfull(float x,float z) const { return carveH(Hraw(x,z),x,z); }
    float ground(float x,float z) const { float h=Hfull(x,z); float wl=waterLevel(x,z); if(x>DAM_X-80.0f&&x<RIVER_X0+40.0f) wl=std::max(wl,0.0f); return std::max(h,wl); }

    bool load(const char* terrainPath){
        FILE* f=fopen(terrainPath,"rb"); if(!f){ fprintf(stderr,"no terrain file %s\n",terrainPath); return false; }
        int n; fread(&n,4,1,f); NL=n; hl.resize((size_t)n*n); flow.resize((size_t)n*n);
        fread(hl.data(),4,(size_t)n*n,f); fread(flow.data(),4,(size_t)n*n,f); fclose(f); return true;
    }
    void buildHeights(){
        H.resize((size_t)TH_N*TH_N); float sc=(float)NL/TH_N;
        parallel_rows(TH_N,[&](int j){ for(int i=0;i<TH_N;i++){ H[(size_t)j*TH_N+i]=lowBicubic((i+0.5f)*sc-0.5f,(j+0.5f)*sc-0.5f); } });
        // 坡度相关的中尺度细节（岩坡粗糙、层状台阶；缓地只有轻微起伏）
        std::vector<float> D((size_t)TH_N*TH_N); float dx=WORLD_SIZE/TH_N;
        parallel_rows(TH_N,[&](int j){ for(int i=0;i<TH_N;i++){
            int i0=std::max(i-2,0), i1=std::min(i+2,TH_N-1), j0=std::max(j-2,0), j1=std::min(j+2,TH_N-1);
            float gx=(H[(size_t)j*TH_N+i1]-H[(size_t)j*TH_N+i0])/((i1-i0)*dx), gz=(H[(size_t)j1*TH_N+i]-H[(size_t)j0*TH_N+i])/((j1-j0)*dx);
            float sl=sqrtf(gx*gx+gz*gz); float h=H[(size_t)j*TH_N+i];
            float x=WORLD_X0+(i+0.5f)*dx, z=WORLD_Z0+(j+0.5f)*dx;
            float rock=l_ss(0.62f,1.05f,sl+0.25f*(w_vn(x/210.0f,z/210.0f,7u)-0.5f));
            float d=rock*(9.0f*(w_ridged(x/75.0f,z/75.0f,4,11u)-0.45f)+2.2f*(w_fbm(x/19.0f,z/19.0f,3,12u)-0.5f));
            // 较大的肋状起伏，打破规则的坡面
            d+=l_ss(0.35f,0.8f,sl)*26.0f*(w_ridged(x/260.0f+3.0f,z/260.0f,3,16u)-0.5f);
            // 断续的层状崖阶：层厚与强度都随位置变化
            float L=9.0f+12.0f*w_vn(x/700.0f,z/700.0f,17u); float tt=(h+0.16f*x-0.11f*z+30.0f*w_vn(x/300.0f,z/300.0f,13u))/L; float ft=tt-floorf(tt);
            float stp=l_ss(0.45f,0.75f,w_vn(x/330.0f+floorf(tt)*3.7f,z/330.0f,18u));
            d+=rock*L*0.38f*stp*(l_ss(0.2f,0.8f,ft)-ft);
            d+=(1.0f-rock)*(1.3f*(w_fbm(x/46.0f,z/46.0f,4,14u)-0.5f)+0.35f*(w_fbm(x/9.0f,z/9.0f,2,15u)-0.5f));
            // 湖区与河廊：压低细节幅度，保持岸线与河岸设计
            float damp=1.0f;
            if(x>DAM_X-300.0f&&x<RIVER_X0+300.0f) damp=std::min(damp,l_ss(-1.0f,6.0f,h)*0.9f+0.1f);
            if(x>RIVER_X0-100.0f&&x<7100.0f){ float rd=fabsf(riverDist(x,z)); damp=std::min(damp,0.12f+0.88f*l_ss(15.0f,90.0f,rd)); }
            D[(size_t)j*TH_N+i]=d*damp; } });
        parallel_rows(TH_N,[&](int j){ for(int i=0;i<TH_N;i++) H[(size_t)j*TH_N+i]+=D[(size_t)j*TH_N+i]; });
    }
    void buildAux(){
        int n=NL; aux.assign((size_t)n*n*4,0); float dx=WORLD_SIZE/n;
        parallel_rows(n,[&](int j){ for(int i=0;i<n;i++){
            float h0=hl[(size_t)j*n+i]; float occ=0; const int ND=8;
            for(int d=0;d<ND;d++){ float a=(d+0.5f)/ND*6.2831853f; float ca=cosf(a), sa=sinf(a); float mt=0; float s=14.0f;
                for(int k=0;k<9;k++){ float fx=i+ca*s/dx, fy=j+sa*s/dx; int ix=(int)fx, iy=(int)fy; if(ix<0||iy<0||ix>=n-1||iy>=n-1) break;
                    float tx=fx-ix, ty=fy-iy; const float* p=&hl[(size_t)iy*n+ix];
                    float hh=(p[0]+(p[1]-p[0])*tx)*(1-ty)+(p[n]+(p[n+1]-p[n])*tx)*ty; mt=std::max(mt,(hh-h0)/s); s*=1.9f; }
                occ+=mt/sqrtf(1.0f+mt*mt); }
            float ao=1.0f-occ/ND; ao=powf(std::max(ao,0.0f),1.3f);
            uint8_t* q=&aux[((size_t)j*n+i)*4];
            q[0]=(uint8_t)std::min(255.0f,std::max(0.0f,(flow[(size_t)j*n+i]-2.0f)/6.0f*255.0f)); q[1]=(uint8_t)(std::min(1.0f,ao)*255.0f); q[2]=0; q[3]=255; } });
    }
    void buildTrees(){
        treeA.assign((size_t)TT_N*TT_N*4,0); treeCol.assign((size_t)TT_N*TT_N*4,0); treeBase.assign((size_t)TT_N*TT_N,0.0f);
        parallel_rows(TT_N,[&](int j){ for(int i=0;i<TT_N;i++){
            uint32_t hs=w_h2(i,j,WSEED); float jx=w_u(hs), jz=w_u(w_hu(hs+1u));
            uint8_t bjx=(uint8_t)(jx*255.0f), bjz=(uint8_t)(jz*255.0f);
            float x=WORLD_X0+(i+0.5f+((bjx/255.0f)-0.5f)*TREE_JIT)*TREE_CELL, z=WORLD_Z0+(j+0.5f+((bjz/255.0f)-0.5f)*TREE_JIT)*TREE_CELL;
            float y=Hfull(x,z); float e=4.0f;
            float gx=(Hfull(x+e,z)-Hfull(x-e,z))/(2*e), gz=(Hfull(x,z+e)-Hfull(x,z-e))/(2*e); float sl=sqrtf(gx*gx+gz*gz);
            float r1=w_u(w_hu(hs+2u)), r2=w_u(w_hu(hs+3u)), r3=w_u(w_hu(hs+4u)), r4=w_u(w_hu(hs+5u)), r5=w_u(w_hu(hs+6u));
            float nA=w_fbm(x/900.0f,z/900.0f,3,21u), nM=w_fbm(x/420.0f,z/420.0f,3,22u), nS=w_vn(x/240.0f,z/240.0f,23u), nP=w_vn(x/70.0f,z/70.0f,24u);
            float tl=1230.0f+330.0f*(nA-0.5f);
            float dens=1.0f-l_ss(tl-260.0f,tl,y);
            dens*=1.0f-l_ss(0.80f,1.20f,sl);
            // 林间空地/草甸：谷底与缓坡更多
            float meadow=l_ss(0.50f,0.62f,nM+0.10f*l_ss(0.35f,0.05f,sl)-0.06f);
            dens*=1.0f-meadow;
            bool lakeRange=(x>DAM_X-150.0f&&x<RIVER_X0+60.0f);
            if(lakeRange && y<1.6f+1.5f*nP) dens=0;
            if(y<waterLevel(x,z)+0.5f) dens=0;
            float rd=1e9f; bool riv=(x>RIVER_X0-60.0f&&x<7050.0f);
            if(riv){ rd=fabsf(riverDist(x,z)); float w=riverHalfW(x); if(rd<w+2.6f) dens=0;
                // 峡谷陡壁上不长树（刻蚀后的坡度已体现在 sl 中）
                else if(rd<w+9.0f && x>RIVER_X0 && x<GORGE_X0) dens=std::max(dens,0.9f*(sl<0.9f?1.0f:0.0f));   // 河岸林带
            }
            if(x<-3600.0f) dens*=l_ss(0.42f,0.58f,nM*0.6f+nS*0.4f+0.08f);   // 西部低地：斑块状
            { // 瀑布顶、北缘与栖树周围：裸岩草坡，不长林木（保证盘旋空域、进近航线与枝头视野）
              float zf=riverZ(FALL_X); float d1=sqrtf((x-(FALL_X+16.0f))*(x-(FALL_X+16.0f))+(z-(zf-26.0f))*(z-(zf-26.0f)));
              float hx=HERO_X, hz=riverZ(HERO_X-2.0f)-24.0f; float d2=sqrtf((x-hx)*(x-hx)+(z-hz)*(z-hz));
              if(d1<66.0f+10.0f*nP) dens=0; if(d2<40.0f+8.0f*nP) dens=0;
              if(x>hx-20.0f && x<FALL_X+40.0f && z>riverZ(x)-78.0f-10.0f*nP && z<riverZ(x)-4.0f) dens=0;
              if(x<hx+5.0f && x>hx-120.0f && fabsf(z-hz)<30.0f+0.9f*(hx-x) && y>24.0f+8.0f*nP) dens=0; }
            uint8_t* A=&treeA[((size_t)j*TT_N+i)*4]; uint8_t* C=&treeCol[((size_t)j*TT_N+i)*4];
            A[2]=bjx; A[3]=bjz;
            // 无树处的颜色：给远景冠层过渡用的地面近似色
            C[0]=72; C[1]=81; C[2]=44; C[3]=(uint8_t)(r5*255.0f);
            if(r1>=dens) continue;
            // 树种
            float ee=y+140.0f*(nS-0.5f)*2.0f; int sp; float pick=r2*0.75f+nP*0.25f;
            if(sl>0.72f) sp=(pick<0.6f)?1:((ee>900.0f)?4:0);
            else if(riv && rd<45.0f && x<GORGE_X0+100.0f) sp=(pick<0.55f)?2:((pick<0.9f)?3:0);
            else if(ee<240.0f) sp=(pick<0.52f)?2:((pick<0.86f)?3:0);
            else if(ee<620.0f) sp=(pick<0.42f)?0:((pick<0.70f)?2:((pick<0.88f)?3:1));
            else if(ee<1000.0f) sp=(pick<0.72f)?0:((pick<0.90f)?4:1);
            else sp=(pick<0.55f)?4:((pick<0.92f)?0:1);
            if(r3<0.014f) sp=5;
            float hb; switch(sp){ case 0: hb=24.0f; break; case 1: hb=14.5f; break; case 2: hb=18.0f; break; case 3: hb=15.5f; break; case 4: hb=15.0f; break; default: hb=11.0f; }
            float H=hb*(0.58f+0.75f*r4*r4+0.12f*nP)*l_mix(1.0f,0.45f,l_ss(tl-380.0f,tl,y));
            H*=0.75f+0.25f*l_sat(dens*2.0f);
            H=std::min(std::max(H,3.5f),36.0f);
            // 颜色（线性反照率）
            float cr,cg,cb; float v=0.8f+0.4f*r5;
            if(sp==0){ cr=0.026f; cg=0.050f; cb=0.026f; }
            else if(sp==1){ cr=0.045f; cg=0.068f; cb=0.028f; }
            else if(sp==2){ // 阔叶：成片变色（同一片林子色相接近），单株只带小的偏差；连续色带而不是几档
                float au=(0.30f*r5+0.45f*nP+0.25f*nS-0.5f)*2.3f+0.5f+0.10f*l_sat(y/500.0f); au=l_sat(au);
                static const float pk[5]={0.0f,0.28f,0.52f,0.78f,1.0f};
                static const float pc[5][3]={{0.060f,0.105f,0.026f},{0.17f,0.20f,0.030f},{0.31f,0.25f,0.030f},{0.33f,0.18f,0.024f},{0.27f,0.075f,0.022f}};
                int k=0; while(k<3 && au>pk[k+1]) k++; float f=(au-pk[k])/(pk[k+1]-pk[k]); f=std::min(std::max(f,0.0f),1.0f);
                cr=pc[k][0]+(pc[k+1][0]-pc[k][0])*f; cg=pc[k][1]+(pc[k+1][1]-pc[k][1])*f; cb=pc[k][2]+(pc[k+1][2]-pc[k][2])*f; }
            else if(sp==3){ float au=l_sat((0.45f*r5+0.55f*nP-0.5f)*2.0f+0.62f); cr=0.16f+0.26f*au; cg=0.20f+0.13f*au; cb=0.035f; }
            else if(sp==4){ cr=0.36f; cg=0.245f; cb=0.04f; }
            else { cr=0.30f; cg=0.28f; cb=0.25f; }
            cr*=v; cg*=v; cb*=v;
            A[0]=(uint8_t)std::min(255.0f,H/40.0f*255.0f+0.5f); A[1]=(uint8_t)(sp | (((int)(r5*31.0f))<<3));
            // 颜色以 sqrt 编码存 8 位，着色器里平方还原
            C[0]=(uint8_t)(sqrtf(cr)*255.0f); C[1]=(uint8_t)(sqrtf(cg)*255.0f); C[2]=(uint8_t)(sqrtf(cb)*255.0f);
            treeBase[(size_t)j*TT_N+i]=y-0.25f;
        } });
        // 最大树高（1024²，带膨胀）
        const int M=1024; maxTree.assign((size_t)M*M,0); std::vector<uint8_t> tmp((size_t)M*M,0); int f=TT_N/M;
        parallel_rows(M,[&](int j){ for(int i=0;i<M;i++){ uint8_t m=0; for(int b=0;b<f;b++) for(int a=0;a<f;a++) m=std::max(m,treeA[(((size_t)(j*f+b))*TT_N+i*f+a)*4]); tmp[(size_t)j*M+i]=m; } });
        parallel_rows(M,[&](int j){ for(int i=0;i<M;i++){ uint8_t m=0; for(int b=-1;b<=1;b++) for(int a=-1;a<=1;a++){ int x=std::min(std::max(i+a,0),M-1), y=std::min(std::max(j+b,0),M-1); m=std::max(m,tmp[(size_t)y*M+x]); } maxTree[(size_t)j*M+i]=m; } });
    }
    // ---- 云噪声（可平铺）----
    static float pgrad(int ix,int iy,int iz,float fx,float fy,float fz,int P,uint32_t s){
        uint32_t h=w_h3(((ix%P)+P)%P,((iy%P)+P)%P,((iz%P)+P)%P,s); float u=w_u(h)*2-1, a=w_u(w_hu(h+1u))*6.2831853f; float r=sqrtf(1-u*u);
        return r*cosf(a)*fx+r*sinf(a)*fy+u*fz;
    }
    static float perlin3(float x,float y,float z,int P,uint32_t s){
        float x0=floorf(x),y0=floorf(y),z0=floorf(z); int ix=(int)x0,iy=(int)y0,iz=(int)z0; float fx=x-x0,fy=y-y0,fz=z-z0;
        float ux=fx*fx*fx*(fx*(fx*6-15)+10),uy=fy*fy*fy*(fy*(fy*6-15)+10),uz=fz*fz*fz*(fz*(fz*6-15)+10);
        float v[8]; for(int k=0;k<8;k++){ int a=k&1,b=(k>>1)&1,c=(k>>2)&1; v[k]=pgrad(ix+a,iy+b,iz+c,fx-a,fy-b,fz-c,P,s); }
        float l0=v[0]+(v[1]-v[0])*ux, l1=v[2]+(v[3]-v[2])*ux, l2=v[4]+(v[5]-v[4])*ux, l3=v[6]+(v[7]-v[6])*ux;
        float m0=l0+(l1-l0)*uy, m1=l2+(l3-l2)*uy; return m0+(m1-m0)*uz;
    }
    static float worley3(float x,float y,float z,int P,uint32_t s){
        float x0=floorf(x),y0=floorf(y),z0=floorf(z); int ix=(int)x0,iy=(int)y0,iz=(int)z0; float md=9;
        for(int c=-1;c<=1;c++) for(int b=-1;b<=1;b++) for(int a=-1;a<=1;a++){
            uint32_t h=w_h3((((ix+a)%P)+P)%P,(((iy+b)%P)+P)%P,(((iz+c)%P)+P)%P,s);
            float px=ix+a+w_u(h), py=iy+b+w_u(w_hu(h+1u)), pz=iz+c+w_u(w_hu(h+2u));
            float d=(px-x)*(px-x)+(py-y)*(py-y)+(pz-z)*(pz-z); md=std::min(md,d); }
        return 1.0f-std::min(sqrtf(md),1.0f);
    }
    void buildCloud(){
        const int n=128; cloud.resize((size_t)n*n*n*4);
        parallel_rows(n,[&](int k){ for(int j=0;j<n;j++) for(int i=0;i<n;i++){
            float x=(i+0.5f)/n, y=(j+0.5f)/n, z=(k+0.5f)/n;
            float pf=0, a=1, nrm=0; int f=4; for(int o=0;o<4;o++){ pf+=a*perlin3(x*f,y*f,z*f,f,100u+o); nrm+=a; a*=0.5f; f*=2; }
            pf=pf/nrm*0.95f+0.5f; pf=std::min(std::max(pf,0.0f),1.0f);
            auto wf=[&](int f0,uint32_t s){ return 0.625f*worley3(x*f0,y*f0,z*f0,f0,s)+0.25f*worley3(x*f0*2,y*f0*2,z*f0*2,f0*2,s+1)+0.125f*worley3(x*f0*4,y*f0*4,z*f0*4,f0*4,s+2); };
            float w4=wf(4,200u), w8=wf(8,300u), w16=wf(16,400u), w32=worley3(x*32,y*32,z*32,32,500u);
            float pw=(pf-(w4-1.0f))/(1.0f-(w4-1.0f)); pw=std::min(std::max(pw,0.0f),1.0f);
            uint8_t* q=&cloud[(((size_t)k*n+j)*n+i)*4];
            q[0]=(uint8_t)(pw*255.0f); q[1]=(uint8_t)(w8*255.0f); q[2]=(uint8_t)(w16*255.0f); q[3]=(uint8_t)(w32*255.0f); } });
    }
    bool build(const char* terrainPath,const char* cachePath){
        if(!load(terrainPath)) return false;
        FILE* f=fopen(cachePath,"rb"); uint32_t tag=0x4246303c;
        size_t nH=(size_t)TH_N*TH_N, nT=(size_t)TT_N*TT_N;
        if(f){ uint32_t t=0; fread(&t,4,1,f);
            if(t==tag){ H.resize(nH); aux.resize((size_t)NL*NL*4); treeA.resize(nT*4); treeCol.resize(nT*4); treeBase.resize(nT); maxTree.resize(1024*1024); cloud.resize((size_t)128*128*128*4);
                bool ok=fread(H.data(),4,nH,f)==nH && fread(aux.data(),1,aux.size(),f)==aux.size() && fread(treeA.data(),1,nT*4,f)==nT*4 && fread(treeCol.data(),1,nT*4,f)==nT*4
                    && fread(treeBase.data(),4,nT,f)==nT && fread(maxTree.data(),1,maxTree.size(),f)==maxTree.size() && fread(cloud.data(),1,cloud.size(),f)==cloud.size();
                fclose(f); if(ok) return true; } else fclose(f); }
        fprintf(stderr,"building world...\n");
        buildHeights(); fprintf(stderr," heights ok\n"); buildAux(); fprintf(stderr," aux ok\n"); buildTrees(); fprintf(stderr," trees ok\n"); buildCloud(); fprintf(stderr," cloud ok\n");
        f=fopen(cachePath,"wb"); if(f){ fwrite(&tag,4,1,f); fwrite(H.data(),4,nH,f); fwrite(aux.data(),1,aux.size(),f); fwrite(treeA.data(),1,nT*4,f); fwrite(treeCol.data(),1,nT*4,f);
            fwrite(treeBase.data(),4,nT,f); fwrite(maxTree.data(),1,maxTree.size(),f); fwrite(cloud.data(),1,cloud.size(),f); fclose(f); }
        return true;
    }
};
