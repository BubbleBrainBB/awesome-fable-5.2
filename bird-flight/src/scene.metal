// scene.metal — 场景几何（地形、森林、水）与着色
struct Ctx {
    texture2d<float,access::sample> tH;
    texture2d<float,access::sample> tTreeA;
    texture2d<float,access::sample> tTreeCol;
    texture2d<float,access::sample> tTreeBase;
    texture2d<float,access::sample> tMaxTree;
    texture2d<float,access::sample> tAux;
    texture2d<float,access::sample> tSky;
    texture2d<float,access::sample> tShadow;
    texture2d<float,access::sample> tSunT;
    texture3d<float,access::sample> tCloud;
    float3 heroPos; float tm; float dip; float shake; float heroOn; float heroLod;
};
struct HeroHit { float d; int part; float3 lp; float id; float2 luv; };   // part: 0 树干/粗枝 1 细枝 2 叶 3 芽
constant float HERO_R = 7.2f;
static HeroHit heroSD(float3 p,float tm,float dip,float shake,float lod);
constant float LOD0 = 650.0f;      // 单株树 → 冠层的过渡距离
constant float LOD1 = 1700.0f;
constant float TMAX = 120000.0f;

static inline float2 w2uv(float2 xz){ return (xz-float2(WORLD_X0,WORLD_Z0))*(1.0f/WORLD_SIZE); }

// ---------- 域外远山（解析），与域内平滑衔接 ----------
static float Hfar(float2 xz){
    float2 q=xz/9000.0f;
    float n=0.0f, a=1.0f; float2 p=q;
    for(int i=0;i<4;i++){ float v=1.0f-fabs(vn2(p)*2.0f-1.0f); n+=a*v*v; p=float2(p.x*1.7f+p.y*1.1f,-p.x*1.1f+p.y*1.7f)+3.1f; a*=0.5f; }
    n/=1.875f;
    float west=smoothstep(-13000.0f,-26000.0f,xz.x);           // 西面：先低地，后远山
    float wr=smoothstep(-24000.0f,-48000.0f,xz.x);
    float inValley=smoothstep(5200.0f,1500.0f,fabs(xz.y+900.0f*sin(xz.x/9000.0f)));
    float lowl=-345.0f+40.0f*vn2(xz/2300.0f);
    float mtn=300.0f+2300.0f*n;
    float h=mix(mtn, mix(lowl, -300.0f+(600.0f+1500.0f*wr)*n*n*1.6f, wr*0.85f+0.15f*smoothstep(0.5f,0.8f,n)*west), inValley*smoothstep(1000.0f,-8000.0f,xz.x));
    return h;
}
static inline float edgeBlend(float2 xz){   // 0 域内, 1 域外
    float2 a=xz-float2(WORLD_X0,WORLD_Z0); float2 b=float2(WORLD_SIZE)-a; float m=min(min(a.x,a.y),min(b.x,b.y));
    return 1.0f-smoothstep(0.0f,1600.0f,m);
}
// 粗高度：硬件双线性（只用于步进）
static float Hc(const Ctx& c, float2 xz){
    float h=c.tH.sample(sl,w2uv(xz),level(0.0f)).x;
    float e=edgeBlend(xz); if(e>0.0f) h=mix(h,Hfar(xz),e);
    return carveH(h,xz.x,xz.y);
}
// 精确双线性（逐纹素读取，避免 8 位过滤权重造成的台阶）
static float Hraw(const Ctx& c, float2 xz){
    float2 g=w2uv(xz)*float(TH_N)-0.5f; float2 fl=floor(g); float2 f=g-fl; int2 i=clamp(int2(fl),int2(0),int2(TH_N-2)); f=clamp(g-float2(i),0.0f,1.0f);
    float a=c.tH.read(uint2(i)).x, b=c.tH.read(uint2(i+int2(1,0))).x, cc=c.tH.read(uint2(i+int2(0,1))).x, d=c.tH.read(uint2(i+int2(1,1))).x;
    float h=mix(mix(a,b,f.x),mix(cc,d,f.x),f.y);
    float e=edgeBlend(xz); if(e>0.0f) h=mix(h,Hfar(xz),e);
    return h;
}
static inline bool inRiverBox(float3 p){ return p.x>RIVER_X0-170.0f && p.x<7200.0f && fabs(p.z)<900.0f; }
// 含细节的高度（近处细节随距离淡出）
static float Hd(const Ctx& c, float2 xz, float t){
    float hm=Hraw(c,xz); float h=carveH(hm,xz.x,xz.y);
    float cut=hm-h;
    float fade=1.0f-smoothstep(200.0f,600.0f,t);
    if(cut>0.8f && fade>0.0f){
        float sd=riverDist(xz.x,xz.y); float e=fabs(sd)-riverHalfW(xz.x);
        if(e<0.5f) h+=0.16f*(vn2(xz/0.8f)-0.5f)*fade;
    }
    float f2=1.0f-smoothstep(40.0f,140.0f,t);
    if(f2>0.0f){
        h+=f2*(0.09f*(vn2(xz/1.1f)-0.5f)+0.035f*(vn2(xz/0.29f)-0.5f));
        // 草丛的丘状起伏（只在陆地且非河床处）
        if(h>0.35f && cut<0.5f){ float tu=vn2(xz/0.42f); float pat=vn2(xz/3.1f); h+=f2*0.26f*tu*tu*smoothstep(0.25f,0.6f,pat)*smoothstep(0.35f,1.2f,h); }
    }
    return h;
}
// 峡谷岩壁的三维位移：水平岩层 + 块体 + 竖向节理（返回向外凸出的量，米）
static float rockDisp(float3 p){
    float y=p.y+3.0f*vn2(p.xz/23.0f);
    float bed=vn2(float2(y/1.7f,0.5f));
    float bed2=vn2(float2(y/0.45f,7.5f));
    float n=vn3(p*float3(0.17f,0.45f,0.17f));
    float n2=vn3(p*float3(0.8f,1.3f,0.8f));
    float cr=fabs(vn3(p*float3(0.42f,0.10f,0.42f))-0.5f);
    float n3=vn3(p*float3(3.1f,5.0f,3.1f));
    return 2.1f*smoothstep(0.38f,0.62f,bed)*(0.35f+0.65f*n)+1.5f*n+0.30f*n2+0.28f*smoothstep(0.35f,0.65f,bed2)+0.07f*n3-1.4f*smoothstep(0.05f,0.0f,cr);
}
// 峡谷岩壁掩膜（0 无，1 完全）
static float gorgeMask(const Ctx& c,float3 p,thread float& sdOut){
    sdOut=0.0f;
    if(p.x<GORGE_X0-45.0f||p.x>FALL_X+30.0f) return 0.0f;
    float sd=riverDist(p.x,p.z); sdOut=sd; float ad=fabs(sd); if(ad>70.0f) return 0.0f;
    float e=ad-riverHalfW(p.x);
    float m=smoothstep(GORGE_X0-45.0f,GORGE_X0+25.0f,p.x)*smoothstep(FALL_X+30.0f,FALL_X+8.0f,p.x)*smoothstep(70.0f,45.0f,ad);
    // 瀑布头墙：河床内也算岩壁
    float head=smoothstep(FALL_X-7.0f,FALL_X-2.0f,p.x)*smoothstep(FALL_BOT+1.5f,FALL_BOT+4.0f,p.y);
    return m*max(sat(e/1.2f),head);
}

