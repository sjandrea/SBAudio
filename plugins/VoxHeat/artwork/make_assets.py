import cv2, numpy as np, math, sys
OUT=sys.argv[1]
src=cv2.imread(sys.argv[2]).astype(np.float32)
H,W=src.shape[:2]
base=src.copy()
def bilinear(img,x,y):
    return cv2.remap(img,x.astype(np.float32),y.astype(np.float32),cv2.INTER_LINEAR,borderMode=cv2.BORDER_REFLECT)
yy,xx=np.mgrid[0:H,0:W].astype(np.float32)

knobs={'input':(274,336,82),'output':(1318,328,83),'density':(474,756,81),'drive':(796,748,106),'tone':(1120,752,82),'mix':(1384,758,81)}
gray=cv2.cvtColor(src.astype(np.uint8),cv2.COLOR_BGR2GRAY).astype(np.float32)
info={}
for k,(cx,cy,R) in knobs.items():
    # pointer angle: brightest direction inside the cap
    best=None
    for d in np.arange(-180,180,0.5):
        a=math.radians(d); rs=np.linspace(0.42*R,0.62*R,12)
        v=gray[(cy-rs*math.cos(a)).astype(int),(cx+rs*math.sin(a)).astype(int)].mean()
        if best is None or v>best[0]: best=(v,d)
    th=best[1]
    Wd=17.0 if k=='drive' else 32.0
    Rmax=R*(0.97 if k=='drive' else 1.08)
    dx=xx-cx; dy=yy-cy; r=np.sqrt(dx*dx+dy*dy)
    ang=np.degrees(np.arctan2(dx,-dy))
    da=(ang-th+180)%360-180
    m=(np.abs(da)<=Wd)&(r<=Rmax)
    t=(da+Wd)/(2*Wd)
    aL=np.radians(th-Wd-2); aR=np.radians(th+Wd+2)
    xL=cx+r*np.sin(aL); yL=cy-r*np.cos(aL); xR=cx+r*np.sin(aR); yR=cy-r*np.cos(aR)
    L=bilinear(src,xL,yL); Rr=bilinear(src,xR,yR)
    fill=L*(1-t[...,None])+Rr*t[...,None]
    base[m]=fill[m]
    info[k]=(cx,cy,R,th)
    print(k,'angle',th)

def sprite(k,name):
    cx,cy,R,th=info[k]; s=int(R*1.12)
    o=src[cy-s:cy+s,cx-s:cx+s]; b=base[cy-s:cy+s,cx-s:cx+s]
    Y,X=np.mgrid[-s:s,-s:s].astype(np.float32); r=np.hypot(X,Y)
    ang=np.degrees(np.arctan2(X,-Y)); da=(ang-th+180)%360-180
    capR=R*(0.86 if k=='drive' else 0.60)
    sector=(np.abs(da)<=(17 if k=='drive' else 32))&(r<=R*1.08)
    ol=o.mean(2); bl=b.mean(2)
    inner=(np.abs(o-b).sum(2)>25)&(r<capR)&sector
    beak=((ol<95)&sector&(r>=capR*0.9)).astype(np.uint8)
    beak=cv2.morphologyEx(beak,cv2.MORPH_CLOSE,np.ones((11,11),np.uint8))
    m=(inner|(beak>0)).astype(np.uint8)
    m=cv2.morphologyEx(m,cv2.MORPH_CLOSE,np.ones((5,5),np.uint8))
    a=cv2.GaussianBlur(m.astype(np.float32),(5,5),0)
    rgba=np.dstack([o,a*255]).astype(np.uint8)
    cv2.imwrite(f'{OUT}/{name}.png',rgba)
    return s

b8=np.clip(base,0,255).astype(np.uint8)
hsv=cv2.cvtColor(b8,cv2.COLOR_BGR2HSV)
V=hsv[...,2].astype(np.float32); S=hsv[...,1]

# value texts -> inpaint
mask=np.zeros((H,W),np.uint8)
for k,(cx,cy,R) in knobs.items():
    yc=478 if k in('input','output') else (930 if k=='drive' else 907)
    x0,x1,y0,y1=cx-78,cx+78,yc-22,yc+22
    sub=(V[y0:y1,x0:x1]>130)&(S[y0:y1,x0:x1]<90)
    mask[y0:y1,x0:x1]=sub*255
# TG / TUBE switch captions (drawn in code so the active one can light up)
for (x0, x1, y0, y1) in [(185, 245, 668, 702), (160, 270, 852, 890)]:
    sub = (V[y0:y1, x0:x1] > 120) & (S[y0:y1, x0:x1] < 110)
    mask[y0:y1, x0:x1] |= (sub * 255).astype(np.uint8)
