"""Per-tier roof probe on BP_Guidemen_V5_4K.

The earlier probe measured the top surface Z against distance from the whole roof
group's centre. On a two-tier roof that is confounded: the upper tier sits near the
centre and higher, the lower tier reaches further out and lower, so the aggregate
falls with radius even if each tier's own slope is inverted. This probe separates the
tiers and measures each one on its own.

For the tile component it reports, per tier:
  - the tier's own centre and bounds;
  - the correlation of each instance's top Z against its centre X and against its
    centre Y, which identifies the slope axis and its sign;
  - the top surface Z binned by offset along that axis from the tier's own centre.
For every roof shell and ridge it reports the world AABB, the Z at each end of the
long axis, and which way that end-to-end Z runs.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-roof-probe-pertier-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"


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


def _corr(pairs):
    if len(pairs) < 8:
        return None
    xs = [p[0] for p in pairs]
    ys = [p[1] for p in pairs]
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    num = sum((x - mx) * (y - my) for x, y in pairs)
    den = (sum((x - mx) ** 2 for x in xs) * sum((y - my) ** 2 for y in ys)) ** 0.5
    return round(num / den, 4) if den else None


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    report = {"blueprint": BLUEPRINT, "shells_and_ridges": [], "tiers": []}

    tile_instances = []
    for component in _components(blueprint):
        mesh = component.static_mesh
        if not mesh:
            continue
        bounds = mesh.get_bounds()
        local_min = bounds.origin - bounds.box_extent
        local_max = bounds.origin + bounds.box_extent
        name = component.get_name()
        text = (name + " " + _path(mesh)).lower()
        is_tile = "t0" in text or "tile" in text
        is_shell_or_ridge = ("roofshell" in text or "ridge" in text) and not is_tile
        count = component.get_instance_count()

        if is_shell_or_ridge:
            for index in range(count):
                low, high = _aabb(component.get_instance_transform(index, False), local_min, local_max)
                extents = [high[a] - low[a] for a in range(3)]
                axis = int(max(range(3), key=lambda a: extents[a]))
                report["shells_and_ridges"].append({
                    "component": name,
                    "index": index,
                    "mesh": _path(mesh),
                    "extents_cm": [round(v, 2) for v in extents],
                    "long_axis": ["X", "Y", "Z"][axis],
                    "world_low": [round(v, 2) for v in low],
                    "world_high": [round(v, 2) for v in high],
                    "z_range": [round(low[2], 2), round(high[2], 2)],
                })
        elif is_tile:
            for index in range(count):
                transform = component.get_instance_transform(index, False)
                low, high = _aabb(transform, local_min, local_max)
                tile_instances.append({
                    "index": index,
                    "cx": (low[0] + high[0]) / 2,
                    "cy": (low[1] + high[1]) / 2,
                    "top": high[2],
                    "bottom": low[2],
                    "low": low,
                    "high": high,
                })

    # split the tile instances into tiers by their own centre Z using a 2-means split
    if tile_instances:
        zs = sorted((item["bottom"] + item["top"]) / 2 for item in tile_instances)
        low_split = zs[len(zs) // 4]
        high_split = zs[(len(zs) * 3) // 4]
        threshold = (low_split + high_split) / 2
        tiers = {}
        for item in tile_instances:
            key = "upper" if (item["bottom"] + item["top"]) / 2 >= threshold else "lower"
            tiers.setdefault(key, []).append(item)
        report["tier_split_z_cm"] = round(threshold, 2)
        for label, items in sorted(tiers.items()):
            xs = [item["cx"] for item in items]
            ys = [item["cy"] for item in items]
            centre_x = (min(xs) + max(xs)) / 2
            centre_y = (min(ys) + max(ys)) / 2
            span_x = max(xs) - min(xs)
            span_y = max(ys) - min(ys)
            entry = {
                "tier": label,
                "instances": len(items),
                "centre": [round(centre_x, 2), round(centre_y, 2)],
                "span_cm": [round(span_x, 2), round(span_y, 2)],
                "corr_topZ_vs_centreX": _corr([(item["cx"], item["top"]) for item in items]),
                "corr_topZ_vs_centreY": _corr([(item["cy"], item["top"]) for item in items]),
                "top_z_range_cm": [round(min(item["top"] for item in items), 2),
                                   round(max(item["top"] for item in items), 2)],
            }
            # profile along the axis with the stronger correlation
            for axis, key, centre in (("X", "cx", centre_x), ("Y", "cy", centre_y)):
                corr = entry["corr_topZ_vs_centreX" if axis == "X" else "corr_topZ_vs_centreY"]
                if corr is None or abs(corr) < 0.2:
                    continue
                bands = {}
                for item in items:
                    offset = item[key] - centre
                    band = int(round(abs(offset) / 150.0)) * 150
                    bands.setdefault(band, []).append(item["top"])
                entry[f"top_z_by_abs_offset_{axis}"] = {
                    str(band): round(max(values), 2) for band, values in sorted(bands.items())
                }
            report["tiers"].append(entry)

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("GUIDEMEN_TIER_SPLIT_Z", report.get("tier_split_z_cm"))
    for entry in report["tiers"]:
        print("GUIDEMEN_TIER", entry["tier"], "n", entry["instances"],
              "centre", entry["centre"], "span", entry["span_cm"],
              "corrX", entry["corr_topZ_vs_centreX"], "corrY", entry["corr_topZ_vs_centreY"],
              "topZ", entry["top_z_range_cm"])
        for axis in ("X", "Y"):
            key = f"top_z_by_abs_offset_{axis}"
            if key in entry:
                print(f"    topZ by |{axis} offset|:", json.dumps(entry[key]))
    print("GUIDEMEN_SHELLS")
    for item in sorted(report["shells_and_ridges"], key=lambda r: (r["component"], r["index"])):
        print(f"    {item['mesh'][:34]:36s} ext {item['extents_cm']} axis {item['long_axis']} z {item['z_range']}")
    print("GUIDEMEN_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
