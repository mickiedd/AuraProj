"""Snapshot the current V5 Wuxianmen Core roof tiling state before repair."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
ROOF_MATERIAL = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/Materials/M_RoofTile_ReferenceTuned"
OUTPUT = ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-baseline-20260917.json"


def _path(value):
    return value.get_path_name() if value else ""


def _vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def _rot(value):
    return [float(value.pitch), float(value.yaw), float(value.roll)]


def _transform(value):
    return {
        "location": _vec(value.translation),
        "rotation": _rot(value.rotation.rotator()),
        "scale": _vec(value.scale3d),
    }


def _transform_hash(component):
    payload = [_transform(component.get_instance_transform(index, False)) for index in range(component.get_instance_count())]
    return hashlib.sha256(json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")).hexdigest()


def _components(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen = set()
    result = []
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


def _material_record(material):
    record = {"path": _path(material), "used_textures": []}
    if not material:
        return record
    record["used_textures"] = sorted(_path(texture) for texture in unreal.MaterialEditingLibrary.get_used_textures(material))
    return record


def main():
    dirty_maps = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = _components(blueprint)
    assert len(components) == 7, len(components)
    records = []
    for component in components:
        mesh = component.static_mesh
        assert mesh, component.get_name()
        body = mesh.get_editor_property("body_setup")
        records.append({
            "name": component.get_name(),
            "mesh": _path(mesh),
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "relative_transform": {
                "location": _vec(component.get_editor_property("relative_location")),
                "rotation": _rot(component.get_editor_property("relative_rotation")),
                "scale": _vec(component.get_editor_property("relative_scale3d")),
            },
            "materials": [_path(component.get_material(index)) for index in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
            "roof_role": "roof_tile" if "lower_tile" in component.get_name().lower() else "ridge" if "ridge" in component.get_name().lower() else "other",
        })
    roof_material = unreal.EditorAssetLibrary.load_asset(ROOF_MATERIAL)
    assert isinstance(roof_material, unreal.Material), ROOF_MATERIAL
    output = {
        "created": "2026-09-17",
        "blueprint": BLUEPRINT,
        "dirty_maps_recorded": dirty_maps,
        "component_count": len(records),
        "instance_count": sum(item["instance_count"] for item in records),
        "components": records,
        "roof_material": _material_record(roof_material),
        "map_saved": False,
    }
    OUTPUT.write_text(json.dumps(output, indent=2), encoding="utf-8")
    print("V5_WUXIANMEN_ROOF_TILING_BASELINE_WRITTEN", OUTPUT)
    print("V5_WUXIANMEN_ROOF_TILING_BASELINE_COUNTS", len(records), output["instance_count"])


if __name__ == "__main__":
    main()
