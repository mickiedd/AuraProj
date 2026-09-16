import unreal
bp=unreal.EditorAssetLibrary.load_asset('/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/BP_Zhengximen_V4')
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem); lib=unreal.SubobjectDataBlueprintFunctionLibrary
seen=set()
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 c=lib.get_object(ss.k2_find_subobject_data_from_handle(h))
 if isinstance(c, unreal.StaticMeshComponent) and c.get_name() not in seen and c.static_mesh:
  seen.add(c.get_name()); print('C',c.get_name(), [c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())])
