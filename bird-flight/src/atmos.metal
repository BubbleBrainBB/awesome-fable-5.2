// atmos.metal — 物理大气：天空 LUT、太阳透射率、环境光立方、空气透视
constant float RE = 6360.0e3f;
constant float RA = 6460.0e3f;
constant float3 BR = float3(5.802e-6f, 13.558e-6f, 33.1e-6f);
constant float BMS = 3.996e-6f;
constant float BME = 4.44e-6f;
constant float3 BO = float3(0.650e-6f, 1.881e-6f, 0.085e-6f);
constant float ALT0 = 1100.0f;        // 湖面海拔
constant float HAZE_K = 15.0f;        // 谷地霾（相对海平面 Mie 的倍数，700 m 海拔处）
constant float HAZE_H = 650.0f;
constant float SKY_W = 256.0f;
constant float SKY_H = 128.0f;

static inline float3 atmDens(float h){   // h: 海拔（m）→ (瑞利, 米氏+霾, 臭氧)
    float hz = HAZE_K*exp(-max(h-700.0f,0.0f)/HAZE_H);
    return float3(exp(-h/8000.0f), exp(-h/1200.0f)+hz, max(0.0f,1.0f-fabs(h-25000.0f)/15000.0f));
}
static inline float phaseR(float mu){ return 3.0f/(16.0f*PI)*(1.0f+mu*mu); }
static inline float phaseM(float mu,float g){ float g2=g*g; return 3.0f/(8.0f*PI)*((1.0f-g2)*(1.0f+mu*mu))/((2.0f+g2)*pow(max(1.0f+g2-2.0f*g*mu,1e-4f),1.5f)); }
static inline float phaseHG(float mu,float g){ float g2=g*g; return (1.0f-g2)/(4.0f*PI*pow(max(1.0f+g2-2.0f*g*mu,1e-4f),1.5f)); }

