import unreal

for level, label in (
    ("/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/L_Wuxianmen_V4_Preview", "Preview_Wuxianmen_V4"),
    ("/Game/Assets/Environment/GuangzhouLandmarks/V4/Zhengximen/L_Zhengximen_V4_Preview", "Preview_Zhengximen_V4"),
):
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(level)
    actor = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == label)
    print("ACTOR", label, "location", actor.get_actor_location(), "rotation", actor.get_actor_rotation(), "scale", actor.get_actor_scale3d())
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if not mesh:
            continue
        print("COMP", component.get_name(), "mesh", mesh.get_path_name(), "rel", component.get_editor_property("relative_location"), component.get_editor_property("relative_rotation"), component.get_editor_property("relative_scale3d"))
