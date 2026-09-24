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
  # Keep texel density in world units. A 30 cm stone must not carry the same
  # number of painted joints as a four metre wall backing face.
  # Offset each block into the seamless material so adjacent blocks do not
  # repeat the same chips and staining at identical coordinates.
  uoff=(x*.373+y*.179+z*.121)%1;voff=(z*.411+x*.137+y*.113)%1
  uv=np.vstack([[[0,0],[np.linalg.norm(vv[4*i+1]-vv[4*i])/scale,0],
                 [np.linalg.norm(vv[4*i+1]-vv[4*i])/scale,np.linalg.norm(vv[4*i+3]-vv[4*i])/scale],
                 [0,np.linalg.norm(vv[4*i+3]-vv[4*i])/scale]] for i in range(6)]).astype(np.float32)
  uv+=np.array((uoff,voff),dtype=np.float32)
  f=np.arange(24,dtype=np.uint32).reshape(6,4)
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
  uv=np.vstack([[[0,0],[np.linalg.norm(vv[4*i+1]-vv[4*i])/scale,0],
                 [np.linalg.norm(vv[4*i+1]-vv[4*i])/scale,np.linalg.norm(vv[4*i+3]-vv[4*i])/scale],
                 [0,np.linalg.norm(vv[4*i+3]-vv[4*i])/scale]] for i in range(6)]).astype(np.float32)
  f=np.arange(24,dtype=np.uint32).reshape(6,4)
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
 def arch_strip(self,m,xc,y0,y1,zc,r,delta=0.32,N=36,joint=.003):
  # Real radial voussoirs at both portals, with a narrow visible mortar reveal.
  ts=np.linspace(0,P,N+1)
  for i in range(N):
   t0,t1=ts[i:i+2];a0=t0+joint;a1=t1-joint
   outer0=np.array([xc-r*math.cos(a0),zc+r*math.sin(a0)]);outer1=np.array([xc-r*math.cos(a1),zc+r*math.sin(a1)]);
   ri=r-delta;inner0=np.array([xc-ri*math.cos(a0),zc+ri*math.sin(a0)]);inner1=np.array([xc-ri*math.cos(a1),zc+ri*math.sin(a1)])
   front=([outer0[0],y0,outer0[1]],[inner0[0],y0,inner0[1]],[inner1[0],y0,inner1[1]],[outer1[0],y0,outer1[1]])
   back=([outer1[0],y1,outer1[1]],[inner1[0],y1,inner1[1]],[inner0[0],y1,inner0[1]],[outer0[0],y1,outer0[1]])
   self.quad(m,*front,scale=.6);self.quad(m,*back,scale=.6)
   # intrados radial line, from near end to far end
   self.quad(m,[inner0[0],y0,inner0[1]],[inner0[0],y1,inner0[1]],[inner1[0],y1,inner1[1]],[inner1[0],y0,inner1[1]],scale=.6)
 def prism_xz(self,m,outline,y0,y1,scale=.65):
  # A watertight, vertically grained panel with a curved top. The outline is
  # ordered around an x/z profile and is convex for the gate's narrow planks.
  points=np.asarray(outline,dtype=np.float32);n=len(points)
  for yy,reverse in ((y0,False),(y1,True)):
   ids=list(range(n))
   if reverse:ids.reverse()
   verts=[[points[i,0],yy,points[i,1]] for i in ids]
   uv=[[points[i,0]/scale,points[i,1]/scale] for i in ids]
   self.add(m,verts,[[0,i,i+1] for i in range(1,n-1)],uv)
  for i in range(n):
   j=(i+1)%n
   self.quad(m,[points[i,0],y0,points[i,1]],[points[j,0],y0,points[j,1]],
             [points[j,0],y1,points[j,1]],[points[i,0],y1,points[i,1]],scale)
 def merge(self):
  meshes=[]
  for k,b in self.bags.items():
   if not b['n']:continue
   v=np.vstack(b['v']);f=np.vstack(b['f']);uv=np.vstack(b['uv'])
   mesh=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv));mesh.metadata['material']=k;mesh.metadata['name']='WM_'+k;meshes.append((k,mesh));self.counts[k]=len(f)
  return meshes

