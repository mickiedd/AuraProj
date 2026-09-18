"""Focused roof probe on BP_Guidemen_V5_4K.

The generic audit flagged Guidemen's roof as inverted: across the 11 name-matched
roof-shell and ridge instances, the perimeter top Z (1818 cm) sits 348 cm above the
centre top Z (1470.5 cm). That metric is noisy because it only saw 11 large
single-instance shells, so this probe measures the roof surface directly from the
tile instances instead:

- group the tile component's instances by their own centre, and report the top
  surface Z as a function of distance from the roof group's centre;
- report the same profile split by world axis, so a ridge along X and slopes in Y
  can be told apart from a ridge along Y;
- report each roof shell and ridge instance's AABB, long axis and Z range, so a
  mis-axed ridge bar is visible directly.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-roof-probe-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"

ROOF_HINT = ("tile", "roof", "ridge", "t0", "r1_", "r2_", "shell")


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


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    components = _components(blueprint)
    report = {"blueprint": BLUEPRINT, "components": []}

    tile_groups = []
    for component in components:
        mesh = component.static_mesh
        if not mesh:
            continue
        bounds = mesh.get_bounds()
        local_min = bounds.origin - bounds.box_extent
        local_max = bounds.origin + bounds.box_extent
        name_text = (component.get_name() + " " + _path(mesh)).lower()
        boxes = []
        for index in range(component.get_instance_count()):
            boxes.append(_aabb(component.get_instance_transform(index, False), local_min, local_max))
        if not boxes:
            continue
        lows = [min(b[0][a] for b in boxes) for a in range(3)]
        highs = [max(b[1][a] for b in boxes) for a in range(3)]
        record = {
            "name": component.get_name(),
            "mesh": _path(mesh),
            "instance_count": component.get_instance_count(),
            "world_low": [round(v, 2) for v in lows],
            "world_high": [round(v, 2) for v in highs],
            "world_extent": [round(highs[a] - lows[a], 2) for a in range(3)],
            "roof_hint": any(token in name_text for token in ROOF_HINT),
        }
        report["components"].append(record)
        if any(token in name_text for token in ("tile", "t0", "roof", "ridge", "shell")):
            tile_groups.append((record, boxes))

    # roof surface profile: use every instance whose centre is in the roof Z band
    all_boxes = [b for _, boxes in tile_groups for b in boxes]
    if all_boxes:
        z_high = max(b[1][2] for b in all_boxes)
        centre_x = (min(b[0][0] for b in all_boxes) + max(b[1][0] for b in all_boxes)) / 2
        centre_y = (min(b[0][1] for b in all_boxes) + max(b[1][1] for b in all_boxes)) / 2
        report["roof_group_bounds"] = {
            "low": [round(min(b[0][a] for b in all_boxes), 2) for a in range(3)],
            "high": [round(max(b[1][a] for b in all_boxes), 2) for a in range(3)],
            "centre_x": round(centre_x, 2),
            "centre_y": round(centre_y, 2),
        }
        # profile along each axis: max top Z per band of |offset| from the centre
        for axis, label in ((0, "X"), (1, "Y")):
            bands = {}
            for low, high in all_boxes:
                mid = (low[axis] + high[axis]) / 2
                offset = mid - (centre_x if axis == 0 else centre_y)
                key = round(abs(offset) / 200.0) * 200.0
                bands.setdefault(key, []).append(high[2])
            report[f"top_z_by_abs_offset_{label}"] = {
                str(int(key)): round(max(values), 2) for key, values in sorted(bands.items())
            }
        report["ridge_instances"] = [
            component for component in report["components"]
            if "ridge" in (component["name"] + component["mesh"]).lower()
        ]
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("GUIDEMEN_ROOF_BOUNDS", json.dumps(report.get("roof_group_bounds")))
    for label in ("X", "Y"):
        print(f"GUIDEMEN_TOP_Z_BY_OFFSET_{label}", json.dumps(report.get(f"top_z_by_abs_offset_{label}")))
    print("GUIDEMEN_RIDGES", json.dumps(report.get("ridge_instances")))
    print("GUIDEMEN_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
