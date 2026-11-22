// render.metal — 着色、主渲染内核、阴影图、后期、探针

// ---------- 地形材质 ----------
static float3 terrainNormal(const Ctx& c,float2 xz,float t){
    float e=max(0.06f,t*0.0010f);
    float hx=Hd(c,xz+float2(e,0.0f),t)-Hd(c,xz-float2(e,0.0f),t);
    float hz=Hd(c,xz+float2(0.0f,e),t)-Hd(c,xz-float2(0.0f,e),t);
    return normalize(float3(-hx,2.0f*e,-hz));
}
struct Mat { float3 alb; float3 n; float ao; float spec; float rough; float snow; };
static float3 stoneCol(float2 xz,float fp){
    float2 w=worley2(xz/0.55f); float f=1.0f-smoothstep(0.3f,1.5f,fp);
    float3 a=mix(float3(0.30f,0.29f,0.27f),float3(0.42f,0.38f,0.32f),w.y);
    a=mix(a,float3(0.20f,0.20f,0.21f),step(0.75f,fract(w.y*7.3f)));
    return mix(float3(0.32f,0.30f,0.27f),a*(0.55f+0.6f*smoothstep(0.0f,0.5f,w.x)),f);
}
static Mat terrainMat(const Ctx& c,float3 p,float3 n,float t){
    Mat m; float2 uv=w2uv(p.xz);
    float4 aux=c.tAux.sample(sl,uv,level(0.0f)); float eb=edgeBlend(p.xz);
    float treeD=c.tTreeA.sample(sl,uv,level(0.0f)).x*(1.0f-eb);
    float slope=1.0f-n.y; float alt=p.y; float fp=t*0.0011f;
    float nA=fbm2(p.xz/95.0f,3), nB=vn2(p.xz/11.0f), nC=mix(0.5f,vn2(p.xz/2.3f),1.0f-smoothstep(1.0f,6.0f,fp)), nD=mix(0.5f,vn2(p.xz/0.37f),1.0f-smoothstep(0.15f,1.0f,fp));
    // 岩石：随高程分层的岩性
    float strat=alt/7.5f+2.4f*vn2(p.xz/170.0f);
    float band=vn2(float2(strat,3.7f)); float band2=vn2(float2(strat*0.31f,9.1f));
    float3 rock=mix(float3(0.20f,0.185f,0.17f),float3(0.41f,0.38f,0.33f),smoothstep(0.3f,0.7f,band));
    rock=mix(rock,float3(0.31f,0.22f,0.15f),smoothstep(0.55f,0.9f,band2)*0.65f);
    rock*=(0.72f+0.56f*nB)*(0.85f+0.3f*nC)*(0.9f+0.2f*nD);
    rock*=1.0f-0.35f*smoothstep(0.55f,0.48f,fabs(fract(strat*2.0f)-0.5f)*2.0f+0.4f*nC)*(1.0f-smoothstep(2.0f,12.0f,fp));   // 层理缝
    // 草甸：低处绿，向上与干燥处转黄、锈红
    float dry=sat(0.30f+(nA-0.5f)*1.5f+alt/1900.0f);
    float3 grass=mix(float3(0.075f,0.125f,0.030f),float3(0.25f,0.21f,0.065f),dry);
    grass=mix(grass,float3(0.20f,0.145f,0.06f),smoothstep(850.0f,1500.0f,alt+260.0f*(nA-0.5f)));
    float shrub=smoothstep(0.60f,0.80f,vn2(p.xz/21.0f+7.3f))*smoothstep(450.0f,1000.0f,alt);
    grass=mix(grass,float3(0.21f,0.055f,0.03f),shrub*0.65f);
    grass*=(0.68f+0.64f*nB)*(0.70f+0.60f*nC)*(0.72f+0.56f*nD);
    { float straw=smoothstep(0.45f,0.75f,vn2(p.xz/0.9f+3.0f))*(1.0f-smoothstep(0.8f,4.0f,fp)); grass=mix(grass,float3(0.36f,0.30f,0.13f),straw*0.45f); }
    float3 scree=float3(0.33f,0.31f,0.29f)*(0.7f+0.6f*nC)*(0.8f+0.4f*nB);
    float r=smoothstep(0.25f,0.39f,slope+(nB-0.5f)*0.10f+(nA-0.5f)*0.06f);
    float s=smoothstep(0.13f,0.23f,slope+(nA-0.5f)*0.14f)*smoothstep(500.0f,1500.0f,alt+300.0f*nA);
    float3 alb=mix(grass,scree,s*(1.0f-r)); alb=mix(alb,rock,r);
    // 林下：枯叶与苔
    float3 litter=mix(float3(0.13f,0.085f,0.04f),float3(0.07f,0.10f,0.03f),smoothstep(0.4f,0.7f,nB))*(0.7f+0.6f*nC);
    alb=mix(alb,litter,sat(treeD*5.0f)*(1.0f-r)*0.85f);
    m.ao=aux.y; m.spec=0.0f; m.rough=1.0f; m.snow=0.0f;
    // 河道与峡谷
    if(inRiverBox(p)){
        float sdv=fabs(riverDist(p.x,p.z)); float w=riverHalfW(p.x);
        float bedm=smoothstep(w+2.2f,w+0.4f,sdv);
        float3 st=stoneCol(p.xz,fp); float wl=riverBed(p.x)+riverDepth(p.x);
        st*=mix(1.0f,0.45f,smoothstep(wl+0.6f,wl+0.05f,p.y));   // 湿石
        alb=mix(alb,st,bedm);
        float gor=smoothstep(GORGE_X0-40.0f,GORGE_X0+40.0f,p.x)*smoothstep(FALL_X+40.0f,FALL_X,p.x);
        float wallm=gor*smoothstep(0.12f,0.3f,slope)*smoothstep(w+60.0f,w+20.0f,sdv);
        float3 wrock=rock*mix(float3(1.0f),float3(0.55f,0.62f,0.50f),smoothstep(0.45f,0.75f,nB)*0.6f);     // 潮湿、带苔
        // 瀑布溅湿的崖壁
        float spray=sat(1.0f-length(float2(p.x-FALL_X,riverDist(p.x,p.z)))/45.0f);
        wrock*=1.0f-0.45f*spray; m.spec=max(m.spec,spray*0.25f);
        alb=mix(alb,wrock,wallm);
    }
    // 湖岸：卵石滩
    if(p.x>DAM_X-200.0f && p.x<RIVER_X0+200.0f && alt<2.2f){
        float sh=smoothstep(1.9f+0.8f*(nB-0.5f),0.8f,alt);
        float3 st=stoneCol(p.xz,fp)*mix(1.0f,0.5f,smoothstep(0.35f,0.02f,alt));
        alb=mix(alb,st,sh*(1.0f-r));
    }
    // 雪
    float snowL=1780.0f+420.0f*(nA-0.5f)-160.0f*sat(-n.z*2.0f);
    float sn=smoothstep(snowL,snowL+260.0f,alt+60.0f*nB)*smoothstep(0.50f,0.30f,slope+(nB-0.5f)*0.1f);
    alb=mix(alb,float3(0.86f,0.89f,0.93f),sn); m.snow=sn;
    // 法线细节（草丛、石面）
    float3 bn=n; float amp=mix(0.35f,0.6f,r)*(1.0f-sn*0.7f);
    float3 d1=vn2d(p.xz/5.5f), d2=vn2d(p.xz/1.25f), d3=vn2d(p.xz/0.27f);
    float f1=1.0f-smoothstep(3.0f,14.0f,fp), f2=1.0f-smoothstep(0.6f,3.0f,fp), f3=1.0f-smoothstep(0.12f,0.6f,fp);
    float2 gsum=d1.yz*0.55f*f1+d2.yz*0.50f*f2+d3.yz*0.34f*f3;
    bn=normalize(n+float3(-gsum.x,0.0f,-gsum.y)*amp);
    m.alb=alb; m.n=bn;
    return m;
}

