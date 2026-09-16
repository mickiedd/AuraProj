"""Import aligned door geometry and replace only two Blueprint mesh references."""
import json,sys,shutil
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/GateDoorFix'
sys.path.insert(0,str(ROOT/'Scripts'))
import ImportV3Buildings as imp
cfg=json.loads((ROOT/'Saved/RawModelImport/V3/packages.json').read_text())[1]
bp_path=cfg['destination']+'/BP_'+cfg['name']
f=ROOT/('Content'+bp_path.removeprefix('/Game')+'.uasset')
backup=OUT/'BP_Xiaobeimen_Production_V3.before.uasset'
if not backup.exists():shutil.copy2(f,backup)
data=json.loads((ROOT/'Saved/RawModelImport/V3/Xiaobeimen_Production_V3-import.json').read_text())
materials={k:unreal.load_asset(v) for k,v in data['materials'].items()}
source={'stem':'GateDoorsAligned','source':str(OUT/'GateDoorsAligned.glb'),'primary':True,'collision':False,'mesh_count':2,'instance_count':2,'relative':'GateDoorsAligned.glb'}
result=imp.import_source(cfg,source,materials)
bp=unreal.load_asset(bp_path);ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
changes=[];seen=set()
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 c=lib.get_object(ss.k2_find_subobject_data_from_handle(h))
 if not isinstance(c,unreal.StaticMeshComponent) or not c.static_mesh or c.get_path_name() in seen:continue
 seen.add(c.get_path_name())
 for rec in result['instances']:
  oldname=c.static_mesh.get_name()
  if rec['label']==oldname+'_DoorAligned':
   changes.append({'component':c.get_name(),'before':c.static_mesh.get_path_name(),'after':rec['mesh']})
   c.set_static_mesh(unreal.load_asset(rec['mesh']))
assert len(changes)==2,changes
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
(OUT/'asset-changes.json').write_text(json.dumps(changes,indent=2))
print('DOOR_FIX_APPLIED',changes)