B=Builder()
def masonry(xmin,xmax,ymin,ymax,zmin,zmax,m='stone',seg=.74,course=.31,
            individual=True,joint=.012,relief=.025):
 B.box(m,((xmin+xmax)/2,(ymin+ymax)/2,(zmin+zmax)/2),(xmax-xmin,ymax-ymin,zmax-zmin),1.2)
 if not individual:return
 # Thin, individually coursed ashlar faces over the wall core. Recessed 24 mm
 # vertical/horizontal joints cast the mortar shadow; albedo has no fake grid.
 for face in [ymin-relief,ymax+relief]:
  n=int(math.ceil((zmax-zmin)/course))
  for j in range(n):
   bottom=zmin+j*course+joint;top=min(zmax-joint,zmin+(j+1)*course-joint)
   if top<=bottom:continue
   zz=(bottom+top)/2;start=xmin-seg*(.45 if j%2 else .1);xx=start
   while xx<xmax:
    ww=seg*(.74+.50*random.random());l=max(xmin,xx+joint);r=min(xmax,xx+ww-joint)
    if r-l>.10:
     B.box(m,((r+l)/2,face,zz),(r-l,2*relief+.010,top-bottom),.8)
    xx+=ww

def gate_facing_courses(front=-2.075,back=2.075):
 # Staggered stone blocks follow the semicircular portal instead of crossing
 # its hollow. The arch ring remains clear and its radial voussoirs readable.
 course=.39;zmax=9.7
 for j in range(math.ceil(zmax/course)):
  z0=j*course+.014;z1=min(zmax-.014,(j+1)*course-.014)
  if z1<=z0:continue
  gap=0.0
  if z0<spring:gap=3.045
  elif z0<spring+3.045:gap=math.sqrt(max(0,3.045**2-(z0-spring)**2))
  spans=[(-9.7,-gap),(gap,9.7)] if gap>.12 else [(-9.7,9.7)]
  for left,right in spans:
   xx=left-1.02*(.48 if j%2 else .08)
   while xx<right:
    ww=1.02*(.73+.50*random.random())
    x0=max(left,xx+.014);x1=min(right,xx+ww-.014)
    if x1-x0>.12:
     for yy in (front,back):
      B.box('limestone',((x0+x1)/2,yy,(z0+z1)/2),(x1-x0,.075,z1-z0),.93)
    xx+=ww
  # A small tapered end stone follows the changing arch radius through each
  # course, removing the stair-step void left by rectangular face blocks.
  if gap>.12 and z0>=spring:
   top_gap=math.sqrt(max(0,3.045**2-(z1-spring)**2)) if z1<spring+3.045 else 0.0
   if gap-top_gap>.015:
    for side in (-1,1):
     outline=([(gap,z0),(gap,z1),(top_gap,z1)] if side>0 else
              [(-gap,z0),(-top_gap,z1),(-gap,z1)])
     B.prism_xz('limestone',outline,front-.0375,front+.0375,.93)
     B.prism_xz('limestone',outline,back-.0375,back+.0375,.93)

def cornice(x0,x1,y0,y1,z,m='stone'):
 for dz,s in [(0,.12),(.13,.18),(.25,.1)]:B.box(m,((x0+x1)/2,(y0+y1)/2,z+dz),(x1-x0+s,y1-y0+s,.1),.7)

# MAIN WALL: pale, large coursed stone at the gate and smaller gray brick in
# the flanking curtain walls, as in the reference's front and detail views.
for xa,xb in [(-9.7,-2.9),(2.9,9.7)]:masonry(xa,xb,-2.05,2.05,0,9.7,m='limestone',individual=False)
# Top spandrel above the curved arch: narrow vertical slices create an actual empty gate opening
r=3.0;spring=3.30;N=60
for i in range(N):
 x0=-r+2*r*i/N;x1=-r+2*r*(i+1)/N;xc=(x0+x1)/2
 zb=spring+math.sqrt(max(0,r*r-xc*xc))-.025
 if zb<9.7:B.box('limestone',(xc,0,(zb+9.7)/2),(x1-x0+.003,4.1,9.7-zb),.85)