// ---------- 表面着色（不含空气透视） ----------
static float3 shadeSolid(const Ctx& c,constant Uni& u,float3 ro,float3 rd,Hit h,bool lowQ){
    float t=h.t; float3 p=ro+rd*t; float3 sd=normalize(u.sunDir.xyz);
    float3 n=float3(0.0f,1.0f,0.0f); float3 alb=float3(0.2f); float ao=1.0f; float trans=0.0f; float spec=0.0f; float wrap=0.0f; float selfSh=1.0f; bool fol=false; bool treeish=false; bool heroFol=false;
    float fp=t*0.0011f;
    if(h.mat==0){
        n=terrainNormal(c,p.xz,t); Mat m=terrainMat(c,p,n,t); alb=m.alb; ao=m.ao; spec=m.spec; n=m.n;
        float treeD=c.tTreeA.sample(sl,w2uv(p.xz),level(0.0f)).x; ao*=1.0f-0.55f*sat(treeD*4.0f);
    } else if(h.mat>=11){
        float e=max(0.012f,t*0.0012f); int mm;
        float3 k0=float3(1.0f,-1.0f,-1.0f),k1=float3(-1.0f,-1.0f,1.0f),k2=float3(-1.0f,1.0f,-1.0f),k3=float3(1.0f,1.0f,1.0f);
        n=normalize(k0*mapScene(c,p+k0*e,t,mm)+k1*mapScene(c,p+k1*e,t,mm)+k2*mapScene(c,p+k2*e,t,mm)+k3*mapScene(c,p+k3*e,t,mm));
        if(h.mat==11){
            float3 st=stoneCol(p.xz*0.35f+p.y,2.0f)*(0.75f+0.5f*vn3(p*2.2f)); float wl=riverBed(p.x)+riverDepth(p.x);
            st=mix(st,float3(0.06f,0.10f,0.03f),smoothstep(0.5f,0.9f,n.y)*smoothstep(0.4f,0.7f,vn3(p*1.1f))*0.8f);      // 石顶的苔
            st*=mix(0.45f,1.0f,smoothstep(wl+0.05f,wl+0.5f,p.y)); alb=st; spec=0.15f*smoothstep(wl+0.5f,wl+0.05f,p.y); ao=0.55f+0.45f*sat((p.y-wl)*1.5f);
        } else if(h.mat==13){   // 灌丛：秋色矮灌木
            float hG=Hd(c,p.xz,t); float treeD=c.tTreeA.sample(sl,w2uv(p.xz),level(0.0f)).x; float kind,info; groundCoverSD(c,p,hG,treeD,kind,info);
            float k2=floor(info); float v=fract(info);
            float3 sc=(k2<1.0f)?float3(0.15f,0.07f,0.035f):((k2<2.0f)?float3(0.17f,0.13f,0.045f):((k2<3.0f)?float3(0.085f,0.105f,0.035f):float3(0.04f,0.062f,0.03f)));
            alb=sc*(0.6f+0.8f*v)*(0.65f+0.7f*vn3(p*7.0f)); trans=0.3f; wrap=0.3f;
            n=normalize(n+(float3(vn3(p*21.0f),vn3(p*21.0f+5.0f),vn3(p*21.0f+9.0f))-0.5f)*1.4f);
            ao=0.45f+0.55f*sat((p.y-hG)/0.8f);
        } else if(h.mat==14){   // 散石：带地衣
            float3 st=mix(float3(0.26f,0.25f,0.24f),float3(0.42f,0.39f,0.34f),vn3(p*0.9f))*(0.75f+0.5f*vn3(p*5.0f));
            float lich=smoothstep(0.55f,0.75f,vn3(p*3.3f)); st=mix(st,mix(float3(0.42f,0.44f,0.30f),float3(0.50f,0.33f,0.06f),step(0.7f,vn3(p*1.7f))),lich*0.6f*smoothstep(0.0f,0.5f,n.y));
            alb=st; ao=0.6f+0.4f*sat(n.y+0.5f);
            n=normalize(n+(float3(vn3(p*30.0f),vn3(p*30.0f+5.0f),vn3(p*30.0f+9.0f))-0.5f)*0.4f);
        } else {
            float bk=vn2(float2(dot(p,float3(0.2f,0.1f,0.97f))*1.2f,atan2(p.y,p.x)*3.0f));
            alb=mix(float3(0.20f,0.17f,0.14f),float3(0.33f,0.31f,0.27f),bk)*(0.7f+0.6f*vn3(p*9.0f)); alb=mix(alb,float3(0.07f,0.11f,0.035f),smoothstep(0.3f,0.8f,n.y)*0.7f); ao=0.8f;
        }
    } else if(h.mat>=6){
        // 主角桦树
        float3 hp=p-c.heroPos; HeroHit hh=heroSD(hp,c.tm,c.dip,c.shake,c.heroLod); float e=0.0005f+0.0004f*t;
        float3 k0=float3(1.0f,-1.0f,-1.0f),k1=float3(-1.0f,-1.0f,1.0f),k2=float3(-1.0f,1.0f,-1.0f),k3=float3(1.0f,1.0f,1.0f);
        n=normalize(k0*heroSD(hp+k0*e,c.tm,c.dip,c.shake,c.heroLod).d+k1*heroSD(hp+k1*e,c.tm,c.dip,c.shake,c.heroLod).d+k2*heroSD(hp+k2*e,c.tm,c.dip,c.shake,c.heroLod).d+k3*heroSD(hp+k3*e,c.tm,c.dip,c.shake,c.heroLod).d);
        ao=0.8f;
        if(hh.part==2){
            float3 ln=normalize(hh.lp); if(dot(ln,rd)>0.0f) ln=-ln; n=normalize(ln+n*0.15f);
            float g0=fract(hh.id), kind=floor(hh.id); float2 q=hh.luv;
            float3 lc=mix(float3(0.50f,0.38f,0.040f),float3(0.56f,0.29f,0.030f),smoothstep(4.0f,7.0f,kind)); if(kind<1.5f) lc=float3(0.25f,0.31f,0.05f);
            float mid=smoothstep(0.07f,0.0f,fabs(q.y)); float lat=smoothstep(0.12f,0.0f,fabs(fract(q.x*3.5f-fabs(q.y)*1.6f)-0.5f))*smoothstep(0.05f,0.3f,fabs(q.y));
            lc*=1.0f-0.22f*lat; lc=mix(lc,float3(0.56f,0.50f,0.22f),mid*0.5f);
            lc=mix(lc,float3(0.17f,0.085f,0.03f),sat(smoothstep(0.78f,0.98f,fabs(q.y))*0.5f+0.7f*smoothstep(0.70f,0.80f,vn2(q*3.5f+g0*30.0f))));
            alb=lc; trans=1.5f; spec=0.05f; wrap=0.08f;
        } else if(hh.part==0){
            float y=hh.lp.x, ang=hh.lp.y; bool trunk=(hh.lp.z==0.0f);
            float dash=smoothstep(0.58f,0.72f,vn2(float2(ang*2.3f,y*(trunk?26.0f:70.0f))))*smoothstep(0.35f,0.6f,vn2(float2(ang*0.9f+3.0f,y*3.1f)));
            float3 wh=float3(0.70f,0.68f,0.62f)*(0.85f+0.3f*vn2(float2(ang*3.0f,y*2.0f)));
            float3 bk=mix(wh,float3(0.05f,0.045f,0.04f),dash);
            float rgh=smoothstep(0.62f,0.80f,vn2(float2(ang*1.3f+7.0f,y*1.1f)));
            bk=mix(bk,float3(0.07f,0.06f,0.05f)*(0.6f+0.8f*vn2(float2(ang*9.0f,y*30.0f))),rgh*0.9f);
            if(trunk) bk=mix(float3(0.09f,0.075f,0.06f)*(0.5f+vn2(float2(ang*7.0f,y*9.0f))),bk,smoothstep(0.5f,2.2f,y));
            else bk=mix(bk,float3(0.14f,0.09f,0.065f)*(0.8f+0.4f*vn2(float2(ang*5.0f,y*40.0f))),0.45f+0.4f*smoothstep(1.0f,2.5f,y));
            float lich=smoothstep(0.66f,0.80f,vn3(hp*9.0f))*smoothstep(0.3f,0.6f,vn3(hp*2.1f));
            bk=mix(bk,mix(float3(0.30f,0.33f,0.12f),float3(0.50f,0.30f,0.05f),step(0.6f,vn3(hp*3.7f))),lich*0.75f);
            alb=bk; n=normalize(n+(float3(vn3(hp*90.0f),vn3(hp*90.0f+7.0f),vn3(hp*90.0f+13.0f))-0.5f)*0.5f);
        } else if(hh.part==1){ alb=float3(0.13f,0.075f,0.05f); spec=0.10f; }
        else if(hh.part==4){   // 树冠上部：与林中桦树同一套叶丛着色
            float bid; float3 bc; heroCrownEnv(hp,c.tm,bid,bc);
            float depth=sat(-hh.luv.x/0.9f); float sv=hh.luv.y; float lk=c.heroLod;
            float3 bN=normalize(hp-mix(bc,float3(0.15f,8.6f,0.0f),0.45f)+float3(0.0f,0.3f,0.0f));
            float3 ln=hh.lp; if(dot(ln,rd)>0.0f) ln=-ln;
            n=normalize(mix(mix(bN,n,0.6f),normalize(ln+bN*0.35f),lk));
            float3 lc=mix(float3(0.44f,0.36f,0.04f),float3(0.49f,0.33f,0.035f),smoothstep(0.5f,0.8f,hh.id));
            float turn=(sv-0.5f)*0.9f+(0.35f-depth)*0.5f;
            lc*=mix(float3(0.84f,1.10f,0.95f),float3(1.12f,0.92f,0.80f),sat(0.5f+turn));
            alb=lc*(0.72f+0.36f*vn3(hp*3.3f))*(0.80f+0.40f*sv);
            ao=mix(1.0f,0.28f,depth)*(0.75f+0.5f*vn3(hp*0.75f));
            trans=0.50f; wrap=0.25f; spec=0.03f; fol=true; heroFol=true;
        }
        else { alb=float3(0.13f,0.07f,0.035f)*(0.8f+0.4f*hh.lp.x); spec=0.12f; }
    } else {
        // 由距离场求法线
        float e=max(0.03f,t*0.0012f); int mm;
        float3 k0=float3(1.0f,-1.0f,-1.0f),k1=float3(-1.0f,-1.0f,1.0f),k2=float3(-1.0f,1.0f,-1.0f),k3=float3(1.0f,1.0f,1.0f);
        n=normalize(k0*mapScene(c,p+k0*e,t,mm)+k1*mapScene(c,p+k1*e,t,mm)+k2*mapScene(c,p+k2*e,t,mm)+k3*mapScene(c,p+k3*e,t,mm));
        float2 uv=w2uv(p.xz);
        float3 canCol=c.tTreeCol.sample(sl,uv,level(0.0f)).xyz; canCol*=canCol;
        if(h.mat==5){
            // 峡谷岩壁：按岩层着色，凸出的硬层偏浅，带竖向水渍与苔
            float y=p.y+3.0f*vn2(p.xz/23.0f); float bed=vn2(float2(y/1.7f,0.5f)); float bed2=vn2(float2(y/0.45f,7.5f));
            float3 rk=mix(float3(0.17f,0.15f,0.135f),float3(0.40f,0.36f,0.30f),smoothstep(0.25f,0.75f,bed));
            rk=mix(rk,float3(0.33f,0.22f,0.14f),smoothstep(0.6f,0.9f,vn2(float2(y/6.3f,2.2f)))*0.6f);
            rk*=(0.8f+0.4f*bed2)*(0.75f+0.5f*vn3(p*1.7f));
            float hcoord=dot(p.xz,float2(0.8f,0.6f));
            float streak=smoothstep(0.55f,0.8f,vn2(float2(hcoord*0.9f,y*0.06f)))*smoothstep(0.3f,-0.2f,n.y);
            rk*=1.0f-0.45f*streak;
            float moss=smoothstep(0.35f,0.75f,n.y)*smoothstep(0.35f,0.6f,vn3(p*0.6f));
            rk=mix(rk,float3(0.06f,0.10f,0.03f)*(0.7f+0.6f*vn3(p*4.0f)),moss*0.8f);
            float spray=sat(1.0f-length(float2(p.x-FALL_X,riverDist(p.x,p.z)))/40.0f); float wl=riverBed(p.x)+riverDepth(p.x);
            float wet=max(spray,smoothstep(wl+1.2f,wl+0.1f,p.y)); rk*=1.0f-0.5f*wet; spec=0.25f*wet;
            alb=rk; ao=0.35f+0.65f*sat(0.5f+0.5f*n.y+0.4f*(vn3(p*float3(0.17f,0.45f,0.17f))-0.3f));
            // 峡谷里的天空可见度：越深越暗
            float hm2=Hraw(c,p.xz); ao*=mix(1.0f,0.55f,sat((hm2-p.y)/70.0f)); wrap=0.0f;
            n=normalize(n+(float3(vn3(p*7.0f),vn3(p*7.0f+5.0f),vn3(p*7.0f+9.0f))-0.5f)*0.35f);
        } else if(h.mat==3){
            alb=canCol*(0.75f+0.5f*vn2(p.xz/2.1f)); trans=0.2f; wrap=0.25f;
            float hcan=canopyH(c,p.xz); ao=0.55f+0.45f*sat((p.y-Hc(c,p.xz))/max(hcan,1.0f)*1.2f-0.2f);
        } else {
            TreeHit th; forestSD(c,p,t,th,false); treeish=true;
            float4 tc=c.tTreeCol.read(uint2(th.col.xy)); tc.xyz*=tc.xyz; float uu=sat(th.q.y/th.H);
            if(h.mat==2){
                float ang=atan2(th.q.z,th.q.x);
                float3 bark=float3(0.105f,0.075f,0.05f);
                if(th.sp==1) bark=mix(float3(0.13f,0.09f,0.06f),float3(0.30f,0.14f,0.065f),smoothstep(0.3f,0.6f,uu));
                else if(th.sp==2) bark=float3(0.17f,0.145f,0.12f);
                else if(th.sp==3){ float mk=smoothstep(0.62f,0.75f,vn2(float2(ang*2.2f,th.q.y*5.0f+th.sd*50.0f))); bark=mix(float3(0.74f,0.72f,0.66f),float3(0.06f,0.055f,0.05f),mk); }
                else if(th.sp==5) bark=float3(0.36f,0.34f,0.31f);
                bark*=0.7f+0.6f*vn2(float2(ang*3.0f,th.q.y*1.3f));
                alb=bark; ao=(0.45f+0.55f*smoothstep(0.0f,0.5f,uu))*mix(1.0f,0.45f,sat(-th.env/1.2f));
            } else {
                bool conif=(th.sp==0||th.sp==1||th.sp==4);
                float lodn=1.0f-smoothstep(70.0f,260.0f,t);
                // 法线：叶团的"鼓包"方向与距离场梯度混合（远处更圆润，近处更碎）
                float3 bN=normalize(th.q-th.bc+float3(0.0f,0.35f,0.0f));
                n=normalize(mix(bN,n,0.30f+0.35f*lodn));
                if(th.lk>0.0f){ float3 ln=th.ln; if(dot(ln,rd)>0.0f) ln=-ln; n=normalize(mix(n,normalize(ln+bN*0.35f),th.lk)); }
                float depth=sat(-th.env/1.2f);                 // 0 树冠表层 → 1 冠内深处
                float nz=vn3(p*0.75f), nz2=vn3(p*3.3f);
                float sv=mix(0.5f,th.sid,lodn);
                float3 base=tc.xyz;
                if(!conif){ // 同一棵树上：外层与顶部先变色，冠内偏绿；每簇叶再有差别
                    float turn=(sv-0.5f)*0.9f+(0.35f-depth)*0.5f+(uu-0.6f)*0.4f;
                    base*=mix(float3(0.82f,1.10f,0.95f),float3(1.15f,0.90f,0.78f),sat(0.5f+turn));
                }
                alb=base*(0.70f+0.34f*nz2+0.22f*nz)*(0.80f+0.40f*sv);
                float m1=1.0f-smoothstep(0.02f,0.10f,t*0.0011f);
                if(m1>0.0f){ float3 j2=float3(vn3(p*19.0f+1.0f),vn3(p*19.0f+5.0f),vn3(p*19.0f+9.0f))-0.5f; n=normalize(n+j2*1.2f*m1); alb*=1.0f+(vn3(p*23.0f)-0.5f)*0.8f*m1; }
                ao=(0.35f+0.65f*smoothstep(0.05f,0.8f,uu))*mix(1.0f,0.28f,depth)*(0.75f+0.5f*nz);
                trans=conif?0.10f:0.45f; wrap=0.25f; spec=conif?0.04f:0.03f; fol=true;
            }
            // 与远处冠层的过渡
            float k=smoothstep(LOD0,LOD1,t); alb=mix(alb,canCol,k*0.7f);
        }
    }
    float3 sunC=sunColAt(c,p.y);
    float sh=macroShadow(c,p);
    if(sh>0.01f) sh*=cloudShadow(c,u,p);
    // 林冠互相遮挡（低太阳时很重要）
    if(sh>0.01f && t<9000.0f && (treeish || h.mat==0 || h.mat==3 || h.mat==13 || h.mat==14)) sh*=mix(canopyShadow(c,p+float3(0.0f,0.3f,0.0f),sd),1.0f,smoothstep(6000.0f,9000.0f,t));
    float scat=0.0f; float shBase=sh;      // shBase：地形、云、林冠的遮挡（不含近处几何）
    if(!lowQ && sh>0.01f && t<1500.0f){
        float ns=(h.mat>=6&&h.mat<=10)?nearShadow(c,p+n*0.0025f,sd,t,0.006f):nearShadow(c,p+n*(0.04f+0.0006f*t),sd,t,0.5f+0.002f*t);
        float k=smoothstep(1100.0f,1500.0f,t);
        if(fol){   // 被自己的叶层挡住的地方：按朝太阳方向 7 m 内的叶层厚度，让一部分光透过来
            TreeHit tq; float thick=0.0f; float bq; float3 cq; float3 hq=p-c.heroPos;
            const float ss[4]={0.6f,1.6f,3.2f,5.6f}; const float ww[4]={1.0f,1.4f,2.0f,2.6f};
            for(int i=0;i<4;i++){ float e=heroFol?heroCrownEnv(hq+sd*ss[i],c.tm,bq,cq):forestSD(c,p+sd*ss[i],t,tq,true); thick+=ww[i]*smoothstep(0.25f,-0.35f,e); }
            scat=(1.0f-ns)*exp(-thick*0.8f)*(1.0f-k);
        }
        sh*=mix(ns,1.0f,k);
    }
    float ndl=dot(n,sd); float dif=sat((ndl+wrap)/(1.0f+wrap));
    float3 amb=ambCube(c,n)*ao;
    // 地面反弹的暖光（太阳照亮的地面向上回照）
    amb+=sunC*sd.y*0.12f*float3(0.9f,0.75f,0.5f)*sat(0.6f-0.5f*n.y)*ao*macroShadow(c,p+float3(0.0f,30.0f,0.0f));
    float3 col=alb/PI*(sunC*dif*sh+amb);
    if(fol && !lowQ){
        // 叶片：背面受光时透射（颜色更饱和）；被叶层挡住时仍有多次散射漏过来的光
        float3 tr=alb*mix(float3(1.0f),alb/max(lum(alb),1e-3f),0.30f)*1.1f;
        col+=tr/PI*sunC*trans*(sat(-ndl)*0.9f*sh+0.50f*scat*shBase);
    } else if(trans>0.0f){ float back=pow(sat(dot(rd,sd)),2.0f)*0.6f+sat(-ndl)*0.4f; col+=alb/PI*sunC*trans*back*(0.25f+0.75f*sh)*ao; }
    if(spec>0.0f){ float3 hv=normalize(sd-rd); col+=sunC*sh*spec*pow(sat(dot(n,hv)),40.0f)*sat(ndl); }
    return col;
}

