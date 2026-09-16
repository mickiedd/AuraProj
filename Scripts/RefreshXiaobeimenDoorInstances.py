import unreal,json,shutil
from pathlib import Path
R=Path('C:/Git/AuraProj');O=R/'Saved/GateDoorFix'
changes=json.loads((O/'asset-changes.json').read_text());s=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp=unreal.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3')
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 c=lib.get_object(ss.k2_find_subobject_data_from_handle(h))
 if isinstance(c,unreal.StaticMeshComponent) and c.static_mesh and ('Wood_Weathered' in c.get_name() or 'Metal_Fittings' in c.get_name()):print('TEMPLATE',c.static_mesh.get_path_name())
f=R/'Content/Scifi_desert_city/Level/L_showcase_level.umap'
if not (O/'L_showcase_level.before.umap').exists():shutil.copy2(f,O/'L_showcase_level.before.umap')
count=0
for a in s.get_all_level_actors():
 if a.get_class()!=bp.generated_class():continue
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  for change in changes:
   if c.static_mesh and c.static_mesh.get_path_name()==change['before']:
    c.set_static_mesh(unreal.load_asset(change['after']));count+=1
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('UPDATED_INSTANCE_COMPONENTS',count)
