"""Unified audit of every building under GuangzhouLandmarks.

The first pass only understood HierarchicalInstancedStaticMesh components, so the
three V3 Blueprints — which carry 102 / 32 / 50 plain StaticMeshComponents —
reported zero. This pass handles both, and fixes the roof-orientation metric to
use each roof group's own centre instead of the whole assembly's centre (the
assembly centre is skewed by ground aprons and wing walls, which made the metric
read ~0 on an asset whose roof is known to be correct).

Per building, per component group:

1. Component inventory — type, mesh, instance count, materials, visibility,
   collision, shadow, Nanite state, custom-data width.
2. Assembly and per-group bounds.
3. **Roof orientation**, measured within each roof group about that group's own
   centre: correlate each instance's top Z against its horizontal radius. Negative
   = rises to a ridge (expected); positive = dips to a valley (the defect that
   caught the Wuxianmen roof).
4. **Elongated outliers** — the longest instances per group against the group
   median, with the world axis they run along. This is the signature of a
   mis-axed ridge bar.
5. Role/mesh name disagreement.

Read-only. Writes Saved/RawModelImport/GuangzhouLandmarks-audit-v2-20260918.json
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/GuangzhouLandmarks-audit-v2-20260918.json"

BLUEPRINTS = [
    ("Zhengnanmen_HighFidelity", "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset"),
    ("Xiaobeimen_AAA_V3", "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3"),
    ("Xiaobeimen_Production_V3", "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3"),
    ("Zhengnanmen_AAA_V3", "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3"),
    ("Guidemen_V5_4K", "V5/Guidemen_4K/BP_Guidemen_V5_4K"),
    ("Wuxianmen_V5_4K_Core", "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"),
    ("Wuxianmen_V5_FullPBR", "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR"),
]
ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/"

ROOF_TOKENS = ("tile", "roof", "ridge", "wawu", "瓦", "脊")
DOOR_OR_VOID_TOKENS = ("door", "arch", "tunnel", "passage", "opening", "void", "門", "门")


def _path(value):
    return value.get_path_name() if value else ""


def _all_components(blueprint):
    """Every component in the Blueprint, whatever its type."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    seen, result = set(), []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = library.get_object(data)
        if obj is None:
            continue
        if not isinstance(obj, (unreal.StaticMeshComponent, unreal.HierarchicalInstancedStaticMeshComponent)):
            continue
        key = _path(obj)
        if key in seen:
            continue
        seen.add(key)
        result.append(obj)
    return result


def _local_box(mesh):
    bounds = mesh.get_bounds()
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def _instance_boxes(component):
    """World AABBs for every instance (HISM) or the single component transform."""
    mesh = component.static_mesh
    if not mesh:
        return []
    local_min, local_max = _local_box(mesh)
    boxes = []
    if isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent):
        for index in range(component.get_instance_count()):
            boxes.append(_aabb(component.get_instance_transform(index, False), local_min, local_max))
    else:
        boxes.append(_aabb(_component_transform(component), local_min, local_max))
    return boxes


def _component_transform(component):
    location = component.get_editor_property("relative_location")
    rotation = component.get_editor_property("relative_rotation")
    scale = component.get_editor_property("relative_scale3d")
    # relative_rotation comes back as a Rotator on StaticMeshComponent and as a
    # Quat on some other component types; normalise to a Rotator.
    rotator = rotation.rotator() if hasattr(rotation, "rotator") else rotation
    return unreal.Transform(
        unreal.Vector(location.x, location.y, location.z),
        unreal.Rotator(pitch=rotator.pitch, yaw=rotator.yaw, roll=rotator.roll),
        unreal.Vector(scale.x, scale.y, scale.z),
    )


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


def _correlation(pairs):
    if len(pairs) < 8:
        return None
    xs = [p[0] for p in pairs]
    ys = [p[1] for p in pairs]
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    num = sum((x - mx) * (y - my) for x, y in pairs)
    den = (sum((x - mx) ** 2 for x in xs) * sum((y - my) ** 2 for y in ys)) ** 0.5
    return round(num / den, 4) if den else None


def _is_roof(text):
    lowered = text.lower()
    return any(token in lowered for token in ROOF_TOKENS)


