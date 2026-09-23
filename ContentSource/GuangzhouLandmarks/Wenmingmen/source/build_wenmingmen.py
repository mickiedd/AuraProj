"""Wenmingmen *interpretive* procedural architectural reconstruction.
Run: python build_wenmingmen.py --out /path/to/Wenmingmen_UE5 [--textures-only]
Requires numpy, Pillow, scipy, trimesh; uses meters, Z-up in source coordinates.
This produces a production-blockout/high-detail study asset, NOT a historically measured scan.
"""
import os, math, argparse, json, time, random
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter
import trimesh
from trimesh.visual.texture import TextureVisuals
from trimesh.visual.material import PBRMaterial
rng = np.random.default_rng(1879)
random.seed(1879)
P=np.pi

class Builder:
 def __init__(self):self.bags={n:{'v':[],'f':[],'uv':[],'n':0} for n in ['stone','limestone','roof','wood','plaster','iron','water','foliage']};self.counts={}
 def add(self,m,verts,faces,uv=None):
  b=self.bags[m]; v=np.asarray(verts,dtype=np.float32);f=np.asarray(faces,dtype=np.uint32);base=b['n'];b['v'].append(v);b['f'].append(f+base);b['n']+=len(v)
  if uv is None:uv=np.zeros((len(v),2),dtype=np.float32)
  b['uv'].append(np.asarray(uv,dtype=np.float32))
 def quad(self,m,a,b,c,d,scale=1):
  verts=[a,b,c,d];a,b,c,d=map(np.asarray,(a,b,c,d))
  lu=np.linalg.norm(b-a)/scale;lv=np.linalg.norm(d-a)/scale
  self.add(m,verts,[[0,1,2],[0,2,3]],[[0,0],[lu,0],[lu,lv],[0,lv]])
 def box(self,m,c,s,scale=1):
  x,y,z=c;hx,hy,hz=np.array(s)/2
  V=[(x-hx,y-hy,z-hz),(x+hx,y-hy,z-hz),(x+hx,y+hy,z-hz),(x-hx,y+hy,z-hz),(x-hx,y-hy,z+hz),(x+hx,y-hy,z+hz),(x+hx,y+hy,z+hz),(x-hx,y+hy,z+hz)]
  qs=np.array([(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
  vv=np.asarray(V,dtype=np.float32)[qs].reshape(-1,3)
  uv=np.tile(np.array([[0,0],[1,0],[1,1],[0,1]],dtype=np.float32),(6,1));f=np.arange(24,dtype=np.uint32).reshape(6,4)
  self.add(m,vv,np.vstack((f[:,[0,1,2]],f[:,[0,2,3]])),uv)
 def beam(self,m,a,b,w,h=None,scale=1):
  a=np.array(a,dtype=float);b=np.array(b,dtype=float);h=w if h is None else h;direction=b-a;L=np.linalg.norm(direction)
  if L<1e-6:return
  direction/=L;other=np.cross(direction,[0,0,1]);
  if np.linalg.norm(other)<.01:other=np.cross(direction,[0,1,0])
  other/=np.linalg.norm(other);up=np.cross(direction,other);up/=np.linalg.norm(up)
  V=[]
  for t in [a,b]:
   for s1,s2 in [(-1,-1),(1,-1),(1,1),(-1,1)]:V.append(t+other*s1*w*.5+up*s2*h*.5)
  qs=np.array([(3,2,1,0),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
  vv=np.asarray(V,dtype=np.float32)[qs].reshape(-1,3)
  uv=np.tile(np.array([[0,0],[1,0],[1,1],[0,1]],dtype=np.float32),(6,1));f=np.arange(24,dtype=np.uint32).reshape(6,4)
  self.add(m,vv,np.vstack((f[:,[0,1,2]],f[:,[0,2,3]])),uv)
 def tube(self,m,a,b,r=0.07,seg=8,scale=.5):
  a=np.asarray(a,dtype=float);b=np.asarray(b,dtype=float);d=b-a;L=np.linalg.norm(d)
  if L<1e-6:return
  d/=L;side=np.cross(d,[0,0,1]);
  if np.linalg.norm(side)<.01:side=np.cross(d,[0,1,0])
  side/=np.linalg.norm(side);up=np.cross(d,side)
  vv=[];ff=[];uv=[]
  for i in range(seg+1):
   theta=2*P*i/seg;offset=r*(math.cos(theta)*side+math.sin(theta)*up)
   vv.extend([a+offset,b+offset]);uv.extend([[i/seg,0],[i/seg,L/scale]])
   if i<seg:k=2*i;ff.extend([[k,k+1,k+3],[k,k+3,k+2]])
  self.add(m,vv,ff,uv)
 def arch_strip(self,m,xc,y0,y1,zc,r,delta=0.32,N=36):
  # arch ring and tunnel lining along horizontal x-z semicircle
  ts=np.linspace(0,P,N+1)
  for i in range(N):
   t0,t1=ts[i:i+2];outer0=np.array([xc-r*math.cos(t0),zc+r*math.sin(t0)]);outer1=np.array([xc-r*math.cos(t1),zc+r*math.sin(t1)]);
   ri=r-delta;inner0=np.array([xc-ri*math.cos(t0),zc+ri*math.sin(t0)]);inner1=np.array([xc-ri*math.cos(t1),zc+ri*math.sin(t1)])
   for yy in [y0,y1]:self.quad(m,[*outer0[:1],yy,outer0[1]],[*inner0[:1],yy,inner0[1]],[*inner1[:1],yy,inner1[1]],[*outer1[:1],yy,outer1[1]],scale=.6)
   # intrados radial line, from near end to far end
   self.quad(m,[inner0[0],y0,inner0[1]],[inner0[0],y1,inner0[1]],[inner1[0],y1,inner1[1]],[inner1[0],y0,inner1[1]],scale=.6)
 def merge(self):
  meshes=[]
  for k,b in self.bags.items():
   if not b['n']:continue
   v=np.vstack(b['v']);f=np.vstack(b['f']);uv=np.vstack(b['uv'])
   mesh=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv));mesh.metadata['material']=k;mesh.metadata['name']='WM_'+k;meshes.append((k,mesh));self.counts[k]=len(f)
  return meshes

B=Builder()
def masonry(xmin,xmax,ymin,ymax,zmin,zmax,m='stone',seg=.74,course=.31,individual=True):
 B.box(m,((xmin+xmax)/2,(ymin+ymax)/2,(zmin+zmax)/2),(xmax-xmin,ymax-ymin,zmax-zmin),1.2)
 if not individual:return
 # irregular individual thin face stones, face positions and staggered courses
 for face in [ymin-.013,ymax+.013]:
  n=int((zmax-zmin)/course)
  for j in range(n):
   zz=zmin+(j+.5)*course;start=xmin-.36*(j%2);xx=start
   while xx<xmax:
    ww=seg*(.78+.36*random.random());l=max(xmin,xx+.018);r=min(xmax,xx+ww-.018)
    if r-l>.10:
     B.box(m,((r+l)/2,face,zz),(r-l,.037,course-.032),.8)
    xx+=ww

def cornice(x0,x1,y0,y1,z,m='stone'):
 for dz,s in [(0,.12),(.13,.18),(.25,.1)]:B.box(m,((x0+x1)/2,(y0+y1)/2,z+dz),(x1-x0+s,y1-y0+s,.1),.7)

# MAIN WALL: 19.6 m center body + 35 m of left/right curtains, river-facing side
for xa,xb in [(-9.7,-2.9),(2.9,9.7)]:masonry(xa,xb,-2.05,2.05,0,9.7,seg=.91,course=.36)
# Top spandrel above the curved arch: narrow vertical slices create an actual empty gate opening
r=2.9;spring=3.30;N=60
for i in range(N):
 x0=-r+2*r*i/N;x1=-r+2*r*(i+1)/N;xc=(x0+x1)/2
 zb=spring+math.sqrt(max(0,r*r-xc*xc))+.15
 if zb<9.7:B.box('stone',(xc,0,(zb+9.7)/2),(x1-x0+.002,4.1,9.7-zb),.85)
B.arch_strip('limestone',0,-2.12,2.12,spring,3.0,.40,64)
for side in [-1,1]:
 B.box('limestone',(side*2.95,0,1.65),(.31,4.23,3.3),.6)
 B.box('limestone',(side*4.0,-2.1,3.9),(.75,.22,.9),.5)
# gate tunnel and double leaf wood gate; intentionally open inward
for sign in [-1,1]:
 B.box('wood',(sign*1.15,1.84,1.62),(2.25,.17,3.24),.6)
 for ix in range(4):
  for iz in range(7):B.box('iron',(sign*(.3+.55*ix),1.71,.34+.41*iz),(.075,.07,.075),.10)
 B.beam('iron',(sign*.18,1.68,1.7),(sign*2.45,1.68,1.7),.075)
# old stone courses for inner central facade rising to the arch shoulders only
for yy in [-2.10,2.10]:
 for j in range(29):
  z=.18+j*.324
  for side in [-1,1]:
   x=side*(3.0+(.42 if j%2 else .12))
   while abs(x)<9.52:
    ww=.72+.30*random.random();x1=abs(x);x2=min(9.68,x1+ww)
    if x2>x1+.1:B.box('stone',(side*(x1+x2)/2,yy,z),(x2-x1,.034,.29),.82)
    x=side*(x2+.025)
# carved stone plaque front and lintel
B.box('limestone',(0,-2.22,7.52),(3.0,.20,1.02),.7)
B.box('stone',(0,-2.36,7.52),(2.58,.05,.68),.45)
# plaque carving geometry indicates strokes but characters supplied separately as editable authoring note
# wall crest cornice, bands
cornice(-9.7,9.7,-2.1,2.1,9.72)
for xa,xb in [(-28.5,-9.7),(9.7,28.5)]:
 masonry(xa,xb,-1.62,1.62,0,7.55,seg=.92,course=.36)
 cornice(xa,xb,-1.65,1.65,7.65)
 for x in np.arange(xa+.45,xb-.2,1.62):
  B.box('stone',(x,-1.42,8.28),(1.10,.88,1.06),.75)
  B.box('stone',(x,1.42,8.28),(1.10,.88,1.06),.75)
# corner pylons and buttresses
for x in [-9.95,9.95,-28.5,28.5]:
 B.box('limestone',(x,0,3.9),(.62,3.65,7.8),.7)

# WALKWAY & SUPERSCTRUCTURE, ground floor pavilion
B.box('wood',(0,0,10.25),(17.1,6.0,.34),1.3)
for z,depth,width in [(10.48,6.5,18.0),(13.58,6.35,17.35)]:
 B.box('wood',(0,0,z),(width,depth,.19),.95)
 for s in [-1,1]:
  for y in [-2.7,2.7]:B.beam('wood',(-8.25,y,z+.10),(8.25,y,z+.10),.21)
# timber posts, beams, lattice windows, balustrades
for x in np.linspace(-7.7,7.7,7):
 for y in [-2.6,2.6]:
  B.beam('wood',(x,y,10.57),(x,y,15.0),.24)
  B.beam('wood',(x,y,13.7),(x,y,15.45),.19)
for y in [-2.7,2.7]:
 for z in [12.2,14.95,15.45]:B.beam('wood',(-8.05,y,z),(8.05,y,z),.22)
for x in [-8.1,8.1]:
 for z in [12.2,15.3]:B.beam('wood',(x,-2.65,z),(x,2.65,z),.20)
# Lattice window grid facade with visible empty gaps
for y in [-2.72,2.72]:
 for panel in range(6):
  a=-7.65+panel*2.55;b=a+2.27
  for xx in [a,b]:B.beam('wood',(xx,y,12.1),(xx,y,14.7),.11)
  for zz in [12.25,14.6]:B.beam('wood',(a,y,zz),(b,y,zz),.12)
  for xx in np.linspace(a+.28,b-.28,5):B.beam('wood',(xx,y,12.4),(xx,y,14.48),.033)
  for zz in np.linspace(12.54,14.36,5):B.beam('wood',(a+.12,y,zz),(b-.12,y,zz),.030)
# Balcony fence front and side
for y in [-3.05,3.05]:
 for z in [10.85,11.43]:B.beam('wood',(-8.2,y,z),(8.2,y,z),.12)
 for x in np.arange(-8.05,8.1,.48):B.beam('wood',(x,y,10.64),(x,y,11.48),.046)
for x in [-8.33,8.33]:
 for y in np.arange(-2.7,2.9,.5):B.beam('wood',(x,y,10.68),(x,y,11.48),.05)
 for z in [10.86,11.44]:B.beam('wood',(x,-2.85,z),(x,2.85,z),.12)
# visible bracket sets beneath eaves, timber cantilever articulation
for x in np.arange(-7.65,7.7,1.55):
 for sign in [-1,1]:
  y=sign*2.75
  for k in range(3):
   B.beam('wood',(x,y+sign*.12*k,14.68+.16*k),(x,y+sign*(.55+.20*k),14.68+.16*k),.11,.19)
   B.beam('wood',(x-.22,y+sign*(.30+.18*k),14.66+.16*k),(x+.22,y+sign*(.30+.18*k),14.66+.16*k),.095)

# Two upturned tiled hipped roofs. All tiles actually modeled as curved geometry.
def roof(z,halfwidth,halfdepth,ridgehalf,step=.30,rows=25):
 # ridge runs x, central slope subdivided into tile rows down y, corner turn-up
 for s in [-1,1]:
  for side in range(2):
   # upper / lower roof faces alternate on x sides? roofs can have taper at x ends
   pass
  xpositions=np.arange(-halfwidth,halfwidth+step,step)
  nrow=rows
  for j in range(len(xpositions)-1):
   x0=xpositions[j];x1=xpositions[j+1];xc=(x0+x1)/2
   # curved hipped end corners: eave bends higher at four corners
   for k in range(nrow):
    t0=k/nrow;t1=(k+1)/nrow
    def facept(x,t):
     Y=s*(.12+(halfdepth-.12)*t)
     edge=max(0,(abs(x)-ridgehalf)/(halfwidth-ridgehalf))
     Z=z-2.38*(t**1.08)+.44*(t**3)+.50*(edge**2)*(t**3)
     return (x,Y,Z)
    v0,v1,v2,v3=facept(x0,t0),facept(x1,t0),facept(x1,t1),facept(x0,t1)
    B.quad('roof',v0,v1,v2,v3,scale=.6)
    # raised ribs at each x-column, each clay overlap row a separate segment
    if j%2==0:
     xa=facept(x0,t0);xb=facept(x0,t1)
     B.tube('roof',(xa[0],xa[1],xa[2]+.048),(xb[0],xb[1],xb[2]+.048),r=.045,seg=7,scale=.44)
  # tall curved outward ridge/corners
  for x in [-halfwidth,halfwidth]:
   pts=[]
   for t in np.linspace(0,1,23):
    y=s*(halfdepth*t);edge=max(0,(abs(x)-ridgehalf)/(halfwidth-ridgehalf));zz=z-2.38*t**1.08+.44*t**3+.50*edge*edge*t**3
    pts.append((x,y,zz+.08))
   for a,b in zip(pts[:-1],pts[1:]):B.tube('roof',a,b,.092,8)
  # curved edge corner eave flashing
  for i in range(len(xpositions)-1):
   x0=xpositions[i];x1=xpositions[i+1]
   t=1
   def eaveheight(x):return z-2.38+.44+.5*max(0,(abs(x)-ridgehalf)/(halfwidth-ridgehalf))**2
   B.beam('wood',(x0,s*halfdepth,eaveheight(x0)-.06),(x1,s*halfdepth,eaveheight(x1)-.06),.10,.12)
 # long ridge and finial ends
 B.tube('roof',(-ridgehalf,0,z+.08),(ridgehalf,0,z+.08),.155,12)
 for sign in [-1,1]:
  x=sign*ridgehalf
  for i in range(6):B.box('roof',(x+sign*.08*i,0,z+.25+.13*i),(.19,.21,.19),.3)
roof(17.67,9.22,4.5,7.2,step=.18,rows=34)
# second thin subsidiary lower canopy + timber beneath, size follows reference lower eave
roof(15.45,8.76,3.57,7.1,step=.22,rows=24)
# slate plinth and small masonry parapet around roof deck
for x in np.arange(-8,8.1,1.6):
 B.box('stone',(x,-2.15,10.2),(.6,.38,.65),.85)
 B.box('stone',(x,2.15,10.2),(.6,.38,.65),.85)

# stone bridge crossing the canal from gate toward foreground, axis Y
# bridge arches have real openings when viewed from canal / X side
for xface in [-4.8,4.8]:
 for jy in range(72):
  y0=-23.5+jy*.30;y1=y0+.30;ym=(y0+y1)/2
  # canal central interval [-19,-6]; rise of masonry over barrel vault
  opening=(-19.2<ym<-6.2)
  arch_z=(-2.62+4.10*math.sqrt(max(0,1-((ym+12.7)/6.5)**2))) if opening else -3.0
  lower=arch_z if opening else -3.0
  # part over intrados supports pedestrian street deck at z=.15
  if lower<.06:B.box('limestone',(xface,ym,(lower+.09)/2),(.35,.28,.09-lower),.75)
 # arch intrados visible on outside side walls and at x-extremes
 for i in range(56):
  th0=P*i/56;th1=P*(i+1)/56
  y0=-12.7-6.5*math.cos(th0);y1=-12.7-6.5*math.cos(th1)
  z0=-2.62+4.1*math.sin(th0);z1=-2.62+4.1*math.sin(th1)
  # upper crown of bridge arch sits above road so decorative voussoir limited by deck
  if z0<.0 and z1<.0:B.beam('limestone',(xface-.22,y0,z0),(xface+.22,y1,z1),.18,.18)
for y in np.arange(-23,-2.3,.85):
 B.box('limestone',(0,y,.10),(9.5,.82,.24),1.0)
# parapet rails and posts
for xx in [-4.52,4.52]:
 for zz in [.65,1.10]:B.beam('limestone',(xx,-23.5,zz),(xx,-3.05,zz),.14,.15)
 for yy in np.arange(-23.25,-3.05,1.42):
  B.box('limestone',(xx,yy,.82),(.39,.39,1.3),.6)
  B.box('limestone',(xx,yy,1.5),(.5,.5,.17),.65)
# aqueduct/canal horizontal x river, shallow modeled water plane
B.box('water',(0,-12.6,-2.8),(65,11.0,.08),2.5)
for y in [-18.35,-6.85]:
 B.box('stone',(0,y,-2.45),(65,.64,1.75),1.0)
# canal stair blocks on waterside
for side in [-1,1]:
 for j in range(5):
  B.box('stone',(side*(7+j*1.25),-6.82,-2.05+.1*j),(1.22,.85,.42),.8)
# supplementary plank awnings and wooden canal mooring barge
B.box('wood',(11.6,-15.1,-2.34),(5.8,2.08,.35),1.2)
for x in np.arange(9.25,14.1,.65):B.box('wood',(x,-15.1,-2.09),(.36,2.20,.065),.6)
for x in [9.45,13.75]:
 for y in [-16.08,-14.08]:B.beam('wood',(x,y,-2.0),(x,y,-.45),.055)
# boat canopy frame
for xx in np.arange(9.35,14.15,.48):B.beam('wood',(xx,-16.02,-.62),(xx,-14.12,-.62),.047)
# simple greenery on facade, vines mesh broad leaves - optional environmental geo
for cx,cy in [(-7.2,-2.18),(7.5,-2.18),(-12,-1.71),(13,-1.71)]:
 for j in range(28):
  xx=cx+float(rng.normal(0,.44));zz=2.1+j*.16+float(rng.normal(0,.10));yy=cy-.085
  B.beam('foliage',(xx,yy,zz),(xx+.11,yy-.02,zz+.05),.018,.055,.12)

# texture shader maps: high res source, balanced file size; does not use any scraped texture.
def tex_generate(name,size,out):
 n=size
 # two-frequency procedural stone/wood texture, tileable-ish X/Y edges
 low=rng.normal(0,1,(256,256)).astype(np.float32)
 from PIL import Image as I
 low=np.asarray(I.fromarray(low,mode='F').resize((n,n),I.Resampling.BILINEAR),dtype=np.float32)
 sm=gaussian_filter(low,sigma=2.0);sm=(sm-sm.mean())/(sm.std()+1e-6)
 hh=rng.normal(0,1,(n,n)).astype(np.float32)
 yy,xx=np.ogrid[:n,:n]
 if name=='stone':
  joints=((xx%354)<8)|((yy%156)<8);h=sm*.18+hh*.16-joints*.60
  rgb=np.stack((np.full((n,n),.49),np.full((n,n),.485),np.full((n,n),.465)),axis=-1);rgb+=(sm*.048+hh*.023)[...,None];rgb[joints]*=.52;rough=.87
 elif name=='limestone':
  joints=((xx%600)<7)|((yy%236)<7);h=sm*.13+hh*.10-joints*.4
  rgb=np.stack((np.full((n,n),.61),np.full((n,n),.59),np.full((n,n),.55)),axis=-1);rgb+=(sm*.045+hh*.018)[...,None];rgb[joints]*=.70;rough=.82
 elif name=='roof':
  lines=np.cos(xx*P/110)**16;h=sm*.13+hh*.12+lines*.24
  rgb=np.stack((np.full((n,n),.265),np.full((n,n),.277),np.full((n,n),.275)),axis=-1);rgb+=(sm*.027+hh*.014-lines*.027)[...,None];rough=.76
 elif name=='wood':
  grain=(np.sin(xx*.05+sm*2.2)**2)*.14 + np.sin(xx*.012+sm)**2*.10;h=sm*.12+grain*.5+hh*.035
  rgb=np.stack((np.full((n,n),.245),np.full((n,n),.168),np.full((n,n),.111)),axis=-1);rgb+=(sm*.018+grain*.10+hh*.012)[...,None];rough=.73
 elif name=='plaster':
  h=sm*.1+hh*.10;rgb=np.stack((np.full((n,n),.70),np.full((n,n),.67),np.full((n,n),.605)),axis=-1);rgb+=(sm*.027+hh*.015)[...,None];rough=.9
 elif name=='iron':
  h=sm*.12+hh*.13;rgb=np.stack((np.full((n,n),.16),np.full((n,n),.157),np.full((n,n),.153)),axis=-1);rgb+=(sm*.018+hh*.012)[...,None];rough=.62
 elif name=='water':
  ripple=np.sin(yy*.09+sm)*.10;h=sm*.09+ripple+hh*.025;rgb=np.stack((np.full((n,n),.125),np.full((n,n),.20),np.full((n,n),.194)),axis=-1);rgb+=(sm*.01+ripple*.13)[...,None];rough=.31
 else:
  h=sm*.13+hh*.12;rgb=np.stack((np.full((n,n),.18),np.full((n,n),.235),np.full((n,n),.118)),axis=-1);rgb+=(sm*.04+hh*.015)[...,None];rough=.95
 base=I.fromarray(np.uint8(np.clip(rgb,0,1)*255),'RGB');base.save(os.path.join(out,name+'_BaseColor.jpg'),quality=88,subsampling=0)
 # normal from a low-frequency height field (avoids artificial per-pixel extreme normals)
 hs=gaussian_filter(h,sigma=1.5);gy,gx=np.gradient(hs);dx=-gx*2.2;dy=-gy*2.2
 normal=np.stack((np.clip(.5+dx*.5,0,1),np.clip(.5+dy*.5,0,1),np.clip(.5+np.sqrt(np.maximum(0,1-dx*dx-dy*dy))*.5,0,1)),axis=-1)
 I.fromarray(np.uint8(normal*255),'RGB').save(os.path.join(out,name+'_Normal.png'))
 orm=np.zeros((n,n,3),np.uint8);orm[:,:,0]=np.uint8(220 if name not in ['water','foliage'] else 248);orm[:,:,1]=np.uint8(np.clip((rough+sm*.035),0,1)*255)
 orm[:,:,2]=205 if name=='iron' else 0
 I.fromarray(orm,'RGB').save(os.path.join(out,name+'_ORM.png'))
 return {'resolution':[n,n], 'basecolor':name+'_BaseColor.jpg','normal':name+'_Normal.png','orm':name+'_ORM.png'}

def main():
 arg=argparse.ArgumentParser();arg.add_argument('--out',default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))));arg.add_argument('--textures-only',action='store_true');arg.add_argument('--resolution',type=int,default=2048)
 opt=arg.parse_args();os.makedirs(opt.out,exist_ok=True);tex_dir=os.path.join(opt.out,'textures');os.makedirs(tex_dir,exist_ok=True)
 sets={}
 for name in B.bags:
  size=4096 if name in ['stone','roof','wood'] and opt.resolution>=2048 else opt.resolution
  if name in ['iron','foliage','water']:size=min(1024,opt.resolution)
  t0=time.time();sets[name]=tex_generate(name,size,tex_dir);print('texture',name,size,'%.1fs'%(time.time()-t0),flush=True)
 if opt.textures_only:return
 sc=trimesh.Scene();meshinfo={}
 for name,mesh in B.merge():
  a=sets[name];normal=Image.open(os.path.join(tex_dir,a['normal']));base=Image.open(os.path.join(tex_dir,a['basecolor']));orm=Image.open(os.path.join(tex_dir,a['orm']))
  # glTF maps AO from R of ORM and metal/rough from G/B; same texture bound to both slots.
  mat=PBRMaterial(name='M_'+name,baseColorTexture=base,normalTexture=normal,metallicRoughnessTexture=orm,occlusionTexture=orm,metallicFactor=(.8 if name=='iron' else 0.0),roughnessFactor=1.0,doubleSided=(name in ['roof','foliage']))
  mesh.visual=TextureVisuals(uv=mesh.visual.uv,material=mat)
  sc.add_geometry(mesh,node_name='WM_'+name,geom_name='WM_'+name)
  meshinfo[name]={'vertices':int(len(mesh.vertices)),'triangles':int(len(mesh.faces))}
  print('mesh',name,meshinfo[name],flush=True)
 target=os.path.join(opt.out,'Wenmingmen_Interpretive.glb');b=sc.export(file_type='glb');open(target,'wb').write(b)
 t=sum(v['triangles'] for v in meshinfo.values());info={'asset':'Wenmingmen / 文明门','status':'interpretive 3D reconstruction from supplied concept art (NOT archival survey or historically validated geometry)','units':'meters','source_up_axis':'Z','output_format':'glTF 2.0 binary GLB, textures embedded; 1 GLB with per-material meshes','approx_footprint_m':[65,35],'overall_height_m':18.2,'requested_triangles':100_000_000,'actual_triangles':t,'mesh_summary':meshinfo,'texture_sets':sets,'notes':['Roof tile shapes, dougong bracket silhouettes, masonry courses, crenellations, true open gate passage, side wall and canal bridge are modeled.','The source concept sheet contains imaginative elements; geometry is not a measured historical reconstruction.','Chinese inscription is not legibly modeled: add verified text using a separate plaque mesh / decal inside Unreal.','Collision, Nanite conversion, lightmap UVs, authored LODs and Unreal material instances must be configured in UE; GLB is an interchange asset, not a .uasset.','Texture tiles are procedural and do not contain scanned historical building surfaces.']}
 open(os.path.join(opt.out,'asset_manifest.json'),'w',encoding='utf-8').write(json.dumps(info,ensure_ascii=False,indent=2))
 print('DONE triangles',t,'glb_bytes',os.stat(target).st_size,flush=True)
if __name__=='__main__':main()
