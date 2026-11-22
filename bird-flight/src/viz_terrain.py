import numpy as np, sys
from PIL import Image
f=open(sys.argv[1],'rb'); N=np.frombuffer(f.read(4),np.int32)[0]
h=np.frombuffer(f.read(4*N*N),np.float32).reshape(N,N); fl=np.frombuffer(f.read(4*N*N),np.float32).reshape(N,N)
dx=24000/N
gy,gx=np.gradient(h,dx)
n=np.stack([-gx,np.ones_like(h),-gy],-1); n/=np.linalg.norm(n,axis=-1,keepdims=True)
L=np.array([-0.75,0.55,0.35]); L/=np.linalg.norm(L)
sh=np.clip((n*L).sum(-1),0,1)
col=np.zeros((N,N,3))
t=np.clip((h+360)/3000,0,1)[...,None]
col=(0.25+0.75*sh[...,None])*( (1-t)*np.array([0.45,0.6,0.35]) + t*np.array([1,1,1]))
col[(h<0)&(np.arange(N)[None,:]*dx-14000>-1800)]=[0.2,0.4,0.8]
col[fl>6.3]*= 0.6
img=Image.fromarray((np.clip(col,0,1)*255).astype(np.uint8))
img.resize((1024,1024),Image.LANCZOS).save(sys.argv[2])
# 路线区域放大
x0,x1,z0,z1=-2500,4500,-2500,2500
i0,i1=int((x0+14000)/dx),int((x1+14000)/dx); j0,j1=int((z0+12000)/dx),int((z1+12000)/dx)
img.crop((i0,j0,i1,j1)).resize(((i1-i0)*2,(j1-j0)*2),Image.BICUBIC).save(sys.argv[2].replace('.png','_zoom.png'))
slope=np.degrees(np.arctan(np.hypot(gx,gy)))
print('slope pct 10/50/90/99:',np.percentile(slope,[10,50,90,99]))
print('h stats',h.min(),h.max(), 'profile along axis z=0:',[round(float(h[N//2,int((x+14000)/dx)]),0) for x in range(-13000,10000,1000)])
