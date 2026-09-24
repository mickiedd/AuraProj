"""Optional geometry-refined Wenmingmen GLB with a baked (embedded) inscription decal.
Source asset is interpretive and NOT surveyed historical architecture.
Subdivision adds stone/roof microgeometry without changing the silhouette materially.
This remains below the 100-million-triangle request; see the manifest for exact count.
"""
import os,json,time,math,argparse,subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import trimesh
from trimesh.visual.texture import TextureVisuals
from trimesh.visual.material import PBRMaterial
ap=argparse.ArgumentParser();ap.add_argument('--root',default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))));ap.add_argument('--subdiv',type=int,default=2);ap.add_argument('--roof-subdiv',type=int,default=1,help='Physical tile geometry needs less subdivision than masonry')
a=ap.parse_args(); root=a.root
src=os.path.join(root,'Wenmingmen_Interpretive.glb');out=os.path.join(root,'Wenmingmen_HighDetail.glb')
s=trimesh.load(src,force='scene');info={}; start=time.time()
for name in list(s.geometry):
 m=s.geometry[name]
 n=0
 if name in ['WM_stone','WM_limestone']:n=a.subdiv
 elif name=='WM_roof':n=a.roof_subdiv
 print(name, 'start',len(m.faces),flush=True)
 if n:
  v=np.asarray(m.vertices,dtype=np.float32);f=np.asarray(m.faces,dtype=np.int32)
  uv=np.asarray(m.visual.uv,dtype=np.float32)
  for i in range(n):
   v,f,att=trimesh.remesh.subdivide(v,f,vertex_attributes={'uv':uv})
   uv=att['uv'];print(name,'subdiv',i+1,len(f),flush=True)
  # Fine-scale physical break-up; up to 4 mm relief; deterministic between coincident vertices.
  if name=='WM_roof':v[:,2]+=(.0008*np.sin(65*v[:,0])*np.sin(61*v[:,1])).astype(np.float32)
  elif name=='WM_stone':v[:,1]+=(.0012*np.sin(57*v[:,0])*np.sin(73*v[:,2])).astype(np.float32)
  elif name=='WM_limestone':v[:,1]+=(.0010*np.sin(37*v[:,0])*np.sin(51*v[:,2])).astype(np.float32)
  oldmat=m.visual.material
  nm=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv,material=oldmat))
  s.geometry[name]=nm
 info[name]={'triangles':int(len(s.geometry[name].faces)),'vertices':int(len(s.geometry[name].vertices))}
 print(name,info[name],flush=True)
# Right-to-left historic plaque reading: visible left-to-right glyph order is
# 門明文, matching the supplied front reference. The named gate is 文明門.
fontfile=next((p for p in (
 '/usr/share/fonts/truetype/arphic-bkai00mp/bkai00mp.ttf',
 '/usr/share/fonts/opentype/noto/NotoSerifCJK-Bold.ttc',
 '/System/Library/Fonts/STHeiti Medium.ttc',
 '/System/Library/Fonts/Hiragino Sans GB.ttc') if os.path.exists(p)),None)
if fontfile is None:
 try:
  fontfile=subprocess.check_output(['fc-match','-f','%{file}','Noto Sans CJK SC'],text=True).strip()
 except (OSError,subprocess.CalledProcessError):
  raise RuntimeError('A CJK font is needed to render the Wenmingmen plaque')
res=(2048,2048)
img=Image.new('RGBA',res,(20,20,20,0));d=ImageDraw.Draw(img)
font=ImageFont.truetype(fontfile,385)
t='門明文';bb=d.textbbox((0,0),t,font=font,stroke_width=0)
w,h=bb[2]-bb[0],bb[3]-bb[1]
d.text(((res[0]-w)/2-bb[0],(560-h)/2-bb[1]),t,font=font,fill=(19,16,13,255))
couplet_font=ImageFont.truetype(fontfile,200)
for column,characters in ((0,'文教通南粵'),(1,'明德化廣州')):
 for row,char in enumerate(characters):
  box=d.textbbox((0,0),char,font=couplet_font)
  width=box[2]-box[0];height=box[3]-box[1]
  d.text((column*400+(400-width)/2-box[0],650+row*270+(230-height)/2-box[1]),
         char,font=couplet_font,fill=(24,20,17,255))
# Keep a standalone texture and embed same pixels into the GLB material.
png=os.path.join(root,'textures','plaque_inscription_BaseColor.png');img.save(png)
v=[];f=[];uv=[]
def decal_quad(points,coords):
 start=len(v);v.extend(points);uv.extend(coords)
 f.extend([[start,start+1,start+2],[start,start+2,start+3]])
top_band=600/res[1]
decal_quad([[-1.27,-2.4025,7.195],[1.27,-2.4025,7.195],
            [1.27,-2.4025,7.845],[-1.27,-2.4025,7.845]],
           [[0,1-top_band],[1,1-top_band],[1,1],[0,1]])
for side,atlas_column in ((-1,0),(1,1)):
 x0=side*4.22-.335;x1=side*4.22+.335
 u0=atlas_column*400/res[0];u1=(atlas_column+1)*400/res[0]
 coords=[[u0,0],[u1,0],[u1,1-top_band],[u0,1-top_band]]
 decal_quad([[x0,-2.279,2.90],[x1,-2.279,2.90],
             [x1,-2.279,6.20],[x0,-2.279,6.20]],coords)
 # The rear has a matching stone couplet rather than a blank panel.
 decal_quad([[x1,2.279,2.90],[x0,2.279,2.90],
             [x0,2.279,6.20],[x1,2.279,6.20]],coords)
mat=PBRMaterial(name='M_inscription_decal',baseColorTexture=img,baseColorFactor=[255,255,255,255],metallicFactor=0.0,roughnessFactor=.96,doubleSided=True,alphaMode='BLEND')
ins=trimesh.Trimesh(vertices=v,faces=f,process=False,visual=TextureVisuals(uv=uv,material=mat))
s.add_geometry(ins,node_name='WM_plaque_inscription',geom_name='WM_plaque_inscription')
info['WM_plaque_inscription']={'triangles':len(f),'vertices':len(v)}
print('exporting...',sum(v['triangles'] for v in info.values()),flush=True)
blob=s.export(file_type='glb');open(out,'wb').write(blob)
manifest=json.load(open(os.path.join(root,'asset_manifest.json')))
manifest['high_detail_file']=os.path.basename(out)
manifest['high_detail_triangles']=sum(v['triangles'] for v in info.values())
manifest['high_detail_mesh_summary']=info
manifest['inscription_note']='The main plaque reads 文明門 right-to-left (visible 門明文). The two five-character side couplets are interpretive, based on the concept sheet rather than verified archival lettering. All characters are textured overlays, not engraved geometry.'
manifest['high_detail_note']='Selective masonry subdivision and lower roof subdivision retain modeled overlapping pan/cap tile detail with restrained millimeter-scale relief. This is an interpretive GLB, not photogrammetry; the native UE 5.5 Blueprint is reimported and freshly reloaded after source changes, with Metal SM5 fallback settings preserved.'
open(os.path.join(root,'asset_manifest.json'),'w',encoding='utf-8').write(json.dumps(manifest,indent=2,ensure_ascii=False))
print('DONE',os.path.getsize(out),'bytes',round(time.time()-start,1),'s',flush=True)
