"""Read-only diagnostics for the active Unreal Editor world."""
import json
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
result = {
    "world": world.get_path_name() if world else None,
    "world_name": world.get_name() if world else None,
    "world_package": world.get_outermost().get_name() if world else None,
    "map_asset_exists": unreal.EditorAssetLibrary.does_asset_exist("/Game/Scifi_desert_city/Level/L_showcase_level"),
    "map_package_exists": unreal.EditorAssetLibrary.does_asset_exist("/Game/Scifi_desert_city/Level/L_showcase_level.L_showcase_level"),
}
if world:
    result["actor_count"] = len(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
print("EDITOR_WORLD", json.dumps(result))