# A continuous recessed annulus closes the bright slits between separated
# voussoirs; the shallow foreground blocks still cast the masonry joints.
# The reference separates the portal ring from the pale gate face. Use the
# darker wall stone here so the radial voussoirs remain legible in UE lighting;
# the wider reveals keep each wedge readable without opening a bright slit.
B.arch_strip('stone',0,-2.14,2.14,spring,3.12,.56,42,joint=.015)
B.arch_strip('stone',0,-2.17,2.17,spring,3.16,.58,21,joint=.018)
for side in [-1,1]:
 B.box('limestone',(side*2.82,0,1.65),(.43,4.23,3.3),.6)
 # Tall vertical couplet stones flanking the gate, not free-standing tiles.
 B.box('limestone',(side*4.22,-2.165,4.55),(.80,.19,3.35),.65)
 B.box('limestone',(side*4.22,2.165,4.55),(.80,.19,3.35),.65)
gate_facing_courses()
# The gatehouse widens toward the ground. Tapered corner cheeks establish a
# credible battered mass at the junction to the lower curtain walls.
for side in (-1,1):
 front=[(side*9.70,-2.105,.03),(side*10.55,-2.105,.03),
        (side*9.86,-2.105,9.68),(side*9.70,-2.105,9.68)]
 back=[(x,2.105,z) for x,_,z in front]
 if side<0:front.reverse()
 else:back.reverse()
 B.quad('limestone',*front,scale=.85)
 B.quad('limestone',*back,scale=.85)
 outside=((side*10.55,-2.105,.03),(side*10.55,2.105,.03),
          (side*9.86,2.105,9.68),(side*9.86,-2.105,9.68))
 B.quad('limestone',*(outside if side>0 else tuple(reversed(outside))),scale=.85)
# Recessed double gate: each timber plank terminates against the inner arch.
# A jamb and arched frame bind the leaves to the stone rather than floating in
# the far side of the tunnel. Clearance is hidden by the 110 mm timber frame.
door_radius=2.49;door_front=-1.91;door_back=-1.74
def door_height(x):return spring+math.sqrt(max(0,door_radius*door_radius-x*x))
for sign in [-1,1]:
 for i in range(7):
  a=.008+i*(door_radius-.016)/7;b=.008+(i+1)*(door_radius-.016)/7
  a+=.004;b-=.004
  sample=np.linspace(a,b,5)
  top=[(sign*x,door_height(x)-.014) for x in sample]
  if sign>0:outline=[(a,.095),(b,.095)]+list(reversed(top))
  else:outline=[(-b,.095),(-a,.095)]+top
  B.prism_xz('wood',outline,door_front,door_back,.58)
 for z in (.90,2.27):
  B.beam('iron',(sign*.20,door_front-.065,z),(sign*2.30,door_front-.065,z),.075,.045)
  B.box('iron',(sign*2.29,door_front-.085,z),(.18,.08,.17),.18)
 for x in (.34,.72,1.10,1.48,1.86,2.24):
  for z in (.43,1.38,2.67):
   B.box('iron',(sign*x,door_front-.048,z),(.045,.035,.045),.15)
 # Cross rails organize the planks into heavy timber leaves. Upper members
 # shorten with the semicircle so they remain entirely within the arch.
 for z,xend in ((.55,2.32),(1.76,2.32),(3.02,2.32),(4.19,2.29),(5.05,1.60)):
  B.beam('wood',(sign*.10,door_front-.070,z),(sign*xend,door_front-.070,z),.12,.16)
 B.beam('wood',(sign*2.35,door_front-.077,.15),(sign*2.35,door_front-.077,3.88),.13,.16)
 B.beam('wood',(sign*2.54,-1.90,.08),(sign*2.54,-1.90,spring+.04),.11,.14)
B.beam('wood',(0,-1.96,.09),(0,-1.96,spring+door_radius-.01),.065,.10)
for i in range(32):
 t0=P*i/32;t1=P*(i+1)/32
 B.beam('wood',(2.54*math.cos(t0),-1.91,spring+2.54*math.sin(t0)),
        (2.54*math.cos(t1),-1.91,spring+2.54*math.sin(t1)),.11,.13)
