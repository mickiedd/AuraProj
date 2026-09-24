"""Transient probe: confirm the live editor is reachable and safe to write to.

Reports dirty packages (importing assets while the editor holds unsaved work is
how you lose someone else's edits), the engine version, and whether the
Zhengximen destination folder already exists.
"""
import json

import unreal

report = {}
try:
    report["engine_version"] = unreal.SystemLibrary.get_engine_version()
except Exception as exc:  # noqa: BLE001
    report["engine_version"] = "unavailable: " + str(exc)

try:
    dirty_content = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    report["dirty_content_packages"] = [str(p) for p in dirty_content]
    report["dirty_map_packages"] = [str(p) for p in dirty_maps]
except Exception as exc:  # noqa: BLE001
    report["dirty_error"] = str(exc)

try:
    report["project_dir"] = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir())
except Exception as exc:  # noqa: BLE001
    report["project_dir"] = str(exc)

dest = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengximen"
report["dest_exists"] = bool(unreal.EditorAssetLibrary.does_directory_exist(dest))

try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    report["current_level"] = world.get_path_name() if world else None
except Exception as exc:  # noqa: BLE001
    report["current_level"] = str(exc)

report["interchange_available"] = hasattr(unreal, "InterchangeAssetImportData")
report["has_import_lod"] = hasattr(unreal.EditorStaticMeshLibrary, "import_lod")
report["has_nanite_fallback_target"] = hasattr(unreal, "NaniteFallbackTarget")

print("ZHENGXIMEN_PROBE " + json.dumps(report, indent=2))
