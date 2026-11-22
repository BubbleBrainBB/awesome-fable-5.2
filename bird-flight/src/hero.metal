// hero.metal — 近景主角：崖边老桦树（栖枝、细枝、叶、芽）、自己的翅膀、同伴燕子、瀑布水雾与虹
// 桦树局部坐标：原点在树根，x 东 y 上 z 南；栖点在 (-2.35, 7.60, 1.10)


static inline float2 sdSeg(float3 p,float3 a,float3 b){   // 返回 (到线段的距离, 参数 h)
    float3 pa=p-a, ba=b-a; float h=sat(dot(pa,ba)/dot(ba,ba)); return float2(length(pa-ba*h),h);
}
// 一段带叶的枝：沿枝每隔 SP 生一根下垂的细枝，细枝上交替着生叶片
static void leafyBranch(float3 p,float3 a,float3 b,float r0,float r1,float seed,float tm,float shake,float leafy,thread HeroHit& hh){
    float3 ba=b-a; float L=length(ba); float3 ax=ba/L; float3 pa=p-a; float s=clamp(dot(pa,ax),0.0f,L);
    float3 q=pa-ax*s; float dr=length(q)-mix(r0,r1,s/L);
    if(dr<hh.d){ hh.d=dr; hh.part=0; hh.lp=float3(s,atan2(q.z,q.x),seed); }
    if(leafy<=0.0f || dr>1.25f) return;
    const float SP=0.21f;
    float fi=floor(s/SP+0.5f);
    for(int k=-1;k<=1;k++){
        float i=fi+float(k); float sc=i*SP; if(sc<0.25f||sc>L-0.02f) continue;
        float h0=hash1(i*1.37f+seed*17.0f), h1=hash1(i*2.11f+seed*31.0f+5.0f), h2=hash1(i*0.73f+seed*7.0f+9.0f);
        if(h2>leafy) continue;
        // 细枝：先斜出再下垂，随风摆动
        float3 o=a+ax*sc; float ang=h0*6.2832f; float3 side=normalize(cross(ax,float3(0.0f,1.0f,0.0f)+0.001f)); float3 upv=cross(side,ax);
        float3 outd=normalize(side*cos(ang)+upv*sin(ang)*0.5f);
        float sway=0.07f*sin(tm*1.9f+h0*20.0f)+0.035f*sin(tm*4.3f+h1*30.0f)+shake*0.5f*sin(tm*23.0f+h0*9.0f);
        float len=0.26f+0.34f*h1;
        // 细枝分两段：先斜出、再下垂（有弧度，不是一根直线）
        float3 d0=normalize(outd*1.0f+ax*0.5f+float3(0.0f,-0.45f,0.0f)+float3(sway*0.5f,0.0f,sway*0.35f));
        float3 tdir=normalize(outd*0.55f+ax*(0.35f+0.5f*h1)+float3(0.0f,-0.95f,0.0f)+float3(sway,0.0f,sway*0.7f));
        float3 e0=o+outd*0.02f; float3 e=e0+d0*(0.10f+0.10f*h0); float3 tip=e+tdir*len;
        { float2 s0=sdSeg(p,e0,e); float d0s=s0.x-0.0034f; if(d0s<hh.d){ hh.d=d0s; hh.part=1; hh.lp=float3(0.0f,0.0f,seed); } }
        float2 st=sdSeg(p,e,tip); float dt=st.x-mix(0.0030f,0.0010f,st.y);
        if(dt<hh.d){ hh.d=dt; hh.part=1; hh.lp=float3(st.y,0.0f,seed); }
        if(st.x>0.11f) continue;
        // 叶：沿细枝每 LS 一片，左右交替
        const float LS=0.050f; float tu=st.y*len; float lj=floor(tu/LS+0.5f);
        for(int m=-1;m<=1;m++){
            float j=lj+float(m); float uc=j*LS; if(uc<0.02f||uc>len) continue;
            float g0=hash1(j*3.1f+i*7.7f+seed*13.0f), g1=hash1(j*1.9f+i*3.3f+seed*23.0f+2.0f);
            float3 lo=e+tdir*uc;
            float3 t1=normalize(cross(tdir,float3(0.3f,0.1f,1.0f))); float3 t2=cross(tdir,t1);
            float la=g0*6.2832f+j*2.4f; float3 ld=normalize(t1*cos(la)+t2*sin(la)+tdir*0.35f+float3(0.0f,-0.75f,0.0f));
            // 叶片颤动（像杨桦叶在风里翻动）
            float fl=0.45f*sin(tm*(6.0f+4.0f*g1)+g0*40.0f)*(0.35f+0.65f*g1)+shake*4.0f*sin(tm*31.0f+g1*50.0f);
            float3 lw=normalize(cross(ld,float3(0.0f,1.0f,0.0f)+float3(0.2f*g1,0.0f,0.1f))); float3 ln=cross(lw,ld);
            float3 lw2=lw*cos(fl)+ln*sin(fl); float3 ln2=ln*cos(fl)-lw*sin(fl);
            float sz=0.021f+0.012f*g1; float3 lc=lo+ld*(0.016f+sz);
            float3 lq=p-lc; float3 ll=float3(dot(lq,ld),dot(lq,lw2),dot(lq,ln2));     // 叶局部：x 沿叶长, y 宽, z 法向
            // 桦叶：卵状三角形，基部宽、先端尖；略沿中脉对折
            float xx=ll.x/sz; float wdt=0.80f*sz*sqrt(sat(1.0f-xx*xx))*(1.0f-0.35f*xx);
            float fold=0.25f*fabs(ll.y);
            float dl=max(max(fabs(ll.x)-sz,fabs(ll.y)-wdt)*0.7f,fabs(ll.z-fold)-0.0007f);
            if(dl<hh.d){ hh.d=dl; hh.part=2; hh.lp=ln2; hh.id=g0+floor(g1*8.0f); hh.luv=float2(xx,ll.y/max(wdt,1e-4f)); }
            // 叶柄
            float dp=sdSeg(p,lo,lo+ld*0.018f).x-0.0007f; if(dp<hh.d){ hh.d=dp; hh.part=1; hh.lp=float3(1.0f,0.0f,seed); }
        }
    }
}
// 主角树上部树冠的包络：许多小团，整体成卵形；避开栖点前方的视野
static float heroCrownEnv(float3 p,float tm,thread float& bid,thread float3& bc){
    float db=1e9f; bid=0.0f; bc=float3(0.15f,8.6f,0.0f);
    for(int i=0;i<22;i++){
        float fi=float(i); float a1=fi*2.399f; float hh1=hash1(fi*3.7f+1.0f), hh2=hash1(fi*5.3f+2.0f);
        float yy=6.4f+4.6f*((fi+0.5f)/22.0f); float rr=(0.55f+1.75f*sin(PI*sat((yy-5.9f)/5.6f)))*(0.55f+0.45f*hh1);
        float3 cc=float3(0.15f+cos(a1)*rr,yy,sin(a1)*rr);
        if(cc.x<-1.0f && cc.y<9.3f && cc.z>-1.0f) continue;          // 栖枝前方留空
        if(cc.z>0.5f && cc.y<9.9f) continue;                          // 进近航线一侧留空
        float r=0.46f+0.30f*hh2;
        cc.x+=0.04f*sin(tm*1.3f+fi); float3 rr3=float3(r,r*0.85f,r); float3 v=p-cc; float k0=length(v/rr3);
        float dd=(k0<1.0f)?(k0-1.0f)*r*0.85f:k0*(k0-1.0f)/max(length(v/(rr3*rr3)),1e-6f);
        if(dd<db){ db=dd; bid=0.25f+0.55f*hh1; bc=cc; } }
    return db;
}
// 主角树距离场。dip：栖枝下沉量（m）
static HeroHit heroSD(float3 p,float tm,float dip,float shake,float lod){
    HeroHit hh; hh.d=1e9f; hh.part=0; hh.lp=float3(0.0f); hh.id=0.0f; hh.luv=float2(0.0f);
    // 树干：略弯，向上渐细，约 6.6 m 处分叉
    { float y=clamp(p.y,-0.6f,8.6f); float3 c=float3(0.22f*sin(y*0.27f)+0.02f*y,y,0.14f*sin(y*0.34f+1.0f));
      float r=mix(0.25f,0.085f,sat(y/8.6f))+0.10f*exp(-max(y,0.0f)*1.6f);
      float d=length(p-c)-r; if(d<hh.d){ hh.d=d; hh.part=0; hh.lp=float3(y,atan2(p.z-c.z,p.x-c.x),0.0f); } }
    // 栖枝（光枝）：从树干向西南伸出，末段转向南；落枝时随 dip 下沉
    float3 A0=float3(0.20f,6.60f,0.20f), A1=float3(-1.55f,7.26f,0.58f), A2=float3(-2.35f,7.524f,1.10f), A3=float3(-2.66f,7.575f,1.98f);
    A1.y+=dip*0.45f; A2.y+=dip; A3.y+=dip*1.45f;
    { HeroHit t=hh; leafyBranch(p,A0,A1,0.050f,0.026f,1.0f,tm,shake,0.0f,t); leafyBranch(p,A1,A2,0.026f,0.0155f,1.1f,tm,shake,0.0f,t); leafyBranch(p,A2,A3,0.0155f,0.006f,1.2f,tm,shake,0.0f,t); hh=t;
      // 芽与短枝
      float3 ax=normalize(A3-A1); float s=dot(p-A1,ax); float fi=floor(s/0.085f+0.5f);
      for(int k=-1;k<=1;k++){ float i=fi+float(k); float sc=i*0.085f; if(sc<0.1f||sc>length(A3-A1)-0.03f) continue;
          float h0=hash1(i*5.3f+3.0f); if(fabs(sc-length(A2-A1))<0.11f) continue;       // 落脚处不长芽
          float3 o=(sc<length(A2-A1))?mix(A1,A2,sc/length(A2-A1)):mix(A2,A3,(sc-length(A2-A1))/length(A3-A2));
          float ang=i*2.4f+h0; float3 dir=normalize(float3(cos(ang)*0.6f,0.75f,sin(ang)*0.6f)+ax*0.6f);
          float3 bc=o+dir*0.014f; float db=sdEllipsoid(p-bc-dir*0.004f,float3(0.0032f,0.0032f,0.0032f)+fabs(dir)*0.0042f);
          if(db<hh.d){ hh.d=db; hh.part=3; hh.lp=float3(h0,0.0f,0.0f); }
          if(h0>0.72f){ float2 st=sdSeg(p,o,o+dir*(0.10f+0.12f*h0)); float dt=st.x-mix(0.0035f,0.0012f,st.y); if(dt<hh.d){ hh.d=dt; hh.part=1; hh.lp=float3(st.y,0.0f,2.0f); } }
      } }
    // 带叶的枝
    leafyBranch(p,float3(0.05f,8.05f,-0.05f),float3(-1.75f,8.95f,-0.80f),0.045f,0.024f,2.0f,tm,shake,0.0f,hh);
    leafyBranch(p,float3(-1.75f,8.95f,-0.80f),float3(-3.55f,8.95f,-1.15f),0.024f,0.008f,2.1f,tm,shake,0.95f,hh);       // 右上方垂下的叶
    leafyBranch(p,float3(-1.75f,8.95f,-0.80f),float3(-3.30f,8.74f,0.60f),0.020f,0.011f,2.3f,tm,shake,0.95f,hh);        // 横过前上方的枝：叶垂进画面上缘
    leafyBranch(p,float3(-3.30f,8.74f,0.60f),float3(-4.75f,8.42f,2.05f),0.011f,0.005f,2.4f,tm,shake,0.95f,hh);
    leafyBranch(p,float3(0.10f,7.00f,-0.10f),float3(-2.1f,7.62f,-1.30f),0.040f,0.018f,2.5f,tm,shake,0.0f,hh);          // 右侧与眼同高的枝（拱起再垂下）
    leafyBranch(p,float3(-2.1f,7.62f,-1.30f),float3(-3.5f,8.12f,-0.95f),0.018f,0.010f,2.6f,tm,shake,0.95f,hh);
    leafyBranch(p,float3(-3.5f,8.12f,-0.95f),float3(-4.7f,7.80f,-0.40f),0.010f,0.004f,2.7f,tm,shake,0.98f,hh);
    leafyBranch(p,float3(-0.9f,8.50f,-0.42f),float3(-2.5f,9.55f,0.55f),0.020f,0.007f,2.2f,tm,shake,0.9f,hh);           // 头顶
    leafyBranch(p,float3(0.12f,5.10f,0.15f),float3(-1.9f,5.85f,1.55f),0.055f,0.028f,3.0f,tm,shake,0.0f,hh);
    leafyBranch(p,float3(-1.9f,5.85f,1.55f),float3(-4.1f,6.35f,2.35f),0.028f,0.008f,3.1f,tm,shake,0.9f,hh);            // 左下方
    leafyBranch(p,float3(-1.2f,5.6f,1.05f),float3(-2.9f,6.5f,0.2f),0.018f,0.006f,3.2f,tm,shake,0.85f,hh);
    leafyBranch(p,float3(0.10f,7.40f,0.0f),float3(1.9f,8.55f,-1.25f),0.05f,0.012f,4.0f,tm,shake,0.9f,hh);
    leafyBranch(p,float3(0.12f,5.9f,0.1f),float3(1.7f,6.9f,1.55f),0.05f,0.012f,4.1f,tm,shake,0.9f,hh);
    leafyBranch(p,float3(0.28f,8.3f,0.10f),float3(0.75f,10.9f,-0.35f),0.07f,0.012f,5.0f,tm,shake,0.9f,hh);
    leafyBranch(p,float3(0.24f,7.9f,0.10f),float3(-0.95f,10.3f,0.75f),0.06f,0.010f,5.1f,tm,shake,0.9f,hh);
    leafyBranch(p,float3(0.1f,6.9f,-0.1f),float3(-0.6f,7.9f,-2.3f),0.04f,0.010f,5.2f,tm,shake,0.9f,hh);
    // 树冠上部与背侧：叶团包络 → 扁平叶簇 → （近处）一片片叶
    if(p.y>5.4f){
        float bid; float3 bc; float db=heroCrownEnv(p,tm,bid,bc);
        if(db<0.9f){ db+=(vn3(p*1.15f+3.0f)-0.5f)*0.55f;
            float env=db; float sid; float3 sw=float3(0.03f*sin(tm*2.0f),0.0f,0.02f*sin(tm*1.6f+1.0f));
            float cl=spraySD(p+sw,0.36f,0.34f,sid);
            float dn=max(db-0.22f,cl); float3 ln=float3(0.0f,1.0f,0.0f); float lid=sid;
            if(lod>0.0f && dn<0.3f){ float3 l2; float lf=leafSD(p+sw,0.080f,lid,l2); dn=mix(dn,max(dn-0.04f,lf),lod); ln=l2; }
            else if(dn<0.2f) dn+=(vn3(p*13.0f)-0.5f)*0.05f;
            dn*=0.7f;
            if(dn<hh.d){ hh.d=dn; hh.part=4; hh.id=bid; hh.lp=ln; hh.luv=float2(env,fract(sid+lid)); }
        } else if(db<hh.d){ hh.d=db-0.35f; hh.part=4; hh.id=bid; hh.lp=float3(0.0f,1.0f,0.0f); hh.luv=float2(db,0.5f); }
    }
    return hh;
}