static float3 sunDisk(const Ctx& c,constant Uni& u,float3 rd,float y){
    float3 sd=normalize(u.sunDir.xyz); float mu=dot(rd,sd); float ang=acos(clamp(mu,-1.0f,1.0f)); float rs=0.00480f;
    float e=1.0f-smoothstep(rs*0.92f,rs,ang); if(e<=0.0f) return float3(0.0f);
    float limb=sqrt(max(1.0f-sq(ang/rs),0.0f)); float3 ld=float3(0.55f,0.50f,0.42f)+float3(0.45f,0.50f,0.58f)*limb;
    return sunColAt(c,y)/(PI*rs*rs)*ld*e;
}

// ---------- 水面着色 ----------
static float3 shadeWater(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float t,bool riverHit,float jit){
    float3 p=ro+rd*t; float3 sd=normalize(u.sunDir.xyz); float foam;
    // 湖与河在河口处平滑过渡（rv: 0 湖 … 1 河）
    float rv=smoothstep(RIVER_X0-60.0f,RIVER_X0+25.0f,p.x); bool river=(rv>0.5f);
    float var; float3 n;
    if(rv<=0.0f) n=waterNormal(u,p,t,false,rd.y,foam,var);
    else if(rv>=1.0f) n=waterNormal(u,p,t,true,rd.y,foam,var);
    else { float f1,v1,f2,v2; float3 n1=waterNormal(u,p,t,false,rd.y,f1,v1), n2=waterNormal(u,p,t,true,rd.y,f2,v2); n=normalize(mix(n1,n2,rv)); foam=f2*rv; var=mix(v1,v2,rv); }
    float3 sunC=sunColAt(c,p.y);
    float sh=macroShadow(c,p);
    if(sh>0.01f && t<1500.0f) sh*=mix(nearShadow(c,p+float3(0.0f,0.06f,0.0f),sd,t,0.5f+0.002f*t),1.0f,smoothstep(1100.0f,1500.0f,t));
    // 瀑布水帘：陡立的水面
    float fall=river?smoothstep(4.0f,1.5f,fabs(p.x-FALL_X))*smoothstep(FALL_BOT+1.0f,FALL_BOT+3.0f,p.y):0.0f;
    float cosi=sat(dot(-rd,n)); float F=0.02f+0.98f*pow(1.0f-cosi,5.0f);
    float3 R=reflect(rd,n);
    if(rv<1.0f){ float sg=min(sqrt(var),0.05f)*(1.0f-rv); uint hj=hu(uint(jit*16777216.0f)); float a1=u2f(hj)*2.0f-1.0f, a2=u2f(hu(hj+7u))*2.0f-1.0f; R=normalize(R+float3(a1*sg*0.5f,a2*sg*1.2f,a1*a2*sg*0.5f)); }
    if(R.y<0.015f){ R.y=0.015f+0.3f*(0.015f-R.y); R=normalize(R); }
    float3 rc;
    Hit hr=marchScene(c,p+float3(0.0f,0.03f,0.0f),R,0.2f,30000.0f,110);
    if(hr.t>0.0f && hr.mat!=4){ rc=shadeSolid(c,u,p,R,hr,true); float3 ins; float3 T=aerial(c,u,p,R,hr.t,ins); rc=rc*T+ins; }
    else {
        rc=skyLUT(c,R);
        float tav; float4 cl=marchClouds(c,u,p,R,TMAX,jit,6,tav); rc=rc*cl.a+cl.rgb;
    }
    // 水体：浅处见底，深处青绿
    float depth=max(p.y-Hc(c,p.xz),0.0f);
    float3 Rf=refract(rd,n,0.75f); float path=depth/max(-Rf.y,0.25f);
    float3 bedc=mix(mix(float3(0.26f,0.24f,0.19f),float3(0.10f,0.12f,0.08f),sat(depth/5.0f)),stoneCol(p.xz+Rf.xz*path,t*0.0011f)*0.6f,rv);
    float3 amb=ambCube(c,float3(0.0f,1.0f,0.0f));
    float3 lit=(sunC*sat(sd.y*1.33f)*sh+amb)/PI;
    float3 ab=exp(-float3(0.42f,0.085f,0.055f)*path*mix(1.0f,0.6f,rv));
    float3 body=bedc*lit*ab+float3(0.012f,0.045f,0.050f)*lit*PI*(1.0f-ab.g);
    float3 col=mix(body,rc,F);
    // 太阳闪光（未解析的小波面用粗糙度表达）
    float3 hv=normalize(sd-rd); float ndh=sat(dot(n,hv));
    float al=mix(sqrt(sq(0.014f)+var*1.6f),0.15f+0.10f*vn2(p.xz*0.7f),rv); al=min(al,0.25f); float a2=al*al;
    float D=a2/(PI*sq(ndh*ndh*(a2-1.0f)+1.0f));
    col+=sunC*sh*D*F*0.25f/max(cosi,0.05f)*sat(dot(n,sd)*8.0f);
    // 白水
    float fm=max(foam,fall*(0.75f+0.25f*vn2(float2(p.z*2.0f,p.y*0.35f+u.tp.x*3.0f))));
    if(fall>0.0f){ float streak=vn2(float2(riverDist(p.x,p.z)*3.5f,p.y*0.22f+u.tp.x*(4.0f+0.03f*(FALL_TOP-p.y))))*0.5f+0.5f*vn2(float2(riverDist(p.x,p.z)*9.0f,p.y*0.6f+u.tp.x*9.0f)); fm=max(fm,fall*(0.55f+0.45f*streak)); }
    float3 foamC=float3(0.80f,0.84f,0.86f)/PI*(sunC*(0.35f+0.65f*sat(dot(n,sd)*0.5f+0.5f))*sh+amb*1.1f);
    col=mix(col,foamC,sat(fm));
    return col;
}

