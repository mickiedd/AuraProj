"""Read Guidemen's roof surfaces by raycast — the decisive slope measurement.

Every indirect route failed: instance AABBs are sign-blind, the shell meshes are not
world-aligned, and vertex readback needs a VertexID that cannot be constructed from
Python. But every roof component is `BlockAll`, so a downward line trace onto a
spawned copy reports the actual top surface, and the HitResult names the component
that was hit — so the tile surface and each shell can be profiled separately in one
pass.

Reports, per component hit, the mean hit Z binned by |y| (the main slopes) and by |x|
(the hip ends). A ridge reads highest at the centre; a valley reads lowest.

Spawns a transient copy and destroys it. Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-raycast-profile-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"


def _path(value):
    return value.get_path_name() if value else ""


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    centre = unreal.Vector(100000.0, 100000.0, 0.0)
    spawned = actors.spawn_actor_from_class(blueprint.generated_class(), centre)
    assert spawned, "spawn failed"
    report = {"blueprint": BLUEPRINT, "hits": {}, "map_saved": False}
    try:
        origin, extent = spawned.get_actor_bounds(False)
        base = unreal.Vector(centre.x, centre.y, origin.z + extent.z + 2000.0)
        top = origin.z + extent.z + 1000.0
        rows = []
        for direction, label in ((1, "from_above"), (-1, "from_below")):
            for xi in range(-1500, 1501, 150):
                for yi in range(-600, 601, 75):
                    if direction > 0:
                        start = unreal.Vector(centre.x + xi, centre.y + yi, top)
                        end = unreal.Vector(centre.x + xi, centre.y + yi, origin.z - 500.0)
                    else:
                        start = unreal.Vector(centre.x + xi, centre.y + yi, origin.z - 500.0)
                        end = unreal.Vector(centre.x + xi, centre.y + yi, top)
                    hit = unreal.SystemLibrary.line_trace_single(
                        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                        unreal.DrawDebugTrace.NONE, True)
                    if not hit:
                        continue
                    location = hit.get_editor_property("location")
                    rows.append({"x": round(xi, 1), "y": round(yi, 1), "z": round(float(location.z), 2),
                                 "component": label})
        report["samples"] = len(rows)
        by_component = {}
        for row in rows:
            by_component.setdefault(row["component"], []).append(row)
        for name, items in sorted(by_component.items()):
            # name is the trace direction label: from_above = outer surface, from_below = underside
            def band(key, step):
                bands = {}
                for item in items:
                    b = int(round(abs(item[key]) / step)) * step
                    bands.setdefault(b, []).append(item["z"])
                return {str(int(b)): {"n": len(v), "mean_z": round(sum(v) / len(v), 2),
                                      "max_z": round(max(v), 2), "min_z": round(min(v), 2)}
                        for b, v in sorted(bands.items())}
            report["hits"][name] = {
                "samples": len(items),
                "z_range": [round(min(i["z"] for i in items), 2), round(max(i["z"] for i in items), 2)],
                "z_by_abs_y_step150": band("y", 150.0),
                "z_by_abs_x_step300": band("x", 300.0),
            }
    finally:
        name = spawned.get_name()
        actors.destroy_actor(spawned)
        leaked = [a.get_name() for a in actors.get_all_level_actors() if a.get_name() == name]
        assert not leaked, leaked
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    print("RAYCAST_SAMPLES", report["samples"])
    for name, entry in sorted(report["hits"].items(), key=lambda kv: -kv[1]["samples"]):
        print(f"RAYCAST {name[:44]:46s} n={entry['samples']:4d} z={entry['z_range']}")
        if entry["samples"] >= 20:
            print("        by |y|:", json.dumps({k: v["mean_z"] for k, v in entry["z_by_abs_y_step150"].items()}))
            print("        by |x|:", json.dumps({k: v["mean_z"] for k, v in entry["z_by_abs_x_step300"].items()}))
    print("RAYCAST_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
