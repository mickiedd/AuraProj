"""Immutable baseline snapshot for the V5 reference-tuning pass."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINTS = [
    ("Guidemen_V5_4K", "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("Wuxianmen_V5_4K_Core", "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"),
    ("Wuxianmen_V5_FullPBR", "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"),
]
EAL = unreal.EditorAssetLibrary


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


def _component_transform(component):
    return {
        "location": _vec(component.get_editor_property("relative_location")),
        "rotation": _rot(component.get_editor_property("relative_rotation")),
        "scale": _vec(component.get_editor_property("relative_scale3d")),
    }


def _uv_channels(mesh):
    getter = getattr(mesh, "get_num_uv_channels", None)
    if getter is None:
        return -1
    try:
        return int(getter())
    except TypeError:
        return int(getter(0))


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


def main():
    dirty_maps = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    dirty_map_paths = [_path(p) for p in dirty_maps]
    if dirty_map_paths:
        print("V5_BASELINE_DIRTY_MAP_RECORDED", dirty_map_paths)
    packages = []
    for name, blueprint_path in BLUEPRINTS:
        blueprint = EAL.load_asset(blueprint_path)
        assert isinstance(blueprint, unreal.Blueprint), blueprint_path
        records = []
        for component in _components(blueprint):
            mesh = component.static_mesh
            assert isinstance(mesh, unreal.StaticMesh), _path(component)
            body = mesh.get_editor_property("body_setup")
            materials = [_path(component.get_material(i)) for i in range(component.get_num_materials())]
            instances = []
            for index in range(component.get_instance_count()):
                instances.append(_transform(component.get_instance_transform(index, False)))
            records.append({
                "component": _path(component),
                "name": component.get_name(),
                "mesh": _path(mesh),
                "instance_count": component.get_instance_count(),
                "instance_transforms_local": instances,
                "relative_transform": _component_transform(component),
                "materials": materials,
                "visible": bool(component.is_visible()),
                "collision_profile": str(component.get_collision_profile_name()),
                "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
                "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
                "cast_shadow": bool(component.get_editor_property("cast_shadow")),
                "uv_channels": _uv_channels(mesh),
                "sections": int(mesh.get_num_sections(0)),
            })
        packages.append({
            "name": name,
            "blueprint": blueprint_path,
            "generated_class": _path(blueprint.generated_class()),
            "components": records,
            "component_count": len(records),
            "instance_count": sum(item["instance_count"] for item in records),
        })
        print("V5_BASELINE_SNAPSHOT", name, len(records), sum(item["instance_count"] for item in records))
    output = ROOT / "reference-tuning-baseline-20260917.json"
    output.write_text(json.dumps({"created": "2026-09-17", "editor_world": {"dirty_maps": dirty_map_paths}, "packages": packages}, indent=2), encoding="utf-8")
    print("V5_BASELINE_WRITTEN", str(output))


if __name__ == "__main__":
    main()
