# 从湖岸一点沿最大汇水支流向上游追踪，输出支谷线（给航线设计用）
import numpy as np, sys
f=open('work/terrain.bin','rb'); N=np.frombuffer(f.read(4),np.int32)[0]
h=np.frombuffer(f.read(4*N*N),np.float32).reshape(N,N); fl=np.frombuffer(f.read(4*N*N),np.float32).reshape(N,N)
dx=24000/N
def ij(x,z): return int((x+14000)/dx), int((z+12000)/dx)
def xz(i,j): return (i+0.5)*dx-14000, (j+0.5)*dx-12000
def upstream(x,z,maxlen=6000):
    i,j=ij(x,z); out=[]; L=0
    while L<maxlen:
        out.append((*xz(i,j),float(h[j,i]),float(fl[j,i])))
        best=None;bv=-1
        for di in (-1,0,1):
            for dj in (-1,0,1):
                if di==0 and dj==0: continue
                a,b=i+di,j+dj
                if h[b,a]>h[j,i]-0.01 and fl[b,a]>bv and fl[b,a]<=fl[j,i]+0.01: bv=fl[b,a];best=(a,b)
        if best is None or bv<4.2: break
        L+=dx*np.hypot(best[0]-i,best[1]-j); i,j=best
    return out
# 找北岸附近汇水最大的入湖口：扫描湖北岸带
cands=[]
for x in np.arange(-1500,1000,60):
    for z in np.arange(-1200,-150,30):
        i,j=ij(x,z)
        if 0.5<h[j,i]<12 and fl[j,i]>5.6: cands.append((fl[j,i],x,z))
cands.sort(reverse=True)
seen=[]
for c in cands:
    if all(abs(c[1]-s[1])>250 for s in seen): seen.append(c)
    if len(seen)>=5: break
for c in seen:
    print('mouth flow=%.2f at x=%.0f z=%.0f'%c)
    p=upstream(c[1],c[2])
    for k in range(0,len(p),max(1,len(p)//14)): print('   x=%7.0f z=%7.0f h=%6.0f flow=%.2f'%p[k])
    print('   end x=%7.0f z=%7.0f h=%6.0f'%p[-1][:3])