// ---------- 树 ----------
// 叶团：三维格点上的随机小球（近处的树冠由一团团叶簇组成，边缘有真实的缺口）
static float clusterSD(float3 wp,float cell){
    float3 g=wp/cell-0.5f; float3 i0=floor(g); float d=1e9f;
    for(int k=0;k<8;k++){ int3 ci=int3(i0)+int3(k&1,(k>>1)&1,k>>2); uint h=h3u(ci);
        float3 o=float3(u2f(h),u2f(hu(h+1u)),u2f(hu(h+2u))); float r=cell*(0.34f+0.27f*u2f(hu(h+3u)));
        float3 ctr=(float3(ci)+0.25f+0.5f*o)*cell; d=min(d,length(wp-ctr)-r); }
    return d;
}
// 小叶簇：三维格点上随机朝向的扁椭球（一簇叶 = 一个小枝梢）。sid 返回最近一簇的随机数
static float spraySD(float3 wp,float cell,float flat,thread float& sid){
    float3 g=wp/cell-0.5f; float3 i0=floor(g); float d=1e9f; sid=0.5f;
    for(int k=0;k<8;k++){ int3 ci=int3(i0)+int3(k&1,(k>>1)&1,k>>2); uint h=h3u(ci); uint h1=hu(h+1u), h2=hu(h+2u), h3=hu(h+3u);
        float3 o=float3(u2f(h),u2f(h1),u2f(h2)); float r=cell*(0.40f+0.24f*u2f(h3));
        float3 q=wp-(float3(ci)+0.25f+0.5f*o)*cell;
        float3 nn=normalize(float3(o.z-0.5f,0.40f,o.x-0.5f));          // 叶簇平面的法向：大体朝上，随机倾斜
        float hg=dot(q,nn); float3 qr=q-nn*hg;
        float k0=length(qr+nn*(hg/flat))/r; float k1=length(qr+nn*(hg/(flat*flat)))/(r*r);
        float dd=(k0<1.0f)?(k0-1.0f)*r*flat:k0*(k0-1.0f)/max(k1,1e-6f);
        if(dd<d){ d=dd; sid=u2f(hu(h+4u)); } }
    return d;
}
// 单片叶：小格点里一片随机朝向的薄圆片。lid 随机数，ln 叶面法向
static float leafSD(float3 wp,float cell,thread float& lid,thread float3& ln){
    float3 g=wp/cell-0.5f; float3 i0=floor(g); float d=1e9f; lid=0.5f; ln=float3(0.0f,1.0f,0.0f);
    for(int k=0;k<8;k++){ int3 ci=int3(i0)+int3(k&1,(k>>1)&1,k>>2); uint h=h3u(ci+int3(91,7,33)); uint h1=hu(h+1u), h2=hu(h+2u), h3=hu(h+3u);
        if((h3&255u)<56u) continue;
        float3 o=float3(u2f(h),u2f(h1),u2f(h2));
        float3 q=wp-(float3(ci)+0.3f+0.4f*o)*cell;
        float3 nn=normalize(float3(u2f(hu(h+4u))-0.5f,(u2f(hu(h+5u))-0.5f)*0.9f,u2f(hu(h+6u))-0.5f)+1e-4f);
        float hg=dot(q,nn); float rr=length(q-nn*hg); float r=cell*(0.34f+0.15f*u2f(hu(h+7u)));
        float dd=max(fabs(hg)-0.0012f,rr-r);
        if(dd<d){ d=dd; lid=u2f(hu(h+8u)); ln=nn; } }
    return d;
}
// 枝端叶团：树的局部坐标里每格一团，团心落在树冠包络椭球内才存在；枝从树干斜伸到团心
static float blobCrown(float3 q,float3 ec,float3 er,float cs,float fl,float sd,float drop,bool branches,thread float3& bc,thread float& dBr){
    float3 off=float3(sd*13.1f+1.7f,sd*7.3f,sd*29.7f+4.1f); int salt=int(sd*31.0f+0.5f)*7919;
    float3 g=(q+off)/cs-0.5f; float3 i0=floor(g); float d=1e9f; float dn=1e9f; dBr=1e9f; bc=ec;
    for(int k=0;k<8;k++){ int3 ci=int3(i0)+int3(k&1,(k>>1)&1,k>>2); uint h=h3u(ci+int3(salt,0,0));
        if(u2f(hu(h+3u))<drop) continue;
        float3 o=float3(u2f(h),u2f(hu(h+1u)),u2f(hu(h+2u)));
        float3 ctr=(float3(ci)+0.5f+(o-0.5f)*0.6f)*cs-off;
        float3 e=(ctr-ec)/er; float ee=dot(e,e); if(ee>1.0f) continue;
        float r=cs*(0.36f+0.16f*u2f(hu(h+4u)))*(1.0f-0.35f*ee);
        float lr=length(ctr.xz); r=min(r,3.45f-lr); if(r<0.3f) continue;
        float3 rr=float3(r,r*fl,r); float3 v=q-ctr; float k0=length(v/rr);
        float dd=(k0<1.0f)?(k0-1.0f)*r*min(fl,1.0f):k0*(k0-1.0f)/max(length(v/(rr*rr)),1e-6f);
        if(dd<dn){ dn=dd; bc=ctr; }
        d=smin(d,dd,0.30f);
        if(branches){ float ya=max(ctr.y-0.75f*lr-0.35f,ec.y*0.42f); float kk=sq(sat(ya/ec.y)); float3 a=float3(ec.x*kk,ya,ec.z*kk);
            dBr=min(dBr,sdCapsule(q,a,ctr,0.030f+0.014f*lr,0.018f)); }
    }
    return d;
}
// 针叶树的轮生枝：每层 5–7 根下垂的枝，枝上的叶成扁平的一片；层与层之间露出树干
static float whorlCrown(float3 q,float H,float R,float y0,float pw,float sd){
    float ang=atan2(q.z,q.x); float ts=0.50f+0.024f*H; float d=1e9f;
    float t0=floor(q.y/ts);
    for(int a=0;a<3;a++){ float ti=t0+float(a);
        float hT=hash1(ti*1.7f+sd*91.0f+3.0f);
        float yA=(ti+0.25f+0.5f*hT)*ts; float uu=(yA-y0)/(H-y0); if(uu<0.0f||uu>0.97f) continue;
        float prof=R*pow(1.0f-uu,pw)*(0.55f+0.45f*smoothstep(0.0f,0.12f,uu));
        float nb=5.0f+floor(hT*2.99f); float ph=hT*6.2832f;
        float af=(ang-ph)/6.2832f*nb; float s0=floor(af);
        for(int b=0;b<2;b++){ float si=s0+float(b); float im=si-nb*floor(si/nb);
            float hb=hash1(im*3.1f+ti*17.3f+sd*57.0f), hb2=hash1(im*5.7f+ti*3.3f+sd*11.0f+1.0f);
            float phi=ph+(si+(hb-0.5f)*0.5f)/nb*6.2832f;
            float L=prof*(0.60f+0.40f*hb2); if(L<0.15f) continue;
            float droop=L*(0.15f+0.25f*hb)*(1.1f-0.6f*uu);
            float3 B=float3(cos(phi)*L,-droop,sin(phi)*L); float3 pa=float3(q.x,q.y-yA,q.z);
            float h=clamp(dot(pa,B)/dot(B,B),0.10f,1.0f); float3 v=pa-B*h;
            float w=(0.09f+0.20f*L)*(1.0f-0.80f*h*h)*smoothstep(0.02f,0.30f,h)+0.04f;
            float dd=(length(float3(v.x,(v.y+0.25f*w)*2.0f,v.z))-w)*0.55f;       // 竖向压扁，叶略垂在枝下
            d=min(d,dd); }
    }
    d=min(d,sdCapsule(q,float3(0.0f,H-1.7f,0.0f),float3(0.0f,H,0.0f),0.17f,0.03f));   // 顶梢
    return d;
}
struct TreeEval { float part; float env; float3 bc; float sid; float3 ln; float lk; };
// 单株树。envOnly：只要树冠包络（给透光厚度估计用）
static float treeSD(float3 q,float H,int sp,float sd,float3 wp,float t,bool envOnly,thread TreeEval& te){
    te.part=0.0f; te.env=1e9f; te.bc=float3(0.0f,H*0.6f,0.0f); te.sid=0.5f; te.ln=float3(0.0f,1.0f,0.0f); te.lk=0.0f;
    float r=length(q.xz);
    float db=max(r-3.55f,max(q.y-H-0.6f,-q.y-1.0f)); if(db>1.2f) return db;
    float dTr=1e9f, dCr=1e9f, dBr=1e9f;
    float lodS=1.0f-smoothstep(70.0f,260.0f,t), lodB=1.0f-smoothstep(380.0f,800.0f,t);
    float2 lean=float2(sin(sd*40.0f),cos(sd*33.0f));
    bool conif=(sp==0||sp==4);
    if(conif){                      // 云杉 / 落叶松：圆锥形，轮生枝层
        float R=min(2.7f,H*(sp==0?0.150f:0.165f)*(0.85f+0.3f*sd)); float y0=H*(0.10f+0.10f*sd); float pw=(sp==0?0.80f:0.62f);
        float uu=sat((q.y-y0)/(H-y0));
        float prof=R*pow(1.0f-uu,pw)*smoothstep(-0.02f,0.07f,uu);
        float dCone=max((r-prof*0.82f)*0.72f,max(y0-q.y,q.y-H));
        dCr=dCone; te.env=dCone;
        if(lodB>0.0f && dCone<1.6f){ float dW=whorlCrown(q,H,R,y0,pw,sd); dCr=mix(dCone,min(dW,3.0f),lodB); }
        te.bc=float3(0.0f,q.y-0.8f,0.0f);
        dTr=max(r-(0.07f+0.008f*H)*(1.0f-0.6f*sat(q.y/H)), q.y-H*0.92f);
    } else if(sp<=3){               // 阔叶 / 桦杨 / 松：枝端叶团
        float R,cs,fl,drop,hT; float3 ec,er;
        if(sp==2){ R=min(3.0f,H*0.2f+0.9f)*(0.85f+0.15f*fract(sd*13.7f)); ec=float3(lean.x*0.5f,H*0.64f,lean.y*0.5f); er=float3(R,H*0.33f,R); cs=1.5f; fl=0.70f; drop=0.18f; hT=H*0.70f; }
        else if(sp==3){ R=min(2.3f,H*0.14f+0.4f); ec=float3(lean.x*0.35f,H*0.63f,lean.y*0.35f); er=float3(R*0.9f,H*0.35f,R*0.9f); cs=1.2f; fl=0.95f; drop=0.30f; hT=H*0.80f; }
        else { R=min(2.7f,H*0.21f); ec=float3(lean.x*0.5f,H*0.82f,lean.y*0.5f); er=float3(R,H*0.16f,R); cs=1.5f; fl=0.55f; drop=0.30f; hT=H*0.84f; }
        float dEl=sdEllipsoid(q-ec,er);
        dCr=dEl; te.bc=ec;
        if(lodB>0.0f && dEl<1.5f){ float3 bc; float dB=blobCrown(q,ec,er,cs,fl,sd,drop,(!envOnly && t<260.0f),bc,dBr); te.bc=mix(bc,ec,0.45f); dCr=mix(dEl+0.2f,min(dB,3.0f),lodB); }
        te.env=dCr;
        float2 tcx=ec.xz*sq(sat(q.y/ec.y));
        dTr=max(length(q.xz-tcx)-((0.08f+0.008f*H)*(1.0f-0.6f*sat(q.y/hT))+0.10f*exp(-max(q.y,0.0f)*1.5f)), q.y-hT);
    } else {                        // 枯立木：断干与残枝
        float bend=0.4f*sin(q.y*0.3f+sd*9.0f);
        dTr=max(length(q.xz-float2(bend,0.0f))-0.17f*(1.0f-0.75f*sat(q.y/(H*0.6f))), q.y-H*0.6f);
        if(t<300.0f){ float ts=1.3f; float ti=floor(q.y/ts);
            for(int a=0;a<2;a++){ float tj=ti+float(a)-0.0f; float hh=hash1(tj*3.3f+sd*77.0f); float yA=(tj+0.5f)*ts; if(hh<0.3f||yA>H*0.55f||yA<H*0.12f) continue;
                float phi=hh*40.0f; float L=0.5f+1.5f*hash1(tj*7.1f+sd*31.0f); float3 A=float3(0.4f*sin(yA*0.3f+sd*9.0f),yA,0.0f);
                dTr=min(dTr,sdCapsule(q,A,A+float3(cos(phi)*L,0.35f*L,sin(phi)*L),0.05f,0.015f)); } }
    }
    if(envOnly) return te.env;
    if(dCr<1.3f){                   // 叶丛：远处用噪声位移，近处换成一簇簇扁平的叶
        float n=vn3(wp*0.75f)-0.5f; float amp=(sp==1?0.9f:0.7f)*(1.0f-0.35f*lodB);
        float n2=(vn3(wp*2.1f+7.0f)-0.5f)*(1.0f-smoothstep(250.0f,600.0f,t));
        float dFar=dCr+n*amp+n2*0.38f;
        if(lodS>0.0f){
            float sid; float cl=spraySD(wp,conif?0.42f:((sp==3)?0.40f:0.52f),conif?0.50f:0.30f,sid);
            float dNear=max(dCr-0.10f+n*0.25f,cl);
            float lodL=1.0f-smoothstep(16.0f,34.0f,t);          // 很近时：叶簇里再分出一片片叶
            if(lodL>0.0f && !conif && dNear<0.3f){ float lid; float3 ln; float lf=leafSD(wp,0.085f,lid,ln); dNear=mix(dNear,max(dNear-0.04f,lf),lodL); te.ln=ln; te.lk=lodL; sid=mix(sid,fract(sid+lid),0.5f*lodL); }
            dFar=mix(dFar,dNear,lodS); te.sid=sid;
        }
        dCr=dFar*0.72f;
    }
    float dW=min(dTr,dBr);
    if(dW<dCr){ te.part=1.0f; return dW; }
    return dCr;
}
struct TreeHit { float H; int sp; float sd; float part; float3 q; float4 col; float env; float3 bc; float sid; float3 ln; float lk; };
static float forestSD(const Ctx& c, float3 p, float t, thread TreeHit& th, bool envOnly){
    float2 g=(p.xz-float2(WORLD_X0,WORLD_Z0))/TREE_CELL-0.5f; int2 i0=int2(floor(g));
    float d=1e9f; th.env=1e9f; th.part=0.0f; th.sid=0.5f; th.ln=float3(0.0f,1.0f,0.0f); th.lk=0.0f; th.H=10.0f; th.sp=0; th.sd=0.0f; th.q=float3(0.0f); th.bc=float3(0.0f); th.col=float4(0.0f);
    for(int k=0;k<4;k++){
        int2 ci=i0+int2(k&1,k>>1); if(ci.x<0||ci.y<0||ci.x>=TT_N||ci.y>=TT_N) continue;
        float4 A=c.tTreeA.read(uint2(ci)); if(A.x<=0.0f) continue;
        float base=c.tTreeBase.read(uint2(ci)).x;
        float2 ctr=(float2(ci)+0.5f+(A.zw-0.5f)*TREE_JIT)*TREE_CELL+float2(WORLD_X0,WORLD_Z0);
        float3 q=float3(p.x-ctr.x,p.y-base,p.z-ctr.y);
        int code=int(A.y*255.0f+0.5f); float H=A.x*40.0f; int sp=code&7; float sd=float(code>>3)/31.0f;
        TreeEval te; float dd=treeSD(q,H,sp,sd,p,t,envOnly,te);
        if(dd<d){ d=dd; th.H=H; th.sp=sp; th.sd=sd; th.part=te.part; th.q=q; th.col=float4(float2(ci),0.0f,0.0f); th.env=te.env; th.bc=te.bc; th.sid=te.sid; th.ln=te.ln; th.lk=te.lk; }
    }
    return d;
}
static float canopyH(const Ctx& c,float2 xz){
    float h=c.tTreeA.sample(sl,w2uv(xz),level(0.0f)).x*40.0f;
    return h*(0.66f+0.24f*vn2(xz/3.3f));
}

