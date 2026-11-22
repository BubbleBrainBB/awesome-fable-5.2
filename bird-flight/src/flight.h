// flight.h — 航线、飞行动力学与"鸟头"相机
// 主角：家燕（Hirundo rustica）。巡航 13–18 m/s，拍翅约 7.5 Hz，典型的"拍几下—滑一段"；
// 贴水面飞行并点水喝水；转弯靠压坡度；头部相对身体稳定，注视是跳视。
#pragma once
#include <simd/simd.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "shared.h"
#include "layout.h"
struct World; struct Renderer;

typedef simd_double3 d3;
static inline d3 D3(double x,double y,double z){ return simd_make_double3(x,y,z); }
static inline double sstep(double a,double b,double x){ double t=std::min(std::max((x-a)/(b-a),0.0),1.0); return t*t*(3-2*t); }
static inline double lerpd(double a,double b,double t){ return a+(b-a)*t; }

static Cam camFromBasis(simd_float3 pos,simd_float3 f,float roll,float fov){
    f=simd_normalize(f); simd_float3 wu=simd_make_float3(0,1,0); simd_float3 r=simd_normalize(simd_cross(f,wu)); simd_float3 up=simd_cross(r,f);
    simd_float3 r2=r*cosf(roll)+up*sinf(roll); simd_float3 u2=up*cosf(roll)-r*sinf(roll);
    Cam c; c.pos=simd_make_float4(pos.x,pos.y,pos.z,tanf(fov*0.5f)); c.right=simd_make_float4(r2.x,r2.y,r2.z,0); c.up=simd_make_float4(u2.x,u2.y,u2.z,0); c.fwd=simd_make_float4(f.x,f.y,f.z,0); return c;
}
static Cam makeCam(simd_float3 pos,float yaw,float pitch,float roll,float fov){
    simd_float3 f=simd_make_float3(cosf(pitch)*cosf(yaw),sinf(pitch),-cosf(pitch)*sinf(yaw)); return camFromBasis(pos,f,roll,fov);
}
// 可重复的平滑噪声（-1..1）
static double fnoise(double x,uint32_t seed){
    double xf=floor(x); int i=(int)xf; double f=x-xf; f=f*f*f*(f*(f*6-15)+10);
    auto h=[&](int k){ uint32_t v=(uint32_t)k*0x9E3779B1u^seed; v^=v>>16; v*=0x7feb352dU; v^=v>>15; v*=0x846ca68bU; v^=v>>16; return (v>>8)*(2.0/16777216.0)-1.0; };
    return h(i)+(h(i+1)-h(i))*f;
}

struct Key { double t; d3 p; };
struct Gaze { double t0,t1; d3 target; double w; double rise; };           // 飞行中的注视（看向某点）
struct Head { double t; double yaw,pitch,tilt,focus; };                     // 停栖后的头部姿态序列
struct Rip { double t; double x,z; double amp; };

struct FState {
    d3 pos, vel, acc; double speed;
    double bodyRoll, flapPhase, flapAmp, flapAm, fold, brake;   // fold: 收翅(俯冲) 0..1；brake: 张翅刹车 0..1
    d3 gaze; double camRoll; double fov, focus, aperture;
    double dip;                                         // 栖枝下沉（m）
    double gload;                                       // 过载
    int zone;                                           // 0 云上 1 云中 2 俯冲 3 湖面 4 河口芦苇 5 林间河道 6 峡谷 7 瀑布 8 盘旋 9 进近 10 停栖
};

struct Flight {
    static constexpr double HZ=240.0;
    double T_END=99.0, T_LAND=85.0;
    std::vector<d3> P;                // 平滑后的位置（240 Hz）
    std::vector<double> rollS, flapPh, flapAm, foldS, brakeS, turbS;
    std::vector<Gaze> gazes; std::vector<Head> heads; std::vector<Rip> rips; std::vector<Key> keys;
    d3 perch, heroBase; double heroYaw=0; double sunAz=0;
    std::vector<float> exposure;      // 逐帧曝光（60 fps 下标）；空则用默认
    World* W=nullptr;
    // 时间段
    double tDive=9.3, tCloudIn=10.6, tCloudOut=15.6, tLake=23.5, tMouth=36.5, tRiver=40.0, tGorge=52.0, tFall=63.2, tTop=68.6, tApproach=79.0;

    template<class WT> double gnd(WT& w,double x,double z){ return w.ground((float)x,(float)z); }

