"""develop.py — "film development" of the renderer's linear float frames (own code: exposure, glare, tone curve)."""
import numpy as np
from scipy.ndimage import gaussian_filter, zoom

def srgb_oetf(x):
    x = np.clip(x, 0, 1)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)

def tone(x, contrast=1.25, mid_in=0.18, mid_out=0.19):
    """Smooth filmic curve y = x^c / (x^c + m^c): soft shoulder, gentle toe."""
    x = np.maximum(x, 0)
    mc = (mid_in ** contrast) * (1 - mid_out) / mid_out
    xc = np.power(x, contrast)
    return xc / (xc + mc)

def glare(img, amount=0.06, halation=0.04):
    """Veiling glare / bloom from three blurred copies at reduced resolution, plus reddish halation."""
    h, w, _ = img.shape
    s = 4
    hh, ww = h // s * s, w // s * s
    small = img[:hh, :ww].reshape(hh // s, s, ww // s, s, 3).mean(axis=(1, 3))
    small = np.minimum(small, 60.0)
    sc = w / 1920.0
    acc = np.zeros_like(small)
    for sig, wgt in ((3.0, 0.5), (10.0, 0.3), (32.0, 0.2)):
        acc += wgt * np.stack([gaussian_filter(small[..., c], sig * sc * 4 / s, mode='nearest') for c in range(3)], axis=-1)
    up = np.stack([zoom(acc[..., c], (h / acc.shape[0], w / acc.shape[1]), order=1) for c in range(3)], axis=-1)
    out = img * (1 - amount) + up * amount
    if halation > 0:
        hl = np.maximum(small - 1.0, 0)
        hb = np.stack([gaussian_filter(hl[..., c], 5.0 * sc * 4 / s, mode='nearest') for c in range(3)], axis=-1)
        hu = np.stack([zoom(hb[..., c], (h / hb.shape[0], w / hb.shape[1]), order=1) for c in range(3)], axis=-1)
        out = out + halation * hu.mean(axis=-1, keepdims=True) * np.array([1.0, 0.35, 0.15], np.float32)
    return out

def despeckle(img, k=1.6):
    """Remove isolated fireflies: a pixel may not exceed k x the 2nd brightest of its 8 neighbours (+small floor)."""
    lum = img @ np.array([0.2126, 0.7152, 0.0722], np.float32)
    p = np.pad(lum, 1, mode='edge')
    nb = np.stack([p[i:i + lum.shape[0], j:j + lum.shape[1]] for i in range(3) for j in range(3) if not (i == 1 and j == 1)], axis=0)
    nb.partition(6, axis=0)
    lim = nb[6] * k + 0.02
    sc = np.minimum(1.0, lim / np.maximum(lum, 1e-6))
    return img * sc[..., None]

def develop(lin, exposure=1.0, wb=(1, 1, 1), contrast=1.25, sat=1.0, glare_amt=0.06, halation=0.04, vignette=0.18, black=0.0, speckle=True, lift=(0, 0, 0)):
    img = lin.astype(np.float32) * exposure * np.array(wb, np.float32)
    if speckle:
        img = despeckle(img)
    if glare_amt > 0:
        img = glare(img, glare_amt, halation)
    h, w, _ = img.shape
    if vignette > 0:
        yy, xx = np.mgrid[0:h, 0:w]
        r2 = ((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (w / 2)) ** 2
        img = img * (1 - vignette * np.clip(r2, 0, 2) ** 1.2)[..., None]
    y = tone(img, contrast)
    # saturation around luma, slight highlight desaturation
    l = (y @ np.array([0.2126, 0.7152, 0.0722], np.float32))[..., None]
    s = sat * (1 - 0.35 * np.clip((l - 0.6) / 0.4, 0, 1) ** 2)
    y = l + (y - l) * s
    y = y + np.array(lift, np.float32) * (1 - y) ** 3
    y = np.clip((y - black) / (1 - black), 0, 1)
    return srgb_oetf(y)