# VU needle (and its shadow)
med=cv2.medianBlur(b8,21).astype(np.float32)
lum=b8.astype(np.float32).mean(2); medl=med.mean(2)
band=np.zeros((H,W),np.uint8)
cv2.line(band,(586,350),(722,488),255,26)
band[:363,:]=0; band[478:,:]=0
mask|=((np.abs(medl-lum)>12)&(band>0)).astype(np.uint8)*255
# footer text on the VU face (redrawn in code)
txt=np.zeros((H,W),np.uint8); txt[440:466,536:730]=255
mask|=(((medl-lum)>10)&(txt>0)).astype(np.uint8)*255
mask=cv2.dilate(mask,np.ones((5,5),np.uint8))
# VU face: replace the needle band with a smooth polynomial fit of the face
band2=np.zeros((H,W),np.uint8); cv2.line(band2,(584,348),(724,490),255,34)
band2[:363,:]=0; band2[470:,:]=0
wx0,wx1,wy0,wy1=530,800,335,469
win=b8[wy0:wy1,wx0:wx1].astype(np.float32)
Y,X=np.mgrid[wy0:wy1,wx0:wx1].astype(np.float32)
bw=band2[wy0:wy1,wx0:wx1]>0
bw[440-wy0:467-wy0,536-wx0:730-wx0]=True
lw=win.mean(2)
good=(~bw)&(lw>150)
xn=(X-667)/120; yn=(Y-400)/70
A=np.stack([np.ones_like(xn),xn,yn,xn*xn,xn*yn,yn*yn,xn**3,yn**3,xn*xn*yn,xn*yn*yn],-1)
fitimg=np.zeros_like(win)
for c in range(3):
    coef,*_=np.linalg.lstsq(A[good],win[...,c][good],rcond=None)
    fitimg[...,c]=A@coef
rng=np.random.default_rng(1); fitimg+=rng.normal(0,1.2,fitimg.shape)
soft=cv2.GaussianBlur(bw.astype(np.float32),(7,7),0)[...,None]
b8[wy0:wy1,wx0:wx1]=np.clip(win*(1-soft)+fitimg*soft,0,255).astype(np.uint8)
mask[wy0:wy1,wx0:wx1][bw]=0
mask[460:494,676:748]|=(((medl-lum)>8)[460:494,676:748]).astype(np.uint8)*255
b8=cv2.inpaint(b8,mask,6,cv2.INPAINT_TELEA)

# lamps -> off (keep a lit copy for the 'on' artwork)
lit_src = b8.copy()
for (lx, ly) in [(230, 77), (1230, 78)]:
    lit_src[ly-40:ly+40, lx-40:lx+40] = src.astype(np.uint8)[ly-40:ly+40, lx-40:lx+40]
lamps={'heat':(230,77),'auto':(1230,78)}
src8=src.astype(np.uint8)
for n,(lx,ly) in lamps.items():
    p=40
    dx=xx-lx; dy=yy-ly; r=np.sqrt(dx*dx+dy*dy)
    hsvb=cv2.cvtColor(b8,cv2.COLOR_BGR2HSV)
    jewel=r<=21; bezel=(r>21)&(r<=31)
    hv=hsvb[...,2].astype(np.float32); hs=hsvb[...,1].astype(np.float32)
    hv[jewel]*=0.28; hs[jewel]*=0.7; hs[bezel]*=0.12; hv[bezel]*=0.92
    hsvb[...,2]=np.clip(hv,0,255).astype(np.uint8); hsvb[...,1]=np.clip(hs,0,255).astype(np.uint8)
    b8=cv2.cvtColor(hsvb,cv2.COLOR_HSV2BGR)
    # halo: coloured pixels outside the bezel
    panel=np.array([b8[ly+45,lx-45],b8[ly-45,lx+45]]).mean(0)
    dist=np.abs(b8.astype(np.float32)-panel).sum(2)
    halo=((r>30)&(r<50)&(dist>18)).astype(np.uint8)*255
    halo&=~(((xx>lx+36))*255).astype(np.uint8)   # keep the label text
    b8=cv2.inpaint(b8,halo,5,cv2.INPAINT_TELEA)
    print(n,'off patch done')

