import unreal

BP_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen/BP_Dadongmen_V4"
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
actor = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector())
print("ACTOR_BOUNDS", actor.get_actor_bounds(False))
for c in actor.get_components_by_class(unreal.StaticMeshComponent):
    loc = c.get_editor_property("relative_location")
    rot = c.get_editor_property("relative_rotation")
    scale = c.get_editor_property("relative_scale3d")
    print("COMP", c.get_name(), c.static_mesh.get_path_name() if c.static_mesh else None, "LOC", loc, "ROT", rot, "SCALE", scale)
actors.destroy_actor(actor)