// ---------- 自己的翅膀（只在落枝前的扑翼刹车中进入视野） ----------
// 在相机坐标（右 x、上 y、前 z）里与两片翅面求交。返回 a=不透明度
static float4 ownWings(float3 rdc,float phase,float brake,float3 sunC,float3 amb,float3 sunCam){
    float4 res=float4(0.0f); if(brake<=0.01f) return res;
    float sph=sin(2.0f*PI*phase), cph=cos(2.0f*PI*phase);
    for(int side=0;side<2;side++){
        float sg=(side==0)?1.0f:-1.0f;
        // 下扑末端：翅向前下伸；上扬：翅向后上收。brake 小的时候翅基本在视野外
        float st=0.5f-0.5f*cph;                         // 0 上扬顶点 → 1 下扑末端
        // 上扬顶点：翅在身后上方（画面外）；下扑末端：两翅向前合拢、翅面迎风像一对小伞
        float3 aUp=normalize(float3(sg*0.75f,0.62f,-0.25f)), aDn=normalize(float3(sg*mix(0.80f,0.42f,brake),0.06f,mix(0.30f,0.90f,brake)));
        float3 bUp=float3(0.0f,-0.30f,-0.95f), bDn=float3(-sg*0.10f,-0.97f,0.20f);
        float w=smoothstep(0.0f,1.0f,st);
        float3 a=normalize(mix(aUp,aDn,w)); float3 b=mix(bUp,bDn,w); b=normalize(b-a*dot(a,b)); float3 n=cross(a,b);
        float3 S=float3(sg*0.020f,-0.012f,-0.020f);
        float dn=dot(rdc,n); if(fabs(dn)<1e-4f) continue; float t=dot(S,n)/dn; if(t<=0.002f) continue;
        float3 hp=rdc*t-S; float uu=dot(hp,a), vv=dot(hp,b);
        const float L=0.158f; if(uu<0.0f||uu>L) continue;
        float x=uu/L;
        // 家燕的翅：窄长而尖。前缘略凸，后缘由一根根初级/次级飞羽的羽端组成
        float lead=-0.010f*(1.0f-x*x)-0.003f; float trail=0.052f*pow(max(1.0f-x,0.0f),0.55f)*(0.55f+0.45f*smoothstep(0.0f,0.25f,x))+0.004f*(1.0f-x);
        float fz=x*11.0f; float ff=fract(fz); trail*=1.0f-0.10f*pow(fabs(ff-0.5f)*2.0f,3.0f)*smoothstep(0.2f,0.9f,x);
        float m=smoothstep(lead-0.0015f,lead+0.0015f,vv)*(1.0f-smoothstep(trail-0.002f,trail+0.0015f,vv))*smoothstep(0.0f,0.01f,uu)*(1.0f-smoothstep(L-0.004f,L,uu));
        if(m<=0.0f) continue;
        // 羽轴与羽片的明暗；上表面蓝黑带金属光泽
        float rach=smoothstep(0.035f,0.0f,fabs(ff-0.18f))*smoothstep(0.1f,0.6f,(vv-lead)/max(trail-lead,1e-3f));
        float3 base=mix(float3(0.012f,0.016f,0.030f),float3(0.030f,0.028f,0.030f),smoothstep(0.3f,1.0f,(vv-lead)/max(trail-lead,1e-3f)));
        base*=0.8f+0.4f*ff; base=mix(base,float3(0.06f,0.05f,0.045f),rach*0.5f);
        float ndl=fabs(dot(n,sunCam)); float3 hv=normalize(sunCam-rdc); float spc=pow(sat(fabs(dot(n,hv))),30.0f);
        float3 col=base/PI*(sunC*(0.25f+0.75f*ndl)+amb)+sunC*spc*0.035f*float3(0.5f,0.65f,1.0f);
        // 羽片边缘略透光
        float thin=smoothstep(trail*0.55f,trail,vv); col+=float3(0.05f,0.035f,0.02f)/PI*sunC*thin*sat(-dot(rdc,sunCam))*1.5f;
        float al=m*(1.0f-res.a); res.rgb+=col*al; res.a+=al;
    }
    return res;
}