# toggle lever: cut out as sprite, remove from background
lx0,lx1,ly0,ly1=190,242,698,792
reg=src8[ly0:ly1,lx0:lx1]
lev=np.zeros((H,W),np.uint8)
pts=np.array([[198,700],[232,700],[236,772],[228,792],[202,792],[194,772]],np.int32)
cv2.fillPoly(lev,[pts],255)
regl=reg.astype(np.float32).mean(2)
panelL=np.median(src8[700:740,160:185].astype(np.float32).mean(2))
cand=(lev[ly0:ly1,lx0:lx1]>0)&((regl>panelL+18)|(regl<panelL-35))
cand=cv2.morphologyEx(cand.astype(np.uint8),cv2.MORPH_CLOSE,np.ones((7,7),np.uint8))
# keep only the lever (above the nut hole)
cand[ly1-ly0-14:,:]=0
alpha=cv2.GaussianBlur(cand.astype(np.float32),(3,3),0)
full=np.zeros((H,W),np.uint8); cv2.fillPoly(full,[np.array([[196,698],[234,698],[238,760],[230,778],[200,778],[192,760]],np.int32)],255)
# above the nut: copy real panel texture from the left (same rows), feathered
top=np.zeros((H,W),np.float32); cv2.fillPoly(top,[np.array([[192,690],[238,690],[240,752],[190,752]],np.int32)],1.0)
top=cv2.GaussianBlur(top,(15,15),0)[...,None]
shifted=np.roll(b8,48,axis=1).astype(np.float32)   # texture from x-48
b8=(b8*(1-top)+shifted*top).astype(np.uint8)
full[:752,:]=0
b8=cv2.inpaint(b8,full,7,cv2.INPAINT_TELEA)
print('lever sprite origin',lx0,ly0,'size',lx1-lx0,ly1-ly0)
# ---- Recolour to the SB Audio hardware palette (OptoTube faceplate) --------
# Panel: steel-blue gradient faceTop -> faceBottom. Texture, engraving and shadows are kept
# by scaling the original relative to a smooth estimate of the old panel colour.
def hexbgr(h): return np.array([h & 0xff, (h >> 8) & 0xff, (h >> 16) & 0xff], np.float32)
faceTop, faceBottom, earCol = hexbgr(0x93a9bd), hexbgr(0x6f879c), hexbgr(0x56697b)
EAR_L, EAR_R = 62, 1530
obj = np.zeros((H, W), np.uint8)
for (cx, cy, R) in knobs.values():
    cv2.circle(obj, (cx, cy), int(R * 1.10), 255, -1)
cv2.rectangle(obj, (476, 152), (1114, 510), 255, -1)     # VU bezel
cv2.rectangle(obj, (468, 26), (1127, 137), 255, -1)      # nameplate
for (lx, ly) in lamps.values():
    cv2.circle(obj, (lx, ly), 33, 255, -1)               # lamps (amber / green)
cv2.circle(obj, (215, 784), 40, 255, -1)               # toggle nut
# Dymo tape labels (black / red): keep their colour
for (x0, y0, x1, y1) in [(212, 140, 337, 193), (1251, 140, 1386, 193), (130, 588, 303, 644), (405, 561, 546, 614),
                         (732, 533, 858, 587), (1058, 561, 1181, 614), (1335, 561, 1437, 614)]:
    cv2.rectangle(obj, (x0, y0), (x1, y1), 255, -1)
panel = (obj == 0)
panel[:, :EAR_L] = False; panel[:, EAR_R:] = False
f = b8.astype(np.float32)
def masked_blur(img, m, sigma):
    w = cv2.GaussianBlur(m.astype(np.float32), (0, 0), sigma)
    v = cv2.GaussianBlur(img * m[..., None], (0, 0), sigma)
    return v / np.maximum(w[..., None], 1e-3)
base_est = masked_blur(f, panel, 28)
t = (yy / (H - 1))[..., None]
target = faceTop * (1 - t) + faceBottom * t
earm = np.zeros((H, W), bool); earm[:, :EAR_L] = True; earm[:, EAR_R:] = True
ear_est = masked_blur(f, earm, 20)
wpanel = cv2.GaussianBlur(panel.astype(np.float32), (0, 0), 2.0)[..., None]
wear = earm.astype(np.float32)[..., None]
def recolour(img8):
    f = img8.astype(np.float32)
    newpanel = target * np.clip(f / np.maximum(base_est, 1.0), 0.0, 6.0) ** 0.75
    newear = earCol * np.clip(f / np.maximum(ear_est, 1.0), 0.0, 4.0) ** 0.9
    f = f * (1 - wpanel - wear) + newpanel * wpanel + newear * wear
    return np.clip(f, 0, 255).astype(np.uint8)
b8 = recolour(b8)
lit = recolour(lit_src)
for n, (lx, ly) in lamps.items():
    cv2.imwrite(f'{OUT}/{n}_on.png', lit[ly-40:ly+40, lx-40:lx+40])
print('recoloured panel to SB Audio steel blue')
cv2.imwrite(f'{OUT}/background.jpg',b8,[cv2.IMWRITE_JPEG_QUALITY,93])