// ---------- 一条光线 ----------
static float3 traceRay(const Ctx& c,constant Uni& u,float3 ro,float3 rd,float jit,float tt,thread float& depthOut){
    float tmax=TMAX;
    // 湖面：解析求交
    float tLake=-1.0f;
    if(rd.y<-1e-5f && ro.y>0.0f){ float tl=-ro.y/rd.y; float3 pl=ro+rd*tl; if(pl.x>DAM_X-80.0f && pl.x<RIVER_X0+40.0f && fabs(pl.z)<1500.0f){ tLake=tl; tmax=tl; } }
    Hit h=marchScene(c,ro,rd,0.02f,tmax,(u.flags.x>0.5f)?340:200);
    float3 col; float dist=TMAX; bool sky=false;
    if(h.t>0.0f){
        dist=h.t;
        if(h.mat==4) col=shadeWater(c,u,ro,rd,h.t,true,jit);
        else col=shadeSolid(c,u,ro,rd,h,false);
    } else if(tLake>0.0f){ dist=tLake; col=shadeWater(c,u,ro,rd,tLake,false,jit); }
    else { sky=true; col=skyLUT(c,rd)+sunDisk(c,u,rd,ro.y); }
    if(!sky){ float3 ins; float3 T=aerial(c,u,ro,rd,dist,ins); col=col*T+ins; }
    // 同伴燕子
    if(u.bird[0].w>=0.0f){ float tb; float4 bd=traceBirds(u,ro,rd,tt,dist,sunColAt(c,ro.y),ambCube(c,float3(0.0f,1.0f,0.0f)),tb); if(bd.a>0.0f){ float3 ins; float3 T=aerial(c,u,ro,rd,tb,ins); col=bd.rgb*T+ins; dist=tb; } }
    // 瀑布水雾与虹
    { float4 ms=marchMist(c,u,ro,rd,dist,jit); col=col*ms.a+ms.rgb; }
    { float4 fw=marchFall(c,u,ro,rd,dist,jit); col=col*fw.a+fw.rgb; }
    // 云
    float tav; float4 cl=marchClouds(c,u,ro,rd,dist,jit,(u.flags.x>0.5f)?22:12,tav);
    if(cl.a<0.999f){ float3 ins; float3 T=aerial(c,u,ro,rd,min(tav,60000.0f),ins); col=col*cl.a+cl.rgb*T+ins*(1.0f-cl.a); }
    depthOut=min(dist,tav+(1.0f-step(cl.a,0.5f))*1e6f);
    return col;
}

