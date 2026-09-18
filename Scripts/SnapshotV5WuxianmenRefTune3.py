"""reftune3 baseline snapshot for both Wuxianmen V5 variants.

Records the current (reftune2-corrected) state of both Blueprints — components,
mesh paths, materials, per-instance transforms, collision, visibility, shadow
flags — as the immutable baseline for the reftune3 detail pass. The snapshot
doubles as the rollback path.

Read-only: loads assets, never saves a package, never saves a map.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"

VARIANTS = {
    "Core": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
    "FullPBR": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
}


def _path(value):
    return value.get_path_name() if value else ""


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def _transform(value):
    return {"location": _vec(value.translation), "rotation": _rot(value.rotation.rotator()),
            "scale": _vec(value.scale3d)}


def _transform_hash(component):
    payload = [
        _transform(component.get_instance_transform(index, False))
        for index in range(component.get_instance_count())
    ]
    return hashlib.sha256(
        json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    ).hexdigest()


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        component = library.get_object(data)
        if not isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
            continue
        if _path(component) in seen:
            continue
        seen.add(_path(component))
        result.append(component)
    return result


def _snapshot(variant, blueprint_path, dirty_maps):
    blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
    assert isinstance(blueprint, unreal.Blueprint), blueprint_path
    components = _components(blueprint)
    records = []
    for component in components:
        mesh = component.static_mesh
        assert mesh, component.get_name()
        body = mesh.get_editor_property("body_setup")
        nanite = mesh.get_editor_property("nanite_settings")
        instances = []
        for index in range(component.get_instance_count()):
            instances.append(_transform(component.get_instance_transform(index, False)))
        records.append({
            "name": component.get_name(),
            "mesh": _path(mesh),
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "nanite_enabled": bool(nanite.get_editor_property("enabled")),
            "nanite_fallback_relative_error": float(nanite.get_editor_property("fallback_relative_error")),
            "nanite_fallback_percent_triangles": float(nanite.get_editor_property("fallback_percent_triangles")),
            "num_custom_data_floats": int(component.get_editor_property("num_custom_data_floats"))
            if hasattr(component, "get_editor_property") else 0,
            "instances": instances,
        })
    output = ROOT / f"Wuxianmen_V5_{variant}-reftune3-baseline-20260917.json"
    output.write_text(json.dumps({
        "created": "2026-09-18",
        "revision": "reftune3",
        "variant": variant,
        "blueprint": blueprint_path,
        "dirty_maps_recorded": dirty_maps,
        "component_count": len(records),
        "instance_count": sum(item["instance_count"] for item in records),
        "components": records,
        "map_saved": False,
    }, indent=2), encoding="utf-8")
    print("REFTUNE3_BASELINE", variant, output)
    for item in records:
        print(f"REFTUNE3_COMPONENT {variant} {item['name']} n={item['instance_count']} "
              f"nanite={item['nanite_enabled']} err={item['nanite_fallback_relative_error']} "
              f"pct={item['nanite_fallback_percent_triangles']} custom={item['num_custom_data_floats']} "
              f"mesh={item['mesh'].split('/')[-1]}")


def main():
    dirty = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    for variant, path in VARIANTS.items():
        _snapshot(variant, path, dirty)


if __name__ == "__main__":
    main()