// p: 以地心为原点；返回到大气顶的太阳透射率
static float3 sunTransP(float3 p, float3 sd){
    float b=dot(p,sd); float c0=dot(p,p)-RE*RE; float disc=b*b-c0;
    if(disc>0.0f && (-b-sqrt(disc))>0.0f) return float3(0.0f);
    float tA=-b+sqrt(max(b*b-(dot(p,p)-RA*RA),0.0f));
    float3 od=float3(0.0f); const int N=24;
    for(int i=0;i<N;i++){ float x0=float(i)/float(N), x1=float(i+1)/float(N); float xm=0.5f*(x0+x1);
        float t=tA*xm*xm; float ds=tA*(x1*x1-x0*x0);
        float h=length(p+sd*t)-RE; od+=atmDens(h)*ds; }
    return exp(-(BR*od.x+BME*od.y+BO*od.z));
}
static inline float3 skyDirFromUV(float2 uv){
    float az=(uv.x-0.5f)*2.0f*PI; float v=uv.y*2.0f-1.0f; float el=sign(v)*v*v*(PI*0.5f);
    return float3(cos(el)*cos(az), sin(el), cos(el)*sin(az));
}
static inline float2 skyUV(float3 d){
    float az=atan2(d.z,d.x); float el=asin(clamp(d.y,-1.0f,1.0f));
    float v=sign(el)*sqrt(fabs(el)/(PI*0.5f));
    return float2(az/(2.0f*PI)+0.5f, v*0.5f+0.5f);
}
// 天空 LUT：相机高度处、各方向的单次散射 + 近似多次散射
kernel void skyK(texture2d<float,access::write> outT, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=uint(SKY_W)||gid.y>=uint(SKY_H)) return;
    float2 uv=(float2(gid)+0.5f)/float2(SKY_W,SKY_H);
    float3 d=skyDirFromUV(uv); float3 sd=normalize(u.sunDir.xyz);
    float camY=0.5f*(u.cam0.pos.y+u.cam1.pos.y);
    float3 p0=float3(0.0f,RE+ALT0+camY,0.0f);
    float b=dot(p0,d); float rg=RE+ALT0-420.0f;
    float tA=-b+sqrt(max(b*b-(dot(p0,p0)-RA*RA),0.0f));
    float dg=b*b-(dot(p0,p0)-rg*rg); float tmax=tA;
    if(dg>0.0f){ float tg=-b-sqrt(dg); if(tg>0.0f) tmax=tg; }
    float mu=dot(d,sd); float pr=phaseR(mu), pm=phaseM(mu,0.78f);
    float3 L=float3(0.0f), T=float3(1.0f); const int N=40;
    for(int i=0;i<N;i++){ float x0=float(i)/float(N), x1=float(i+1)/float(N); float xm=0.5f*(x0+x1);
        float t=tmax*xm*xm; float ds=tmax*(x1*x1-x0*x0);
        float3 p=p0+d*t; float h=length(p)-RE; float3 dn=atmDens(h);
        float3 ext=BR*dn.x+BME*dn.y+BO*dn.z; float3 Ts=sunTransP(p,sd);
        float3 sR=BR*dn.x, sM=float3(BMS*dn.y);
        float3 J=(sR*pr+sM*pm)*Ts + (sR+sM)*(Ts*0.035f+float3(0.004f,0.006f,0.010f)*sat(sd.y*4.0f+0.6f));   // 后项：多次散射近似
        float3 Tstep=exp(-ext*ds);
        L+=T*J*(1.0f-Tstep)/max(ext,float3(1e-12f)); T*=Tstep;
    }
    outT.write(float4(L*u.sunI.xyz, lum(T)), gid);
}
// tSunT: 第 0 行：太阳透射率随高度（y=-400..3000）；第 1 行：前 6 格为环境光立方（+x,-x,+y,-y,+z,-z）
kernel void sunTK(texture2d<float,access::write> outT, texture2d<float,access::sample> tSky, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=64u||gid.y>=2u) return;
    float3 sd=normalize(u.sunDir.xyz);
    if(gid.y==0u){ float y=-400.0f+float(gid.x)/63.0f*3400.0f; float3 T=sunTransP(float3(0.0f,RE+ALT0+y,0.0f),sd); outT.write(float4(T*u.sunI.xyz,1.0f),gid); return; }
    if(gid.x>=7u){ outT.write(float4(0.0f),gid); return; }
    float3 acc=float3(0.0f); float wsum=0.0f;
    if(gid.x==6u){ // 全天平均辐亮度（上半球）
        for(int j=0;j<8;j++) for(int i=0;i<24;i++){ float el=(float(j)+0.5f)/8.0f*PI*0.5f; float az=(float(i)+0.5f)/24.0f*2.0f*PI;
            float3 d=float3(cos(el)*cos(az),sin(el),cos(el)*sin(az)); float w=cos(el); acc+=tSky.sample(sl,skyUV(d),level(0.0f)).xyz*w; wsum+=w; }
        outT.write(float4(acc/wsum,1.0f),gid); return; }
    float3 ax=float3(0.0f); int a=int(gid.x)>>1; float sg=(gid.x&1u)?-1.0f:1.0f; if(a==0) ax.x=sg; else if(a==1) ax.y=sg; else ax.z=sg;
    for(int j=0;j<16;j++) for(int i=0;i<32;i++){ float el=((float(j)+0.5f)/16.0f-0.5f)*PI; float az=(float(i)+0.5f)/32.0f*2.0f*PI;
        float3 d=float3(cos(el)*cos(az),sin(el),cos(el)*sin(az)); float cw=max(dot(d,ax),0.0f); float w=cw*cos(el);
        float3 Ls=tSky.sample(sl,skyUV(d),level(0.0f)).xyz; if(d.y<0.0f) Ls=float3(0.05f,0.045f,0.035f)*lum(tSky.sample(sl,skyUV(float3(d.x,0.3f,d.z)),level(0.0f)).xyz)*2.0f;   // 地面反照的粗略替代
        acc+=Ls*w; wsum+=w; }
    outT.write(float4(acc/wsum*PI,1.0f),gid);    // 辐照度
}
