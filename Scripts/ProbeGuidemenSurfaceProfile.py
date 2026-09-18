"""Measure Guidemen's roof surface profile along the main slopes, per tier.

Every previous attempt to read this roof's slope was confounded or indirect. This
does the one thing that cannot be argued with: take the tile instances that lie in
the central X band (so only the two main front/back slopes are sampled, not the hip
ends), bin them by distance from the ridge line at y = 0, and report the tile top Z
per bin. A ridge shows the centre bin highest; a valley shows it lowest.

Also reports the ridge bars' own Z against the centre bin, so it is clear whether the
bar caps the highest line or floats above a valley, and the same profile restricted to
the central Y band for the hip ends along X.

Read-only.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-surface-profile-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
TILE_COMPONENT = "HISM_029_R1_T0_GEN_VARIABLE"
RIDGES = ("HISM_024_R1_MainRidge_GEN_VARIABLE", "HISM_030_R2_MainRidge_GEN_VARIABLE")
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


def _profile(items, key, band_cm=100.0):
    bands = {}
    for item in items:
        band = int(round(abs(item[key]) / band_cm)) * band_cm
        bands.setdefault(band, []).append(item["top"])
    return {str(int(band)): {"n": len(values), "mean_top": round(sum(values) / len(values), 2),
                             "max_top": round(max(values), 2)}
            for band, values in sorted(bands.items())}


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    components = {c.get_name(): c for c in _components(blueprint)}
    tile = components[TILE_COMPONENT]
    tmin, tmax = _local_box(tile.static_mesh)
    rows = []
    for index in range(tile.get_instance_count()):
        transform = tile.get_instance_transform(index, False)
        low, high = _aabb(transform, tmin, tmax)
        rows.append({"cx": (low[0] + high[0]) / 2, "cy": (low[1] + high[1]) / 2,
                     "cz": (low[2] + high[2]) / 2, "top": high[2], "bottom": low[2]})

    report = {"blueprint": BLUEPRINT, "tiers": {}}
    for tier, subset in (("lower", [r for r in rows if r["cz"] < TIER_SPLIT_Z]),
                         ("upper", [r for r in rows if r["cz"] >= TIER_SPLIT_Z])):
        central_x = [r for r in subset if abs(r["cx"]) < 800.0]
        central_y = [r for r in subset if abs(r["cy"]) < 200.0]
        report["tiers"][tier] = {
            "instances": len(subset),
            "main_slope_band_instances": len(central_x),
            "top_z_by_abs_y__central_x_band": _profile(central_x, "cy"),
            "top_z_by_abs_x__central_y_band": _profile(central_y, "cx"),
        }

    report["ridges"] = []
    for name in RIDGES:
        component = components.get(name)
        if not component:
            continue
        lmin, lmax = _local_box(component.static_mesh)
        low, high = _aabb(component.get_instance_transform(0, False), lmin, lmax)
        report["ridges"].append({
            "component": name,
            "world_low": [round(v, 2) for v in low],
            "world_high": [round(v, 2) for v in high],
            "z_cm": [round(low[2], 2), round(high[2], 2)],
        })
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    for tier, entry in report["tiers"].items():
        print(f"PROFILE {tier} main-slope band n={entry['main_slope_band_instances']}")
        for band, values in entry["top_z_by_abs_y__central_x_band"].items():
            print(f"    |y|={band:>5s}  n={values['n']:5d}  mean_top={values['mean_top']:8.2f}  max_top={values['max_top']:8.2f}")
        print(f"    hip ends (|y|<200), top Z by |x|:")
        for band, values in entry["top_z_by_abs_x__central_y_band"].items():
            print(f"    |x|={band:>5s}  n={values['n']:5d}  mean_top={values['mean_top']:8.2f}  max_top={values['max_top']:8.2f}")
    for item in report["ridges"]:
        print("RIDGE", item["component"], "z", item["z_cm"], "y", [item["world_low"][1], item["world_high"][1]])
    print("PROFILE_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
