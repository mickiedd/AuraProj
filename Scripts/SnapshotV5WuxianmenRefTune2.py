"""Baseline snapshot for the Wuxianmen V5 Core reference-tuning revision 2.

Records the immutable pre-mutation state of BP_Wuxianmen_V5_4K_Core (components,
mesh paths, materials, per-instance transform hashes, collision, visibility,
bounds) and probes the Unreal Python API surface this pass depends on, so the
repair script can be written against confirmed signatures rather than guesses.

Writes:
  Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-baseline-20260917.json
  Saved/RawModelImport/V5/Wuxianmen_V5_4K_Core-reftune2-api-probe-20260917.json

Read-only: loads assets, never saves a package, never saves a map.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = (
    "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
)
BASELINE = ROOT / "Wuxianmen_V5_4K_Core-reftune2-baseline-20260917.json"
API_PROBE = ROOT / "Wuxianmen_V5_4K_Core-reftune2-api-probe-20260917.json"

# Tier identification: the main roof sits above z = 13.5 m, the lower skirt roof
# below it. Measured from the source assembly.
TIER_SPLIT_Z = 1350.0


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


def _role(name):
    lowered = name.lower()
    for token in ("plaque", "iron", "lower_tile", "plaster", "ridge", "stone", "wood"):
        if token in lowered:
            return token
    return "unknown"


def snapshot():
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
        instances = []
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, False)
            instances.append({
                "index": index,
                "location": _vec(transform.translation),
                "rotation": _rot(transform.rotation.rotator()),
                "scale": _vec(transform.scale3d),
            })
        records.append({
            "name": component.get_name(),
            "role": _role(component.get_name()),
            "mesh": _path(mesh),
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "relative_transform": {
                "location": _vec(component.get_editor_property("relative_location")),
                "rotation": _rot(component.get_editor_property("relative_rotation")),
                "scale": _vec(component.get_editor_property("relative_scale3d")),
            },
            "materials": [
                _path(component.get_material(index))
                for index in range(component.get_num_materials())
            ],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
            "nanite_enabled": bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")),
            "instances": instances,
        })

    report = {
        "created": "2026-09-17",
        "revision": "reftune2",
        "blueprint": BLUEPRINT,
        "dirty_maps_recorded": dirty_maps,
        "component_count": len(records),
        "instance_count": sum(item["instance_count"] for item in records),
        "tier_split_z": TIER_SPLIT_Z,
        "components": records,
        "map_saved": False,
    }
    BASELINE.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("REFTUNE2_BASELINE", BASELINE)
    for item in records:
        tier_counts = {"main": 0, "lower": 0}
        if item["role"] in ("lower_tile", "ridge", "wood"):
            for instance in item["instances"]:
                tier_counts["main" if instance["location"][2] >= TIER_SPLIT_Z else "lower"] += 1
        print(
            f"REFTUNE2_COMPONENT {item['name']} role={item['role']} "
            f"n={item['instance_count']} mesh={item['mesh'].split('/')[-1]} "
            f"main={tier_counts['main']} lower={tier_counts['lower']}"
        )


def probe_api():
    names = {
        "mathlibrary": [
            name for name in dir(unreal.MathLibrary)
            if any(token in name.lower() for token in ("quat", "rotator", "matrix", "compose", "inverse"))
        ],
        "hism": [
            name for name in dir(unreal.HierarchicalInstancedStaticMeshComponent)
            if "instance" in name.lower() or "transform" in name.lower()
        ],
        "material_editing": [
            name for name in dir(unreal.MaterialEditingLibrary)
            if any(token in name.lower() for token in ("expression", "connect", "recompile", "layout", "texture"))
        ],
        "transform_ctor": "unreal.Transform",
        "rotator_ctor": "unreal.Rotator",
    }
    try:
        probe = unreal.Transform(unreal.Vector(1.0, 2.0, 3.0), unreal.Rotator(0.0, 0.0, 180.0), unreal.Vector(1.0, 1.0, 1.0))
        names["transform_ctor_ok"] = True
        names["transform_rotation_type"] = str(type(probe.rotation))
        names["transform_scale_type"] = str(type(probe.scale3d))
        names["rotator_repr"] = _rot(probe.rotation.rotator())
    except Exception as error:  # pragma: no cover - diagnostic path
        names["transform_ctor_ok"] = False
        names["transform_ctor_error"] = repr(error)
    API_PROBE.write_text(json.dumps(names, indent=2), encoding="utf-8")
    print("REFTUNE2_API_PROBE", API_PROBE)
    print("REFTUNE2_MATHLIB", json.dumps(names["mathlibrary"]))
    print("REFTUNE2_HISM", json.dumps(names["hism"]))
    print("REFTUNE2_CTOR", json.dumps({k: v for k, v in names.items() if k.startswith("transform_ctor") or k.startswith("rotator_")}))


def main():
    snapshot()
    probe_api()


if __name__ == "__main__":
    main()
