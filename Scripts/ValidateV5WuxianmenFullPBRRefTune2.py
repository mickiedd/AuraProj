"""Validate the Wuxianmen V5 FullPBR reference-tuning revision 2 in the live editor.

Spawns a transient copy of the Blueprint in an isolated location, asserts the
post-repair invariants against the recorded baseline, measures the roof surface
orientation, then destroys everything. No map is saved.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"
BASELINE = ROOT / "Wuxianmen_V5_FullPBR-reftune2-baseline-20260917.json"
OUTPUT = ROOT / "Wuxianmen_V5_FullPBR-reftune2-validation-20260917.json"

TILE_COMPONENT = "HISM_002_lower_tile_1_077_GEN_VARIABLE"
RIDGE_COMPONENT = "HISM_004_ridge_000010_GEN_VARIABLE"
WOOD_COMPONENT = "HISM_006_wood_000451_GEN_VARIABLE"


def _path(value):
    return value.get_path_name() if value else ""


def _axis_angle(axis, degrees):
    return unreal.MathLibrary.rotator_from_axis_and_angle(unreal.Vector(*axis), degrees)


def _rotator_from_record(record):
    """Keyword constructor: probed to round-trip exactly."""
    return unreal.Rotator(pitch=record[0], yaw=record[1], roll=record[2])


def _aabb(transform, local_min, local_max):
    corners = []
    for x in (local_min.x, local_max.x):
        for y in (local_min.y, local_max.y):
            for z in (local_min.z, local_max.z):
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    transform.rotation,
                    unreal.Vector(x * transform.scale3d.x, y * transform.scale3d.y, z * transform.scale3d.z),
                )
                corners.append(rotated + transform.translation)
    low = [min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)]
    high = [max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)]
    return low, high


def _matches_index(component, index, item, tolerance=1e-3):
    transform = component.get_instance_transform(index, False)
    if any(
        abs(a - b) > tolerance
        for a, b in zip(
            [transform.translation.x, transform.translation.y, transform.translation.z], item["location"]
        )
    ):
        return False
    if any(
        abs(a - b) > tolerance
        for a, b in zip([transform.scale3d.x, transform.scale3d.y, transform.scale3d.z], item["scale"])
    ):
        return False
    expected = unreal.MathLibrary.conv_rotator_to_quaternion(_rotator_from_record(item["rotation"]))
    live = transform.rotation
    dot = abs(live.x * expected.x + live.y * expected.y + live.z * expected.z + live.w * expected.w)
    return dot >= 1.0 - 1e-6


def _matches_baseline(component, expected, tolerance=1e-3):
    if component.get_instance_count() != len(expected):
        return False
    return all(_matches_index(component, index, item, tolerance) for index, item in enumerate(expected))


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
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    baseline_by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT

    dirty_before = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    components = {component.get_name(): component for component in _components(blueprint)}
    assert len(components) == 7, len(components)

    checks = []
    for name, component in sorted(components.items()):
        mesh = component.static_mesh
        body = mesh.get_editor_property("body_setup")
        record = {
            "name": name,
            "instance_count": component.get_instance_count(),
            "mesh": _path(mesh),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "double_sided_geometry": bool(body.get_editor_property("double_sided_geometry")),
            "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "nanite_enabled": bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")),
        }
        expected = baseline_by_name[name]
        assert record["instance_count"] == expected["instance_count"], name
        assert record["visible"] == expected["visible"], name
        assert record["collision_profile"] == expected["collision_profile"], name
        assert record["cast_shadow"] == expected["cast_shadow"], name
        checks.append(record)

    # Untouched components keep their exact baseline payload.
    untouched = {}
    for name, component in sorted(components.items()):
        if name in (TILE_COMPONENT, RIDGE_COMPONENT, WOOD_COMPONENT):
            continue
        untouched[name] = _matches_baseline(component, baseline_by_name[name]["instances"])
    assert all(untouched.values()), untouched

    # Inside the repaired components, only the intended roles changed.
    repaired_roles = {
        RIDGE_COMPONENT: lambda transform, box: (box[1][0] - box[0][0]) > 1000.0,
        WOOD_COMPONENT: lambda transform, box: (box[1][0] - box[0][0]) > 1500.0 and (box[1][2] - box[0][2]) > 100.0,
    }
    partial = {}
    for name, predicate in repaired_roles.items():
        component = components[name]
        expected = baseline_by_name[name]["instances"]
        mesh = component.static_mesh
        bounds = mesh.get_bounds()
        local_min = bounds.origin - bounds.box_extent
        local_max = bounds.origin + bounds.box_extent
        changed = unchanged = 0
        mismatches = []
        for index, item in enumerate(expected):
            transform = component.get_instance_transform(index, False)
            box = _aabb(transform, local_min, local_max)
            if predicate(transform, box):
                changed += 1
            else:
                unchanged += 1
                if not _matches_index(component, index, item):
                    mismatches.append(index)
        partial[name] = {"changed_instances": changed, "unchanged_instances": unchanged,
                         "unchanged_mismatches": mismatches}
    assert not any(item["unchanged_mismatches"] for item in partial.values()), partial

    ridge = next(item for item in checks if item["name"] == RIDGE_COMPONENT)
    assert ridge["mesh"].endswith("main_ridge.main_ridge"), ridge["mesh"]

    # Roof surface orientation from a transient spawn.
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center = unreal.Vector(120000.0, 120000.0, 0.0)
    spawned = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    assert spawned, "transient spawn failed"
    try:
        origin, extent = spawned.get_actor_bounds(False)
        tile_component = None
        for component in spawned.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent):
            if component.get_name().startswith("HISM_002"):
                tile_component = component
                break
        assert tile_component, "tile component not found on spawned actor"
        bounds = tile_component.static_mesh.get_bounds()
        local_min = bounds.origin - bounds.box_extent
        local_max = bounds.origin + bounds.box_extent
        corners = []
        for index in range(tile_component.get_instance_count()):
            transform = tile_component.get_instance_transform(index, False)
            for x in (local_min.x, local_max.x):
                for y in (local_min.y, local_max.y):
                    for z in (local_min.z, local_max.z):
                        rotated = unreal.MathLibrary.quat_rotate_vector(
                            transform.rotation,
                            unreal.Vector(x * transform.scale3d.x, y * transform.scale3d.y, z * transform.scale3d.z),
                        )
                        point = rotated + transform.translation
                        corners.append((point.y, point.z))
        ridge_z = max(z for y, z in corners if abs(y) < 100.0)
        eave_z = max(z for y, z in corners if abs(y) > 380.0)
        assert ridge_z > eave_z + 100.0, (ridge_z, eave_z)
        bounds_record = {
            "actor_bounds_origin": [round(origin.x, 3), round(origin.y, 3), round(origin.z, 3)],
            "actor_bounds_extent": [round(extent.x, 3), round(extent.y, 3), round(extent.z, 3)],
            "tile_corner_samples": len(corners),
            "roof_ridge_z_cm": round(ridge_z, 3),
            "roof_eave_z_cm": round(eave_z, 3),
            "roof_rise_cm": round(ridge_z - eave_z, 3),
        }
    finally:
        spawned_name = spawned.get_name()
        actors.destroy_actor(spawned)
        leaked = [a.get_name() for a in actors.get_all_level_actors() if a.get_name() == spawned_name]
        assert not leaked, leaked

    result = {
        "created": "2026-09-17",
        "revision": "reftune2",
        "blueprint": BLUEPRINT,
        "dirty_maps_recorded": dirty_before,
        "components": checks,
        "untouched_instance_payload_preserved": untouched,
        "repaired_component_breakdown": partial,
        "spawned_actor_validation": bounds_record,
        "transient_actor_destroyed": True,
        "map_saved": False,
        "passed": True,
    }
    OUTPUT.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("FULLPBR_REFTUNE2_VALIDATION", OUTPUT)
    print("FULLPBR_REFTUNE2_VALIDATION_BOUNDS", json.dumps(bounds_record))
    print("FULLPBR_REFTUNE2_VALIDATION_PRESERVED", json.dumps(untouched))
    print("FULLPBR_REFTUNE2_VALIDATION_BREAKDOWN", json.dumps(partial))


if __name__ == "__main__":
    main()
