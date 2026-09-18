"""Fix the inverted roof shells on BP_Guidemen_V5_4K.

Diagnosis (all measured, no guessing):

  tier   shell Z now      ridge bar        tile surface     shell reflected about its min Z
  R1     1462 .. 1634     1472 .. 1500     1292 .. 1472     1290 .. 1462
  R2     1855 .. 2002     1864.5 .. 1891.5 1710 .. 1865     1708 .. 1855

Each ridge bar sits exactly on its tier's tile apex, and the tile surface rises to
that apex, so the tiles and the ridges agree. The eight roof shells do not: their Z
range sits above the tiled surface, reaching 342 cm above the tiles at the eaves,
which is the inversion that is visible in the editor.

Reflecting each shell about the horizontal plane at its own minimum Z — its
ridge-side edge — makes it sit 2 cm under the tiled surface, parallel to it, on both
tiers. A 2 cm clearance on both tiers is the authored relationship, so the shells are
flipped in Z about their ridge-side edge rather than merely mispositioned.

Repair: apply a 180 degree rotation about the world X axis through the pivot
(y = 0, z = the shell's own minimum Z). A rotation rather than a Z mirror, so
triangle winding and shading normals stay valid. Both the front/back pair and the
left/right pair are mirror-symmetric about y = 0, so the rotation's incidental y
flip maps each shell onto its partner's footprint exactly, and each shell's slope in
X (the hip ends) is flipped without moving it.

The tiles and both ridge bars are left untouched: they are the surfaces that agree
with each other.

Also records the 12 roof-ornament (`Beast_0_-1_0`) instances' positions so the
reviewer can see whether they need reseating onto the corrected surface.

Never saves a map.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport"
BASELINE = ROOT / "Guidemen_V5_4K-roofshell-baseline-20260918.json"
REPORT = ROOT / "Guidemen_V5_4K-roofshell-fix-20260918.json"

BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
TILE_COMPONENT = "HISM_029_R1_T0_GEN_VARIABLE"
SHELL_COMPONENTS = [
    "HISM_025_R1_RoofShellBack_GEN_VARIABLE",
    "HISM_026_R1_RoofShellFront_GEN_VARIABLE",
    "HISM_027_R1_RoofShellLeft_GEN_VARIABLE",
    "HISM_028_R1_RoofShellRight_GEN_VARIABLE",
    "HISM_031_R2_RoofShellBack_GEN_VARIABLE",
    "HISM_032_R2_RoofShellFront_GEN_VARIABLE",
    "HISM_033_R2_RoofShellLeft_GEN_VARIABLE",
    "HISM_034_R2_RoofShellRight_GEN_VARIABLE",
]
ORNAMENT_COMPONENT = "HISM_008_Beast_0__1_0_GEN_VARIABLE"
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


def _transform_hash(component):
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
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = {c.get_name(): c for c in _components(blueprint)}
    for name in SHELL_COMPONENTS + [TILE_COMPONENT]:
        assert name in components, name

    # ---- baseline ----
    baseline = {"created": "2026-09-18", "blueprint": BLUEPRINT, "components": []}
    for name, component in components.items():
        baseline["components"].append({
            "name": name,
            "instance_count": component.get_instance_count(),
            "transform_hash": _transform_hash(component),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
        })
    BASELINE.write_text(json.dumps(baseline, indent=2), encoding="utf-8")

    # ---- tile surface per tier, for the verification target ----
    tile = components[TILE_COMPONENT]
    tile_min, tile_max = _local_box(tile.static_mesh)
    tile_bands = {"lower": [], "upper": []}
    for index in range(tile.get_instance_count()):
        low, high = _aabb(tile.get_instance_transform(index, False), tile_min, tile_max)
        tier = "upper" if (low[2] + high[2]) / 2 >= TIER_SPLIT_Z else "lower"
        tile_bands[tier].append((low[2], high[2]))
    tile_surface = {
        tier: {"z_min": round(min(b[0] for b in band), 2), "z_max": round(max(b[1] for b in band), 2),
               "instances": len(band)}
        for tier, band in tile_bands.items() if band
    }

    report = {"created": "2026-09-18", "blueprint": BLUEPRINT,
              "tile_surface": tile_surface, "shells": [], "ornaments": None, "map_saved": False}

    r180 = unreal.MathLibrary.rotator_from_axis_and_angle(unreal.Vector(1.0, 0.0, 0.0), 180.0)

    for name in SHELL_COMPONENTS:
        component = components[name]
        assert component.get_instance_count() == 1, (name, component.get_instance_count())
        local_min, local_max = _local_box(component.static_mesh)
        old = component.get_instance_transform(0, False)
        old_low, old_high = _aabb(old, local_min, local_max)
        pivot_z = old_low[2]
        tier = "upper" if (old_low[2] + old_high[2]) / 2 >= TIER_SPLIT_Z else "lower"
        new = unreal.Transform(
            unreal.Vector(old.translation.x, -old.translation.y, 2.0 * pivot_z - old.translation.z),
            unreal.MathLibrary.quat_rotator(unreal.MathLibrary.multiply_quat_quat(
                unreal.MathLibrary.conv_rotator_to_quaternion(r180), old.rotation)),
            old.scale3d,
        )
        component.update_instance_transform(0, new, False)
        new_low, new_high = _aabb(new, local_min, local_max)
        surface = tile_surface.get(tier)
        record = {
            "component": name,
            "tier": tier,
            "pivot_z_cm": round(pivot_z, 2),
            "before_z_cm": [round(old_low[2], 2), round(old_high[2], 2)],
            "after_z_cm": [round(new_low[2], 2), round(new_high[2], 2)],
            "before_y_cm": [round(old_low[1], 2), round(old_high[1], 2)],
            "after_y_cm": [round(new_low[1], 2), round(new_high[1], 2)],
            "before_x_cm": [round(old_low[0], 2), round(old_high[0], 2)],
            "after_x_cm": [round(new_low[0], 2), round(new_high[0], 2)],
            "tile_surface": surface,
            "clearance_below_tile_top_cm": round(surface["z_max"] - new_high[2], 2) if surface else None,
            "clearance_above_tile_base_cm": round(new_low[2] - surface["z_min"], 2) if surface else None,
        }
        # the corrected shell must sit under the tiled surface, parallel to it
        if surface:
            assert new_high[2] < surface["z_max"], record
            assert abs(new_low[2] - surface["z_min"]) < 60.0, record
        # the footprint must be preserved
        assert abs((new_high[0] - new_low[0]) - (old_high[0] - old_low[0])) < 1.0, record
        assert abs((new_high[1] - new_low[1]) - (old_high[1] - old_low[1])) < 1.0, record
        report["shells"].append(record)
        print("ROOFSHELL_FIX", name, tier,
              "z", record["before_z_cm"], "->", record["after_z_cm"],
              "tile", [surface["z_min"], surface["z_max"]] if surface else None,
              "clearance", record["clearance_below_tile_top_cm"])

    # ---- ornaments, measured not moved ----
    ornament = components.get(ORNAMENT_COMPONENT)
    if ornament:
        local_min, local_max = _local_box(ornament.static_mesh)
        rows = []
        for index in range(ornament.get_instance_count()):
            low, high = _aabb(ornament.get_instance_transform(index, False), local_min, local_max)
            rows.append({"index": index, "x": round((low[0] + high[0]) / 2, 2),
                         "y": round((low[1] + high[1]) / 2, 2),
                         "z": [round(low[2], 2), round(high[2], 2)]})
        report["ornaments"] = {"component": ORNAMENT_COMPONENT, "instances": rows}

    # ---- invariants ----
    changed = set(SHELL_COMPONENTS)
    for item in baseline["components"]:
        component = components[item["name"]]
        assert component.get_instance_count() == item["instance_count"], item["name"]
        assert bool(component.is_visible()) == item["visible"], item["name"]
        assert str(component.get_collision_profile_name()) == item["collision_profile"], item["name"]
        assert bool(component.get_editor_property("cast_shadow")) == item["cast_shadow"], item["name"]
        digest = _transform_hash(component)
        if item["name"] in changed:
            assert digest != item["transform_hash"], (item["name"], "shell transform did not change")
        else:
            assert digest == item["transform_hash"], (item["name"], "unexpected transform change")
    report["instance_total"] = sum(item["instance_count"] for item in baseline["components"])
    report["transforms_changed"] = sorted(changed)
    report["transforms_preserved"] = report["instance_total"] - len(changed)
    REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    print("ROOFSHELL_FIX_COUNTS instances", report["instance_total"],
          "changed", len(changed), "preserved", report["transforms_preserved"])
    print("ROOFSHELL_FIX_ORN", json.dumps(report["ornaments"]))
    print("ROOFSHELL_FIX_REPORT", REPORT)


if __name__ == "__main__":
    main()