// ---------- 同伴燕子 ----------
static float udTri(float3 p,float3 a,float3 b,float3 c){
    float3 ba=b-a, pa=p-a, cb=c-b, pb=p-b, ac=a-c, pc=p-c; float3 nor=cross(ba,ac);
    float s=sign(dot(cross(ba,nor),pa))+sign(dot(cross(cb,nor),pb))+sign(dot(cross(ac,nor),pc));
    float d2=(s<2.0f)? min(min(dot(ba*sat(dot(ba,pa)/dot(ba,ba))-pa,ba*sat(dot(ba,pa)/dot(ba,ba))-pa),
                               dot(cb*sat(dot(cb,pb)/dot(cb,cb))-pb,cb*sat(dot(cb,pb)/dot(cb,cb))-pb)),
                               dot(ac*sat(dot(ac,pc)/dot(ac,ac))-pc,ac*sat(dot(ac,pc)/dot(ac,ac))-pc))
                     : dot(nor,pa)*dot(nor,pa)/dot(nor,nor);
    return sqrt(d2);
}
// 鸟局部坐标：x 前, y 上, z 右；sc: 翼展/0.33
static float swallowSD(float3 q,float flap,thread float& part){
    part=0.0f;
    float d=sdEllipsoid(q-float3(0.0f,0.0f,0.0f),float3(0.062f,0.021f,0.021f));
    d=min(d,length(q-float3(0.062f,0.006f,0.0f))-0.0165f);                               // 头
    for(int s=0;s<2;s++){ float sg=(s==0)?1.0f:-1.0f;
        float ca=cos(flap), sa=sin(flap);
        float3 root=float3(0.018f,0.004f,sg*0.016f);
        float3 wrist=root+float3(0.012f,sa*0.055f,sg*ca*0.055f);
        float3 tip=wrist+float3(-0.055f,sin(flap*1.25f)*0.105f,sg*cos(flap*1.25f)*0.105f);
        float3 rear=root+float3(-0.050f,0.0f,sg*0.010f);
        float dw=min(udTri(q,root,wrist,rear),udTri(q,wrist,tip,rear))-0.0022f;
        if(dw<d){ d=dw; part=1.0f; }
        // 叉尾的两根尾羽
        float dt=sdSeg(q,float3(-0.055f,0.0f,sg*0.006f),float3(-0.135f,0.0f,sg*0.026f)).x-0.0030f; if(dt<d){ d=dt; part=2.0f; }
    }
    return d;
}
// 返回 rgb, a 覆盖；tHit 输出命中距离
static float4 traceBirds(constant Uni& u,float3 ro,float3 rd,float tt,float tmax,float3 sunC,float3 amb,thread float& tHit){
    float4 res=float4(0.0f); tHit=tmax; float3 sd=normalize(u.sunDir.xyz);
    for(int i=0;i<MAXBIRD;i++){
        float4 b=u.bird[i]; if(b.w<0.0f) break;
        float3 f=u.birdv[i].xyz; float3 c=b.xyz+f*(u.wind.w*u.res.w*tt);
        float3 oc=ro-c; float bq=dot(oc,rd); float cq=dot(oc,oc)-0.19f*0.19f; float disc=bq*bq-cq; if(disc<0.0f) continue;
        float t0=-bq-sqrt(disc), t1=-bq+sqrt(disc); if(t1<0.0f||t0>tHit) continue; t0=max(t0,0.0f);
        float3 up0=float3(0.0f,1.0f,0.0f); float3 r=normalize(cross(f,up0)); float3 up=cross(r,f);
        float ph=b.w+7.5f*u.res.w*tt; float glide=smoothstep(0.55f,0.9f,sin(ph*0.83f+float(i)*2.1f));
        float flap=mix(0.95f*sin(2.0f*PI*ph)+0.15f,0.12f,glide);
        float t=t0; float part=0.0f; bool hit=false;
        for(int k=0;k<20;k++){ float3 p=ro+rd*t-c; float3 q=float3(dot(p,f),dot(p,up),dot(p,r)); float d=swallowSD(q,flap,part);
            if(d<0.0006f*max(t,1.0f)){ hit=true; break; } t+=d; if(t>t1) break; }
        if(!hit) continue;
        float3 p=ro+rd*t-c; float3 q=float3(dot(p,f),dot(p,up),dot(p,r)); float e=0.002f; float pp;
        float3 nq=normalize(float3(swallowSD(q+float3(e,0,0),flap,pp)-swallowSD(q-float3(e,0,0),flap,pp),swallowSD(q+float3(0,e,0),flap,pp)-swallowSD(q-float3(0,e,0),flap,pp),swallowSD(q+float3(0,0,e),flap,pp)-swallowSD(q-float3(0,0,e),flap,pp)));
        float3 n=f*nq.x+up*nq.y+r*nq.z;
        // 上体蓝黑，腹面米白，喉部栗红
        float belly=smoothstep(0.0f,-0.008f,q.y)*(1.0f-step(0.5f,part));
        float3 alb=mix(float3(0.014f,0.018f,0.034f),float3(0.62f,0.56f,0.46f),belly);
        alb=mix(alb,float3(0.22f,0.05f,0.02f),belly*smoothstep(0.035f,0.055f,q.x));
        if(part>0.5f) alb=mix(float3(0.016f,0.018f,0.03f),float3(0.10f,0.09f,0.085f),smoothstep(0.0f,-1.0f,nq.y)*0.6f);
        float ndl=sat(dot(n,sd)*0.8f+0.2f);
        float3 col=alb/PI*(sunC*ndl+amb*(0.6f+0.4f*n.y));
        res=float4(col,1.0f); tHit=t;
    }
    return res;
}