// ---------- 主渲染内核：每次调用累积一个采样 ----------
kernel void renderK(texture2d<float,access::read_write> acc,
                    texture2d<float,access::sample> tH, texture2d<float,access::sample> tTreeA, texture2d<float,access::sample> tTreeCol,
                    texture2d<float,access::sample> tTreeBase, texture2d<float,access::sample> tMaxTree, texture2d<float,access::sample> tAux,
                    texture2d<float,access::sample> tSky, texture2d<float,access::sample> tShadow, texture2d<float,access::sample> tSunT,
                    texture3d<float,access::sample> tCloud, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=acc.get_width()||gid.y>=acc.get_height()) return;
    int s=int(u.tp.z); float N=u.tp.w;
    float2 r2=fract(float2(0.7548776662f,0.5698402910f)*float(s+1));
    float2 off=(r2-0.5f)*1.5f; float w=exp(-2.2f*dot(off,off));
    float jt=fract(ign(float2(gid),u.tp.y)+float(s)*0.61803399f);
    float tt=(float(s)+jt)/N;
    Ctx c={tH,tTreeA,tTreeCol,tTreeBase,tMaxTree,tAux,tSky,tShadow,tSunT,tCloud,u.hero.xyz,u.tp.x+tt*u.res.w,mix(u.anim0.z,u.anim1.z,tt),fabs(u.anim1.z-u.anim0.z)*6.0f,u.flags.z,0.0f};
    float3 ro=mix(u.cam0.pos.xyz,u.cam1.pos.xyz,tt);
    c.heroLod=1.0f-smoothstep(22.0f,48.0f,length(u.cam0.pos.xyz-(u.hero.xyz+float3(0.0f,8.5f,0.0f))));
    float3 fw=normalize(mix(u.cam0.fwd.xyz,u.cam1.fwd.xyz,tt)); float3 up0=mix(u.cam0.up.xyz,u.cam1.up.xyz,tt);
    float3 rt=normalize(cross(fw,up0)); float3 up=cross(rt,fw);
    float tanF=u.cam0.pos.w; float2 res=u.res.xy;
    float2 ndc=((float2(gid)+0.5f+off)/res)*2.0f-1.0f;
    float3 rd=normalize(fw+rt*(ndc.x*tanF*res.x/res.y)-up*(ndc.y*tanF));
    if(u.lens.y>0.0f){   // 薄透镜景深
        float ang=2.0f*PI*(ign(float2(gid),0.0f)+float(s)*0.7548776662f); float rr=sqrt(fract(float(s)*0.5698402910f+0.5f*ign(float2(gid.yx),3.0f)))*u.lens.y;
        float3 fpnt=ro+rd*(u.lens.x/dot(rd,fw)); ro+=rt*(cos(ang)*rr)+up*(sin(ang)*rr); rd=normalize(fpnt-ro);
    }
    float depth; float3 col=traceRay(c,u,ro,rd,jt,tt,depth);
    { float brake=mix(u.anim0.w,u.anim1.w,tt)*u.flags.y; if(brake>0.01f){ float3 rdc=float3(dot(rd,rt),dot(rd,up),dot(rd,fw)); float3 sdw=normalize(u.sunDir.xyz); float3 sunCam=float3(dot(sdw,rt),dot(sdw,up),dot(sdw,fw));
        float4 wg=ownWings(rdc,mix(u.anim0.x,u.anim1.x,tt),brake,sunColAt(c,ro.y),ambCube(c,up),sunCam); col=col*(1.0f-wg.a)+wg.rgb; } }
    col=min(col,float3(900.0f));
    if(any(isnan(col))) col=float3(0.0f);
    float4 prev=(s==0)?float4(0.0f):acc.read(gid);
    acc.write(prev+float4(col*w,w),gid);
}

