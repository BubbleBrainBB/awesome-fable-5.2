// common.metal — 哈希、噪声、小工具（全部解析计算，不经纹理过滤）
constant float PI = 3.14159265f;
constexpr sampler sl(filter::linear, address::clamp_to_edge);
constexpr sampler sr(filter::linear, address::repeat);

static inline uint hu(uint x){ x^=x>>16; x*=0x7feb352dU; x^=x>>15; x*=0x846ca68bU; x^=x>>16; return x; }
static inline float u2f(uint h){ return float(h>>8)*(1.0f/16777216.0f); }
static inline uint h2u(int2 p){ return hu(uint(p.x)*0x9E3779B1u ^ hu(uint(p.y)*0x85EBCA77u)); }
static inline uint h3u(int3 p){ return hu(uint(p.x)*0x9E3779B1u ^ hu(uint(p.y)*0x85EBCA77u ^ hu(uint(p.z)*0xC2B2AE3Du))); }
static inline float hash1(float n){ return u2f(hu(uint(int(n*7919.0f))+0x68bc21ebu)); }
static inline float sat(float x){ return clamp(x,0.0f,1.0f); }
static inline float3 sat3(float3 x){ return clamp(x,0.0f,1.0f); }
static inline float sq(float x){ return x*x; }
static inline float remap(float v,float a,float b,float c,float d){ return c+(v-a)/(b-a)*(d-c); }
static inline float lum(float3 c){ return dot(c,float3(0.2126f,0.7152f,0.0722f)); }

// 值噪声 [0,1]
static float vn2(float2 p){
    float2 i=floor(p), f=p-i; f=f*f*f*(f*(f*6.0f-15.0f)+10.0f); int2 ip=int2(i);
    float a=u2f(h2u(ip)), b=u2f(h2u(ip+int2(1,0))), c=u2f(h2u(ip+int2(0,1))), d=u2f(h2u(ip+int2(1,1)));
    return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);
}
// 值噪声及其梯度：x=值, yz=d/dx,d/dz
static float3 vn2d(float2 p){
    float2 i=floor(p), f=p-i; int2 ip=int2(i);
    float2 u=f*f*f*(f*(f*6.0f-15.0f)+10.0f); float2 du=30.0f*f*f*(f*(f-2.0f)+1.0f);
    float a=u2f(h2u(ip)), b=u2f(h2u(ip+int2(1,0))), c=u2f(h2u(ip+int2(0,1))), d=u2f(h2u(ip+int2(1,1)));
    float k0=a,k1=b-a,k2=c-a,k3=a-b-c+d;
    return float3(k0+k1*u.x+k2*u.y+k3*u.x*u.y, du.x*(k1+k3*u.y), du.y*(k2+k3*u.x));
}
static float vn3(float3 p){
    float3 i=floor(p), f=p-i; f=f*f*(3.0f-2.0f*f); int3 ip=int3(i);
    float a=u2f(h3u(ip)), b=u2f(h3u(ip+int3(1,0,0))), c=u2f(h3u(ip+int3(0,1,0))), d=u2f(h3u(ip+int3(1,1,0)));
    float e=u2f(h3u(ip+int3(0,0,1))), g=u2f(h3u(ip+int3(1,0,1))), h=u2f(h3u(ip+int3(0,1,1))), k=u2f(h3u(ip+int3(1,1,1)));
    return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y), mix(mix(e,g,f.x),mix(h,k,f.x),f.y), f.z);
}
static float fbm2(float2 p,int oct){
    float a=0.5f,s=0.0f; for(int i=0;i<oct;i++){ s+=a*vn2(p); p=float2(p.x*1.62f+p.y*1.18f, -p.x*1.18f+p.y*1.62f)+17.3f; a*=0.5f; } return s/(1.0f-a*2.0f);
}
static float fbm3(float3 p,int oct){
    float a=0.5f,s=0.0f; for(int i=0;i<oct;i++){ s+=a*vn3(p); p=p*2.03f+float3(11.1f,3.7f,7.9f); a*=0.5f; } return s/(1.0f-a*2.0f);
}
// 2D Worley（返回到最近特征点的距离与单元哈希）
static float2 worley2(float2 p){
    float2 i=floor(p), f=p-i; float md=9.0f; float id=0.0f;
    for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++){ int2 c=int2(i)+int2(x,y); uint h=h2u(c);
        float2 o=float2(u2f(h),u2f(hu(h+77u))); float2 d=float2(x,y)+o-f; float dd=dot(d,d); if(dd<md){md=dd; id=u2f(hu(h+991u));} }
    return float2(sqrt(md),id);
}
// 交错梯度噪声（逐像素抖动）
static inline float ign(float2 px, float frame){ px+=5.588238f*frame; return fract(52.9829189f*fract(0.06711056f*px.x+0.00583715f*px.y)); }
static inline float3 rotY(float3 p,float a){ float c=cos(a),s=sin(a); return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z); }
static inline float smin(float a,float b,float k){ float h=sat(0.5f+0.5f*(b-a)/k); return mix(b,a,h)-k*h*(1.0f-h); }
static inline float sdCapsule(float3 p,float3 a,float3 b,float ra,float rb){
    float3 pa=p-a, ba=b-a; float h=sat(dot(pa,ba)/dot(ba,ba)); return length(pa-ba*h)-mix(ra,rb,h);
}
static inline float sdEllipsoid(float3 p,float3 r){ float k0=length(p/r); float k1=length(p/(r*r)); return k0*(k0-1.0f)/max(k1,1e-6f); }