// ---------- 瀑布水雾与虹 ----------
static float mistDens(float3 p,float tm){
    float zf=riverZ(FALL_X); float3 c=float3(FALL_X-9.0f,FALL_BOT+1.0f,zf);
    float3 q=p-c; float up=max(q.y,0.0f);
    float r2=(q.x*q.x*0.55f+q.z*q.z)/sq(10.0f+0.30f*up)+sq(max(-q.y,0.0f)/2.0f);
    float d=exp(-r2)*exp(-up/26.0f);
    // 沿水帘上升的雾柱
    float col=exp(-(sq(q.x-6.0f)+q.z*q.z)/60.0f)*smoothstep(58.0f,8.0f,q.y)*0.30f;
    float n=vn3(p*0.22f+float3(0.0f,-tm*0.9f,tm*0.3f))*0.7f+vn3(p*0.6f+float3(tm*0.4f,-tm*1.6f,0.0f))*0.3f;
    return (d+col)*smoothstep(0.15f,0.75f,n)*0.11f;
}
// 峡谷里的薄雾（让斜射进来的阳光成为可见的光柱）
static float gorgeHaze(float3 p){
    if(p.x<GORGE_X0-20.0f||p.x>FALL_X+10.0f||p.y>78.0f) return 0.0f;
    float sd=fabs(riverDist(p.x,p.z)); if(sd>30.0f) return 0.0f;
    float bed=riverBed(p.x);
    return 0.0024f*smoothstep(22.0f,9.0f,sd)*smoothstep(GORGE_X0+10.0f,GORGE_X0+70.0f,p.x)*smoothstep(55.0f,25.0f,p.y-bed)*smoothstep(76.0f,60.0f,p.y)*(0.6f+0.8f*vn3(p*0.07f));
}
// 沿视线积分水雾；返回 rgb 内散射与 a 透射率
static float4 marchMist(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float tmax,float jit){
    // 包围盒：峡谷段
    float zc=riverZ(0.5f*(GORGE_X0+FALL_X));
    float3 bmin=float3(GORGE_X0-25.0f,0.0f,zc-75.0f), bmax=float3(FALL_X+48.0f,150.0f,zc+75.0f);
    float3 inv=1.0f/rd; float3 ta=(bmin-ro)*inv, tb=(bmax-ro)*inv; float3 tmn=min(ta,tb), tmx=max(ta,tb);
    float t0=max(max(tmn.x,tmn.y),max(tmn.z,0.0f)), t1=min(min(tmx.x,tmx.y),min(tmx.z,tmax));
    if(t1<=t0) return float4(0.0f,0.0f,0.0f,1.0f);
    float3 sd=normalize(u.sunDir.xyz); float3 sunC=sunColAt(c,FALL_BOT+30.0f); float3 amb=skyAvg(c);
    const int N=14; float ds=(t1-t0)/float(N); float3 L=float3(0.0f); float T=1.0f;
    float mu=dot(rd,sd); float ph=0.6f*phaseHG(mu,0.6f)+0.4f*phaseHG(mu,-0.1f);
    // 虹：视线与反日方向夹角 ~42°（主虹）与 ~51°（副虹）
    float ang=acos(clamp(dot(rd,-sd),-1.0f,1.0f))*57.29578f;
    float3 bow=float3(exp(-sq((ang-42.3f)/0.75f)),exp(-sq((ang-41.6f)/0.75f)),exp(-sq((ang-40.7f)/0.85f)))*0.9f
              +float3(exp(-sq((ang-50.6f)/1.2f)),exp(-sq((ang-52.0f)/1.2f)),exp(-sq((ang-53.4f)/1.3f)))*0.22f
              +float3(0.25f)*smoothstep(42.5f,36.0f,ang)*smoothstep(20.0f,36.0f,ang);
    for(int i=0;i<N;i++){ float t=t0+(float(i)+jit)*ds; float3 p=ro+rd*t; float sm=mistDens(p,u.tp.x); float sh2=gorgeHaze(p); float sg=sm+sh2; if(sg<=1e-5f) continue;
        float sh=macroShadow(c,p); if(sh>0.01f){ int m; float s=1.0f; float3 q=p; float st=1.5f; for(int k=0;k<7;k++){ q+=sd*st; st*=1.6f; float d=mapScene(c,q,60.0f,m); if(d<0.0f){ s=0.0f; break; } } sh*=s; }
        float a=1.0f-exp(-sg*ds);
        float3 S=sunC*sh*((0.045f+bow*0.055f*sat(sm*40.0f))*sm+ph*sh2*4.0f)/max(sg,1e-6f)+amb*0.085f;
        L+=T*a*S*PI; T*=1.0f-a; }
    return float4(L,T);
}

