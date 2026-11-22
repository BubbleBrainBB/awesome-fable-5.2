# exposure.py — 由预演的逐帧平均对数亮度生成"眼睛明暗适应"曝光表（60 fps）
# 用法：exposure.py lum.txt fps_of_lum out.txt
import sys, numpy as np
lum=np.loadtxt(sys.argv[1]); fps=float(sys.argv[2]); out=sys.argv[3]
fr=lum[:,0]; ll=lum[:,1]
t=fr/fps; T=t[-1]
t60=np.arange(0,int(T*60)+1)/60.0
l60=np.interp(t60,t,ll)
KEY=-1.85; P=0.62
target=np.exp(P*(KEY-l60))
target=np.clip(target,0.30,3.2)
# 非对称适应：变亮（要收光）快，变暗（要放光）慢
e=np.empty_like(target); cur=target[0]
for i,tg in enumerate(target):
    tau=0.30 if tg<cur else 0.85
    cur+=(tg-cur)*(1-np.exp(-1/60.0/tau)); e[i]=cur
# 再做一次零相位的轻微平滑，去掉逐帧抖动
k=np.exp(-0.5*(np.arange(-30,31)/12.0)**2); k/=k.sum()
e=np.convolve(np.pad(e,30,mode='edge'),k,mode='valid')
np.savetxt(out,e,fmt='%.4f')
print('exposure: min %.2f max %.2f'%(e.min(),e.max()), 'frames',len(e))