// ---------- 太阳方向阴影高度图（每帧） ----------
kernel void shadowK(texture2d<float,access::write> outT, texture2d<float,access::sample> tH, texture2d<float,access::sample> tTreeA, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    uint W=outT.get_width(); if(gid.x>=W||gid.y>=W) return;
    float2 xz=float2(WORLD_X0,WORLD_Z0)+(float2(gid)+0.5f)/float(W)*WORLD_SIZE;
    float3 sd=normalize(u.sunDir.xyz); float2 a=normalize(sd.xz); float tanE=sd.y/length(sd.xz);
    float hs=-1e5f; float s=6.0f;
    for(int i=0;i<220;i++){
        float2 q=xz+a*s; float2 uv=w2uv(q);
        if(uv.x<0.0f||uv.y<0.0f||uv.x>1.0f||uv.y>1.0f) break;
        float h=tH.sample(sl,uv,level(0.0f)).x; if(q.x>RIVER_X0-170.0f && q.x<7200.0f && fabs(q.y)<900.0f) h=carveH(h,q.x,q.y);
        h+=tTreeA.sample(sl,uv,level(0.0f)).x*40.0f*0.6f;
        hs=max(hs,h-s*tanE);
        s=s*1.025f+10.0f;
    }
    outT.write(float4(hs,0.0f,0.0f,0.0f),gid);
}