// ---------- 瀑布水帘（离壁抛出的水舌）、河中巨石、倒木 ----------
static float fallSD(float3 p,float tm,thread float2& fuv){
    float zf=riverZ(FALL_X); float lipX=FALL_X-2.2f; float top=FALL_TOP+0.45f;
    float drop=max(top-p.y,0.0f); float tt=sqrt(2.0f*drop/9.81f);
    float xj=lipX-1.6f*tt-0.25f;                         // 水舌中心线：水平初速约 1.6 m/s
    float wz=2.3f+0.022f*drop+0.6f*smoothstep(40.0f,62.0f,drop);
    float th=0.30f+0.012f*drop;
    float dz=p.z-zf; fuv=float2(dz,drop);
    // 水帘被拉成一缕一缕
    float st=vn2(float2(dz*2.6f,drop*0.10f-tm*(1.8f+0.35f*tt)))+0.5f*vn2(float2(dz*7.0f,drop*0.33f-tm*(4.0f+1.2f*tt)));
    float d=max(fabs(p.x-xj)-th*(0.55f+0.6f*st),fabs(dz)-wz*(0.80f+0.20f*vn2(float2(drop*0.08f-tm*0.9f,3.0f))));
    d=max(d,p.y-top); d=max(d,(FALL_BOT+0.3f)-p.y);
    return d*0.6f;
}
static float riverRocksSD(float3 p,thread float& rid){
    // 河道里的巨石：沿河每 6.5 m 一格
    float sd=riverDist(p.x,p.z); float d=1e9f; rid=0.0f;
    float fi=floor(p.x/6.5f);
    for(int k=0;k<2;k++){ float i=fi+float(k)-((fract(p.x/6.5f)<0.5f)?1.0f:0.0f)+ (k==0?0.0f:0.0f); i=fi+float(k)-((fract(p.x/6.5f)<0.5f)?1.0f:0.0f);
        float h0=hash1(i*1.13f+2.0f), h1=hash1(i*2.71f+5.0f), h2=hash1(i*0.57f+9.0f);
        if(h0>0.42f) continue;
        float cx=(i+0.3f+0.4f*h1)*6.5f; float w=riverHalfW(cx); float lat=(h2*2.0f-1.0f)*(w+0.8f);
        float r=0.45f+1.0f*h1*h1; float cy=riverBed(cx)+r*0.35f;
        float3 ctr=float3(cx,cy,riverZ(cx)+lat);
        float3 q=p-ctr; q=rotY(q,h2*6.0f);
        float dd=sdEllipsoid(q,float3(r*(1.0f+0.5f*h0),r*0.72f,r*(0.8f+0.4f*h2)));
        if(dd<d){ d=dd; rid=h1; }
    }
    { // 一块需要跃过的大石（x≈1215）
      float cx=1215.0f; float3 ctr=float3(cx,riverBed(cx)+0.5f,riverZ(cx)+0.4f); float dd=sdEllipsoid(rotY(p-ctr,0.6f),float3(2.3f,1.25f,1.7f)); if(dd<d){ d=dd; rid=0.77f; } }
    if(d<1.2f) d+=(vn3(p*1.3f)-0.5f)*0.35f+(vn3(p*4.7f)-0.5f)*0.08f;
    return d*0.8f;
}
static float logSD(float3 p){
    // 横在河上的倒木（x≈1168）：一头搭在北岸，略斜
    float cx=1168.0f; float zc=riverZ(cx); float wl=riverBed(cx)+riverDepth(cx);
    float3 a=float3(cx-1.6f,wl+3.9f,zc-8.5f), b=float3(cx+1.4f,wl+2.9f,zc+8.0f);
    float d=sdCapsule(p,a,b,0.34f,0.24f);
    // 残枝
    d=min(d,sdCapsule(p,mix(a,b,0.3f),mix(a,b,0.3f)+float3(0.3f,1.5f,0.2f),0.07f,0.03f));
    d=min(d,sdCapsule(p,mix(a,b,0.62f),mix(a,b,0.62f)+float3(-0.5f,1.1f,-0.3f),0.06f,0.025f));
    d=min(d,sdCapsule(p,mix(a,b,0.8f),mix(a,b,0.8f)+float3(0.2f,-0.9f,0.5f),0.05f,0.02f));
    return d;
}

