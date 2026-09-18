"""Decide Guidemen's roof slope direction from the tile instances' own orientations.

There is no Guidemen source geometry on disk, and an instance AABB cannot express a
slope, so this reads the slope from the transforms. A roof tile strip lies flat on
the slope, so its local +Z axis is the surface normal. For a correct roof the normal
tilts away from the ridge: at y > 0 it leans toward +Y, at y < 0 toward -Y. For an
inverted roof it leans the other way. The hip ends are the same test on X.

Reports, per tier and per slope family: the correlation of the normal's horizontal
component against position, the mean tilt angle, and a verdict. Also reports each
roof shell's and ridge's world AABB with the Z at each end of its slope so the
ridge-to-surface relationship is explicit.

Read-only.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-roof-normal-probe-20260918.json"
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
    report = {"blueprint": BLUEPRINT, "tiles": {}, "shells_and_ridges": []}

    for component in _components(blueprint):
        mesh = component.static_mesh
        if not mesh:
            continue
        bounds = mesh.get_bounds()
        local_min = bounds.origin - bounds.box_extent
        local_max = bounds.origin + bounds.box_extent
        text = (component.get_name() + " " + _path(mesh)).lower()
        count = component.get_instance_count()
        is_tile = "t0" in text or "tile" in text
        if not is_tile:
            if "roofshell" in text or "ridge" in text:
                for index in range(count):
                    low, high = _aabb(component.get_instance_transform(index, False), local_min, local_max)
                    report["shells_and_ridges"].append({
                        "component": component.get_name(),
                        "index": index,
                        "mesh": _path(mesh).split("/")[-1].split(".")[0],
                        "world_low": [round(v, 2) for v in low],
                        "world_high": [round(v, 2) for v in high],
                        "extents_cm": [round(high[a] - low[a], 2) for a in range(3)],
                    })
            continue

        samples = []
        for index in range(count):
            transform = component.get_instance_transform(index, False)
            normal = unreal.MathLibrary.quat_rotate_vector(transform.rotation, unreal.Vector(0.0, 0.0, 1.0))
            low, high = _aabb(transform, local_min, local_max)
            samples.append({
                "index": index,
                "cx": (low[0] + high[0]) / 2,
                "cy": (low[1] + high[1]) / 2,
                "cz": (low[2] + high[2]) / 2,
                "nx": float(normal.x),
                "ny": float(normal.y),
                "nz": float(normal.z),
            })

        zs = sorted(item["cz"] for item in samples)
        threshold = (zs[len(zs) // 4] + zs[(len(zs) * 3) // 4]) / 2
        for tier, subset in (("lower", [s for s in samples if s["cz"] < threshold]),
                             ("upper", [s for s in samples if s["cz"] >= threshold])):
            if len(subset) < 8:
                continue
            # slope in Y: tiles away from the ridge should lean away from it
            far_y = [s for s in subset if abs(s["cy"]) > 100.0]
            tilt_y = [math.degrees(math.asin(max(-1.0, min(1.0, s["ny"]))))
                      * (1.0 if s["cy"] > 0 else -1.0) for s in far_y]
            # slope in X, for the hip ends
            far_x = [s for s in subset if abs(s["cx"]) > 400.0]
            tilt_x = [math.degrees(math.asin(max(-1.0, min(1.0, s["nx"]))))
                      * (1.0 if s["cx"] > 0 else -1.0) for s in far_x]
            entry = {
                "instances": len(subset),
                "centre_z_cm": round(sum(s["cz"] for s in subset) / len(subset), 2),
                "corr_normalY_vs_cy": _corr([(s["cy"], s["ny"]) for s in subset]),
                "corr_normalX_vs_cx": _corr([(s["nx"], s["cx"]) for s in subset]),
                "mean_outward_tilt_Y_deg": round(sum(tilt_y) / len(tilt_y), 2) if tilt_y else None,
                "mean_outward_tilt_X_deg": round(sum(tilt_x) / len(tilt_x), 2) if tilt_x else None,
                "samples_used_Y": len(tilt_y),
            }
            if entry["mean_outward_tilt_Y_deg"] is not None:
                if entry["mean_outward_tilt_Y_deg"] < -5.0:
                    entry["verdict_Y"] = "INVERTED — normals lean back toward the ridge"
                elif entry["mean_outward_tilt_Y_deg"] > 5.0:
                    entry["verdict_Y"] = "correct — normals lean outward, away from the ridge"
                else:
                    entry["verdict_Y"] = "near-flat in Y"
            report["tiles"].setdefault(component.get_name(), {})[tier] = entry

    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for name, tiers in report["tiles"].items():
        for tier, entry in tiers.items():
            print("GUIDEMEN_TILE", name, tier, json.dumps(entry))
    print("GUIDEMEN_SHELLS")
    for item in report["shells_and_ridges"]:
        print(f"    {item['mesh'][:32]:34s} ext {item['extents_cm']} "
              f"low {item['world_low']} high {item['world_high']}")
    print("GUIDEMEN_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
