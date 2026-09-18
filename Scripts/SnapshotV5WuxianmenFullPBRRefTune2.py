"""Baseline snapshot for the Wuxianmen V5 FullPBR reference-tuning revision 2.

Records the immutable pre-mutation state of BP_Wuxianmen_V5_FullPBR: components,
mesh paths, materials, per-instance transforms, collision, visibility, shadow
flags and bounds. The snapshot doubles as the rollback path — the repair script
restores any component that does not match it before planning new transforms.

Also records whether the FullPBR assembly's node transforms are identical to the
Core assembly's, so the Core correction pivots can be reused with evidence.

Read-only: loads assets, never saves a package, never saves a map.

Writes:
  Saved/RawModelImport/V5/Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"
CORE_BASELINE = ROOT / "Wuxianmen_V5_4K_Core-reftune2-baseline-20260917.json"
OUTPUT = ROOT / "Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json"

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

    # Evidence that the Core correction pivots apply unchanged: compare this
    # assembly's recorded transforms against the Core baseline by role.
    if CORE_BASELINE.exists():
        core = json.loads(CORE_BASELINE.read_text(encoding="utf-8"))
        core_by_role = {item["role"]: item for item in core["components"]}
        comparison = {}
        for item in records:
            other = core_by_role.get(item["role"])
            if not other:
                comparison[item["name"]] = {"role": item["role"], "present_in_core": False}
                continue
            same_count = item["instance_count"] == other["instance_count"]
            same_hash = item["transform_hash"] == other["transform_hash"]
            comparison[item["name"]] = {
                "role": item["role"],
                "present_in_core": True,
                "instance_count": item["instance_count"],
                "core_instance_count": other["instance_count"],
                "instance_count_matches": same_count,
                "transform_payload_identical": same_hash,
                "mesh_name": item["mesh"].split("/")[-1],
                "core_mesh_name": other["mesh"].split("/")[-1],
            }
        report["core_comparison"] = comparison
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("FULLPBR_REFTUNE2_BASELINE", OUTPUT)
    for item in records:
        counts = {"main": 0, "lower": 0}
        if item["role"] in ("lower_tile", "ridge", "wood"):
            for instance in item["instances"]:
                counts["main" if instance["location"][2] >= TIER_SPLIT_Z else "lower"] += 1
        print(
            f"FULLPBR_REFTUNE2_COMPONENT {item['name']} role={item['role']} n={item['instance_count']} "
            f"mesh={item['mesh'].split('/')[-1]} main={counts['main']} lower={counts['lower']}"
        )
    if "core_comparison" in report:
        for name, item in report["core_comparison"].items():
            print(
                f"FULLPBR_REFTUNE2_CORE_CMP {name} role={item['role']} "
                f"identical={item.get('transform_payload_identical')} "
                f"mesh={item.get('mesh_name')} core_mesh={item.get('core_mesh_name')}"
            )


if __name__ == "__main__":
    main()