// ---------- 灌丛与散石（解析格点，不占纹理） ----------
// 返回距离；kind: 0 灌丛, 1 石；info: 随机数
static float groundCoverSD(const Ctx& c,float3 p,float hG,float treeD,thread float& kind,thread float& info){
    float d=1e9f; kind=0.0f; info=0.0f;
    float dens=vn2(p.xz/37.0f+11.0f);
    // 灌丛：格距 2.4 m
    { const float G=2.4f; float2 g=p.xz/G-0.5f; float2 i0=floor(g);
      for(int k=0;k<4;k++){ int2 ci=int2(i0)+int2(k&1,k>>1); uint h=h2u(ci+int2(977,131)); float h0=u2f(h), h1=u2f(hu(h+1u)), h2=u2f(hu(h+2u)), h3=u2f(hu(h+3u));
          if(h0>0.04f+0.30f*smoothstep(0.45f,0.80f,dens)+0.10f*smoothstep(0.5f,0.8f,vn2(p.xz/9.0f))-0.2f*sat(treeD*3.0f)) continue;
          float2 ctr=(float2(ci)+0.5f+(float2(h1,h2)-0.5f)*0.5f)*G; float r=0.28f+0.80f*h3*h3*h3;
          float3 q=float3(p.x-ctr.x,p.y-hG-r*0.45f,p.z-ctr.y);
          float dd=sdEllipsoid(q,float3(r,r*0.62f,r));
          if(dd<0.7f){ dd+=(vn3(p*1.9f+5.0f)-0.5f)*0.5f*r; float sid; dd=max(dd-0.14f,spraySD(p,0.27f,0.42f,sid)); }
          if(dd<d){ d=dd; kind=0.0f; info=h3+floor(h1*4.0f); } } }
    // 散石：格距 5.1 m
    { const float G=5.1f; float2 g=p.xz/G-0.5f; float2 i0=floor(g);
      for(int k=0;k<4;k++){ int2 ci=int2(i0)+int2(k&1,k>>1); uint h=h2u(ci+int2(31,557)); float h0=u2f(h), h1=u2f(hu(h+1u)), h2=u2f(hu(h+2u)), h3=u2f(hu(h+3u));
          if(h0>0.16f+0.25f*smoothstep(0.5f,0.8f,vn2(p.xz/90.0f+3.0f))) continue;
          float2 ctr=(float2(ci)+0.5f+(float2(h1,h2)-0.5f)*0.6f)*G; float r=0.22f+0.85f*h3*h3*h3;
          float3 q=rotY(float3(p.x-ctr.x,p.y-hG-r*0.15f,p.z-ctr.y),h1*6.0f);
          float dd=sdEllipsoid(q,float3(r*(1.0f+0.6f*h2),r*0.6f,r));
          if(dd<0.6f) dd+=(vn3(p*2.3f)-0.5f)*0.28f*r+(vn3(p*8.0f)-0.5f)*0.04f;
          if(dd<d){ d=dd; kind=1.0f; info=h3; } } }
    return d*0.75f;
}

