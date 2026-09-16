import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
print('WORLD', world.get_path_name() if world else None)
for a in actors:
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    print('ACTOR', a.get_actor_label(), a.get_class().get_name(), len(comps))
    for c in comps:
        print('  C', c.get_name(), c.static_mesh.get_path_name() if c.static_mesh else None, c.get_editor_property('visible'))
