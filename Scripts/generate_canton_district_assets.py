"""Deterministic interpretive district modules. No surveyed/period asset claim.
Uses the existing terrain tool environment (numpy/Pillow); emits standard GLB + PNG.
"""
import json, math, struct, hashlib, random
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'ContentSource/CantonDistrict'; OUT.mkdir(parents=True,exist_ok=True)
class Mesh:
 def __init__(self):self.v=[];self.n=[];self.uv=[];self.f=[]
 def tri(self,a,b,c):
  a,b,c=map(np.array,(a,b,c));n=np.cross(b-a,c-a);n=n/max(np.linalg.norm(n),1e-8)
  for v in (a,b,c):self.f.append(len(self.v));self.v.append(v/100);self.n.append(n);self.uv.append([v[0]/100,v[1]/100])
 def quad(self,a,b,c,d):self.tri(a,b,c);self.tri(a,c,d)
 def box(self,x0,x1,y0,y1,z0,z1):
  p=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
  for f in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)):self.quad(*(p[i] for i in f))
 def slab(self,x0,x1,y0,y1,z0,z1,b):
  self.box(x0,x1,y0,y1,z0,z1-b)
  low=[(x0,y0,z1-b),(x1,y0,z1-b),(x1,y1,z1-b),(x0,y1,z1-b)]
  top=[(x0+b,y0+b,z1),(x1-b,y0+b,z1),(x1-b,y1-b,z1),(x0+b,y1-b,z1)]
  self.quad(*top)
  for i in range(4):self.quad(low[i],low[(i+1)%4],top[(i+1)%4],top[i])
 def save(self,name):
  # Export glTF Y-up, baking the axis swap into vertices/normals. The UE mesh
  # must be Z-up at identity so instance non-uniform scales remain correct.
  self.v=[(p[0],p[2],p[1]) for p in self.v]
  self.n=[(p[0],p[2],p[1]) for p in self.n]
  self.f=[v for i in range(0,len(self.f),3) for v in (self.f[i],self.f[i+2],self.f[i+1])]
  data=bytearray();views=[];acc=[]
  for values,dtype,typ,component in ((self.v,'<f4','VEC3',5126),(self.n,'<f4','VEC3',5126),(self.uv,'<f4','VEC2',5126),(self.f,'<u4','SCALAR',5125)):
   arr=np.array(values,dtype=dtype);raw=arr.tobytes();offset=len(data);data.extend(raw)
   views.append({'buffer':0,'byteOffset':offset,'byteLength':len(raw)})
   a={'bufferView':len(views)-1,'componentType':component,'count':len(values),'type':typ}
   if len(acc)==0:a.update(min=arr.min(axis=0).tolist(),max=arr.max(axis=0).tolist())
   acc.append(a)
  doc={'asset':{'version':'2.0','generator':'AuraProj deterministic interpretive modules'},'buffers':[{'byteLength':len(data)}],'bufferViews':views,'accessors':acc,'meshes':[{'name':name,'primitives':[{'attributes':{'POSITION':0,'NORMAL':1,'TEXCOORD_0':2},'indices':3}]}],'nodes':[{'mesh':0,'name':name}],'scenes':[{'nodes':[0]}],'scene':0}
  j=json.dumps(doc,separators=(',',':')).encode();j+=b' '*((-len(j))%4)
  (OUT/(name+'.glb')).write_bytes(struct.pack('<III',0x46546c67,2,12+8+len(j)+8+len(data))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(data),0x004e4942)+data)
  return len(self.f)//3
counts={}
m=Mesh();m.box(-50,50,-50,50,-50,30)
for x in range(6):
 for y in range(4):m.slab(-50+x*100/6+.18,-50+(x+1)*100/6-.18,-50+y*25+.4,-50+(y+1)*25-.4,30,50,.4)
counts['SM_Canton_Paving']=m.save('SM_Canton_Paving')
m=Mesh();rng=random.Random(1890)
for i in range(22):
 angle=rng.uniform(0,2*math.pi);r=rng.uniform(0,15);x,y=r*math.cos(angle),r*math.sin(angle);h=rng.uniform(14,38);w=rng.uniform(.7,1.5)
 points=[]
 for j in range(5):
  t=j/4;lean=t*t*12;points.append((x+math.cos(angle)*lean,y+math.sin(angle)*lean,h*t))
 for j in range(4):
  a,b=points[j:j+2];wa=w*(1-j/4);wb=w*(1-(j+1)/4)
  m.quad((a[0]-wa*math.sin(angle),a[1]+wa*math.cos(angle),a[2]),(a[0]+wa*math.sin(angle),a[1]-wa*math.cos(angle),a[2]),(b[0]+wb*math.sin(angle),b[1]-wb*math.cos(angle),b[2]),(b[0]-wb*math.sin(angle),b[1]+wb*math.cos(angle),b[2]))
counts['SM_Canton_GrassTuft']=m.save('SM_Canton_GrassTuft')
m=Mesh()
for i in range(12):
 a=2*math.pi*i/12;b=2*math.pi*(i+1)/12
 m.tri((0,0,1.2),(5*math.cos(a),2*math.sin(a),.2),(5*math.cos(b),2*math.sin(b),.2))
counts['SM_Canton_Leaf']=m.save('SM_Canton_Leaf')
m=Mesh()
for i in range(32):
 a=2*math.pi*i/32;b=2*math.pi*(i+1)/32
 r=lambda t:1+.12*math.sin(3*t)+.08*math.cos(7*t)
 m.tri((0,0,0),(45*r(a)*math.cos(a),60*r(a)*math.sin(a),0),(45*r(b)*math.cos(b),60*r(b)*math.sin(b),0))
counts['SM_Canton_DampPatch']=m.save('SM_Canton_DampPatch')
# Period-neutral mineral/soil microstructure, not an image edit or historic colour sample.
n=512;rng=np.random.default_rng(1890)
for family,color in [('Stone',(105,103,96)),('Earth',(85,70,50)),('Pebble',(100,89,72))]:
 fine=rng.normal(0,1,(n,n));coarse=np.asarray(Image.fromarray(np.uint8(rng.uniform(0,255,(32,32)))).resize((n,n),Image.Resampling.BICUBIC),dtype=float)/255-.5
 height=.5+.05*fine+.28*coarse
 if family=='Pebble':
  yy,xx=np.mgrid[:n,:n]
  for _ in range(210):
   x,y=rng.uniform(0,n,2);rad=rng.uniform(3,9);d=((xx-x)/rad)**2+((yy-y)/rad)**2;height+=np.maximum(0,1-d)*.32
 rgb=np.clip(np.array(color)[None,None,:]+(height-.5)[:,:,None]*48,0,255).astype('uint8')
 gy,gx=np.gradient(height);norm=np.dstack((-gx*1.2,-gy*1.2,np.ones_like(gx)));norm/=np.linalg.norm(norm,axis=2)[:,:,None]
 Image.fromarray(rgb).save(OUT/(family+'_BaseColor.png'))
 Image.fromarray(np.uint8((norm*.5+.5)*255)).save(OUT/(family+'_Normal.png'))
 rough=np.uint8(np.clip(.85+.08*coarse,0,1)*255);Image.fromarray(rough).save(OUT/(family+'_Roughness.png'))
manifest={'historical_confidence':'D','source':'project-authored engineering interpretation; no period dimensions/species/colour claim','licence':'project-authored','triangles':counts,'assets':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(OUT.iterdir()) if p.suffix in ('.glb','.png')}}
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(counts)