// ---------- 场景距离场 ----------
// mat: 0 地形, 1 树叶, 2 树干, 3 远处冠层, 4 水, 5 峡谷岩壁, 6-9 主角树, 11 河中石, 12 倒木, 13 灌丛, 14 散石
static float mapScene(const Ctx& c, float3 p, float t, thread int& mat){
    float hC=Hc(c,p.xz); float above=p.y-hC; mat=0;
    bool rb=inRiverBox(p);
    float d;
    if(rb){
        float sdv; float gm=gorgeMask(c,p,sdv);
        if(gm>0.0f){
            // 峡谷：用保守的距离估计 + 三维岩石位移
            float hm=Hraw(c,p.xz); float hcv=carveH(hm,p.x,p.z); float cut=hm-hcv; above=p.y-hcv;
            float wm=gm*smoothstep(0.8f,4.0f,cut)*smoothstep(1.5f,6.0f,hm-p.y+4.0f);
            float dTrue=above*mix(0.45f,0.16f,sat(wm*2.0f));
            if(wm>0.0f && dTrue<5.5f){ dTrue-=rockDisp(p)*wm; dTrue*=0.8f; if(wm>0.08f) mat=5; }
            d=dTrue;
        } else if(above<1.2f){ above=p.y-Hd(c,p.xz,t); d=above*0.40f; }
        else d=(above-0.4f)*0.5f;
        if(p.y<470.0f){ float wl=waterLevel(p.x,p.z); if(wl>-1.0e3f){ float dW=(p.y-wl)*((fabs(p.x-FALL_X)<8.0f)?0.10f:0.6f);
            if(p.x>FALL_X-9.0f && p.x<FALL_X+4.0f && p.y>FALL_BOT+2.5f && p.y<FALL_TOP-0.2f) dW=1e9f;      // 头墙处不画"竖直水面"，由水帘代替
            if(dW<d){ d=dW; mat=4; } } }
        if(p.x>RIVER_X0+20.0f && p.x<FALL_X+5.0f){
            if(fabs(sdv)<16.0f || fabs(riverDist(p.x,p.z))<16.0f){
                float rid; float dR=riverRocksSD(p,rid); if(dR<d){ d=dR; mat=11; }
                if(fabs(p.x-1168.0f)<4.0f){ float dL=logSD(p); if(dL<d){ d=dL; mat=12; } }
            }
        }
    } else {
        if(above<0.9f){ above=p.y-Hd(c,p.xz,t); d=above*((t<140.0f)?0.40f:0.55f); }
        else d=(above-0.45f)*0.55f;
    }
    // 灌丛与散石：只在近处（<260 m）、陆地、离地 2 m 内
    if(t<200.0f && above<1.9f && hC>0.4f && mat==0){
        float2 uvg=w2uv(p.xz); float treeD=c.tTreeA.sample(sl,uvg,level(0.0f)).x;
        bool nearWater=rb && fabs(riverDist(p.x,p.z))<riverHalfW(p.x)+1.0f;
        if(!nearWater){ float kind,info; float dG=groundCoverSD(c,p,p.y-above,treeD,kind,info); float fd=smoothstep(200.0f,130.0f,t); dG=mix(1e3f,dG,step(0.001f,fd))+ (1.0f-fd)*1.2f;
            if(dG<d){ d=dG; mat=(kind>0.5f)?14:13; } else d=min(d,max(dG,0.25f)); }
    }
    float tm=c.tMaxTree.sample(sl,w2uv(p.xz),level(0.0f)).x*40.0f;
    if(tm>0.5f){
        float av=p.y-hC;
        if(av<tm+1.0f){
            float k=smoothstep(LOD0,LOD1,t); float dF=1e9f; int m2=1;
            if(k<1.0f){ TreeHit th; dF=forestSD(c,p,t,th,false); m2=(th.part>0.5f)?2:1; dF=min(dF,0.26f*TREE_CELL); }
            if(k>0.0f){ float hcan=canopyH(c,p.xz); float dC=(av-hcan)*0.5f; if(hcan<0.4f) dC=1e9f;
                if(k>=1.0f){ dF=dC; m2=3; } else { dF=mix(dF,min(dC,8.0f),k); if(k>0.5f) m2=3; } }
            if(dF<d){ d=dF; mat=m2; }
        } else d=min(d,(av-tm)*0.5f+0.4f);
    }
    if(c.heroOn>0.5f){
        float3 hp=p-c.heroPos; float db=length(hp-float3(-0.6f,6.8f,0.3f))-HERO_R;
        if(db<0.6f){ HeroHit hh=heroSD(hp,c.tm,c.dip,c.shake,c.heroLod); if(hh.d<d){ d=hh.d; mat=6+hh.part; } }
        else d=min(d,db);
    }
    return d;
}