// ---------- 后期 ----------
kernel void bloomDownK(texture2d<float,access::read> src, texture2d<float,access::write> dst, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=dst.get_width()||gid.y>=dst.get_height()) return;
    uint f=src.get_width()/dst.get_width(); float3 s=float3(0.0f); float n=0.0f;
    for(uint y=0;y<f;y++) for(uint x=0;x<f;x++){ uint2 q=gid*f+uint2(x,y); if(q.x<src.get_width()&&q.y<src.get_height()){ float4 v=src.read(q); float3 cc=(v.a>0.0f)?v.rgb/max(v.a,1e-6f):v.rgb; if(u.dbg.x>0.5f) cc=v.rgb; s+=cc; n+=1.0f; } }
    dst.write(float4(s/max(n,1.0f),1.0f),gid);
}
kernel void blurK(texture2d<float,access::read> src, texture2d<float,access::write> dst, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=dst.get_width()||gid.y>=dst.get_height()) return;
    int2 dir=(u.dbg.y>0.5f)?int2(0,1):int2(1,0); float3 s=float3(0.0f); float wsum=0.0f;
    int W=int(src.get_width()), H=int(src.get_height());
    for(int k=-8;k<=8;k++){ int2 q=clamp(int2(gid)+dir*k,int2(0),int2(W-1,H-1)); float w=exp(-float(k*k)/18.0f); s+=src.read(uint2(q)).rgb*w; wsum+=w; }
    dst.write(float4(s/wsum,1.0f),gid);
}
static float3 acesFit(float3 v){
    // Stephen Hill 的 ACES 拟合（RRT+ODT）
    float3 a=float3(dot(float3(0.59719f,0.35458f,0.04823f),v),dot(float3(0.07600f,0.90834f,0.01566f),v),dot(float3(0.02840f,0.13383f,0.83777f),v));
    float3 x=(a*(a+0.0245786f)-0.000090537f)/(a*(0.983729f*a+0.4329510f)+0.238081f);
    return sat3(float3(dot(float3(1.60475f,-0.53108f,-0.07367f),x),dot(float3(-0.10208f,1.10813f,-0.00605f),x),dot(float3(-0.00327f,-0.07276f,1.07602f),x)));
}
kernel void postK(texture2d<float,access::read> acc, texture2d<float,access::sample> b1, texture2d<float,access::sample> b2, texture2d<float,access::write> outT, constant Uni& u, uint2 gid [[thread_position_in_grid]]){
    if(gid.x>=outT.get_width()||gid.y>=outT.get_height()) return;
    float4 v=acc.read(gid); float3 col=v.rgb/max(v.a,1e-6f);
    float2 uv=(float2(gid)+0.5f)/float2(outT.get_width(),outT.get_height());
    float3 bl1=b1.sample(sl,uv,level(0.0f)).rgb, bl2=b2.sample(sl,uv,level(0.0f)).rgb;
    col=col*0.94f+bl1*0.04f+bl2*0.02f;
    col*=u.res.z;
    float2 q=uv*2.0f-1.0f; q.x*=float(outT.get_width())/float(outT.get_height()); float vg=1.0f-u.lens.z*sq(dot(q,q)/3.2f); col*=vg;
    col=acesFit(col*1.25f);
    col=mix(col,float3(lum(col)),-0.06f);
    col=pow(col,float3(1.0f/2.2f));
    col*=u.lens.w;
    float dn=ign(float2(gid),u.tp.y*1.7f+3.0f)-0.5f;
    col+=dn/255.0f;
    outT.write(float4(sat3(col),1.0f),gid);
}

