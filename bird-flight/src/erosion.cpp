// erosion.cpp — 生成 24 km × 24 km 的宏观地形：设计的谷地骨架 + 河流侵蚀（stream power，隐式）+ 崩塌坡度限制
// 输出 work/terrain.bin：int N; float h[N*N]; float flow[N*N]（log10 汇水面积）
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <queue>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include "layout.h"
using namespace std;
static const int N = 2048;
static const float DX = WORLD_SIZE / N;
static uint32_t SEED = 20261117u;

static inline uint32_t hashu(uint32_t x){ x ^= x>>16; x*=0x7feb352dU; x^=x>>15; x*=0x846ca68bU; x^=x>>16; return x; }
static inline uint32_t hash2(int x,int y,uint32_t s){ return hashu((uint32_t)x*0x9E3779B1u ^ hashu((uint32_t)y*0x85EBCA77u ^ s)); }
static inline float grad2(int ix,int iy,float fx,float fy,uint32_t s){
    uint32_t h = hash2(ix,iy,s); float a = (h & 0xffff) * (6.2831853f/65536.0f);
    return cosf(a)*fx + sinf(a)*fy;
}
static float pnoise(float x,float y,uint32_t s){
    float fx0=floorf(x), fy0=floorf(y); int ix=(int)fx0, iy=(int)fy0; float fx=x-fx0, fy=y-fy0;
    float ux=fx*fx*fx*(fx*(fx*6-15)+10), uy=fy*fy*fy*(fy*(fy*6-15)+10);
    float a=grad2(ix,iy,fx,fy,s), b=grad2(ix+1,iy,fx-1,fy,s), c=grad2(ix,iy+1,fx,fy-1,s), d=grad2(ix+1,iy+1,fx-1,fy-1,s);
    return (a+(b-a)*ux + ((c+(d-c)*ux)-(a+(b-a)*ux))*uy)*1.41f;
}
static float fbm(float x,float y,int oct,uint32_t s,float gain=0.5f){
    float a=1,f=1,sum=0,n=0; for(int i=0;i<oct;i++){ sum+=a*pnoise(x*f,y*f,s+i*131u); n+=a; a*=gain; f*=2.03f; } return sum/n;
}
static float ridged(float x,float y,int oct,uint32_t s){
    float a=1,f=1,sum=0,n=0,w=1; for(int i=0;i<oct;i++){ float v=1.0f-fabsf(pnoise(x*f,y*f,s+i*77u)); v*=v; sum+=a*v*w; n+=a; w=min(1.0f,max(0.0f,v*1.6f)); a*=0.52f; f*=2.07f; } return sum/n;
}
// 主谷轴线
static float axisZ(float x){
    if(x>=RIVER_X0-100.0f) return riverZ(x);
    return 260.0f*sinf((x-900.0f)/2300.0f)*l_ss(900.0f,-900.0f,x) + 900.0f*l_ss(-4000.0f,-12000.0f,x)*sinf(x/3100.0f);
}
static float floorY(float x){
    float y=-360.0f;
    y = l_mix(y, -45.0f, l_ss(-9500.0f,-2500.0f,x));
    y = l_mix(y, 9.0f, l_ss(-2500.0f, DAM_X-30.0f, x));
    y = l_mix(y, -3.0f, l_ss(DAM_X+20.0f, DAM_X+260.0f, x));
    y = l_mix(y, 2.0f, l_ss(700.0f, 1000.0f, x));
    y += 6.0f*l_sat((x-RIVER_X0)/(GORGE_X0-RIVER_X0));
    y += (FALL_TOP+4.0f-8.0f)*l_ss(GORGE_X0-5.0f, GORGE_X0+75.0f, x);   // 岩坎
    y += 260.0f*l_sat((x-FALL_X)/4200.0f);
    y += 700.0f*l_ss(6800.0f, 9500.0f, x);
    return y;
}
static float halfW(float x){
    float w=2600.0f;
    w = l_mix(w, 900.0f, l_ss(-9000.0f,-3200.0f,x));
    w = l_mix(w, 130.0f, l_ss(-3600.0f,-2300.0f,x));
    w = l_mix(w, 520.0f, l_ss(-1900.0f,-1100.0f,x));
    w = l_mix(w, 170.0f, l_ss(600.0f,1100.0f,x));
    w = l_mix(w, 110.0f, l_ss(1180.0f,1300.0f,x));
    w = l_mix(w, 230.0f, l_ss(1500.0f,2200.0f,x));
    w = l_mix(w, 0.0f, l_ss(5600.0f,7600.0f,x));
    return w;
}
// 湖岸有符号距离（内部为负）
static float lakeSD(float x,float z){
    float cx=-340.0f, a=1390.0f, b=430.0f;
    float bb = b*(0.80f+0.30f*sinf(x/610.0f+1.0f)) ;
    float ang=atan2f((z-axisZ(x)*0.6f)/bb,(x-cx)/a);
    float r=sqrtf((x-cx)*(x-cx)/(a*a)+(z-axisZ(x)*0.6f)*(z-axisZ(x)*0.6f)/(bb*bb));
    float wob = 0.10f*pnoise(x/330.0f,z/330.0f,991u)+0.05f*pnoise(x/120.0f,z/120.0f,992u)+0.03f*sinf(ang*5.0f+0.7f);
    return (r-1.0f-wob)*min(a,bb)*0.9f;
}
int main(int argc,char**argv){
    const char* out = argc>1?argv[1]:"work/terrain.bin";
    int iters = argc>2?atoi(argv[2]):70;
    vector<float> h(N*N), h0(N*N), hard(N*N), maxinc(N*N), upl(N*N), lowb(N*N);
    fprintf(stderr,"init...\n");
    #pragma omp parallel for
    for(int j=0;j<N;j++) for(int i=0;i<N;i++){
        float x=WORLD_X0+(i+0.5f)*DX, z=WORLD_Z0+(j+0.5f)*DX;
        float wx = 520.0f*fbm(x/3700.0f+3.1f,z/3700.0f,3,11u), wz=520.0f*fbm(x/3700.0f,z/3700.0f+7.7f,3,12u);
        float az=axisZ(x); float side = z>az?1.0f:-1.0f;
        float hw = halfW(x);
        float wv = hw*(1.0f+0.35f*pnoise(x/2100.0f+side*17.0f, 3.3f, 5u)) + min(hw*0.6f,260.0f)*pnoise(x/1100.0f-side*9.0f, 8.1f, 6u) + min(hw*0.3f,80.0f)*pnoise(x/380.0f-side*5.0f, 1.1f, 7u);
        float u=fabsf(z-az)-max(wv,hw*0.4f);
        float fy=floorY(x);
        // 远离谷轴处不保留谷底的台阶（岩坎只在河谷附近）
        float fyFar = -360.0f + 420.0f*l_ss(-10000.0f,3000.0f,x) + 600.0f*l_ss(3000.0f,9500.0f,x);
        fy = l_mix(fy, max(fy,fyFar), l_ss(200.0f,1800.0f,u));
        float S=0, rid=0;
        if(u>0){
            float big = 0.62f+0.38f*fbm((x+wx)/5200.0f,(z+wz)/5200.0f,2,21u)*1.6f;
            big = min(1.25f,max(0.30f,big));
            float ridgeH = 1500.0f*big + 380.0f*l_ss(2500.0f,8000.0f,x) - 650.0f*l_ss(-5000.0f,-12500.0f,x);
            ridgeH = max(ridgeH, 300.0f);
            float L=1500.0f+900.0f*big;
            S = ridgeH*(1.0f-expf(-u/L))*(u/(u+260.0f));
            float rn = ridged((x+wx)/4300.0f,(z+wz)/4300.0f,7,31u);
            float fb = fbm((x)/1700.0f,(z)/1700.0f,6,41u);
            float k = S/ridgeH;
            rid = (rn-0.45f)*1100.0f*k*(0.35f+0.65f*k) + fb*240.0f*k;
        } else {
            // 西部低地：缓丘
            rid = 55.0f*(0.5f+0.5f*fbm(x/1500.0f,z/1500.0f,4,43u))*l_ss(-3500.0f,-7000.0f,x)*l_sat(-u/500.0f);
        }
        float v = fy + S + rid;
        v += 6.0f*fbm(x/260.0f,z/260.0f,3,51u)*l_sat((u+60.0f)/300.0f);
        h[j*N+i]=v;
        upl[j*N+i] = powf(l_sat((S+rid)/700.0f),0.8f)*(0.55f+0.45f*l_sat((S+rid)/2200.0f))*(u>0?1.0f:0.0f) + 0.02f*l_sat((u+100.0f)/400.0f) + (u<=0? max(0.0f,rid)/2000.0f:0.0f);
        maxinc[j*N+i] = 3.0f + 5000.0f*l_ss(0.0f,500.0f,u);
        lowb[j*N+i] = floorY(x) - 3.0f + 0.035f*max(fabsf(z-az)-hw,0.0f);
        hard[j*N+i]=0.5f;
    }
    h0=h;
    vector<float> hf(N*N), A(N*N); vector<int> rcv(N*N); vector<int> order(N*N); vector<uint8_t> closed(N*N);
    const int dxs[8]={1,-1,0,0,1,1,-1,-1}, dys[8]={0,0,1,-1,1,-1,1,-1};
    const float dls[8]={1,1,1,1,1.41421f,1.41421f,1.41421f,1.41421f};
    typedef pair<float,int> PI;
    auto hardness=[&](int i,int j,float v)->float{
        float x=WORLD_X0+(i+0.5f)*DX, z=WORLD_Z0+(j+0.5f)*DX;
        // 倾斜岩层（向东北倾约 11°），厚度随位置变化
        float T=70.0f+90.0f*(0.5f+0.5f*pnoise(x/5200.0f,z/5200.0f,61u));
        float lv=v+0.16f*x-0.11f*z+220.0f*pnoise(x/2600.0f,z/2600.0f,62u);
        float strata=sinf(lv/T*3.14159f);
        float hd=0.5f+0.5f*strata; hd=hd*hd*(3-2*hd); hd=hd*hd*(3-2*hd);
        float reg=0.5f+0.5f*pnoise(x/3300.0f,z/3300.0f,71u)*1.6f;      // 区域性的岩性差异
        float base=l_sat(0.08f+0.92f*hd*l_sat(0.30f+0.9f*reg)); float alp=l_ss(1150.0f,2100.0f,v+250.0f*pnoise(x/1900.0f,z/1900.0f,63u)); return std::max(base*(1.0f-0.35f*alp), 0.78f*alp);
    };
    for(int it=0; it<iters; it++){
        #pragma omp parallel for
        for(int j=0;j<N;j++) for(int i=0;i<N;i++) hard[j*N+i]=hardness(i,j,h[j*N+i]);
        // (a) priority-flood 填洼（带极小坡度）
        hf=h; fill(closed.begin(),closed.end(),0);
        priority_queue<PI,vector<PI>,greater<PI>> pq;
        for(int j=0;j<N;j++) for(int i=0;i<N;i++) if(i==0||j==0||i==N-1||j==N-1){ pq.push({hf[j*N+i],j*N+i}); closed[j*N+i]=1; rcv[j*N+i]=j*N+i; }
        int cnt=0;
        while(!pq.empty()){
            PI c=pq.top(); pq.pop(); int ci=c.second%N, cj=c.second/N; order[cnt++]=c.second;
            for(int k=0;k<8;k++){ int ni=ci+dxs[k], nj=cj+dys[k]; if(ni<0||nj<0||ni>=N||nj>=N) continue; int n=nj*N+ni; if(closed[n]) continue;
                closed[n]=1; float e=c.first+1e-3f*dls[k]; if(hf[n]<e) hf[n]=e; pq.push({hf[n],n}); }
        }
        // (b) 接收者：最陡下降
        #pragma omp parallel for
        for(int j=1;j<N-1;j++) for(int i=1;i<N-1;i++){ int c=j*N+i; float best=0; int br=c;
            for(int k=0;k<8;k++){ int n=(j+dys[k])*N+i+dxs[k]; float s=(hf[c]-hf[n])/dls[k]; 
                s *= 1.0f+0.25f*((hash2(i*8+k,j,SEED+it)&1023)/1023.0f-0.5f);   // 随机化，减弱 D8 的方向性
                if(s>best){best=s;br=n;} }
            rcv[c]=br; }
        // (c) 汇水面积：order 是填洼后从低到高的出队顺序，反向累积
        fill(A.begin(),A.end(),DX*DX);
        for(int q=N*N-1;q>=0;q--){ int c=order[q]; int r=rcv[c]; if(r!=c) A[r]+=A[c]; }
        // (d) 隐式 stream power，从低到高
        float dt=25.0f; float Kbase = 3.0e-2f;
        float ramp = min(1.0f, (it+1)/15.0f);
        for(int q=0;q<N*N;q++){ int c=order[q]; int r=rcv[c]; if(r==c) continue;
            int ci=c%N,cj=c/N,ri=r%N,rj=r/N; float L=DX*sqrtf((float)((ci-ri)*(ci-ri)+(cj-rj)*(cj-rj)));
            float hu = h[c] + upl[c]*dt*ramp;
            if(h[c] < hf[c]-0.01f){ h[c] = hf[c]; continue; }   // 洼地：淤平，之后由出口溯源下切
            float K = Kbase*(1.7f-1.4f*hard[c]);
            float cc = K*dt*ramp*powf(A[c],0.45f)/L;
            float hn = (hu+cc*h[r])/(1.0f+cc);
            h[c]=max(min(hn,hu), max(h0[c]-maxinc[c], lowb[c]));
        }
        // (e) 崩塌（坡度限制，硬岩更陡）
        for(int sweep=0;sweep<(it==iters-1?60:8);sweep++){
            #pragma omp parallel for
            for(int j=1;j<N-1;j++) for(int i=1+((j+sweep)&1);i<N-1;i+=2){ int c=j*N+i; 
                float tal = 0.40f+1.05f*hard[c]*hard[c];      // tan(22°)…tan(55°)
                for(int k=0;k<8;k++){ int n=(j+dys[k])*N+i+dxs[k]; float ex=(h[c]-h[n])-tal*DX*dls[k];
                    if(ex>0){ float m=0.22f*ex; h[c]-=m; h[n]+=m; } } }
        }
        if(it%10==0||it==iters-1){ double mn=1e9,mx=-1e9; for(float v:h){mn=min(mn,(double)v);mx=max(mx,(double)v);} fprintf(stderr,"iter %d  h=[%.0f, %.0f]\n",it,mn,mx); }
    }
    // 设计约束：湖盆、堰塞垄、河廊
    #pragma omp parallel for
    for(int j=0;j<N;j++) for(int i=0;i<N;i++){
        float x=WORLD_X0+(i+0.5f)*DX, z=WORLD_Z0+(j+0.5f)*DX; float &v=h[j*N+i];
        float d=lakeSD(x,z);
        if(x>DAM_X-600.0f && x<RIVER_X0+400.0f){
            float gate = l_ss(DAM_X-600.0f,DAM_X-100.0f,x);
            if(d<0){ float bed=-min(26.0f, 0.3f+(-d)*0.085f+(-d)*(-d)*0.0002f); v=l_mix(v,min(v,bed),gate); }
            else { float sh=0.45f+d*0.035f; float w=l_ss(320.0f,60.0f,d)*gate; v=l_mix(v, max(v, sh), w); if(d<40.0f) v=l_mix(v, sh+(v-sh)*l_ss(0.0f,40.0f,d), gate); }
        }
        if(d>=0){ float g2=l_ss(DAM_X-700.0f,DAM_X-60.0f,x); v=max(v, 0.45f-420.0f*(1.0f-g2)); }
        if(x>RIVER_X0-50.0f && x<7000.0f){
            float dd=fabsf(z-riverZ(x))/sqrtf(1.0f+riverDz(x)*riverDz(x));
            float bank=riverBed(x)+1.3f;
            // 瀑布以下：峡谷段两侧必须高于河床；瀑布以上：同理
            float w=l_ss(170.0f,70.0f,dd)*l_ss(RIVER_X0-30.0f,RIVER_X0+70.0f,x);
            v=l_mix(v, max(v,bank+0.004f*dd), w);
            // 森林河廊段（非峡谷）近河处不要比河岸高太多，保证开阔
            if(x<GORGE_X0-60.0f){ float w2=l_ss(60.0f,20.0f,dd)*l_ss(GORGE_X0-60.0f,GORGE_X0-200.0f,x); v=l_mix(v, min(v,bank+1.2f+0.03f*dd), w2); }
        }
    }
    // 轻微平滑一次，去掉单像素沟
    { vector<float> t=h; 
      #pragma omp parallel for
      for(int j=1;j<N-1;j++) for(int i=1;i<N-1;i++){ int c=j*N+i; t[c]=0.5f*h[c]+0.125f*(h[c-1]+h[c+1]+h[c-N]+h[c+N]); } h.swap(t); }
    vector<float> fl(N*N); for(int i=0;i<N*N;i++) fl[i]=log10f(A[i]);
    FILE*f=fopen(out,"wb"); int n=N; fwrite(&n,4,1,f); fwrite(h.data(),4,N*N,f); fwrite(fl.data(),4,N*N,f); fclose(f);
    fprintf(stderr,"wrote %s\n",out); return 0;
}