struct Hit { float t; int mat; };
static Hit marchScene(const Ctx& c, float3 ro, float3 rd, float tmin, float tmax, int maxSteps){
    Hit h; h.t=-1.0f; h.mat=-1;
    float t=tmin; float tPrev=tmin; int mat=0;
    for(int i=0;i<maxSteps;i++){
        float3 p=ro+rd*t; if(p.y>3900.0f && rd.y>0.0f) break;
        float d=mapScene(c,p,t,mat);
        if(d<0.0f){   // 越过了表面：二分
            float a=tPrev,b=t; for(int k=0;k<6;k++){ float m=0.5f*(a+b); int mm; float dm=mapScene(c,ro+rd*m,m,mm); if(dm<0.0f) b=m; else { a=m; } }
            int mm; mapScene(c,ro+rd*b,b,mm); h.t=a; h.mat=mm; return h;
        }
        if(d<0.0004f*t+((mat>=6)?0.0002f:0.002f)){ h.t=t; h.mat=mat; return h; }
        tPrev=t; t+=d; if(t>tmax) break;
    }
    return h;
}

// ---------- 光照辅助 ----------
static inline float3 sunColAt(const Ctx& c,float y){ return c.tSunT.sample(sl,float2((clamp((y+400.0f)/3400.0f,0.0f,1.0f)*63.0f+0.5f)/64.0f,0.25f),level(0.0f)).xyz; }
static inline float3 ambCube(const Ctx& c,float3 n){
    float3 n2=n*n;
    float3 ax=c.tSunT.read(uint2(n.x>0.0f?0u:1u,1u)).xyz, ay=c.tSunT.read(uint2(n.y>0.0f?2u:3u,1u)).xyz, az=c.tSunT.read(uint2(n.z>0.0f?4u:5u,1u)).xyz;
    return n2.x*ax+n2.y*ay+n2.z*az;
}
static inline float3 skyAvg(const Ctx& c){ return c.tSunT.read(uint2(6u,1u)).xyz; }
static inline float3 skyLUT(const Ctx& c,float3 d){ float2 uv=skyUV(d); uv.y=clamp(uv.y,0.5f/SKY_H,1.0f-0.5f/SKY_H); return c.tSky.sample(sr,uv,level(0.0f)).xyz; }
// 宏观地形阴影（太阳方向的阴影高度图）
static float macroShadow(const Ctx& c,float3 p){
    float hs=c.tShadow.sample(sl,w2uv(p.xz),level(0.0f)).x;
    return smoothstep(-3.0f,6.0f,p.y-hs);
}
// 林冠尺度的阴影：沿太阳方向在"冠层高度场"上走几步（低太阳时，树的下半截被前面的林子挡住）
static float canopyShadow(const Ctx& c,float3 p,float3 L){
    float T=1.0f; float s=7.0f;
    for(int i=0;i<9;i++){
        float3 q=p+L*s; float2 uv=w2uv(q.xz);
        float hc=c.tTreeA.sample(sl,uv,level(0.0f)).x*40.0f;
        if(hc>0.5f){ float g=c.tH.sample(sl,uv,level(0.0f)).x; T*=1.0f-0.62f*smoothstep(-1.5f,3.5f,g+hc*0.9f-q.y); }
        s*=1.48f; if(T<0.03f) break;
    }
    return T;
}
static float nearShadow(const Ctx& c,float3 p,float3 L,float t,float s0){
    float res=1.0f; float s=s0; int m;
    for(int i=0;i<((s0<0.1f)?26:16);i++){
        float d=mapScene(c,p+L*s,t,m);
        res=min(res,9.0f*d/s); if(res<0.02f) return 0.0f;
        s+=clamp(d,min(0.5f,0.2f*s+0.004f)+0.01f*s,6.0f+0.2f*s); if(s>220.0f) break;
    }
    return sat(res);
}

// ---------- 空气透视 ----------
// 返回透射率 T（rgb），并输出内散射
static float3 aerial(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float d,thread float3& insc){
    float h0=ro.y+ALT0; float dy=rd.y*d;
    float3 Hs=float3(8000.0f,1200.0f,HAZE_H);
    float3 a=dy/Hs; float3 f=float3(1.0f);
    for(int i=0;i<3;i++){ float ai=a[i]; f[i]=(fabs(ai)<1e-3f)?(1.0f-0.5f*ai):((1.0f-exp(-ai))/ai); }
    float3 e0=float3(exp(-h0/8000.0f),exp(-h0/1200.0f),HAZE_K*exp(-max(h0-700.0f,0.0f)/HAZE_H));
    float3 col=e0*f*d;                       // 柱密度：瑞利, 米氏, 霾
    float3 tauR=BR*col.x; float tauMs=BMS*(col.y+col.z); float tauMe=BME*(col.y+col.z);
    float3 tau=tauR+float3(tauMe);
    float3 T=exp(-tau);
    float3 sd=normalize(u.sunDir.xyz); float mu=dot(rd,sd);
    float ym=ro.y+0.5f*dy;
    float3 sunC=sunColAt(c,ym);
    float3 wR=tauR/max(tau,float3(1e-9f)); float3 wM=float3(tauMs)/max(tau,float3(1e-9f));
    // 霾里的山影：取路径中点的宏观阴影
    float shm=0.35f+0.65f*macroShadow(c,ro+rd*(d*0.5f));
    float3 J=sunC*shm*(wR*phaseR(mu)+wM*(0.75f*phaseM(mu,0.76f)+0.25f*phaseM(mu,0.2f)))+skyAvg(c)*(wR+wM)*0.9f;
    insc=J*(1.0f-T);
    return T;
}

