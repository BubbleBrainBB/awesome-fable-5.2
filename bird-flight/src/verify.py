# verify.py — 抽帧独立重渲比对：每隔 step 帧，用同一程序在低分辨率下独立重渲一帧，与成片对应帧按块比较
# 用法：verify.py video.mp4 step out_report.txt
import subprocess, sys, numpy as np, os
from PIL import Image
vid, step, rep = sys.argv[1], int(sys.argv[2]), sys.argv[3]
info = subprocess.run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=r_frame_rate,nb_frames','-of','csv=p=0',vid],capture_output=True,text=True).stdout.strip().split(',')
num, den = info[0].split('/'); fps = float(num)/float(den); nb = int(info[1])
W, H, BS = 480, 270, 30
os.makedirs('work/verify', exist_ok=True)
res=[]
for f in range(step//2, nb, step):
    t=f/fps
    raw=subprocess.run(['ffmpeg','-v','error','-ss','%.5f'%t,'-i',vid,'-frames:v','1','-vf','scale=%d:%d:flags=area'%(W,H),'-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True).stdout
    a=np.frombuffer(raw,np.uint8).reshape(H,W,3).astype(np.float32)
    out='work/verify/v%05d.png'%f
    r=subprocess.run(['./work/bird','still','--t','%.6f'%t,'--res',str(W),str(H),'--spp','8','--q','1','--out',out],capture_output=True,text=True)
    if 'frame ok' not in r.stderr: res.append((f,999.0,999.0)); continue
    b=np.asarray(Image.open(out).convert('RGB')).astype(np.float32)
    da=np.abs(a-b).mean(2).reshape(H//BS,BS,W//BS,BS).mean((1,3))
    res.append((f,float(da.max()),float(da.mean())))
res=np.array(res)
lines=['independent low-res re-render check: %d frames (every %d), %dx%d, 8 spp'%(len(res),step,W,H),
       'worst block difference: %.1f/255 at frame %d'%(res[:,1].max(),int(res[np.argmax(res[:,1]),0])),
       'median of per-frame worst block: %.1f; mean block difference overall: %.2f'%(np.median(res[:,1]),res[:,2].mean()),
       'frames with worst block > 24: %s'%[(int(f),round(m,1)) for f,m,_ in res if m>24]]
open(rep,'w').write('\n'.join(lines)+'\n'); print('\n'.join(lines))
