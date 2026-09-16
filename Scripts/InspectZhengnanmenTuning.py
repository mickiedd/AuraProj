import json
import unreal

BP = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3'
PREVIEW = '/Game/Assets/Environment/GuangzhouLandmarks/V3/Zhengnanmen_AAA_V3/L_Zhengnanmen_AAA_V3_Preview'
bp = unreal.EditorAssetLibrary.load_asset(BP)
ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
records=[]
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 d=ss.k2_find_subobject_data_from_handle(h)
 if lib.is_component(d):
  c=lib.get_object(d)
  if c:
   mesh = c.static_mesh if isinstance(c, unreal.StaticMeshComponent) else None
   records.append({'name':c.get_name(),'visible':bool(c.get_editor_property('visible')),'mesh':mesh.get_path_name() if mesh else ''})
print('BP_COMPONENTS',json.dumps(records))
le=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); assert le.load_level(PREVIEW)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors(); a=next(x for x in actors if x.get_actor_label()=='Preview_Zhengnanmen_AAA_V3')
print('PREVIEW_COMPONENTS',json.dumps([{'name':c.get_name(),'visible':bool(c.get_editor_property('visible')),'mesh':c.static_mesh.get_path_name() if c.static_mesh else ''} for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
