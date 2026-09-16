import unreal

bp_path = '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4'
bp = unreal.EditorAssetLibrary.load_asset(bp_path)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector())
for c in actor.get_components_by_class(unreal.StaticMeshComponent):
    if c.get_name().startswith('Detail_'):
        continue
    print('BASE_COMPONENT', c.get_name(), c.static_mesh.get_path_name())
    for i in range(c.get_num_materials()):
        mat = c.get_material(i)
        print('SLOT', i, mat.get_path_name() if mat else None, 'blend', mat.get_editor_property('blend_mode') if mat else None)
actors.destroy_actor(actor)
