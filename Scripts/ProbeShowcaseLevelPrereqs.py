"""Report the editor's current level and dirty-package state before the
showcase level is created.

Creating a level switches the editor's open world, so this first records what
was open (so it can be restored) and whether anything is unsaved (so a
save-prompt cannot block the unattended run).

Read-only.
"""

import json

import unreal

report = {}

editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor_subsystem.get_editor_world()
report["open_world"] = world.get_path_name() if world else None
report["open_world_name"] = world.get_name() if world else None

try:
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    report["dirty_content"] = [package.get_name() for package in dirty]
except Exception as exc:
    report["dirty_content"] = "unreadable: {}".format(exc)

try:
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report["dirty_maps"] = [package.get_name() for package in dirty_maps]
except Exception as exc:
    report["dirty_maps"] = "unreadable: {}".format(exc)

# Confirm the level-creation API exists in this build before relying on it.
report["has_new_level"] = hasattr(unreal.LevelEditorSubsystem, "new_level")

# Confirm the ground/lit-building helpers the showcase needs are reachable.
report["basic_shapes_plane"] = bool(
    unreal.EditorAssetLibrary.does_asset_exist("/Engine/BasicShapes/Plane"))
for candidate in (
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround",
    "/Game/Assets/Environment/GuangzhouLandmarks/V3/M_V3PreviewGround",
    "/Game/Assets/Environment/GuangzhouLandmarks/UltraAAA/M_UltraAAA_CaptureGround",
):
    report[candidate.rsplit("/", 1)[-1]] = bool(
        unreal.EditorAssetLibrary.does_asset_exist(candidate))

# Level templates that new_level may need.
for template in ("/Engine/Maps/Templates/OpenWorld", "/Engine/Maps/Templates/Template_Default"):
    report["template:" + template.rsplit("/", 1)[-1]] = bool(
        unreal.EditorAssetLibrary.does_asset_exist(template))

unreal.log("EDITOR_STATE " + json.dumps(report))
print("EDITOR_STATE", json.dumps(report, indent=2))
