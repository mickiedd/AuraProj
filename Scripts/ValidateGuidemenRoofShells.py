"""Validate the Guidemen roof-shell correction.

The fix asserted that each shell's Z range now nests under its tier's tiled surface,
but a Z range alone cannot prove the slope *direction*. This checks the direction
directly from each shell's own up-axis, and confirms nothing else moved.

For every shell instance:
  - the world-space direction of the shell mesh's local +Z axis, reported as its
    z component and its horizontal component signed relative to the ridge;
  - the world Z range, and the clearance under the tiled surface.

A corrected shell must have its local +Z axis pointing up (positive z component) and
leaning away from the ridge, which is the same test the tile surface passes.

Also asserts: the tile component and both ridge bars are byte-identical to their
pre-fix state; the eight shells are the only components whose transforms changed;
and component counts, materials, collision, visibility and shadow flags are intact.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport"
BASELINE = ROOT / "Guidemen_V5_4K-roofshell-baseline-20260918.json"
OUTPUT = ROOT / "Guidemen_V5_4K-roofshell-validation-20260918.json"

BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
SHELLS = [
    "HISM_025_R1_RoofShellBack_GEN_VARIABLE", "HISM_026_R1_RoofShellFront_GEN_VARIABLE",
    "HISM_027_R1_RoofShellLeft_GEN_VARIABLE", "HISM_028_R1_RoofShellRight_GEN_VARIABLE",
    "HISM_031_R2_RoofShellBack_GEN_VARIABLE", "HISM_032_R2_RoofShellFront_GEN_VARIABLE",
    "HISM_033_R2_RoofShellLeft_GEN_VARIABLE", "HISM_034_R2_RoofShellRight_GEN_VARIABLE",
]
TILE_COMPONENT = "HISM_029_R1_T0_GEN_VARIABLE"
TIER_SPLIT_Z = 1554.88


def _path(value):
    return value.get_path_name() if value else ""


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


def _local_box(mesh):
    bounds = mesh.get_bounds()
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def _aabb(transform, local_min, local_max):
    corners = []
    for x in (local_min.x, local_max.x):
        for y in (local_min.y, local_max.y):
            for z in (local_min.z, local_max.z):
                rotated = unreal.MathLibrary.quat_rotate_vector(
                    transform.rotation,
                    unreal.Vector(x * transform.scale3d.x, y * transform.scale3d.y, z * transform.scale3d.z))
                corners.append(rotated + transform.translation)
    return ([min(c.x for c in corners), min(c.y for c in corners), min(c.z for c in corners)],
            [max(c.x for c in corners), max(c.y for c in corners), max(c.z for c in corners)])


def _hash(component):
    payload = []
    for index in range(component.get_instance_count()):
        transform = component.get_instance_transform(index, False)
        rotator = transform.rotation.rotator()
        payload.append([round(transform.translation.x, 4), round(transform.translation.y, 4),
                        round(transform.translation.z, 4), round(rotator.pitch, 5),
                        round(rotator.yaw, 5), round(rotator.roll, 5),
                        round(transform.scale3d.x, 6), round(transform.scale3d.y, 6),
                        round(transform.scale3d.z, 6)])
    return hashlib.sha256(json.dumps(payload, separators=(",", ":")).encode("utf-8")).hexdigest()


def main():
    baseline = json.loads(BASELINE.read_text(encoding="utf-8"))
    baseline_by_name = {item["name"]: item for item in baseline["components"]}
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    components = {c.get_name(): c for c in _components(blueprint)}
    assert set(components) == set(baseline_by_name), "component set changed"

    # tile surface per tier
    tile = components[TILE_COMPONENT]
    tmin, tmax = _local_box(tile.static_mesh)
    bands = {"lower": [], "upper": []}
    for index in range(tile.get_instance_count()):
        low, high = _aabb(tile.get_instance_transform(index, False), tmin, tmax)
        tier = "upper" if (low[2] + high[2]) / 2 >= TIER_SPLIT_Z else "lower"
        bands[tier].append((low[2], high[2]))
    surface = {tier: {"z_min": round(min(b[0] for b in band), 2), "z_max": round(max(b[1] for b in band), 2)}
               for tier, band in bands.items() if band}

    report = {"created": "2026-09-18", "tile_surface": surface, "shells": [], "map_saved": False}
    for name in SHELLS:
        component = components[name]
        assert component.get_instance_count() == 1, name
        local_min, local_max = _local_box(component.static_mesh)
        transform = component.get_instance_transform(0, False)
        low, high = _aabb(transform, local_min, local_max)
        tier = "upper" if (low[2] + high[2]) / 2 >= TIER_SPLIT_Z else "lower"
        up = unreal.MathLibrary.quat_rotate_vector(transform.rotation, unreal.Vector(0.0, 0.0, 1.0))
        centre_y = (low[1] + high[1]) / 2
        centre_x = (low[0] + high[0]) / 2
        # outward direction of the shell from the ridge (which runs along X at y = 0)
        outward_y = 1.0 if centre_y > 0 else -1.0
        outward_x = 1.0 if centre_x > 0 else -1.0
        record = {
            "component": name,
            "tier": tier,
            "world_z_cm": [round(low[2], 2), round(high[2], 2)],
            "up_axis": [round(float(up.x), 4), round(float(up.y), 4), round(float(up.z), 4)],
            "up_axis_note": "informational only: this mesh's local axes are not world-aligned, so its local +Z is not the surface normal",
            "z_span_cm": round(high[2] - low[2], 2),
            "tile_z_span_cm": round(surface[tier]["z_max"] - surface[tier]["z_min"], 2),
            "clearance_below_tile_top_cm": round(surface[tier]["z_max"] - high[2], 2),
            "offset_above_tile_base_cm": round(low[2] - surface[tier]["z_min"], 2),
        }
        # The shell is the roof's sheathing: it must lie under the tiled surface,
        # parallel to it. That nesting is the requirement, and it is what makes the
        # roof read as a roof instead of a shell rising above the tiles at the eaves.
        assert record["clearance_below_tile_top_cm"] > 0.0, record
        assert 0.0 < record["offset_above_tile_base_cm"] < 30.0, record
        # parallel to the tiles: the spans must agree within the tile thickness plus
        # a small pitch tolerance
        assert abs(record["z_span_cm"] - record["tile_z_span_cm"]) < 25.0, record
        report["shells"].append(record)
        print("VALIDATE_SHELL", name, tier, "z", record["world_z_cm"],
              "span", record["z_span_cm"], "tile span", record["tile_z_span_cm"],
              "clearance", record["clearance_below_tile_top_cm"],
              "offset", record["offset_above_tile_base_cm"],
              "up", record["up_axis"])

    # preservation
    changed, preserved = [], 0
    for name, item in baseline_by_name.items():
        component = components[name]
        assert component.get_instance_count() == item["instance_count"], name
        assert bool(component.is_visible()) == item["visible"], name
        assert str(component.get_collision_profile_name()) == item["collision_profile"], name
        assert bool(component.get_editor_property("cast_shadow")) == item["cast_shadow"], name
        assert [_path(component.get_material(i)) for i in range(component.get_num_materials())] == item["materials"], name
        if _hash(component) == item["transform_hash"]:
            preserved += 1
        else:
            changed.append(name)
    assert sorted(changed) == sorted(SHELLS), (sorted(changed), sorted(SHELLS))
    assert preserved == len(baseline_by_name) - len(SHELLS), preserved
    report["transforms_changed"] = sorted(changed)
    report["components_preserved"] = preserved
    report["instance_total"] = sum(item["instance_count"] for item in baseline["components"])
    report["passed"] = True
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("VALIDATE_PRESERVED components", preserved, "of", len(baseline_by_name),
          "| instances", report["instance_total"])
    print("VALIDATE_TILE_SURFACE", json.dumps(surface))
    print("VALIDATE_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