// ---------- 瀑布水帘：按体积积分（边缘柔和、有缝隙的水幕） ----------
static float fallDens(float3 p,float tm,float nearK){
    float zf=riverZ(FALL_X); float lipX=FALL_X-2.2f; float top=FALL_TOP+0.5f;
    if(p.y>top||p.y<FALL_BOT+0.2f) return 0.0f;
    float drop=top-p.y; float tt=sqrt(2.0f*drop/9.81f);
    float xj=lipX-1.6f*tt-0.2f; float dz=p.z-zf;
    float wz=2.2f+0.025f*drop+0.8f*smoothstep(40.0f,62.0f,drop); float th=0.32f+0.016f*drop;
    float ex=fabs(p.x-xj)/th, ez=fabs(dz)/wz; if(ex>1.6f||ez>1.25f) return 0.0f;
    float v=1.2f+3.2f*tt;                                                    // 下落速度的纹理滚动
    float st=vn2(float2(dz*2.4f,drop*0.05f-tm*v*0.05f*6.0f))*0.6f+vn2(float2(dz*8.5f,drop*0.10f-tm*v*0.10f*5.0f))*0.4f;
    float fine=vn2(float2(dz*23.0f,drop*0.16f-tm*v*0.16f*4.0f));
    float prof=smoothstep(1.5f,0.2f,ex+(st-0.5f)*0.9f)*smoothstep(1.2f,0.75f,ez+(st-0.5f)*0.25f);
    float gaps=smoothstep(0.30f,0.62f,st+0.30f*fine-0.004f*drop);
    float dFar=prof*(0.22f+0.78f*gaps)*(3.4f-0.02f*drop)*(0.7f+0.6f*fine);          // 远看：较实的白水幕
    float dNear=prof*(0.10f+0.90f*gaps*gaps)*(2.1f-0.012f*drop)*(0.6f+0.8f*fine);   // 贴近：更薄、更成缕
    return mix(dFar,dNear,nearK);
}
static float4 marchFall(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float tmax,float jit){
    float zf=riverZ(FALL_X);
    float3 bmin=float3(FALL_X-12.5f,FALL_BOT,zf-5.0f), bmax=float3(FALL_X-1.2f,FALL_TOP+0.6f,zf+5.0f);
    float3 inv=1.0f/rd; float3 ta=(bmin-ro)*inv, tb=(bmax-ro)*inv; float3 tmn=min(ta,tb), tmx=max(ta,tb);
    float t0=max(max(tmn.x,tmn.y),max(tmn.z,0.0f)), t1=min(min(tmx.x,tmx.y),min(tmx.z,tmax));
    if(t1<=t0) return float4(0.0f,0.0f,0.0f,1.0f);
    float3 sd=normalize(u.sunDir.xyz); float3 sunC=sunColAt(c,FALL_BOT+30.0f); float3 amb=ambCube(c,-rd)*0.6f+skyAvg(c)*0.9f;
    const int N=18; float ds=(t1-t0)/float(N); float3 L=float3(0.0f); float T=1.0f; float mu=dot(rd,sd);
    float tm=c.tm; float nearK=smoothstep(60.0f,30.0f,length(ro-float3(FALL_X-6.0f,0.5f*(FALL_BOT+FALL_TOP),zf)));
    for(int i=0;i<N;i++){ float t=t0+(float(i)+jit)*ds; float3 p=ro+rd*t; float sg=fallDens(p,tm,nearK); if(sg<=1e-4f) continue;
        float sh=macroShadow(c,p); if(sh>0.01f){ int m; float s=1.0f; float3 q=p; float st=1.5f; for(int k=0;k<6;k++){ q+=sd*st; st*=1.7f; float d=mapScene(c,q,60.0f,m); if(d<0.0f){ s=0.0f; break; } } sh*=s; }
        // 朝太阳方向水幕自身的遮挡
        float od=fallDens(p+sd*0.35f,tm,nearK)*0.5f+fallDens(p+sd*0.9f,tm,nearK)*0.7f;
        float a=1.0f-exp(-sg*ds);
        float bright=0.45f+1.1f*sat(sg*0.55f);                               // 水缕的芯更亮、薄处更暗，贴近时仍看得出一缕一缕
        float3 S=(sunC*sh*exp(-od*0.8f)*(0.22f+0.25f*sat(mu)*sat(mu))+amb*0.30f)*mix(1.0f,bright,nearK);
        L+=T*a*S; T*=1.0f-a; if(T<0.01f){ T=0.0f; break; } }
    return float4(L,T);
}