// ---------- 云 ----------
constant float DECK_Y0 = 240.0f;
constant float DECK_Y1 = 475.0f;
constant float SEA_Y0 = -350.0f;
constant float SEA_Y1 = -95.0f;
static float cloudCover(float3 p,int layer){
    if(layer==0){
        float n=fbm2(p.xz/2300.0f+float2(3.3f,1.7f),3); float n2=vn2(p.xz/900.0f+5.0f);
        float edgeZ=-330.0f-0.40f*max(-200.0f-p.x,0.0f)-0.45f*max(p.x-1150.0f,0.0f)+240.0f*(n2-0.5f);
        float north=smoothstep(edgeZ+260.0f,edgeZ-700.0f,p.z);
        float east=smoothstep(4300.0f,6800.0f,p.x);
        float south=smoothstep(3000.0f,5000.0f,p.z)*smoothstep(-7000.0f,-2500.0f,p.x);
        float tongue=exp(-(sq(p.x-530.0f)+sq(p.z+480.0f))/sq(340.0f));
        north=max(north,tongue*1.15f);
        float cov=max(north,max(east,south))*1.05f+(n-0.5f)*0.50f*step(0.02f,max(north,max(east,south)));
        cov*=smoothstep(-11000.0f,-6000.0f,p.x);
        return sat(cov);
    }
    float n=fbm2(p.xz/3300.0f+float2(9.1f,4.2f),3);
    return sat(smoothstep(-3000.0f,-6500.0f,p.x)*1.05f+(n-0.5f)*0.35f-0.25f*smoothstep(-3000.0f,-1000.0f,p.x));
}
// 穿云通道：沿俯冲航线在云里留出一条不规则的"云谷"
static float tubeCut(constant Uni& u,float3 p){
    float best=1e9f; float rad=0.0f;
    for(int i=0;i<7;i++){ float4 a=u.tube[i], b=u.tube[i+1]; if(b.w<=0.0f) break;
        float3 pa=p-a.xyz, ba=b.xyz-a.xyz; float h=sat(dot(pa,ba)/dot(ba,ba)); float d=length(pa-ba*h); if(d<best){ best=d; rad=mix(a.w,b.w,h); } }
    if(best>400.0f) return 0.0f;
    float wob=(vn3(p/38.0f)-0.5f)*1.5f+(vn3(p/13.0f)-0.5f)*0.55f;
    return 1.0f-smoothstep(0.55f,1.6f,best/max(rad,1.0f)+wob);
}
// 返回消光系数（1/m）
static float cloudDens(const Ctx& c,constant Uni& u,float3 p,bool cheap){
    float d=0.0f; float3 w=float3(u.tp.x*3.0f,0.0f,u.tp.x*1.2f);
    if(p.y>DECK_Y0 && p.y<DECK_Y1){
        float hf=(p.y-DECK_Y0)/(DECK_Y1-DECK_Y0);
        float cov=cloudCover(p,0); if(cov<=0.01f) return 0.0f;
        float3 pw=p+w;
        if(!cheap){ float4 wr=c.tCloud.sample(sr,pw/float3(1700.0f,900.0f,1700.0f)+0.37f,level(0.0f)); pw+=(float3(wr.g,wr.b,wr.a)-0.5f)*float3(260.0f,90.0f,260.0f); }
        float4 nz=c.tCloud.sample(sr,pw/float3(2500.0f,820.0f,2500.0f),level(0.0f));
        float topH=0.50f+0.45f*nz.g+0.16f*(nz.b-0.5f);
        float prof=smoothstep(0.0f,0.13f,hf)*(1.0f-smoothstep(topH-0.30f,topH,hf));
        float base=(0.30f+0.70f*nz.r)*prof;
        d=sat(remap(base,1.0f-cov*0.9f,1.0f,0.0f,1.0f)*2.6f);
        if(d>0.0f){
            float cut=tubeCut(u,p); d*=1.0f-0.97f*cut;
        }
        if(d>0.0f && !cheap){
            float4 dz=c.tCloud.sample(sr,(pw+w*0.6f)/float3(390.0f,300.0f,390.0f),level(0.0f));
            float det=dz.g*0.50f+dz.b*0.30f+dz.a*0.20f;
            d=sat(remap(d,(1.0f-det)*0.80f*(0.45f+0.55f*hf),1.0f,0.0f,1.0f));
            if(d>0.0f){ float4 dw=c.tCloud.sample(sr,(pw+w*1.2f)/float3(87.0f),level(0.0f)); d=sat(d-(1.0f-dw.g)*0.26f*(1.0f-d*0.7f)); }
        }
        d*=0.042f;
    } else if(p.y>SEA_Y0 && p.y<SEA_Y1){
        float hf=(p.y-SEA_Y0)/(SEA_Y1-SEA_Y0);
        float cov=cloudCover(p,1); if(cov<=0.01f) return 0.0f;
        float4 nz=c.tCloud.sample(sr,(p+w*0.4f)/float3(4200.0f,1500.0f,4200.0f),level(0.0f));
        float prof=smoothstep(1.0f,0.25f+0.5f*nz.r,hf);
        d=sat(remap((0.55f+0.45f*nz.r)*prof,1.0f-cov,1.0f,0.0f,1.0f)*2.5f);
        if(d>0.0f && !cheap){ float4 dz=c.tCloud.sample(sr,(p+w)/float3(700.0f),level(0.0f)); d=sat(remap(d,(1.0f-dz.g)*0.4f,1.0f,0.0f,1.0f)); }
        d*=0.030f;
    }
    return d;
}
// 体积步进一段 [ta,tb]；累积到 L / T
static void marchSlab(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float ta,float tb,float jit,int N,float3 sunC,float3 ambT,float3 ambB,float y0,float y1,
                      thread float3& L,thread float& T,thread float& tw,thread float& ww){
    if(tb<=ta) return;
    float3 sd=normalize(u.sunDir.xyz); float mu=dot(rd,sd);
    tb=min(tb,ta+26000.0f);
    bool nearCam=(ta<60.0f); float kexp=nearCam?5.0f:1.6f; float ek=exp(kexp)-1.0f;
    for(int i=0;i<N;i++){
        float x0=float(i)/float(N), x1=float(i+1)/float(N); float xm=x0+(x1-x0)*jit;
        float t=ta+(tb-ta)*(exp(kexp*xm)-1.0f)/ek;
        float ds=(tb-ta)*(exp(kexp*x1)-exp(kexp*x0))/ek;
        float3 p=ro+rd*t;
        float sg=cloudDens(c,u,p,false);
        if(sg>0.0f){
            // 朝太阳的光学厚度：4 次取样，步长递增
            float od=0.0f; float st=35.0f; float tl=0.0f;
            for(int k=0;k<5;k++){ tl+=st*0.5f; od+=cloudDens(c,u,p+sd*tl,k>1)*st; tl+=st*0.5f; st*=2.3f; }
            float hf=sat((p.y-y0)/(y1-y0));
            // 多次散射近似（逐级衰减的 Beer 项）
            float sc=0.0f; float aa=1.0f, bb=1.0f, cg=1.0f;
            for(int k=0;k<3;k++){ sc+=aa*exp(-od*bb)*mix(phaseHG(mu,0.80f*cg),phaseHG(mu,-0.25f*cg),0.25f); aa*=0.55f; bb*=0.35f; cg*=0.5f; }
            float powder=1.0f-0.6f*exp(-sg*60.0f*2.0f)*sat(-mu*0.5f+0.5f);
            float3 amb=mix(ambB,ambT,hf)*(0.35f+0.65f*hf);
            float3 S=sunC*sc*powder*PI*1.0f+amb*0.32f;
            float a=1.0f-exp(-sg*ds);
            L+=T*a*S; tw+=T*a*t; ww+=T*a; T*=1.0f-a;
            if(T<0.012f){ T=0.0f; break; }
        }
    }
}
// 返回 rgb=云的出射光, a=透射率；tavg 输出云的平均距离
static float4 marchClouds(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float tmax,float jit,int N,thread float& tavg){
    float3 L=float3(0.0f); float T=1.0f; float tw=0.0f, ww=0.0f;
    float3 ambT=skyLUT(c,float3(0.0f,1.0f,0.0f))*2.2f+skyAvg(c)*1.2f; float3 ambB=skyAvg(c)*0.9f;
    // 两层云的进出距离
    float ta[2], tb[2]; float y0s[2]={DECK_Y0,SEA_Y0}, y1s[2]={DECK_Y1,SEA_Y1};
    for(int k=0;k<2;k++){
        float a,b;
        if(fabs(rd.y)<1e-5f){ if(ro.y>y0s[k]&&ro.y<y1s[k]){a=0.0f;b=tmax;} else {a=1.0f;b=0.0f;} }
        else { float t0=(y0s[k]-ro.y)/rd.y, t1=(y1s[k]-ro.y)/rd.y; a=max(min(t0,t1),0.0f); b=min(max(t0,t1),tmax); }
        ta[k]=a; tb[k]=b;
    }
    int first=(ta[0]<=ta[1])?0:1;
    for(int q=0;q<2;q++){ int k=(q==0)?first:1-first;
        if(tb[k]>ta[k] && T>0.0f){ float3 sunC=sunColAt(c,0.5f*(y0s[k]+y1s[k]));
            marchSlab(c,u,ro,rd,ta[k],tb[k],jit,N,sunC,ambT,ambB,y0s[k],y1s[k],L,T,tw,ww); } }
    tavg=(ww>1e-4f)?tw/ww:tmax;
    return float4(L,T);
}
// 云对地面的遮挡（粗略：沿太阳方向取云层中部一点）
static float cloudShadow(const Ctx& c,constant Uni& u,float3 p){
    float3 sd=normalize(u.sunDir.xyz); if(sd.y<0.02f) return 1.0f;
    float od=0.0f;
    for(int k=0;k<3;k++){ float y=DECK_Y0+(DECK_Y1-DECK_Y0)*(0.25f+0.25f*float(k)); if(p.y<y){ float3 q=p+sd*((y-p.y)/sd.y); od+=cloudDens(c,u,q,true)*(DECK_Y1-DECK_Y0)/3.0f/sd.y; } }
    return exp(-od*0.5f);
}