    template<class WT> void build(WT& w){
        W=(World*)&w; keys.clear();
        auto K=[&](double t,double x,double y,double z){ keys.push_back({t,D3(x,y,z)}); };
        // ---- A 云海之上：向西，贴着云顶 ----
        const double SX=-110.0;
        K(-1.0, 919+SX,497,-421); K(0.0, 900+SX,496,-420); K(2.5, 853+SX,489,-417); K(5.0, 806+SX,483,-412); K(7.5, 759+SX,487,-408); K(9.3, 726+SX,490,-402);
        // ---- B/C 左转俯冲：穿云，冲向湖面 ----
        K(10.3, 706+SX,485,-392); K(11.3, 690+SX,468,-372); K(12.4, 684+SX,435,-345); K(13.6, 692+SX,392,-316); K(14.8, 708+SX,341,-290); K(16.0, 727+SX,286,-265);
        K(17.3, 745+SX,226,-240); K(18.6, 760+SX,165,-216); K(19.8, 771+SX,110,-195); K(20.9, 779+SX,64,-178); K(21.8, 786+SX,33,-166); K(22.6, 795+SX,13,-156); K(23.5, 812+SX,3.2,-146);
        // ---- D 贴湖面向东：点水 ----
        double lakeT[]={24.5,25.6,26.7,27.6,28.1,28.6,29.6,30.8,32.0,33.2,34.4,35.5,36.5};
        double lakeX[]={728,752,776,798,809,820,842,866,889,910,928,943,957};
        double lakeZ[]={-134,-121,-108,-97,-91,-85,-72,-55,-39,-24,-12,-4,0};
        double lakeY[]={1.3,0.8,0.6,0.45,0.10,0.5,0.9,0.7,0.6,0.8,1.0,1.1,1.2};
        for(int i=0;i<13;i++) K(lakeT[i],lakeX[i],lakeY[i],lakeZ[i]);
        // ---- E/F/G 河口 → 林间河道 → 峡谷：沿河道中心线，带横向摆动 ----
        { double x=957, t=36.5; int n=0;
          while(x<FALL_X-17.0){ double sp=(x<1055)?15.5:((x<GORGE_X0)?18.5:16.0); double dzdx=riverDz((float)x); double dt=0.25; x+=sp*dt/sqrt(1+dzdx*dzdx); t+=dt; n++;
              double lat=(x<GORGE_X0-10)? 1.4*sin(x/19.0+1.0)*sstep(1010,1050,x) : 0.6*sin(x/15.0);
              double hgt=(x<1060)? 1.2+0.6*sstep(980,1060,x) : ((x<GORGE_X0)? 1.9+0.7*sin(x/23.0) : 1.9+2.6*sstep(GORGE_X0,GORGE_X0+25,x)+1.1*sin(x/17.0+0.5));
              // 倒木：压低钻过去；巨石：抬高跃过
              hgt+=-0.7*exp(-pow((x-1168.0)/5.0,2))+1.3*exp(-pow((x-1215.0)/6.0,2));
              double wl=riverBed((float)x)+riverDepth((float)x); if(x<1010) wl=std::max(wl,0.0);
              K(t,x,wl+hgt,riverZ((float)x)+lat*sqrt(1+dzdx*dzdx)); if(x<1055) tRiver=t; if(x<GORGE_X0) tGorge=t; }
          tFall=t; }
        // ---- H 沿瀑布拉起 ----
        { double zf=riverZ(FALL_X); double t=tFall;
          K(t+0.9, FALL_X-13.0, FALL_BOT+8.0, zf-0.5); K(t+1.8, FALL_X-11.5, FALL_BOT+20.0, zf+0.6); K(t+2.8, FALL_X-11.0, FALL_BOT+36.0, zf+1.2);
          K(t+3.8, FALL_X-10.5, FALL_BOT+52.0, zf+0.4); K(t+4.7, FALL_X-8.0, FALL_TOP+7.0, zf-0.4); K(t+5.5, FALL_X+3.0, FALL_TOP+11.5, zf-0.8);
          tTop=t+5.5;
          // ---- I 瀑布上方借上升气流左盘（东→北→西），高度渐增 ----
          double cx=FALL_X+16.0, cz=zf-26.0, R=27.0; double t0=tTop; int N=8;
          for(int i=1;i<=N;i++){ double a=-M_PI/2+ (double)i/N*(1.22*M_PI); // 从正南侧（z 大）起，逆时针（俯视：东→北→西）
              double px=cx+R*cos(a)*(1.0+0.15*i/N), pz=cz-R*sin(a)-R*0.0; double py=FALL_TOP+11.5+ (double)i/N*24.0;
              K(t0+i*1.05, px, py, pz); }
          tApproach=t0+N*1.05;
        }
        // 栖点：峡谷北缘的老桦树（位置由地形查询确定）
        { double bx=HERO_X, bz=riverZ(HERO_X-2.0f)-24.0; double gy=gnd(w,bx,bz);
          heroBase=D3(bx,gy,bz); heroYaw=0.0;
          perch=D3(bx-2.35, gy+7.60, bz+1.10);
          fprintf(stderr,"hero tree base (%.1f, %.1f, %.1f), perch y=%.1f\n",bx,gy,bz,perch.y);
          double t0=tApproach;
          // ---- J 进近：沿峡谷北缘向西滑翔下降、减速、张翅刹车、落枝 ----
          d3 a0=perch+D3(62.0,13.0,-15.0), app=perch+D3(26.0,5.2,-8.0), app2=perch+D3(11.0,2.5,-1.5), app3=perch+D3(3.2,0.75,0.9), app4=perch+D3(0.75,0.22,0.25);
          K(t0+2.6, a0.x,a0.y,a0.z); K(t0+5.2, app.x,app.y,app.z); K(t0+6.9, app2.x,app2.y,app2.z); K(t0+8.1, app3.x,app3.y,app3.z); K(t0+8.9, app4.x,app4.y,app4.z);
          T_LAND=t0+9.5; K(T_LAND, perch.x,perch.y,perch.z); K(T_LAND+1.0, perch.x,perch.y,perch.z); K(T_LAND+30.0, perch.x,perch.y,perch.z);
        }
        T_END=T_LAND+14.0;
        fprintf(stderr,"timeline: fall %.2f top %.2f approach %.2f land %.2f end %.2f\n",tFall,tTop,tApproach,T_LAND,T_END);
        // ---- 采样 + 时间域高斯平滑 ----
        int n=(int)ceil((T_END+1.0)*HZ)+1; std::vector<d3> raw(n);
        size_t ki=1;
        for(int i=0;i<n;i++){ double t=i/HZ; while(ki+2<keys.size() && keys[ki+1].t<=t) ki++;
            size_t k1=ki; if(t<keys[1].t) k1=1; size_t k0=k1-1,k2=std::min(k1+1,keys.size()-1),k3=std::min(k1+2,keys.size()-1);
            double t1=keys[k1].t,t2=keys[k2].t; double u=(t2>t1)?std::min(std::max((t-t1)/(t2-t1),0.0),1.0):0.0;
            // 非均匀 Catmull-Rom（按时间求切线）
            d3 m1=(keys[k2].p-keys[k0].p)/(keys[k2].t-keys[k0].t)*(t2-t1), m2=(k3>k2)?(keys[k3].p-keys[k1].p)/(keys[k3].t-keys[k1].t)*(t2-t1):(keys[k2].p-keys[k1].p);
            double u2=u*u,u3=u2*u; raw[i]=keys[k1].p*(2*u3-3*u2+1)+m1*(u3-2*u2+u)+keys[k2].p*(-2*u3+3*u2)+m2*(u3-u2); }
        P.resize(n);
        for(int i=0;i<n;i++){ double t=i/HZ; double sig=0.20*(1.0-sstep(T_LAND-1.6,T_LAND-0.15,t)); if(t>tFall-1.0&&t<tTop) sig=std::min(sig,0.16);
            if(sig<1e-3){ P[i]=raw[i]; continue; } int r=(int)(sig*3*HZ); d3 s=D3(0,0,0); double ws=0;
            for(int k=-r;k<=r;k++){ int j=std::min(std::max(i+k,0),n-1); double wv=exp(-0.5*(k/HZ/sig)*(k/HZ/sig)); s+=raw[j]*wv; ws+=wv; } P[i]=s/ws; }
        // ---- 拍翅节律：拍几下 / 滑一段 ----
        flapPh.assign(n,0); flapAm.assign(n,0); foldS.assign(n,0); brakeS.assign(n,0); turbS.assign(n,0); rollS.assign(n,0);
        double ph=0; double burstT=0; bool on=true; double amp=0; uint32_t rs=1234567u;
        auto rnd=[&](){ rs=rs*1664525u+1013904223u; return (rs>>8)/16777216.0; };
        for(int i=0;i<n;i++){ double t=i/HZ;
            // 各段的拍翅意愿（0 纯滑翔，1 持续拍）与翅频
            double want=0.62, f=7.5;
            if(t>tDive-0.4 && t<tLake-2.6){ want=0.0; }                                   // 俯冲：收翅
            else if(t>=tLake-2.6 && t<tLake+0.8){ want=0.0; }                             // 拉起后的高速滑翔
            else if(t>=tLake+0.8 && t<tMouth){ want=0.5; }
            else if(t>=tMouth && t<tFall){ want=0.66; f=7.8; }
            else if(t>=tFall && t<tTop-0.3){ want=1.0; f=9.2; }                           // 沿瀑布爬升：急拍
            else if(t>=tTop-0.3 && t<tApproach){ want=0.10; }                             // 上升气流里滑翔盘旋
            else if(t>=tApproach && t<T_LAND-1.25){ want=0.25; }
            else if(t>=T_LAND-1.25 && t<T_LAND+0.12){ want=1.0; f=11.5; }                 // 落枝前的扑翼刹车
            else if(t>=T_LAND+0.12){ want=0.0; }
            burstT-=1.0/HZ;
            if(burstT<=0){ if(want>=0.99){ on=true; burstT=0.3; } else if(want<=0.01){ on=false; burstT=0.2; }
                else { on=!on; burstT=on? (0.45+0.7*rnd())*(0.6+want) : (0.30+0.55*rnd())*(1.4-want); } }
            double target=on?1.0:0.0; if(want<=0.01) target=0; if(want>=0.99) target=1;
            amp+=(target-amp)*std::min(1.0,(1.0/HZ)/0.07);
            ph+=f/HZ; flapPh[i]=ph; flapAm[i]=amp;
            foldS[i]=sstep(tDive+0.5,tDive+2.2,t)*(1.0-sstep(tLake-3.4,tLake-2.2,t));
            brakeS[i]=sstep(T_LAND-1.25,T_LAND-0.55,t)*(1.0-sstep(T_LAND+0.05,T_LAND+0.45,t));
            // 湍流强度：云中、峡谷上空的上升气流里更强
            turbS[i]=0.35+0.9*sstep(tCloudIn-0.5,tCloudIn+0.6,t)*(1.0-sstep(tCloudOut-0.3,tCloudOut+0.8,t))+0.55*sstep(tTop-0.5,tTop+1.0,t)*(1.0-sstep(tApproach,tApproach+2.0,t));
            if(t>T_LAND-0.3) turbS[i]*=1.0-sstep(T_LAND-0.3,T_LAND,t);
        }
        // ---- 坡度：由横向加速度求出（协调转弯），略带提前量并限速 ----
        std::vector<double> rr(n,0);
        for(int i=0;i<n;i++){ FState s; kin(i/HZ+0.10,s); d3 v=s.vel; double sp=simd_length(v); if(sp<1.0){ rr[i]=0; continue; }
            d3 fh=simd_normalize(D3(v.x,0,v.z)); d3 right=D3(-fh.z,0,fh.x);   // 右手方向（x 东 z 南，y 上）：前×上
            right=simd_cross(fh,D3(0,1,0));
            double ar=simd_dot(s.acc,right); double ay=s.acc.y; double phi=atan2(ar,9.81+std::max(ay,-6.0)); rr[i]=std::min(std::max(phi,-1.25),1.25); }
        for(int i=0;i<n;i++){ double sig=0.07; int r=(int)(sig*3*HZ); double s=0,ws=0; for(int k=-r;k<=r;k++){ int j=std::min(std::max(i+k,0),n-1); double wv=exp(-0.5*(k/HZ/sig)*(k/HZ/sig)); s+=rr[j]*wv; ws+=wv; } rollS[i]=s/ws; }
        // ---- 注视（飞行中的跳视） ----
        gazes.clear();
        gazes.push_back({3.2,4.6, D3(650,465,-520),0.55,0.12});                          // 看一眼右前方云海上的同伴
        gazes.push_back({6.4,7.4, D3(560,250,-150),0.5,0.12});                           // 向左下瞥：云边缘外的谷地
        gazes.push_back({15.2,22.4, D3(940,10,-10),0.70,0.6});                           // 俯冲：视线抬向湖东端（要去的方向），地平线留在画面里
        gazes.push_back({29.5,30.4, D3(872,0.0,-22),0.85,0.09});                         // 贴湖：瞥一眼右前方鱼跃的涟漪
        gazes.push_back({tFall-3.4,tFall-2.2, D3(FALL_X,FALL_BOT+52,riverZ(FALL_X)),0.7,0.10});   // 峡谷里抬头看瀑布顶
        gazes.push_back({tFall+0.2,tFall+4.3, D3(FALL_X+1.0,FALL_TOP+1.0,riverZ(FALL_X)),0.86,0.35});   // 拉起：一直看着水帘与崖顶
        gazes.push_back({tTop+2.6,tTop+4.0, D3(FALL_X+320,FALL_TOP+40,riverZ(FALL_X+320)),0.5,0.12});   // 盘旋：看上游谷地
        gazes.push_back({tApproach-0.3,T_LAND-0.05, perch+D3(0,-0.02,0),0.93,0.25});     // 进近：盯住落点
        // ---- 停栖后的头部动作：小幅快速转头 + 停顿，最后望向远方 ----
        heads.clear(); double tl=T_LAND; double vy=M_PI;       // 朝西
        heads.push_back({tl+0.00, vy-0.20,-0.30, 0.00, 45});     // 刚落下：先低头看一眼崖下的林与河
        heads.push_back({tl+0.95, vy+0.55,-0.10, 0.06, 2.2});    // 右（北）：树干与枝叶
        heads.push_back({tl+1.85, vy+0.15, 0.10,-0.04, 30});     // 抬头：云边
        heads.push_back({tl+2.75, vy-0.75,-0.16, 0.05, 1.4});    // 左（南）：近处的叶子，峡谷对岸
        heads.push_back({tl+3.55, vy-0.30,-0.42, 0.00, 60});     // 向下：峡谷与河
        heads.push_back({tl+4.45, vy+0.10,-0.06,-0.03, 400});    // 大致望向远方
        heads.push_back({tl+5.30, vy-0.035,-0.035,0.00, 2000});  // 最后一次微调：定住
        // ---- 涟漪：鱼跃与点水 ----
        rips.clear();
        rips.push_back({21.5, 742,-112,1.0}); rips.push_back({24.2, 779,-128,0.8}); rips.push_back({25.9, 825,-62,1.0}); rips.push_back({27.0, 811,-110,0.7});
        rips.push_back({28.1, 809,-91,1.3});   // 自己点水
        rips.push_back({29.2, 872,-22,1.0}); rips.push_back({31.0, 902,-40,0.8}); rips.push_back({30.2, 846,-40,0.6}); rips.push_back({20.0, 770,-70,0.7}); rips.push_back({33.0, 925,-22,0.6});
        // 太阳方位：沿峡谷轴线照进去
        { double dz=riverZ(1425.0f)-riverZ(1285.0f); sunAz=atan2(-dz,140.0); }
        // 逐帧曝光表
        exposure.clear(); FILE* f=fopen("work/exposure.txt","r"); if(f){ float e; while(fscanf(f,"%f",&e)==1) exposure.push_back(e); fclose(f); fprintf(stderr,"exposure table: %zu frames\n",exposure.size()); }
    }
    d3 posAt(double t) const { double x=t*HZ; int n=(int)P.size(); if(x<=0) return P[0]; if(x>=n-1) return P[n-1]; int i=(int)x; double f=x-i; return P[i]*(1-f)+P[i+1]*f; }
    double arr(const std::vector<double>& a,double t) const { double x=t*HZ; int n=(int)a.size(); if(x<=0) return a[0]; if(x>=n-1) return a[n-1]; int i=(int)x; double f=x-i; return a[i]*(1-f)+a[i+1]*f; }
    void kin(double t,FState& s) const { double h=1.0/60.0; d3 p0=posAt(t-h),p1=posAt(t),p2=posAt(t+h); s.pos=p1; s.vel=(p2-p0)/(2*h); s.acc=(p2-p1*2.0+p0)/(h*h); s.speed=simd_length(s.vel); }

