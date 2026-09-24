"""Preserve full Wenmingmen geometry on Metal SM5 and other non-Nanite paths.

Run once in the live Aura editor with ``py <absolute path>``. This changes only
the five existing StaticMesh Nanite fallback settings and never saves a map.
It does not reimport or rename meshes, materials, textures, or the Blueprint.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ImportWenmingmen import (  # noqa: E402
    DEST, EAL, IMPORT_MANIFEST, NANITE, PROJECT, validate,
)
from UpdateWenmingmen import inspect_glb  # noqa: E402

OUT = PROJECT / "Saved/Reports/Wenmingmen/fallback-fix.json"
MESHES = DEST + "/Meshes"


def _settings(mesh):
    nanite = mesh.get_editor_property("nanite_settings")
    return {
        "enabled": bool(nanite.get_editor_property("enabled")),
        "target": str(nanite.get_editor_property("fallback_target")),
        "percent": float(nanite.get_editor_property("fallback_percent_triangles")),
        "error": float(nanite.get_editor_property("fallback_relative_error")),
        "render_triangles": int(mesh.get_num_triangles(0)),
    }


def main():
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty_maps:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        dirty_names = [package.get_name() for package in dirty_maps]
        # The earlier transient SceneCapture left the blank Login map marked
        # dirty after every temporary actor was destroyed. The map is never
        # saved here; allow precisely that known empty preview state.
        assert world.get_name() == "Login" and len(actors.get_all_level_actors()) == 0, (
            "Unexpected dirty world or remaining actor", world.get_name(), dirty_names
        )
        assert all(name.rsplit("/", 1)[-1] == "Login" for name in dirty_names), dirty_names
    source_triangles, _ = inspect_glb()
    assert set(NANITE) == {"stone", "limestone", "roof", "wood", "iron"}
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert subsystem, "StaticMeshEditorSubsystem is required in the live editor"
    rows = []
    for part in ("stone", "limestone", "roof", "wood", "iron"):
        asset_path = MESHES + "/WM_" + part + ".WM_" + part
        mesh = EAL.load_asset(asset_path)
        assert isinstance(mesh, unreal.StaticMesh), asset_path
        before = _settings(mesh)
        assert before["enabled"], part
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
        nanite.set_editor_property("fallback_percent_triangles", 1.0)
        nanite.set_editor_property("fallback_relative_error", 0.0)
        subsystem.set_nanite_settings(mesh, nanite, True)
        assert EAL.save_loaded_asset(mesh), part
        after = _settings(mesh)
        assert after["enabled"] and after["percent"] == 1.0 and after["error"] == 0.0, (part, after)
        assert after["render_triangles"] >= source_triangles[part] * 0.9, (
            part, source_triangles[part], after["render_triangles"]
        )
        row = {"part": part, "asset": asset_path, "source_triangles": source_triangles[part],
               "before": before, "after": after}
        rows.append(row)
        print("WENMINGMEN_FALLBACK_FIXED", part, json.dumps(row))
    data = json.loads(IMPORT_MANIFEST.read_text(encoding="utf-8"))
    blueprint = EAL.load_asset(data["blueprint"])
    validate(blueprint, data, completion_marker="WENMINGMEN_FALLBACK_VALIDATED")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({
        "validated": True, "source": str(PROJECT / "ContentSource/GuangzhouLandmarks/Wenmingmen/Wenmingmen_HighDetail.glb"),
        "meshes": rows, "blueprint": data["blueprint"], "bounds_cm": data["bounds_cm"],
        "map_saved": False,
    }, indent=2) + "\n", encoding="utf-8")
    print("WENMINGMEN_FALLBACK_COMPLETE", str(OUT))


if __name__ == "__main__":
    main()
