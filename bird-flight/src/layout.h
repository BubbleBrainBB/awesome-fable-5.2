// layout.h — 世界布局的标量函数，C++ 主机与 Metal 着色器共用（只用标量 float 运算）
// 坐标：x 向东，y 向上，z 向南；湖面 y = 0；单位：米
#ifndef LAYOUT_H
#define LAYOUT_H
#define WORLD_X0 (-14000.0f)
#define WORLD_Z0 (-12000.0f)
#define WORLD_SIZE 24000.0f
#define RIVER_X0 1000.0f      // 河口（入湖处）
#define GORGE_X0 1260.0f      // 峡谷入口
#define FALL_X 1440.0f        // 瀑布
#define FALL_TOP 78.0f        // 瀑布顶河床高
#define FALL_BOT 15.0f        // 瀑布底水潭河床高
#define DAM_X (-1800.0f)      // 湖西端堰塞垄
#define HERO_X 1337.0f        // 栖树（崖边老桦）

static inline float l_sat(float x){ return x<0.0f?0.0f:(x>1.0f?1.0f:x); }
static inline float l_ss(float a,float b,float x){ float t=l_sat((x-a)/(b-a)); return t*t*(3.0f-2.0f*t); }
static inline float l_mix(float a,float b,float t){ return a+(b-a)*t; }

// 河道中心线 z = Rz(x)（x >= RIVER_X0 有效，向东为上游）
static inline float riverZ(float x){
    float a = l_ss(RIVER_X0-40.0f, RIVER_X0+120.0f, x);
    float g = 1.0f - 0.45f*l_ss(GORGE_X0-60.0f, GORGE_X0+60.0f, x);
    float m = 17.0f*sin(x/31.0f+0.6f) + 6.0f*sin(x/13.5f+2.1f) + 110.0f*sin(x/1400.0f-0.75f);
    return a*g*m + l_ss(1900.0f,3400.0f,x)*(330.0f*sin((x-1900.0f)/1250.0f) + 90.0f*sin(x/520.0f));
}
static inline float riverDz(float x){ return (riverZ(x+1.0f)-riverZ(x-1.0f))*0.5f; }
// 河床高度
static inline float riverBed(float x){
    float y = l_mix(-1.2f, 0.4f, l_ss(RIVER_X0-120.0f, RIVER_X0+60.0f, x));
    y += 6.0f*l_sat((x-RIVER_X0-60.0f)/(GORGE_X0-RIVER_X0-60.0f));
    float gt = l_sat((x-GORGE_X0)/(FALL_X-GORGE_X0));
    y += (FALL_BOT-6.4f)*gt*(0.5f+0.5f*gt);
    // 峡谷内的小跌水（台阶）
    y += 1.5f*(l_ss(1308.0f,1311.0f,x)+l_ss(1356.0f,1359.0f,x)+l_ss(1401.0f,1404.5f,x)) - 4.5f*gt;
    y += (FALL_TOP-FALL_BOT)*l_ss(FALL_X-1.2f, FALL_X+1.6f, x);
    y += 260.0f*l_sat((x-FALL_X)/4200.0f);
    return y;
}
// 河床半宽
static inline float riverHalfW(float x){
    float w = l_mix(8.0f, 4.2f, l_ss(RIVER_X0, RIVER_X0+150.0f, x));
    w *= 1.0f + 0.22f*sin(x/11.0f+1.3f) + 0.10f*sin(x/4.7f);
    w = l_mix(w, 4.7f+0.7f*sin(x/9.0f), l_ss(GORGE_X0-50.0f, GORGE_X0+40.0f, x));
    w += 9.0f*exp(-(x-FALL_X+12.0f)*(x-FALL_X+12.0f)/200.0f)*l_ss(FALL_X+1.0f,FALL_X-2.0f,x); // 瀑布下水潭
    w = l_mix(w, 4.5f, l_ss(FALL_X, FALL_X+40.0f, x));
    return w;
}
// 河水深
static inline float riverDepth(float x){
    float d = 0.50f + 1.4f*exp(-(x-FALL_X+12.0f)*(x-FALL_X+12.0f)/220.0f)*l_ss(FALL_X+1.0f,FALL_X-2.0f,x);
    return d;
}

// 把河道/峡谷刻进宏观地形。h：宏观高度；返回刻蚀后的高度。
static inline float l_smin(float a,float b,float k){ float hh=l_sat(0.5f+0.5f*(b-a)/k); return l_mix(b,a,hh)-k*hh*(1.0f-hh); }
static inline float riverDist(float x,float z){ float dz=riverDz(x); return (z-riverZ(x))/sqrt(1.0f+dz*dz); }   // 有符号横向距离
static inline float carveH(float h,float x,float z){
    if(x<RIVER_X0-160.0f || x>7200.0f) return h;
    float sd=riverDist(x,z); float d=fabs(sd);
    if(d>240.0f) return h;
    // 让瀑布崖壁在平面上不是一条直线
    float xw = x + 2.6f*sin(sd*0.21f+1.0f) + 1.1f*sin(sd*0.53f+0.3f) - 0.012f*sd*sd*l_ss(60.0f,0.0f,d);
    float bed=riverBed(xw); float w=riverHalfW(x);
    float gor=l_ss(GORGE_X0-40.0f,GORGE_X0+40.0f,x)*l_ss(FALL_X+30.0f,FALL_X-3.0f,xw);
    float e=d-w;
    float k=l_mix(0.75f,6.5f,gor);
    float prof = bed + (e>0.0f ? e*k*(e/(e+l_mix(1.2f,0.5f,gor))) : -0.30f*(1.0f-(d/w)*(d/w)));
    float fade=l_ss(RIVER_X0-160.0f,RIVER_X0-40.0f,x)*l_ss(7200.0f,6800.0f,x);
    float c=l_smin(h,prof,l_mix(1.2f,2.5f,gor));
    return l_mix(h,c,fade);
}
// 水面高度（无水处返回 -1e4）。湖：y=0；河：河床+水深
static inline float waterLevel(float x,float z){
    float wl=-1.0e4f;
    if(x>DAM_X-80.0f && x<RIVER_X0+40.0f) wl=0.0f;
    if(x>RIVER_X0-30.0f && x<7000.0f){
        float sd=riverDist(x,z); float d=fabs(sd);
        float xw = x + 2.6f*sin(sd*0.21f+1.0f) + 1.1f*sin(sd*0.53f+0.3f) - 0.012f*sd*sd*l_ss(60.0f,0.0f,d);
        if(d<riverHalfW(x)+6.0f){ float r=riverBed(xw)+riverDepth(x); wl = wl>r?wl:r; }
    }
    return wl;
}
#endif