B.box('limestone',(0,-1.90,.043),(5.20,.45,.086),.8)
# carved stone plaque front and lintel
B.box('limestone',(0,-2.22,7.52),(3.0,.20,1.02),.7)
B.box('limestone',(0,-2.36,7.52),(2.58,.05,.68),.45)
# plaque carving geometry indicates strokes but characters supplied separately as editable authoring note
# wall crest cornice, bands
cornice(-9.7,9.7,-2.1,2.1,9.72)
for xa,xb in [(-28.5,-9.7),(9.7,28.5)]:
 masonry(xa,xb,-1.62,1.62,0,7.55,seg=.68,course=.29,joint=.025,relief=.060)
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
# The reference's upper story is a timber room with framed windows, not an
# exposed skeleton. The four sides are now one CONTINUOUS closed wall each: the
# earlier per-bay infill panels left visible slots between bays, open corners,
# and an unclosed band above the balcony rail, so the pavilion could be seen
# straight through as a hollow shell. A single solid wall per side closes every
# slot and void, running from the pavilion deck to the clerestory sill. The
# shutter, sash, and lattice members stay proud of it as surface articulation,
# so each side still reads as joined timber windows rather than blank boarding.
room_hx=8.125;room_hy=2.60
wall_bottom=10.40;wall_top=14.66;wall_t=.16
wall_mid=(wall_bottom+wall_top)/2
for y in [-room_hy,room_hy]:
 B.box('wood',(0,y,wall_mid),(2*room_hx,wall_t,wall_top-wall_bottom),.65)
for x in [-8.02,8.02]:
 # The end walls run past the front/rear wall centrelines so the corners are
 # closed by overlap instead of meeting on a hairline seam.
 B.box('wood',(x,0,wall_mid),(wall_t,2*room_hy+.16,wall_top-wall_bottom),.65)
for y in [-2.60,2.60]:
 for panel in range(6):
  a=-7.65+panel*2.55;b=a+2.27;mid=(a+b)/2
  # paired shutter leaves and a central meeting stile
  face_y=y-.11 if y<0 else y+.11
  B.beam('wood',(mid-.015,face_y,12.18),(mid-.015,face_y,14.62),.075,.075)
  for xx in np.linspace(a+.30,b-.30,4):
   B.beam('wood',(xx,face_y,12.24),(xx,face_y,14.56),.040,.045)
  # Narrow top and bottom rails make the panel read as a joined window sash.
  B.beam('wood',(a,face_y,12.18),(b,face_y,12.18),.075,.075)
  B.beam('wood',(a,face_y,14.62),(b,face_y,14.62),.075,.075)
for y in [-2.72,2.72]:
 for panel in range(6):
  a=-7.65+panel*2.55;b=a+2.27
  for xx in [a,b]:B.beam('wood',(xx,y,12.1),(xx,y,14.7),.11)
  for zz in [12.25,14.6]:B.beam('wood',(a,y,zz),(b,y,zz),.12)
  for xx in np.linspace(a+.28,b-.28,5):B.beam('wood',(xx,y,12.4),(xx,y,14.48),.033)
  for zz in np.linspace(12.54,14.36,5):B.beam('wood',(a+.12,y,zz),(b-.12,y,zz),.030)
# The two end walls carry the same continuous wall plus their own sash framing.
for x in [-8.02,8.02]:
 for panel in range(2):
  a=-2.38+panel*2.38;b=a+2.15
  for yy in [a,b]:B.beam('wood',(x,yy,12.1),(x,yy,14.7),.11)
  for yy in np.linspace(a+.30,b-.30,4):B.beam('wood',(x,yy,12.24),(x,yy,14.56),.040,.045)
  for zz in [12.18,14.62]:B.beam('wood',(x,a,zz),(x,b,zz),.075,.075)
