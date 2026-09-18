"""Audit every building Blueprint under GuangzhouLandmarks for the defect
signatures found on Wuxianmen.

Per Blueprint, and without mutating anything:

1. Component inventory — mesh, instance count, materials, visibility, collision,
   shadow, Nanite state and per-instance custom data width.
2. Assembly bounds and per-component bounds.
3. **Roof orientation.** For every roof-role component (mesh or name containing
   tile/roof/ridge/wawu/wa), correlate each instance's top Z against its horizontal
   distance from the assembly centre. A roof that rises to a ridge gives a negative
   correlation; a roof that dips to a valley gives a positive one. This is the
   signature that caught the inverted Wuxianmen roof.
4. **Elongated instances.** The three longest instances per component, with the
   world axis they run along and how far they exceed the component median. This is
   the signature that caught the mis-axed Wuxianmen ridge bars, which ran along the
   assembly's short axis and projected past the eaves.
5. **Role/mesh name agreement.** A component whose name says one role but whose
   mesh says another is the signature that caught `HISM_004_ridge_*` being bound to
   a roof-tile mesh.

Read-only: loads assets, never saves a package, never saves a map.
Writes Saved/RawModelImport/GuangzhouLandmarks-inventory-audit-20260918.json
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/GuangzhouLandmarks-inventory-audit-20260918.json"

BLUEPRINTS = [
    "GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset",
    "V3/Xiaobeimen_AAA_V3/BP_Xiaobeimen_AAA_V3",
    "V3/Xiaobeimen_Production_V3/BP_Xiaobeimen_Production_V3",
    "V3/Zhengnanmen_AAA_V3/BP_Zhengnanmen_AAA_V3",
    "V5/Guidemen_4K/BP_Guidemen_V5_4K",
    "V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
    "V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
]
ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/"

ROOF_TOKENS = ("tile", "roof", "ridge", "wa_", "_wa", "wawu", "瓦")
ROLE_TOKENS = {
    "ridge": ("ridge", "脊", "zhengji"),
    "tile": ("tile", "roof", "瓦"),
    "stone": ("stone", "brick", "wall", "masonry", "石"),
    "wood": ("wood", "timber", "column", "beam", "door", "railing", "木"),
    "plaster": ("plaster", "wall_panel", "灰"),
    "iron": ("iron", "metal", "stud", "nail", "铁"),
    "plaque": ("plaque", "sign", "board", "匾", "額"),
}


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


def _local_box(mesh):
    bounds = mesh.get_bounds()
    return bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent


def _role_of(text):
    lowered = text.lower()
    for role, tokens in ROLE_TOKENS.items():
        if any(token in lowered for token in tokens):
            return role
    return "unknown"


def _correlation(pairs):
    if len(pairs) < 8:
        return None
    xs = [p[0] for p in pairs]
    ys = [p[1] for p in pairs]
    mx = sum(xs) / len(xs)
    my = sum(ys) / len(ys)
    num = sum((x - mx) * (y - my) for x, y in pairs)
    den = (sum((x - mx) ** 2 for x in xs) * sum((y - my) ** 2 for y in ys)) ** 0.5
    return round(num / den, 4) if den else None


def _audit(relative_path):
    asset_path = ROOT + relative_path
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(blueprint, unreal.Blueprint):
        return {"asset": asset_path, "error": "not a Blueprint"}
    components = _components(blueprint)
    entry = {"asset": asset_path, "component_count": len(components), "components": [],
             "instance_total": 0}
    boxes = []
    for component in components:
        mesh = component.static_mesh
        if not mesh:
            entry["components"].append({"name": component.get_name(), "error": "no static mesh"})
            continue
        local_min, local_max = _local_box(mesh)
        nanite = mesh.get_editor_property("nanite_settings")
        instances = []
        for index in range(component.get_instance_count()):
            transform = component.get_instance_transform(index, False)
            low, high = _aabb(transform, local_min, local_max)
            instances.append({"index": index, "low": low, "high": high})
            boxes.append((low, high))
        longest = []
        for item in instances:
            extents = [item["high"][axis] - item["low"][axis] for axis in range(3)]
            axis = int(max(range(3), key=lambda a: extents[a]))
            longest.append({
                "index": item["index"],
                "extents_cm": [round(value, 2) for value in extents],
                "long_axis": ["X", "Y", "Z"][axis],
                "length_cm": round(extents[axis], 2),
                "low": [round(value, 2) for value in item["low"]],
                "high": [round(value, 2) for value in item["high"]],
            })
        longest.sort(key=lambda item: -item["length_cm"])
        medians = sorted(item["length_cm"] for item in longest)
        median = medians[len(medians) // 2] if medians else 0.0
        record = {
            "name": component.get_name(),
            "mesh": _path(mesh),
            "mesh_name": _path(mesh).split("/")[-1].split(".")[0],
            "role_from_name": _role_of(component.get_name()),
            "role_from_mesh": _role_of(_path(mesh).split("/")[-1]),
            "instance_count": component.get_instance_count(),
            "materials": [_path(component.get_material(i)) for i in range(component.get_num_materials())],
            "visible": bool(component.is_visible()),
            "collision_profile": str(component.get_collision_profile_name()),
            "cast_shadow": bool(component.get_editor_property("cast_shadow")),
            "nanite_enabled": bool(nanite.get_editor_property("enabled")),
            "nanite_relative_error": float(nanite.get_editor_property("fallback_relative_error")),
            "nanite_percent_triangles": float(nanite.get_property("fallback_percent_triangles"))
            if hasattr(nanite, "get_property") else None,
            "custom_data_floats": int(component.get_editor_property("num_custom_data_floats")),
            "median_instance_length_cm": round(median, 2),
            "longest_instances": longest[:3],
            "roof_role": any(token in (component.get_name() + _path(mesh)).lower() for token in ROOF_TOKENS),
            "instances": instances if component.get_instance_count() <= 600 else None,
        }
        entry["components"].append(record)
        entry["instance_total"] += component.get_instance_count()

    if boxes:
        entry["assembly_low"] = [round(min(b[0][a] for b in boxes), 2) for a in range(3)]
        entry["assembly_high"] = [round(max(b[1][a] for b in boxes), 2) for a in range(3)]
        entry["assembly_extent_cm"] = [round(entry["assembly_high"][a] - entry["assembly_low"][a], 2)
                                       for a in range(3)]
        centre_x = (entry["assembly_low"][0] + entry["assembly_high"][0]) / 2
        centre_y = (entry["assembly_low"][1] + entry["assembly_high"][1]) / 2
        z_low, z_high = entry["assembly_low"][2], entry["assembly_high"][2]
        roof_pairs, all_pairs = [], []
        for component in entry["components"]:
            if not component.get("instances"):
                continue
            for item in component["instances"]:
                mid_x = (item["low"][0] + item["high"][0]) / 2
                mid_y = (item["low"][1] + item["high"][1]) / 2
                radius = ((mid_x - centre_x) ** 2 + (mid_y - centre_y) ** 2) ** 0.5
                top = item["high"][2]
                all_pairs.append((radius, top))
                if component.get("roof_role"):
                    roof_pairs.append((radius, top))
        entry["roof_orientation"] = {
            "roof_role_instances": len(roof_pairs),
            "roof_correlation_radius_vs_top_z": _correlation(roof_pairs),
            "all_correlation_radius_vs_top_z": _correlation(all_pairs),
            "verdict_hint": None,
        }
        corr = entry["roof_orientation"]["roof_correlation_radius_vs_top_z"]
        if corr is not None:
            if corr > 0.25:
                entry["roof_orientation"]["verdict_hint"] = "POSITIVE — roof may rise toward the eaves (inverted)"
            elif corr < -0.25:
                entry["roof_orientation"]["verdict_hint"] = "negative — roof rises toward the centre (expected)"
            else:
                entry["roof_orientation"]["verdict_hint"] = "flat / ambiguous"

    # role/mesh name agreement
    mismatches = []
    for component in entry["components"]:
        if "role_from_name" not in component:
            continue
        if component["role_from_name"] != "unknown" and component["role_from_mesh"] != "unknown" \
                and component["role_from_name"] != component["role_from_mesh"]:
            mismatches.append({"component": component["name"], "name_role": component["role_from_name"],
                               "mesh_role": component["role_from_mesh"], "mesh": component["mesh_name"]})
    entry["role_mesh_mismatches"] = mismatches
    return entry


def main():
    report = {"created": "2026-09-18", "buildings": []}
    for relative in BLUEPRINTS:
        entry = _audit(relative)
        report["buildings"].append(entry)
        if entry.get("error"):
            print("AUDIT", relative, "ERROR", entry["error"])
            continue
        print("AUDIT", relative)
        print("   components", entry["component_count"], "instances", entry["instance_total"],
              "extent_cm", entry.get("assembly_extent_cm"))
        orientation = entry.get("roof_orientation", {})
        print("   roof: role_instances", orientation.get("roof_role_instances"),
              "corr", orientation.get("roof_correlation_radius_vs_top_z"),
              "->", orientation.get("verdict_hint"))
        if entry.get("role_mesh_mismatches"):
            print("   ROLE/MESH MISMATCH", json.dumps(entry["role_mesh_mismatches"]))
        for component in entry["components"]:
            if "longest_instances" not in component:
                continue
            top = component["longest_instances"][0]
            flag = ""
            if component["median_instance_length_cm"] and top["length_cm"] > 4 * max(
                    component["median_instance_length_cm"], 1.0) and top["length_cm"] > 300:
                flag = "  <-- ELONGATED OUTLIER"
            print(f"      {component['name'][:52]:54s} n={component['instance_count']:5d} "
                  f"median={component['median_instance_length_cm']:8.1f} longest={top['length_cm']:9.1f} "
                  f"axis={top['long_axis']}{flag}")
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("AUDIT_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
