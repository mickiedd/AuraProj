"""Optional geometry-refined Wenmingmen GLB with a baked (embedded) inscription decal.
Source asset is interpretive and NOT surveyed historical architecture.
Subdivision adds stone/roof microgeometry without changing the silhouette materially.
This remains below the 100-million-triangle request; see the manifest for exact count.
"""
import os,json,time,math,argparse
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import trimesh
from trimesh.visual.texture import TextureVisuals
from trimesh.visual.material import PBRMaterial
ap=argparse.ArgumentParser();ap.add_argument('--root',default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))));ap.add_argument('--subdiv',type=int,default=2)
a=ap.parse_args(); root=a.root
src=os.path.join(root,'Wenmingmen_Interpretive.glb');out=os.path.join(root,'Wenmingmen_HighDetail.glb')
s=trimesh.load(src,force='scene');info={}; start=time.time()
for name in list(s.geometry):
 m=s.geometry[name]
 n=0
 if name in ['WM_roof','WM_stone','WM_limestone']:n=a.subdiv
 print(name, 'start',len(m.faces),flush=True)
 if n:
  v=np.asarray(m.vertices,dtype=np.float32);f=np.asarray(m.faces,dtype=np.int32)
  uv=np.asarray(m.visual.uv,dtype=np.float32)
  for i in range(n):
   v,f,att=trimesh.remesh.subdivide(v,f,vertex_attributes={'uv':uv})
   uv=att['uv'];print(name,'subdiv',i+1,len(f),flush=True)
  # Fine-scale physical break-up; up to 4 mm relief; deterministic between coincident vertices.
  if name=='WM_roof':v[:,2]+=(.0037*np.sin(65*v[:,0])*np.sin(61*v[:,1])).astype(np.float32)
  elif name=='WM_stone':v[:,1]+=(.0039*np.sin(57*v[:,0])*np.sin(73*v[:,2])).astype(np.float32)
  elif name=='WM_limestone':v[:,1]+=(.002*np.sin(37*v[:,0])*np.sin(51*v[:,2])).astype(np.float32)
  oldmat=m.visual.material
  nm=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv,material=oldmat))
  s.geometry[name]=nm
 info[name]={'triangles':int(len(s.geometry[name].faces)),'vertices':int(len(s.geometry[name].vertices))}
 print(name,info[name],flush=True)
# Etched sign graphic "文明門" (traditional characters) on central dark plaque. Font baked to PNG; no font files in package.
fontfile='/usr/share/fonts/truetype/arphic-bkai00mp/bkai00mp.ttf'
if not os.path.exists(fontfile):fontfile='/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc'
res=(2048,560)
img=Image.new('RGBA',res,(20,20,20,0));d=ImageDraw.Draw(img)
font=ImageFont.truetype(fontfile,385)
t='文明門';bb=d.textbbox((0,0),t,font=font,stroke_width=0)
w,h=bb[2]-bb[0],bb[3]-bb[1]
d.text(((res[0]-w)/2-bb[0],(res[1]-h)/2-bb[1]),t,font=font,fill=(19,16,13,255))
# Keep a standalone texture and embed same pixels into the GLB material.
png=os.path.join(root,'textures','plaque_inscription_BaseColor.png');img.save(png)
y=-2.4025
v=[[-1.27,y,7.195],[1.27,y,7.195],[1.27,y,7.845],[-1.27,y,7.845]]
f=[[0,1,2],[0,2,3]]
uv=[[0,1],[1,1],[1,0],[0,0]]
mat=PBRMaterial(name='M_inscription_decal',baseColorTexture=img,baseColorFactor=[255,255,255,255],metallicFactor=0.0,roughnessFactor=.96,doubleSided=True,alphaMode='BLEND')
ins=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv,material=mat))
s.add_geometry(ins,node_name='WM_plaque_inscription',geom_name='WM_plaque_inscription')
info['WM_plaque_inscription']={'triangles':2,'vertices':4}
print('exporting...',sum(v['triangles'] for v in info.values()),flush=True)
blob=s.export(file_type='glb');open(out,'wb').write(blob)
manifest=json.load(open(os.path.join(root,'asset_manifest.json')))
manifest['high_detail_file']=os.path.basename(out)
manifest['high_detail_triangles']=sum(v['triangles'] for v in info.values())
manifest['high_detail_mesh_summary']=info
manifest['inscription_note']='文明門 is added as a textured overlay, not modeled engraved character geometry. Verify source lettering against archival material.'
manifest['high_detail_note']='Two rounds of stone / clay roof triangle subdivision with millimeter-scale procedural microrelief; does not equal a 100-million-triangle sculpt or photogrammetry.'
open(os.path.join(root,'asset_manifest.json'),'w',encoding='utf-8').write(json.dumps(manifest,indent=2,ensure_ascii=False))
print('DONE',os.path.getsize(out),'bytes',round(time.time()-start,1),'s',flush=True)