# The main roof sits well above the lower canopy at the front and rear eaves.
# Close that clerestory band so the roof is carried by a continuous timber
# room rather than isolated beams with a black void behind them. The openings
# below remain the balcony; this band is the enclosed upper wall seen in the
# reference's roof/eave and door-window details.
clerestory_bottom=14.66;clerestory_top=16.30;clerestory_z=(clerestory_bottom+clerestory_top)/2
for y in [-2.60,2.60]:
 B.box('wood',(0,y,clerestory_z),(16.25,.18,clerestory_top-clerestory_bottom),.65)
 for x in np.linspace(-7.72,7.72,7):
  B.beam('wood',(x,y-.11 if y<0 else y+.11,clerestory_bottom-.02),(x,y-.11 if y<0 else y+.11,clerestory_top+.02),.12,.14)
 for z in [clerestory_bottom,clerestory_top]:
  B.beam('wood',(-8.08,y,z),(8.08,y,z),.14,.16)
for x in [-8.02,8.02]:
 B.box('wood',(x,0,clerestory_z),(.18,5.20,clerestory_top-clerestory_bottom),.65)
 for y in np.linspace(-2.46,2.46,5):
  B.beam('wood',(x,y,clerestory_bottom-.02),(x,y,clerestory_top+.02),.11,.13)
 for z in [clerestory_bottom,clerestory_top]:
  B.beam('wood',(x,-2.60,z),(x,2.60,z),.14,.16)
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
 def surface_z(x,t):
  edge=max(0,(abs(x)-ridgehalf)/(halfwidth-ridgehalf))
  return (z-2.38*(t**1.08)+.44*(t**3)
          -1.25*(edge**1.55)*((1-t)**2)+.84*(edge**2)*(t**3))
 for s in [-1,1]:
  def facept(x,t):return (x,s*(.12+(halfdepth-.12)*t),surface_z(x,t))
  xpositions=np.linspace(-halfwidth,halfwidth,math.ceil(2*halfwidth/step)+1)
  nrow=rows
  for j in range(len(xpositions)-1):
   x0=xpositions[j];x1=xpositions[j+1];xc=(x0+x1)/2
   # curved hipped end corners: eave bends higher at four corners
   for k in range(nrow):
    t0=k/nrow;t1=(k+1)/nrow
    v0,v1,v2,v3=facept(x0,t0),facept(x1,t0),facept(x1,t1),facept(x0,t1)
    if s<0:B.quad('roof',v3,v2,v1,v0,scale=.6)
    else:B.quad('roof',v0,v1,v2,v3,scale=.6)
    # Each overlapping clay pan has a shallow U section rather than a flat
    # roof sheet; a raised half-round cap covers every longitudinal joint.
    vv=[];uv=[];ff=[]
    for t in (max(0,t0-.003),min(1,t1+.009)):
     for q in range(7):
      u=q/6;x=x0+(x1-x0)*u;p=facept(x,t)
      vv.append((p[0],p[1],p[2]+.018+.032*(2*u-1)**2))
      uv.append((x/.42,t*halfdepth/.42))
    for q in range(6):
     tris=((q,q+1,q+8),(q,q+8,q+7))
     ff.extend(tuple(reversed(tri)) if s<0 else tri for tri in tris)
    B.add('roof',vv,ff,uv)
    # The exposed downhill rim gives the tile course a real, narrow shadow.
    rim0=facept(x0,t1);rim1=facept(x1,t1)
    rim=((rim0[0],rim0[1],rim0[2]+.047),
         (rim1[0],rim1[1],rim1[2]+.047),
         (rim1[0],rim1[1],rim1[2]+.020),
         (rim0[0],rim0[1],rim0[2]+.020))
    B.quad('roof',*(tuple(reversed(rim)) if s<0 else rim),scale=.42)
    vv=[];uv=[];ff=[]
    for t in (max(0,t0-.004),min(1,t1+.011)):
     p=facept(x0,t)
     for q in range(7):
      angle=P*q/6;dx=.055*math.cos(angle);rise=.058*math.sin(angle)
      vv.append((p[0]+dx,p[1],p[2]+.045+rise))
      uv.append((q/6,t*halfdepth/.42))
    for q in range(6):
     tris=((q,q+1,q+8),(q,q+8,q+7))
     ff.extend(tuple(reversed(tri)) if s<0 else tri for tri in tris)
    B.add('roof',vv,ff,uv)
  # tall curved outward ridge/corners
  for x in [-halfwidth,halfwidth]:
   pts=[]
   for t in np.linspace(0,1,23):
    y=s*(halfdepth*t);zz=surface_z(x,t)
    pts.append((x,y,zz+.08))
   for a,b in zip(pts[:-1],pts[1:]):B.tube('roof',a,b,.092,8)
  # Four sloping hip ribs join the finite ridge to the swept corners.
  for sign in (-1,1):
   hip=[]
   for t in np.linspace(0,1,28):
    x=sign*(ridgehalf+(halfwidth-ridgehalf)*t)
    hip.append((x,s*halfdepth*t,surface_z(x,t)+.105))
   for a,b in zip(hip[:-1],hip[1:]):B.tube('roof',a,b,.095,9)
  # curved edge corner eave flashing
  for i in range(len(xpositions)-1):
   x0=xpositions[i];x1=xpositions[i+1]
   t=1
   def eaveheight(x):return surface_z(x,1)
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
def bridge_deck(y):
 t=max(0,min(1,(y+23.5)/20.45))
 return .15+1.50*(math.sin(P*t)**1.3)
