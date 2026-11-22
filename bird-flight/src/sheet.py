# sheet.py — 从视频均匀抽帧拼成总览图；或抽一段连续帧拼成"帧条"
# 用法：sheet.py video out.jpg [--n 60] [--cols 10] [--t0 a --t1 b] [--w 320]
import subprocess, sys, numpy as np
from PIL import Image, ImageDraw
def arg(k,d):
    return type(d)(sys.argv[sys.argv.index(k)+1]) if k in sys.argv else d
vid,out=sys.argv[1],sys.argv[2]
n=arg('--n',60); cols=arg('--cols',10); w=arg('--w',320)
info=subprocess.run(['ffprobe','-v','error','-select_streams','v:0','-show_entries','stream=width,height,r_frame_rate,nb_frames','-of','csv=p=0',vid],capture_output=True,text=True).stdout.strip().split(',')
W,H=int(info[0]),int(info[1]); num,den=info[2].split('/'); fps=float(num)/float(den); nb=int(info[3])
t0=arg('--t0',0.0); t1=arg('--t1',nb/fps)
h=int(round(w*H/W/2))*2
times=[t0+(t1-t0)*(i+0.5)/n for i in range(n)] if '--t0' not in sys.argv else [t0+(t1-t0)*i/max(n-1,1) for i in range(n)]
rows=(n+cols-1)//cols
sheet=Image.new('RGB',(cols*w,rows*h),(0,0,0)); dr=ImageDraw.Draw(sheet)
for i,t in enumerate(times):
    raw=subprocess.run(['ffmpeg','-v','error','-ss','%.4f'%t,'-i',vid,'-frames:v','1','-vf','scale=%d:%d'%(w,h),'-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True).stdout
    if len(raw)!=w*h*3: continue
    im=Image.frombuffer('RGB',(w,h),raw,'raw','RGB',0,1)
    sheet.paste(im,((i%cols)*w,(i//cols)*h))
    dr.text(((i%cols)*w+4,(i//cols)*h+3),'%.2f'%t,fill=(255,255,0))
sheet.save(out,quality=88)
print('saved',out,sheet.size)
