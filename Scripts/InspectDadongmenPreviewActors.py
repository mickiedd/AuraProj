import json
import unreal
ROOT = '/Game/Assets/Environment/GuangzhouLandmarks/V4/Dadongmen'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level(ROOT + '/L_Dadongmen_V4_Preview')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
print('ACTORS', [(a.get_actor_label(), a.get_class().get_name()) for a in actors])
for a in actors:
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if c.static_mesh:
            print('COMP', a.get_actor_label(), c.get_name(), c.static_mesh.get_path_name(), c.get_editor_property('relative_location'), c.get_editor_property('relative_rotation'))