bridge_spans=((-18.85,3.15,3.10),(-10.65,4.40,3.55))
def bridge_intrados(y):
 for center,rise_half,rise in bridge_spans:
  d=(y-center)/rise_half
  if abs(d)<1:return -2.64+rise*math.sqrt(max(0,1-d*d))
 return -3.0
# Two real openings and a shallow arched deck match the bridge silhouette in
# the reference. The older flat deck made its vault pierce the walking surface.
for xface in [-4.8,4.8]:
 for jy in range(72):
  y0=-23.5+jy*.30;y1=y0+.30;ym=(y0+y1)/2
  lower=bridge_intrados(ym);upper=bridge_deck(ym)-.14
  if lower<upper:B.box('limestone',(xface,ym,(lower+upper)/2),(.36,.28,upper-lower),.75)
 for center,radius,rise in bridge_spans:
  for i in range(40):
   th0=P*i/40;th1=P*(i+1)/40
   y0=center-radius*math.cos(th0);y1=center-radius*math.cos(th1)
   z0=-2.64+rise*math.sin(th0);z1=-2.64+rise*math.sin(th1)
   B.beam('limestone',(xface,y0,z0),(xface,y1,z1),.15,.18)
for y0 in np.linspace(-23.5,-3.05,42)[:-1]:
 y1=y0+20.45/41
 B.beam('limestone',(0,y0,bridge_deck(y0)),(0,y1,bridge_deck(y1)),9.5,.24,1.0)
