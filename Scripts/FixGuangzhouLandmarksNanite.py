"""Normalise the Nanite fallback on thin geometry across the Guangzhou landmark buildings.

The audit found `fallback_relative_error = 1.0` on 111 meshes across five
buildings — every mesh in Guidemen V5 4K (including the 16,248-instance roof
tiles, both roof shells and both main ridges), the roof tiles, roof shells,
ornaments, plaques and doors of both Xiaobeimen builds and Zhengnanmen AAA V3,
and the dressed-stone, door-ring, finial, ridge-end and stud meshes of
Zhengnanmen HighFidelity. Wuxianmen already carries 0.0.

Why this matters on this project specifically: the visual change archive records
that the editor has rendered with **Nanite disabled** (an SM5 fallback path when
the NVIDIA adapter is not enumerable), and on that path the *fallback mesh* is what
draws. A relative error of 1.0 simplifies the fallback, so thin roofs, tile strips,
rails, caps and plaques can lose surfaces exactly when they are being rendered
without Nanite. The tuning workflow asks for a full-triangle fallback with zero
relative error wherever thin geometry must stay visible.

Action, per flagged mesh: set `fallback_relative_error = 0.0` and
`fallback_percent_triangles = 100.0`, rebuild/apply through the static mesh editor
subsystem, and save. Records the before/after state and the source triangle count
for every mesh so the fallback cost is reviewable.

Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport"
AUDIT = ROOT / "GuangzhouLandmarks-audit-v2-20260918.json"
REPORT = ROOT / "GuangzhouLandmarks-nanite-fallback-fix-20260918.json"


def _path(value):
    return value.get_path_name() if value else ""


def _read_settings(mesh):
    settings = mesh.get_editor_property("nanite_settings")
    record = {"enabled": bool(settings.get_editor_property("enabled"))}
    for name in ("fallback_relative_error", "fallback_percent_triangles", "keep_percent_triangles",
                 "fallback_target_min_resolution"):
        try:
            record[name] = float(settings.get_editor_property(name))
        except Exception:
            record[name] = None
    return settings, record


def _triangle_count(mesh):
    try:
        return int(mesh.get_num_triangles(0))
    except Exception:
        try:
            return int(mesh.get_editor_property("nanite_settings").get_editor_property("fallback_percent_triangles"))
        except Exception:
            return None


BLUEPRINTS = [
    ("Zhengnanmen_HighFidelity", "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"),
    ("Xiaobeimen_AAA_V3", "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3"),
    ("Xiaobeimen_Production_V3", "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3"),
    ("Zhengnanmen_AAA_V3", "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3"),
    ("Guidemen_V5_4K", "V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("Wuxianmen_V5_4K_Core", "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"),
    ("Wuxianmen_V5_FullPBR", "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"),
]
ASSET_ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/"


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if not isinstance(obj, (unreal.StaticMeshComponent,
                                unreal.HierarchicalInstancedStaticMeshComponent)):
            continue
        if _path(obj) in seen:
            continue
        seen.add(_path(obj))
        result.append(obj)
    return result


def _collect_targets():
    """Every Nanite mesh in every building whose fallback is not full-detail.

    Scanning the Blueprints rather than the earlier audit report matters: the audit
    flagged only `fallback_relative_error > 0`, which missed 103 Zhengnanmen meshes
    that already had error 0.0 but `fallback_percent_triangles = 1.0`.
    """
    by_mesh = {}
    for label, relative in BLUEPRINTS:
        blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_ROOT + relative)
        if not isinstance(blueprint, unreal.Blueprint):
            continue
        for component in _components(blueprint):
            mesh = component.static_mesh
            if not mesh:
                continue
            settings = mesh.get_editor_property("nanite_settings")
            if not bool(settings.get_editor_property("enabled")):
                continue
            err = float(settings.get_editor_property("fallback_relative_error"))
            pct = float(settings.get_editor_property("fallback_percent_triangles"))
            if err == 0.0 and pct == 100.0:
                continue
            count = (component.get_instance_count()
                     if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent) else 1)
            entry = by_mesh.setdefault(_path(mesh), {"mesh": _path(mesh), "buildings": set(), "instances": 0})
            entry["buildings"].add(label)
            entry["instances"] += count
    return by_mesh


def main():
    subsystem = None
    try:
        subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    except Exception:
        subsystem = None

    by_mesh = _collect_targets()
    report = {"created": "2026-09-18", "unique_meshes": len(by_mesh),
              "applied": [], "skipped": [], "map_saved": False}
    for mesh_path, item in sorted(by_mesh.items()):
        mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
        if not isinstance(mesh, unreal.StaticMesh):
            report["skipped"].append({"mesh": mesh_path, "reason": "not a StaticMesh"})
            continue
        settings, before = _read_settings(mesh)
        if not before["enabled"]:
            report["skipped"].append({"mesh": mesh_path, "reason": "nanite disabled"})
            continue
        try:
            settings.set_editor_property("fallback_relative_error", 0.0)
            settings.set_editor_property("fallback_percent_triangles", 100.0)
            mesh.set_editor_property("nanite_settings", settings)
        except Exception as error:
            report["skipped"].append({"mesh": mesh_path, "reason": "set failed: " + repr(error)[:120]})
            continue
        applied_via = "property only"
        if subsystem is not None:
            for args in (
                (mesh, settings, True, False),
                (mesh, settings, True),
                (mesh, settings),
            ):
                try:
                    subsystem.set_nanite_settings(*args)
                    applied_via = "StaticMeshEditorSubsystem.set_nanite_settings"
                    break
                except Exception:
                    continue
        try:
            mesh.post_edit_change()
        except Exception:
            pass
        saved = unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        _, after = _read_settings(mesh)
        report["applied"].append({
            "mesh": mesh_path,
            "mesh_name": mesh_path.split("/")[-1].split(".")[0],
            "buildings": sorted(item["buildings"]),
            "instances": item["instances"],
            "triangles": _triangle_count(mesh),
            "before": before,
            "after": after,
            "applied_via": applied_via,
            "saved": bool(saved),
        })
        print("NANITE_FIX", mesh_path.split("/")[-1], "before err", before["fallback_relative_error"],
              "pct", before["fallback_percent_triangles"], "-> after err",
              after["fallback_relative_error"], "pct", after["fallback_percent_triangles"],
              "tris", _triangle_count(mesh))

    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("NANITE_FIX_SUMMARY", "unique", report["unique_meshes"],
          "applied", len(report["applied"]),
          "skipped", len(report["skipped"]))
    for item in report["skipped"][:10]:
        print("NANITE_SKIPPED", item["mesh"].split("/")[-1], item["reason"])
    print("NANITE_REPORT", REPORT)


if __name__ == "__main__":
    main()