// ---------- 水 ----------
// 水面法线（湖：风浪；河：顺流的涟漪）
static float3 waterNormal(constant Uni& u,float3 p,float t,bool river,float rdy,thread float& foam,thread float& var){
    float tm=u.tp.x; foam=0.0f; var=0.0f; float fpe=t*0.0011f/max(fabs(rdy),0.03f);
    float2 g=float2(0.0f);
    float att=1.0f/(1.0f+t*0.004f);
    if(!river){
        // 平静区与被风吹皱区交替
        float gust=smoothstep(0.35f,0.7f,vn2(p.xz/170.0f+float2(tm*0.015f,0.0f)));
        float amp=mix(0.10f,1.0f,gust);
        float2 q=p.xz; float a=0.022f*amp; float fq=0.9f; float2 dir=float2(0.85f,0.52f);
        for(int i=0;i<5;i++){
            float fade=1.0f/(1.0f+sq(fpe*fq*0.9f));
            float3 n=vn2d(q*fq+dir*tm*(0.5f+0.25f*float(i))); g+=n.yz*fq*a*fade; var+=sq(fq*a)*(1.0f-fade*fade)*0.35f;
            q=float2(q.x*0.8f-q.y*0.6f,q.x*0.6f+q.y*0.8f); dir=float2(dir.x*0.8f-dir.y*0.6f,dir.x*0.6f+dir.y*0.8f); a*=0.62f; fq*=1.9f;
        }
        // 涟漪环（鱼跃、点水）
        for(int i=0;i<MAXRIP;i++){ float4 r=u.rip[i]; if(r.w<=0.0f) continue; float age=tm-r.z; if(age<0.0f||age>9.0f) continue;
            float2 dv=p.xz-r.xy; float dist=length(dv); float rad=age*0.55f+0.05f; float wdt=0.25f+0.12f*age;
            float e=(dist-rad)/wdt; float env=exp(-e*e)*r.w*exp(-age*0.45f)/(0.6f+rad);
            g+=dv/max(dist,1e-3f)*cos((dist-rad)*18.0f/(1.0f+0.3f*age))*env*0.45f; }
    } else {
        float dz=riverDz(p.x); float2 fl=normalize(float2(-1.0f,-dz));     // 向西（下游）
        float sd=riverDist(p.x,p.z); float w=riverHalfW(p.x);
        float slope=(riverBed(p.x+1.5f)-riverBed(p.x-1.5f))/3.0f;
        float spd=1.3f+14.0f*sat(slope);
        float2 q=float2(dot(p.xz,fl),sd);
        float a=0.030f+0.45f*sat(slope*4.0f); float fq=1.3f;
        for(int i=0;i<4;i++){ float3 n=vn2d(float2(q.x*0.45f-tm*spd*0.45f*(1.0f+0.2f*float(i)),q.y)*fq+float(i)*7.7f);
            float fade=1.0f/(1.0f+sq(fpe*fq*0.9f)); float2 gg=float2(n.y*0.45f,n.z)*fq*a*fade; g+=fl*gg.x+float2(-fl.y,fl.x)*gg.y; var+=sq(fq*a)*(1.0f-fade*fade)*0.3f; a*=0.6f; fq*=2.1f; }
        foam=sat(slope*3.0f-0.12f+0.5f*(vn2(float2(q.x*0.5f-tm*spd*0.5f,q.y)*1.6f)-0.5f)*sat(slope*8.0f+0.15f));
        foam=max(foam, smoothstep(0.25f,1.2f,slope));
        // 瀑布下的水潭：翻涌的白水
        float px=p.x-(FALL_X-6.0f); foam=max(foam, sat(1.2f-(px*px+sd*sd)/90.0f)*(0.55f+0.45f*vn2(p.xz*1.3f+tm*1.7f))*step(p.x,FALL_X));
    }
    return normalize(float3(-g.x,1.0f,-g.y));
}