def _audit(label, relative):
    blueprint = unreal.EditorAssetLibrary.load_asset(ROOT + relative)
    if not isinstance(blueprint, unreal.Blueprint):
        return {"label": label, "asset": relative, "error": "not a Blueprint"}
    components = _all_components(blueprint)
    entry = {"label": label, "asset": ROOT + relative, "component_count": len(components),
             "hism_count": 0, "smc_count": 0, "instance_total": 0, "groups": []}
    all_boxes = []
    for component in components:
        is_hism = isinstance(component, unreal.HierarchicalInstancedStaticMeshComponent)
        entry["hism_count" if is_hism else "smc_count"] += 1
        mesh = component.static_mesh
        boxes = _instance_boxes(component)
        all_boxes.extend(boxes)
        count = component.get_instance_count() if is_hism else 1
        entry["instance_total"] += count
        nanite = mesh.get_editor_property("nanite_settings") if mesh else None
        extents = []
        for low, high in boxes:
            values = [high[a] - low[a] for a in range(3)]
            axis = int(max(range(3), key=lambda a: values[a]))
            extents.append({"extents_cm": [round(v, 2) for v in values], "long_axis": ["X", "Y", "Z"][axis],
                            "length_cm": round(values[axis], 2),
                            "low": [round(v, 2) for v in low], "high": [round(v, 2) for v in high]})
        lengths = sorted(item["length_cm"] for item in extents)
        median = lengths[len(lengths) // 2] if lengths else 0.0
        name_text = component.get_name() + " " + _path(mesh)
        record = {
            "name": component.get_name(),
            "type": "HISM" if is_hism else "StaticMeshComponent",
            "mesh": _path(mesh),
            "mesh_name": _path(mesh).split("/")[-1].split(".")[0] if mesh else "",
            "instance_count": count,
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "nanite_enabled": bool(nanite.get_editor_property("enabled")) if nanite else None,
            "nanite_relative_error": float(nanite.get_editor_property("fallback_relative_error")) if nanite else None,
            "custom_data_floats": int(component.get_editor_property("num_custom_data_floats")) if is_hism else 0,
            "median_length_cm": round(median, 2),
            "longest": sorted(extents, key=lambda item: -item["length_cm"])[:2],
            "roof_role": _is_roof(name_text),
            "door_or_void_role": any(token in name_text.lower() for token in DOOR_OR_VOID_TOKENS),
        }
        entry["groups"].append(record)

    if all_boxes:
        entry["assembly_low"] = [round(min(b[0][a] for b in all_boxes), 2) for a in range(3)]
        entry["assembly_high"] = [round(max(b[1][a] for b in all_boxes), 2) for a in range(3)]
        entry["assembly_extent_cm"] = [round(entry["assembly_high"][a] - entry["assembly_low"][a], 2)
                                       for a in range(3)]

    # roof orientation, measured within the roof groups about their own centre
    roof_boxes = []
    for record in entry["groups"]:
        if record["roof_role"]:
            roof_boxes.extend(_instance_boxes(next(
                c for c in components if c.get_name() == record["name"])))
    if len(roof_boxes) >= 8:
        centre_x = (min(b[0][0] for b in roof_boxes) + max(b[1][0] for b in roof_boxes)) / 2
        centre_y = (min(b[0][1] for b in roof_boxes) + max(b[1][1] for b in roof_boxes)) / 2
        pairs = []
        for low, high in roof_boxes:
            mid_x = (low[0] + high[0]) / 2
            mid_y = (low[1] + high[1]) / 2
            pairs.append((((mid_x - centre_x) ** 2 + (mid_y - centre_y) ** 2) ** 0.5, high[2]))
        corr = _correlation(pairs)
        near = [z for r, z in pairs if r < (max(p[0] for p in pairs) * 0.25)]
        far = [z for r, z in pairs if r > (max(p[0] for p in pairs) * 0.75)]
        entry["roof_orientation"] = {
            "roof_instances": len(roof_boxes),
            "correlation_radius_vs_top_z": corr,
            "mean_top_z_near_centre": round(sum(near) / len(near), 2) if near else None,
            "mean_top_z_far": round(sum(far) / len(far), 2) if far else None,
        }
        if corr is not None:
            if corr > 0.30:
                entry["roof_orientation"]["verdict"] = "POSITIVE — roof dips toward the centre (inverted)"
            elif corr < -0.30:
                entry["roof_orientation"]["verdict"] = "negative — roof rises to a ridge (expected)"
            else:
                entry["roof_orientation"]["verdict"] = "flat or multi-tier; inspect the group list"
    else:
        entry["roof_orientation"] = {"roof_instances": len(roof_boxes),
                                     "verdict": "no roof-role group identified by name"}
    return entry


def main():
    report = {"created": "2026-09-18", "buildings": []}
    for label, relative in BLUEPRINTS:
        entry = _audit(label, relative)
        report["buildings"].append(entry)
        print("AUDIT2", label, "components", entry.get("component_count"),
              "hism", entry.get("hism_count"), "smc", entry.get("smc_count"),
              "instances", entry.get("instance_total"), "extent", entry.get("assembly_extent_cm"))
        orientation = entry.get("roof_orientation", {})
        print("   roof:", orientation.get("roof_instances"), "instances | corr",
              orientation.get("correlation_radius_vs_top_z"),
              "| near", orientation.get("mean_top_z_near_centre"),
              "far", orientation.get("mean_top_z_far"),
              "|", orientation.get("verdict"))
        outliers = []
        for record in entry.get("groups", []):
            longest = record["longest"][0] if record["longest"] else None
            if not longest:
                continue
            if record["median_length_cm"] and longest["length_cm"] > 4 * max(record["median_length_cm"], 1.0) \
                    and longest["length_cm"] > 400:
                outliers.append((record["mesh_name"] or record["name"], record["instance_count"],
                                 record["median_length_cm"], longest["length_cm"], longest["long_axis"]))
        if outliers:
            print("   ELONGATED OUTLIERS:")
            for name, count, median, length, axis in outliers:
                print(f"      {name[:46]:48s} n={count:5d} median={median:8.1f} longest={length:9.1f} axis={axis}")
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("AUDIT2_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
