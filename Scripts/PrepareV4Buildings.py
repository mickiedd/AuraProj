"""Inventory V4 source packages and resolve full-resolution material bindings."""
import json,struct,hashlib,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]/'Saved/RawModelImport/V4'
CONFIGS=[('Dadongmen','Dadongmen_GreatEastGate_UE5','Meshes/SM_Dadongmen_LOD0.gltf'),('Guidemen','Guidemen_GuideGate_UE5_AAA','Meshes/GLB/Guidemen_GuideGate_LOD0.glb'),('Wuxianmen','Wuxianmen_UE5_AAA_Package','Meshes/GLTF/Wuxianmen_NaniteHigh.glb'),('Zhengximen','Zhengximen_GreatWestGate_UE5','Meshes/SM_Zhengximen_LOD0.fbx')]
def data(p):
 if p.suffix=='.gltf':return json.loads(p.read_text())
 b=p.read_bytes();return json.loads(b[20:20+struct.unpack_from('<I',b,12)[0]])
def main():
 ROOT.mkdir(parents=True,exist_ok=True)
 archives=[]
 for archive in sorted(Path("C:/Works/Raw3DModels/V4").glob("*.zip")):
  archives.append({"path":str(archive),"sha256":hashlib.sha256(archive.read_bytes()).hexdigest()})
  with zipfile.ZipFile(archive) as z:
   assert all((ROOT/n).resolve().is_relative_to(ROOT.resolve()) for n in z.namelist())
   for item in z.infolist():
    if not (ROOT/item.filename).exists():z.extract(item,ROOT)
 (ROOT/"archives.json").write_text(json.dumps(archives,indent=2))
 configs=[]
 for name,folder,primary in CONFIGS:
  package=ROOT/folder; meshes=[]; definitions={}
  files=[{'file':p.relative_to(package).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(package.rglob('*')) if p.is_file()]
  sources=sorted(package.rglob('*.glb'))+sorted(package.rglob('*.gltf')) if name!='Zhengximen' else sorted((package/'Meshes').glob('*.fbx'))
  for p in sources:
   rel=p.relative_to(package).as_posix();d=data(p) if p.suffix!='.fbx' else None
   meshes.append({'relative':rel,'source':str(p),'stem':p.stem,'primary':rel==primary,'collision':'Collision' in rel,'mesh_count':len(d['meshes']) if d else 1,'instance_count':sum('mesh' in n for n in d['nodes']) if d else 1})
  if name=='Guidemen':
   manifest=json.loads((package/'Materials/MaterialManifest.json').read_text())
   for mat,v in manifest.items():definitions[mat]={ch:f"{v['folder']}/{v['prefix']}_{'NormalGL' if ch=='Normal' else ch}.png" for ch in ('BaseColor','Normal','Roughness','Metallic','AO','Height')}
  else:
   names=[m['name'] for m in data(package/primary)['materials']] if name!='Zhengximen' else ['M_'+p.stem[3:] for p in (package/'Materials').glob('MI_*.json')]
   for mat in names:
    key=mat.replace('M_Dadongmen_','').removeprefix('M_'); definitions[mat]={}
    for ch in ('BaseColor','Normal','Roughness','Metallic','AO','Height'):
     suffix={'BaseColor':'BC','Normal':'N','Roughness':'R','Metallic':'M','Height':'H'}.get(ch,ch) if name=='Dadongmen' else ch
     hits=list((package/'Textures').rglob(f'T_{key}_{suffix}*.png'))
     hits=[p for p in hits if p.stem == f'T_{key}_{suffix}' or p.stem.startswith(f'T_{key}_{suffix}_')]
     if not hits and key=='Vegetation':continue
     assert len(hits)==1,(name,mat,ch,hits)
     definitions[mat][ch]=hits[0].relative_to(package).as_posix()
  assert all((package/f).exists() for maps in definitions.values() for f in maps.values())
  configs.append({'name':name+'_V4','root':str(package),'destination':'/Game/Assets/Environment/GuangzhouLandmarks/V4/'+name,'meshes':meshes,'materials':definitions,'files':files})
 (ROOT/'packages.json').write_text(json.dumps(configs,indent=2))
 from PrepareV4ZhengximenFbx import main as prepare_fbx
 prepare_fbx()
 print([(c['name'],len(c['meshes']),len(c['materials'])) for c in configs])
if __name__=='__main__':main()

