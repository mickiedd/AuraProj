import unreal
paths = [
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Guidemen/BP_Guidemen_V4',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/BP_Wuxianmen_V4',
 '/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/BP_Zhengximen_V4',
]
ss = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem); lib = unreal.SubobjectDataBlueprintFunctionLibrary
for path in paths:
    bp = unreal.EditorAssetLibrary.load_asset(path); print('BP', path)
    seen=set()
    for h in ss.k2_gather_subobject_data_for_blueprint(bp):
        c = lib.get_object(ss.k2_find_subobject_data_from_handle(h))
        if not isinstance(c, unreal.StaticMeshComponent) or c.get_name() in seen or not c.static_mesh: continue
        seen.add(c.get_name()); b=c.static_mesh.get_bounds(); print(' ',c.get_name(), c.static_mesh.get_path_name(), 'loc', c.get_editor_property('relative_location'), 'rot', c.get_editor_property('relative_rotation'), 'ext', b.box_extent)
