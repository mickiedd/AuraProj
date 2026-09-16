"""Correct only the baked Production V3 door leaves and their studs; preserve source."""
import json, struct, math, hashlib
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/GateDoorFix';OUT.mkdir(exist_ok=True)
src=ROOT/'Saved/RawModelImport/V3/XiaobeiMen_UE5_Production_Package/Meshes/SM_XiaobeiMen_Gate_Nanite.glb'
b=src.read_bytes(); n=struct.unpack_from('<I',b,12)[0];d=json.loads(b[20:20+n]);blob=bytearray(b[28+n:])
def array(i):
 a=d['accessors'][i];v=d['bufferViews'][a['bufferView']]
 return np.frombuffer(blob,dtype='<f4',count=a['count']*3,offset=v.get('byteOffset',0)+a.get('byteOffset',0)).reshape(-1,3)
report={'source_sha256':hashlib.sha256(b).hexdigest(),'open_angle_degrees':65,'hinge_x_m':2.4,'hinge_y_m':3.7,'changes':[]}
for mi in (3,4):
 p=d['meshes'][mi]['primitives'][0];ai=p['attributes']['POSITION'];v=array(ai);before=v.copy()
 for side in (-1,1):
  mask=(v[:,0]*side>0)&(abs(v[:,0])<2)&(v[:,1]>3)&(v[:,1]<4.5)&(v[:,2]<=4.01)
  q=v[mask].copy();q[:,:2]-=[side*1.02,3.7]
  if mi==3:
   angle=math.radians(-side*38);c,s=math.cos(angle),math.sin(angle)
   q[:,:2]=q[:,:2]@np.array([[c,s],[-s,c]])
  # Outer edge is the hinge; rotate mirrored leaves into the passage.
  q[:,0]-=side*.925
  angle=math.radians(side*65);c,s=math.cos(angle),math.sin(angle)
  q[:,:2]=q[:,:2]@np.array([[c,s],[-s,c]])
  q[:,:2]+=[side*2.4,3.7]
  v[mask]=q
  assert len(q)==(8 if mi==3 else 504),(mi,side,len(q))
  report['changes'].append({'mesh':d['meshes'][mi]['name'],'side':side,'vertices':len(q),'bounds':[q.min(0).tolist(),q.max(0).tolist()]})
 mask=(abs(before[:,0])<2)&(before[:,1]>3)&(before[:,1]<4.5)&(before[:,2]<=4.01)
 assert np.array_equal(before[~mask],v[~mask])
 d['accessors'][ai]['min']=v.min(0).tolist();d['accessors'][ai]['max']=v.max(0).tolist()
 # Keep imported corrected assets distinguishable from preserved originals.
 d['meshes'][mi]['name']=d['meshes'][mi]['name']+'_DoorAligned'
d['meshes']=[d['meshes'][i] for i in (3,4)]
d['nodes']=[{'name':m['name'],'mesh':i} for i,m in enumerate(d['meshes'])];d['scenes']=[{'nodes':[0,1]}];d['scene']=0
j=json.dumps(d,separators=(',',':')).encode();j+=b' '*((-len(j))%4)
out=struct.pack('<III',0x46546c67,2,12+8+len(j)+8+len(blob))+struct.pack('<II',len(j),0x4e4f534a)+j+struct.pack('<II',len(blob),0x004e4942)+blob
(OUT/'GateDoorsAligned.glb').write_bytes(out)
report['unchanged_non_door_vertices']=True
(OUT/'geometry-validation.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