    void state(double t,FState& s) const {
        kin(t,s);
        s.bodyRoll=arr(rollS,t); s.flapPhase=arr(flapPh,t); s.flapAm=arr(flapAm,t); s.flapAmp=s.flapAm; s.fold=arr(foldS,t); s.brake=arr(brakeS,t);
        double turb=arr(turbS,t);
        s.gload=simd_length(s.acc+D3(0,9.81,0))/9.81;
        s.zone=(t<tCloudIn)?0:(t<tCloudOut)?1:(t<tLake)?2:(t<tMouth)?3:(t<tRiver)?4:(t<tGorge)?5:(t<tFall)?6:(t<tTop)?7:(t<tApproach)?8:(t<T_LAND)?9:10;
        s.fov=62.0*M_PI/180; s.focus=60; s.aperture=0.0; s.dip=0;
        bool perched=(t>=T_LAND);
        // 位置上的小扰动：阵风里的小修正 + 拍翅引起的身体起伏（头部只残留一小部分）
        d3 fh=simd_length(D3(s.vel.x,0,s.vel.z))>0.5?simd_normalize(D3(s.vel.x,0,s.vel.z)):D3(-1,0,0); d3 right=simd_cross(fh,D3(0,1,0));
        double land=1.0-sstep(T_LAND-0.8,T_LAND,t);
        double gx=0.10*turb*(fnoise(t*1.3,11u)+0.5*fnoise(t*3.1,12u))*land, gy=0.08*turb*(fnoise(t*1.1,13u)+0.5*fnoise(t*2.7,14u))*land;
        double bob=0.006*s.flapAm*sin(2*M_PI*s.flapPhase)*(land+0.8*s.brake*(1.0-land));
        s.pos+=right*gx+D3(0,1,0)*(gy+bob);
        if(perched){
            // 落枝：树枝一沉再回弹；之后是微风里的轻晃
            double tt=t-T_LAND; s.dip=-0.034*exp(-tt*1.9)*cos(2*M_PI*2.3*tt)*(1.0-exp(-tt*40.0)) - 0.012*(1.0-exp(-tt*6.0))
                  +0.0025*sin(2*M_PI*0.55*tt+0.4)*sstep(1.5,4.0,tt)+0.0012*sin(2*M_PI*1.3*tt);
            s.pos=perch+D3(0.0012*sin(2*M_PI*0.4*tt), s.dip, 0.0018*sin(2*M_PI*0.55*tt+1.0)*sstep(1.5,4.0,tt));
        }
        // ---- 注视方向 ----
        d3 g;
        if(!perched){
            double la=0.45; d3 ahead=posAt(t+la)-posAt(t); if(simd_length(ahead)<0.05) ahead=s.vel; g=simd_normalize(ahead);
            // 头部稳定：对注视方向做低通（用未来/过去位置的对称平均，相当于零相位滤波）
            d3 acc=D3(0,0,0); double ws=0; for(int k=-6;k<=6;k++){ double tk=t+k*0.03; d3 a2=posAt(tk+la)-posAt(tk); if(simd_length(a2)>0.02){ double wv=exp(-0.5*(k*0.03/0.10)*(k*0.03/0.10)); acc+=simd_normalize(a2)*wv; ws+=wv; } }
            if(ws>0) g=simd_normalize(acc/ws);
            // 俯冲时视线不要完全朝下：稍微抬一点，看前方落点
            for(const Gaze& G:gazes){ if(t<G.t0-G.rise||t>G.t1+G.rise) continue; double wv=sstep(G.t0-G.rise,G.t0,t)*(1.0-sstep(G.t1,G.t1+G.rise,t))*G.w;
                d3 d=simd_normalize(G.target-s.pos); g=simd_normalize(g*(1-wv)+d*wv); }
            // 轻微的视线扰动（阵风中的修正）
            d3 up=D3(0,1,0); d3 r2=simd_normalize(simd_cross(g,up)); d3 u2=simd_cross(r2,g);
            double jy=0.006*turb*(fnoise(t*1.7,21u)+0.6*fnoise(t*4.3,22u))*land, jp=0.005*turb*(fnoise(t*1.9,23u)+0.6*fnoise(t*4.9,24u))*land;
            jp+=0.0020*s.flapAm*sin(2*M_PI*s.flapPhase+0.6)*land;        // 拍翅同步的轻微点头
            jp+=0.0045*s.brake*s.flapAm*sin(2*M_PI*s.flapPhase+0.6);       // 扑翼刹车时身体的抖动传到头上一点
            g=simd_normalize(g+r2*jy+u2*jp);
            // 相机横滚：身体坡度的一部分（头部做了部分补偿）+ 阵风
            double headComp=0.10+0.50*sstep(0.12,0.70,fabs(s.bodyRoll)); s.camRoll=-(s.bodyRoll*headComp)+0.012*turb*fnoise(t*1.5,25u)*land+0.004*s.flapAm*sin(2*M_PI*s.flapPhase)*land;
            // 刹车时身体后仰：视线稍微上抬再回到落点
            s.camRoll*=1.0;
        } else {
            double tt=t; size_t k=0; while(k+1<heads.size() && heads[k+1].t<=tt) k++;
            const Head& a=heads[(k>0)?k-1:0]; const Head& b=heads[k];
            double dur=0.11; double u=(k>0)?sstep(b.t,b.t+dur,tt):1.0; u=u*u*(3-2*u);
            double yaw=lerpd(a.yaw,b.yaw,u), pit=lerpd(a.pitch,b.pitch,u), tilt=lerpd(a.tilt,b.tilt,u); s.focus=exp(lerpd(log(a.focus),log(b.focus),u));
            if(k==0){ // 从进近时的注视平滑过渡到第一个姿态
                yaw=b.yaw; pit=b.pitch; tilt=b.tilt; s.focus=b.focus; }
            // 跳视后的微小回摆
            double since=tt-b.t; double ov=(k>0)?0.012*exp(-since*14.0)*sin(since*38.0):0.0;
            yaw+=ov*(b.yaw>a.yaw?1:-1);
            g=D3(cos(pit)*cos(yaw),sin(pit),-cos(pit)*sin(yaw)); s.camRoll=tilt+0.002*sin(2*M_PI*0.55*(tt-T_LAND));
            // 转头时头的位置也随之微移
            s.pos+=D3(cos(yaw)*0.012,0.004*sin(pit*2.0),-sin(yaw)*0.012);
            s.aperture=0.0030;
        }
        s.gaze=g;
        // 进近末段：与停栖姿态衔接（注视落点 → 第一个头部姿态），同时拉近对焦
        if(!perched && t>T_LAND-1.5){ double u=sstep(T_LAND-1.5,T_LAND-0.1,t); s.focus=lerpd(60,std::max(simd_length(perch-s.pos),0.3),u); s.aperture=0.0030*u; }
        if(!perched && t>T_LAND-0.25){ const Head& b=heads[0]; d3 gh=D3(cos(b.pitch)*cos(b.yaw),sin(b.pitch),-cos(b.pitch)*sin(b.yaw)); double u=sstep(T_LAND-0.25,T_LAND,t); s.gaze=simd_normalize(s.gaze*(1-u)+gh*u); s.camRoll*=(1-u); }
    }
    double sunEl(double t) const { return (8.6-4.3*sstep(0.0,T_LAND+2.0,t)-0.25*sstep(T_LAND,T_END,t))*M_PI/180; }
    void defaults(Uni& u,double t){
        memset(&u,0,sizeof u);
        float el=(float)sunEl(t), az=(float)sunAz;
        u.sunDir=simd_make_float4(-cosf(el)*cosf(az),sinf(el),-cosf(el)*sinf(az),0);
        u.sunI=simd_make_float4(20,20,20,0); u.tp=simd_make_float4((float)t,0,0,1); u.res=simd_make_float4(0,0,1.0f,0); u.lens=simd_make_float4(50,0,0.22f,1);
        u.hero=simd_make_float4((float)heroBase.x,(float)heroBase.y,(float)heroBase.z,(float)heroYaw); u.perch=simd_make_float4((float)perch.x,(float)perch.y,(float)perch.z,0);
        int k=0; for(const Rip& r:rips){ if(k>=MAXRIP) break; u.rip[k++]=simd_make_float4((float)r.x,(float)r.z,(float)r.t,(float)r.amp); }
        for(int i=0;i<MAXBIRD;i++) u.bird[i]=simd_make_float4(0,0,0,-1);
        u.wind=simd_make_float4(0.9f,0.43f,2.0f,17.0f);
        // 穿云通道：俯冲航线在云层高度内的一段
        { int k=0; for(double tt=tDive+0.6; tt<tDive+8.6 && k<8; tt+=1.1){ d3 q=posAt(tt); u.tube[k++]=simd_make_float4((float)q.x,(float)q.y,(float)q.z,(float)(30.0+16.0*sin(tt*1.7))); } }
        u.flags.z=1.0f;
    }
    Cam camAt(double t,FState& s){ state(t,s); return camFromBasis(simd_make_float3((float)s.pos.x,(float)s.pos.y,(float)s.pos.z),simd_make_float3((float)s.gaze.x,(float)s.gaze.y,(float)s.gaze.z),(float)s.camRoll,(float)s.fov); }
    void frameUni(Uni& u,double t0,double t1){
        t1=t0+(t1-t0)*shutterScale(t0,t1);
        defaults(u,t0); FState s0,s1; u.cam0=camAt(t0,s0); u.cam1=camAt(t1,s1);
        u.res.w=(float)(t1-t0);
        u.lens=simd_make_float4((float)s0.focus,(float)s0.aperture,0.22f,1.0f);
        // 片头淡入、片尾淡出
        double fade=sstep(0.0,0.9,t0)*(1.0-sstep(T_END-1.6,T_END-0.05,t0)); u.lens.w=(float)fade;
        int fi=(int)llround(t0*60.0); if(!exposure.empty()) u.res.z=exposure[std::min((size_t)std::max(fi,0),exposure.size()-1)];
        u.anim0=simd_make_float4((float)s0.flapPhase,(float)(s0.flapAm),(float)s0.dip,(float)s0.brake); u.anim1=simd_make_float4((float)s1.flapPhase,(float)(s1.flapAm),(float)s1.dip,(float)s1.brake);
        u.flags.y=0.0f;   // 自己的翅膀：试过，不够可信，关闭
        u.flags.z=1.0f;
        birds(u,t0);
    }
    double shutterScale(double t0,double t1){
        FState a,b; state(t0,a); state(t1,b); double ang=acos(std::min(1.0,std::max(-1.0,simd_dot(simd_normalize(a.gaze),simd_normalize(b.gaze)))));
        const double lim=0.8*M_PI/180; return (ang>lim)?lim/ang:1.0;
    }
    // 同伴（另外几只家燕）：云上并行，随后先于我们扎进云里；结尾在远处掠过
    void birds(Uni& u,double t){
        int k=0;
        auto add=[&](d3 p,d3 v,double ph,double span){ if(k>=MAXBIRD) return; u.bird[k]=simd_make_float4((float)p.x,(float)p.y,(float)p.z,(float)ph); d3 d=simd_normalize(v); u.birdv[k]=simd_make_float4((float)d.x,(float)d.y,(float)d.z,(float)span); k++; };
        if(t<13.5){
            for(int i=0;i<5;i++){ double lead=0.55+0.33*i; double tb=t+lead; d3 p=posAt(tb); d3 v=posAt(tb+0.05)-posAt(tb-0.05);
                d3 off=D3(2.0*fnoise(t*0.5+i*7.1,31u+i), (i%2?1.2:-0.6)+1.2*fnoise(t*0.6+i*3.3,41u+i)+0.9*i*0.3, (i-2)*2.6+2.2*fnoise(t*0.45+i*5.7,51u+i));
                add(p+off,v,t*7.5+i*0.37,0.33); }
        }
        if(t>T_LAND+2.0){ // 远处两只燕子掠过谷地
            for(int i=0;i<3;i++){ double tt=t-T_LAND-2.0-i*1.7; d3 p=perch+D3(-45.0-10*i-9.0*tt, -6.0+2.0*i+3.5*sin(tt*0.9+i), -30.0+17.0*i+ (i%2?-1:1)*7.5*tt+6.0*sin(tt*0.7+i*2.0));
                d3 v=D3(-9.0,3.1*cos(tt*0.9+i),(i%2?-1:1)*7.5+4.2*cos(tt*0.7+i*2.0)); add(p,v,t*7.5+i*0.41,0.33); }
        }
    }
    // 导出逐帧飞行数据（给音轨合成与质检）
    void dump(const char* path){
        FILE* f=fopen(path,"w"); if(!f) return;
        fprintf(f,"t,x,y,z,speed,vy,bodyRoll,camRoll,flapPhase,flapAmp,fold,brake,gload,zone,agl,gyaw,gpitch\n");
        for(double t=0;t<T_END+1e-9;t+=1.0/120.0){ FState s; state(t,s); double agl=s.pos.y-(W?((World*)W)->ground((float)s.pos.x,(float)s.pos.z):0);
            fprintf(f,"%.5f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%d,%.2f,%.4f,%.4f\n",t,s.pos.x,s.pos.y,s.pos.z,s.speed,s.vel.y,s.bodyRoll,s.camRoll,s.flapPhase,s.flapAm,s.fold,s.brake,s.gload,s.zone,agl,atan2(-s.gaze.z,s.gaze.x),asin(s.gaze.y)); }
        fclose(f);
        std::string ev=std::string(path)+".events"; f=fopen(ev.c_str(),"w");
        { std::string sp=std::string(path)+".shutter"; FILE* g=fopen(sp.c_str(),"w"); for(int fr=0; fr<(int)(T_END*60.0+0.5); fr++){ double tt=fr/60.0; double sc=shutterScale(tt,tt+0.5/60.0); if(sc<0.999) fprintf(g,"%d %.3f\n",fr,sc); } fclose(g); }
        fprintf(f,"tDive %.3f\ntCloudIn %.3f\ntCloudOut %.3f\ntLake %.3f\ntDrink 28.1\ntMouth %.3f\ntRiver %.3f\ntGorge %.3f\ntFall %.3f\ntTop %.3f\ntApproach %.3f\ntLand %.3f\ntEnd %.3f\n",tDive,tCloudIn,tCloudOut,tLake,tMouth,tRiver,tGorge,tFall,tTop,tApproach,T_LAND,T_END);
        for(const Head& h:heads) fprintf(f,"head %.3f\n",h.t);
        for(const Rip& r:rips) fprintf(f,"rip %.3f %.1f %.1f\n",r.t,r.x,r.z);
        fclose(f);
    }
    void check(Renderer& R);
};