// ---------- 探针：在一批点上求地形高/场景距离/水位（给主机设计与校验航线用） ----------
kernel void probeK(texture2d<float,access::sample> tH, texture2d<float,access::sample> tTreeA, texture2d<float,access::sample> tTreeCol,
                   texture2d<float,access::sample> tTreeBase, texture2d<float,access::sample> tMaxTree, texture2d<float,access::sample> tAux,
                   texture2d<float,access::sample> tSky, texture2d<float,access::sample> tShadow, texture2d<float,access::sample> tSunT,
                   texture3d<float,access::sample> tCloud, constant Uni& u, device const float4* pin, device float4* pout, uint gid [[thread_position_in_grid]]){
    Ctx c={tH,tTreeA,tTreeCol,tTreeBase,tMaxTree,tAux,tSky,tShadow,tSunT,tCloud,u.hero.xyz,0.0f,0.0f,0.0f,u.flags.z,0.0f};
    float3 p=pin[gid].xyz; int m;
    float hd=Hd(c,p.xz,1.0f); float d=mapScene(c,p,1.0f,m); float wl=waterLevel(p.x,p.z);
    if(p.x>DAM_X-80.0f && p.x<RIVER_X0+40.0f) wl=max(wl,0.0f);
    pout[gid]=float4(hd,d,wl,float(m));
}
