"""Rebind only Wuxianmen Blueprint mesh references to outward-winding variants."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINTS = [
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


def _role_from_materials(component):
    materials = [_path(component.get_material(index)).lower() for index in range(component.get_num_materials())]
    tokens = (("stone", "stone"), ("wood", "wood"), ("plaster", "plaster"), ("iron", "iron"), ("tile", "rooftile"), ("plaque", "plaque_wuxianmen"))
    for role, token in tokens:
        if any("m_" + token in material for material in materials):
            return role
    raise AssertionError((component.get_name(), materials))


def _component_state(component):
    return {
        "name": component.get_name(),
        "mesh": _path(component.static_mesh),
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
    }


def _rebind(name, blueprint_path):
    baseline = json.loads((ROOT / "reference-tuning-baseline-20260917.json").read_text(encoding="utf-8"))
    baseline_pkg = next(item for item in baseline["packages"] if item["name"] == name)
    winding = json.loads((ROOT / (name + "-winding-import.json")).read_text(encoding="utf-8"))
    by_role = {}
    for item in winding["meshes"]:
        by_role.setdefault(item["role"], []).append(item["mesh"])
    blueprint = EAL.load_asset(blueprint_path)
    assert isinstance(blueprint, unreal.Blueprint), blueprint_path
    current = {component.get_name(): component for component in _components(blueprint)}
    assert len(current) == baseline_pkg["component_count"]
    baseline_by_name = {item["name"]: item for item in baseline_pkg["components"]}
    records = []
    for component_name, expected in baseline_by_name.items():
        component = current[component_name]
        before = _component_state(component)
        role = _role_from_materials(component)
        candidates = by_role.get(role, [])
        if role == "tile":
            candidates = [path for path in candidates if "ridge" not in path.lower()]
        assert len(candidates) == 1, (name, component_name, role, candidates)
        replacement = EAL.load_asset(candidates[0])
        assert isinstance(replacement, unreal.StaticMesh), candidates[0]
        component.set_static_mesh(replacement)
        for index, material_path in enumerate(expected["materials"]):
            material = EAL.load_asset(material_path)
            assert material, material_path
            component.set_material(index, material)
        after = _component_state(component)
        assert after["instance_count"] == expected["instance_count"], (name, component_name, after["instance_count"], expected["instance_count"])
        assert after["transform_hash"] == before["transform_hash"], (name, component_name, "instance transform payload changed")
        assert after["relative_transform"] == before["relative_transform"], (name, component_name, "component transform changed")
        assert after["materials"] == expected["materials"], (name, component_name, after["materials"], expected["materials"])
        assert after["visible"] == expected["visible"] and after["collision_profile"] == expected["collision_profile"]
        records.append({"component": component_name, "role": role, "before": before, "after": after, "replacement": candidates[0]})
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert EAL.save_loaded_asset(blueprint, only_if_is_dirty=False)
    result = {
        "name": name,
        "blueprint": blueprint_path,
        "blueprint_touched": True,
        "baseline_component_count": baseline_pkg["component_count"],
        "baseline_instance_count": baseline_pkg["instance_count"],
        "post_component_count": len(records),
        "post_instance_count": sum(item["after"]["instance_count"] for item in records),
        "transform_payload_preserved": True,
        "records": records,
    }
    (ROOT / (name + "-winding-rebind.json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("V5_WINDING_REBOUND", name, len(records), result["post_instance_count"])


def main():
    for name, blueprint_path in BLUEPRINTS:
        _rebind(name, blueprint_path)


if __name__ == "__main__":
    main()
