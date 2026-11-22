# qc.py — 成片逐帧质检：黑帧、冻结帧、亮度跳变、坏块（丢块特征）、时间闪烁指标
# 用法：qc.py video.mp4 report.txt [fade_in_s fade_out_s]
import subprocess, sys, numpy as np
vid, rep = sys.argv[1], sys.argv[2]
fin = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0; fout = float(sys.argv[4]) if len(sys.argv) > 4 else 1.8
info = subprocess.run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_frames','-of','csv=p=0',vid],capture_output=True,text=True).stdout.strip().split(',')
num, den = info[2].split('/'); fps = float(num)/float(den); nb = int(info[3])
W, H = 480, 270; BS = 30
p = subprocess.Popen(['ffmpeg','-v','error','-i',vid,'-vf','scale=%d:%d:flags=area'%(W,H),'-f','rawvideo','-pix_fmt','gray','-'],stdout=subprocess.PIPE)
means=[]; blocks=[]; diffs=[]; prev=None; n=0
while True:
    raw = p.stdout.read(W*H)
    if len(raw) < W*H: break
    f = np.frombuffer(raw,np.uint8).reshape(H,W).astype(np.float32)
    means.append(f.mean()); blocks.append(f.reshape(H//BS,BS,W//BS,BS).mean((1,3)))
    diffs.append(0.0 if prev is None else float(np.abs(f-prev).mean())); prev=f; n+=1
p.wait()
means=np.array(means); B=np.array(blocks); diffs=np.array(diffs); T=n/fps
t=np.arange(n)/fps; body=(t>fin)&(t<T-fout)
out=[]
out.append('file: %s'%vid); out.append('decoded frames: %d (container says %d), fps %.3f, duration %.3f s'%(n,nb,fps,T))
black=np.where((means<4.0)&body)[0]; out.append('black frames (mean<4, outside fades): %d %s'%(len(black),list(black[:20])))
frozen=np.where((diffs<0.02)&(np.arange(n)>0)&body)[0]; out.append('frozen frames (mean abs diff to previous < 0.02/255, outside fades): %d %s'%(len(frozen),[int(i) for i in frozen[:20]]))
out.append('smallest frame-to-frame difference in the body: %.3f at frame %d'%(diffs[body][1:].min(), int(np.where(body)[0][1:][np.argmin(diffs[body][1:])])))
jump=np.abs(np.diff(means)); jb=np.where((jump>6.0)&body[1:])[0]+1
out.append('luminance jumps (|d mean|>6/255 between consecutive frames, outside fades): %d %s'%(len(jb),[(int(i),round(float(jump[i-1]),1)) for i in jb[:20]]))
out.append('largest luminance step in the body: %.2f at frame %d (t=%.2f s)'%(jump[body[1:]].max(), int(np.where(body[1:])[0][np.argmax(jump[body[1:]])]+1), (np.where(body[1:])[0][np.argmax(jump[body[1:]])]+1)/fps))
# 丢块特征：某块突然接近全黑，而前后两帧同一块都不黑
lost=[]
for i in range(1,n-1):
    if not body[i]: continue
    m=(B[i]<3.0)&(B[i-1]>14.0)&(B[i+1]>14.0)
    if m.sum()>=1: lost.append((i,int(m.sum())))
out.append('lost-tile signature (block ~black while both temporal neighbours are not): %d frames %s'%(len(lost),lost[:20]))
# 单帧异常块：与前后帧都差很多，而前后帧彼此接近
odd=[]
for i in range(1,n-1):
    if not body[i]: continue
    a=np.abs(B[i]-B[i-1]); c=np.abs(B[i]-B[i+1]); nn=np.abs(B[i+1]-B[i-1])
    s=np.minimum(a,c)-nn; k=int((s>30.0).sum())
    if k>=3: odd.append((i,k,round(float(s.max()),1)))
out.append('single-frame outlier blocks (>=3 blocks deviating >30/255 from both neighbours): %d frames %s'%(len(odd),odd[:30]))
# 时间闪烁：块亮度的二阶差分（越小越稳）
fl=np.zeros(n); fl[1:-1]=np.abs(B[:-2]-2*B[1:-1]+B[2:]).mean((1,2))
out.append('temporal flicker index (mean |2nd difference| of 30px-block luma): median %.3f, p95 %.3f, p99 %.3f, max %.3f at frame %d'%(np.median(fl[body]),np.percentile(fl[body],95),np.percentile(fl[body],99),fl[body].max(),int(np.where(body)[0][np.argmax(fl[body])])))
top=np.argsort(-fl*body)[:12]; out.append('highest-flicker frames: %s'%[(int(i),round(float(fl[i]),2)) for i in top])
# 分段统计
out.append('per-10s: mean luma / mean frame diff / flicker')
for s in range(0,int(T)+1,10):
    m=(t>=s)&(t<s+10)
    if m.sum(): out.append('  %3d-%3d s: %6.1f %6.2f %6.3f'%(s,s+10,means[m].mean(),diffs[m].mean(),fl[m].mean()))
# 标注：被标记的帧里，哪些落在脚本里的跳视（快速转头/转眼）时刻附近
import os
if os.path.exists('work/flight.csv.shutter'):
    sac=np.loadtxt('work/flight.csv.shutter')[:,0].astype(int); near=lambda f: bool(np.any(np.abs(sac-f)<=4))
    fl_all=sorted(set([int(i) for i in jb]+[o[0] for o in odd]+[int(i) for i in top]))
    not_sac=[f for f in fl_all if not near(f)]
    out.append('of the %d flagged frames above, %d lie within 4 frames of a scripted saccade (fast gaze shift); not explained by a saccade: %s'%(len(fl_all),len(fl_all)-len(not_sac),not_sac))
open(rep,'w').write('\n'.join(out)+'\n'); print('\n'.join(out))
np.save('work/qc_report.npy',np.stack([means,diffs,fl]))