# parapet rails and posts
for xx in [-4.52,4.52]:
 for zz in [.62,1.12]:
  for y0 in np.linspace(-23.5,-3.05,42)[:-1]:
   y1=y0+20.45/41
   B.beam('limestone',(xx,y0,bridge_deck(y0)+zz),
          (xx,y1,bridge_deck(y1)+zz),.14,.15)
 for yy in np.arange(-23.25,-3.05,1.42):
  B.box('limestone',(xx,yy,bridge_deck(yy)+.70),(.39,.39,1.3),.6)
  B.box('limestone',(xx,yy,bridge_deck(yy)+1.43),(.5,.5,.17),.65)
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
# Climbing vines rooted at the wall base, then branching into small leaf
# clusters. Earlier passes started each stem 1.65 m up the face, so the vine
# appeared to float with no root; every vine now begins at the ground in a
# visible root cluster and climbs from there. Each leaf cluster remains
# physically attached to its stem.
for cx,cy in [(-7.2,-2.18),(7.5,-2.18),(-12,-1.71),(13,-1.71)]:
 yy=cy-.09;points=[]
 # Root cluster: a thickened basal stem plus short lateral roots spreading
 # along the wall foot, so the planting reads as rooted rather than floating.
 B.tube('foliage',(cx,yy,.02),(cx,yy,.36),r=.026,seg=6,scale=.35)
 for k in range(6):
  ang=-1.15+.46*k
  B.tube('foliage',(cx,yy+.004,.10),(cx+math.sin(ang)*.30,yy+.004,.03+.012*abs(math.cos(ang))),
         r=.014,seg=5,scale=.35)
 for j in range(26):
  z=.06+j*.235
  x=cx+.19*math.sin(j*.47)+float(rng.normal(0,.025))
  points.append((x,yy,z))
  if j:
   B.tube('foliage',points[-2],points[-1],r=.012,seg=5,scale=.35)
  if j>1 and j%2==0:
   direction=-1 if (j//2)%2 else 1
   reach=.23+float(rng.uniform(.02,.28))
   tip=(x+direction*reach,yy-.02,z+.12)
   B.tube('foliage',points[-1],tip,r=.009,seg=5,scale=.35)
   for t in (.48,.73,1.0):
    px=x+direction*reach*t;pz=z+.12*t
    leaf_y=yy-.045-float(rng.uniform(0,.012))
    leaf_w=float(rng.uniform(.085,.14));leaf_h=float(rng.uniform(.13,.22))
    # Small diamond leaves retain visible area at the building preview scale.
    B.quad('foliage',(px,leaf_y,pz-leaf_h*.52),
           (px+direction*leaf_w,leaf_y,pz-leaf_h*.06),
           (px+direction*leaf_w*.42,leaf_y,pz+leaf_h*.52),
           (px-direction*leaf_w*.35,leaf_y,pz+leaf_h*.05),scale=.18)

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
 arg=argparse.ArgumentParser();arg.add_argument('--out',default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))));arg.add_argument('--textures-only',action='store_true');arg.add_argument('--resolution',type=int,default=2048);arg.add_argument('--reuse-textures',action='store_true',help='Embed the existing edited PBR maps without regenerating them')
 opt=arg.parse_args();os.makedirs(opt.out,exist_ok=True);tex_dir=os.path.join(opt.out,'textures');os.makedirs(tex_dir,exist_ok=True)
 sets={}
 for name in B.bags:
  if opt.reuse_textures:
   base=next((name+'_BaseColor'+ext for ext in ('.png','.jpg') if os.path.exists(os.path.join(tex_dir,name+'_BaseColor'+ext))),None)
   normal=name+'_Normal.png';orm=name+'_ORM.png'
   if not base or not all(os.path.exists(os.path.join(tex_dir,f)) for f in (normal,orm)):raise FileNotFoundError('Missing PBR maps for '+name)
   with Image.open(os.path.join(tex_dir,base)) as im:size=im.size
   sets[name]={'resolution':list(size),'basecolor':base,'normal':normal,'orm':orm}
   print('texture reused',name,size,flush=True)
  else:
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
 t=sum(v['triangles'] for v in meshinfo.values())
 info={
  'asset':'Wenmingmen / 文明门',
  'status':'interpretive 3D reconstruction from supplied concept art (NOT archival survey or historically validated geometry)',
  'units':'meters','source_up_axis':'Z',
  'output_format':'glTF 2.0 binary GLB, textures embedded; 1 GLB with per-material meshes',
  'approx_footprint_m':[65,35],'overall_height_m':18.2,
  'requested_triangles':100_000_000,'actual_triangles':t,
  'mesh_summary':meshinfo,'texture_sets':sets,
  'notes':[
   'Modeled features include coursed pale gate ashlar, darker relieved side-wall brick, radial arch voussoirs, crenellations, a recessed double timber door fitted to the arch, hipped roofs with overlapping clay pans and caps, climbing vines, and a two-arch raised bridge.',
   'Four reference-guided generated PBR sets for stone, limestone, roof clay, and aged wood are embedded in the GLB. These are generated interpretations, not historical scans.',
   'The supplied concept board is artistic reference. Dimensions and architectural details are not verified by a measured historical survey.',
   'The main plaque and side couplets are added by create_high_detail.py as textured overlays. Verify the wording against archival evidence before historical presentation.',
   'Native UE 5.5 reimport and fresh Blueprint reload are validated separately; Metal SM5 uses an explicit full-resolution Nanite fallback.'
  ]
 }
 open(os.path.join(opt.out,'asset_manifest.json'),'w',encoding='utf-8').write(json.dumps(info,ensure_ascii=False,indent=2))
 print('DONE triangles',t,'glb_bytes',os.stat(target).st_size,flush=True)
if __name__=='__main__':main()
