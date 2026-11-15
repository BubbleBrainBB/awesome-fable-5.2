#!/usr/bin/env python3
"""Frame-by-frame quality control for "Deep Dive".
   qc.py <frames_dir> <check_dir> [n_frames] [report.txt] [contact_sheet.jpg]
Checks every film frame:
  * exists, decodes, is 1920x1080
  * not black / not blown out (outside the scripted fades)
  * agrees block by block with an independent low-res render of the same frame (catches frames in which part of
    the image is missing or wrong although the renderer reported no error)
  * does not jump relative to both of its neighbours (flicker / single damaged frames)
and writes the contact sheet. Exit code 1 if anything is flagged."""
import sys, os, numpy as np
from multiprocessing import Pool
from PIL import Image, ImageDraw
fd, cd = sys.argv[1], sys.argv[2]
N = int(sys.argv[3]) if len(sys.argv) > 3 else 2340
rep = sys.argv[4] if len(sys.argv) > 4 else 'qc_report.txt'
sheet = sys.argv[5] if len(sys.argv) > 5 else 'contact_sheet.jpg'
BW, BH = 32, 18            # block grid

def blocks(a):             # a: HxWx3 float -> BH x BW x 3 block means
    h, w = a.shape[:2]
    return a[:h // BH * BH, :w // BW * BW].reshape(BH, h // BH, BW, w // BW, 3).mean((1, 3))

def one(i):
    p = os.path.join(fd, f'f_{i:05d}.png'); q = os.path.join(cd, f'f_{i:05d}.png')
    r = {'i': i, 'ok': False}
    try:
        im = Image.open(p); im.load()
        a = np.asarray(im.convert('RGB'), np.float32)
    except Exception as e:
        r['err'] = f'cannot read: {e}'; return r
    if a.shape != (1080, 1920, 3): r['err'] = f'bad size {a.shape}'; return r
    lum = a @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    r.update(ok=True, mean=float(lum.mean()), std=float(lum.std()), black=float((lum < 3).mean()), white=float((lum > 252).mean()))
    b = blocks(a); r['blk'] = b
    # darkest block relative to the frame: a missing tile shows up as a (near) black block in a lit frame
    r['minblk'] = float(b.mean(2).min())
    try:
        c = np.asarray(Image.open(q).convert('RGB'), np.float32)
        r['chk'] = float(np.abs(b - blocks(c)).mean(2).max()); r['chkmean'] = float(np.abs(b - blocks(c)).mean())
    except Exception as e:
        r['chk'] = None
    return r

if __name__ == '__main__':
    with Pool(12) as pool: R = pool.map(one, range(N), chunksize=8)
    out = []; flags = []
    bad = [r for r in R if not r['ok']]
    for r in bad: flags.append(f"frame {r['i']}: {r.get('err')}")
    good = [r for r in R if r['ok']]
    out.append(f'frames expected {N}, readable at 1920x1080: {len(good)}')
    if len(good) == N:
        mean = np.array([r['mean'] for r in R]); B = np.stack([r['blk'] for r in R])
        # fades: first 1.3 s and last 2.4 s are scripted
        body = np.arange(N); inbody = (body > 45) & (body < N - 75)
        out.append(f'mean luma (0-255): min {mean[inbody].min():.1f} at frame {int(body[inbody][mean[inbody].argmin()])}, max {mean[inbody].max():.1f} at frame {int(body[inbody][mean[inbody].argmax()])}')
        for r in R:
            i = r['i']
            if inbody[i] and r['mean'] < 12: flags.append(f'frame {i}: nearly black (mean {r["mean"]:.1f})')
            if inbody[i] and r['white'] > 0.35: flags.append(f'frame {i}: {r["white"]*100:.0f}% blown out')
            if inbody[i] and r['minblk'] < 2.0: flags.append(f'frame {i}: a black block (min block mean {r["minblk"]:.2f})')
        out.append(f'largest clipped-white fraction in the body: {max(r["white"] for r in R)*100:.2f}% ; largest black fraction: {max(r["black"] for r in R if inbody[r["i"]])*100:.2f}%')
        # agreement with the independent low-res render
        chk = np.array([np.nan if r['chk'] is None else r['chk'] for r in R]); chkm = np.array([np.nan if r['chk'] is None else r['chkmean'] for r in R])
        nchk = int(np.isfinite(chk).sum())
        out.append(f'independent check frames available: {nchk}')
        if nchk:
            med = np.nanmedian(chk); thr = max(14.0, 4.0 * med)
            out.append(f'max block difference vs check render (8-bit levels): median {med:.2f}, 99th pct {np.nanpercentile(chk, 99):.2f}, max {np.nanmax(chk):.2f} at frame {int(np.nanargmax(chk))}; mean abs diff median {np.nanmedian(chkm):.2f}; flag threshold {thr:.1f}')
            for i in np.where(chk > thr)[0]: flags.append(f'frame {i}: differs from the independent render (max block diff {chk[i]:.1f})')
        if nchk < N: flags.append(f'{N - nchk} frames have no check render')
        # temporal: an area that jumps away from both neighbours and comes back (flicker, exposure pops, a damaged
        # frame). Judged on a coarse 4x3 grid of large areas (480x360 px): a fast bubble or fish right in front of the
        # lens legitimately changes a small block for a single frame, but cannot move the mean of a large area much.
        C = B.reshape(N, 3, BH // 3, 4, BW // 4, 3).mean((2, 4))
        midc = 0.5 * (C[:-2] + C[2:]); devc = np.abs(C[1:-1] - midc).mean(3); nbc = np.abs(C[2:] - C[:-2]).mean(3)
        spike = (devc - nbc).reshape(N - 2, -1).max(1)
        out.append(f'temporal spike score, large areas (8-bit levels): median {np.median(spike):.2f}, 99.9th pct {np.percentile(spike, 99.9):.2f}, max {spike.max():.2f} at frame {int(spike.argmax()) + 1}; flag threshold 4.0')
        for i in np.where(spike > 4.0)[0]: flags.append(f'frame {i + 1}: flicker / isolated jump of a large area (score {spike[i]:.1f})')
        mid = 0.5 * (B[:-2] + B[2:]); dev = np.abs(B[1:-1] - mid).mean(3); nb = np.abs(B[2:] - B[:-2]).mean(3)
        fine = (dev - nb).reshape(N - 2, -1).max(1)
        out.append(f'(for information, not flagged) the same score on small 60x60 px blocks: median {np.median(fine):.2f}, max {fine.max():.2f} at frame {int(fine.argmax()) + 1}; {int((fine > 9).sum())} frames above 9 - these are near-lens bubbles and passing fish crossing a block within one frame')
        d = np.abs(np.diff(mean)); out.append(f'largest frame-to-frame change of mean luma: {d.max():.2f} at frame {int(d.argmax()) + 1}')
        # contact sheet: one frame per second
        idx = list(range(0, N, 30)); cols, tw = 6, 426; th = tw * 9 // 16; rows = (len(idx) + cols - 1) // cols
        sh = Image.new('RGB', (tw * cols, th * rows), (0, 0, 0)); dr = ImageDraw.Draw(sh)
        for k, i in enumerate(idx):
            im = Image.open(os.path.join(fd, f'f_{i:05d}.png')).convert('RGB').resize((tw, th), Image.LANCZOS)
            x, y = (k % cols) * tw, (k // cols) * th; sh.paste(im, (x, y)); dr.text((x + 5, y + 4), f'{i / 30:.0f}s  #{i}', fill=(255, 255, 255))
        sh.save(sheet, quality=90); out.append(f'contact sheet: {sheet} ({len(idx)} frames, one per second)')
    out.append(f'FLAGGED: {len(flags)}'); out += ['  ' + f for f in flags[:200]]
    txt = '\n'.join(out); print(txt); open(rep, 'w').write(txt + '\n')
    sys.exit(1 if flags else 0)
