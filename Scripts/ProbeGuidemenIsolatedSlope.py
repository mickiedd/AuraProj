"""Read each Guidemen roof shell's own surface slope, one shell at a time.

Every earlier attempt was confounded: the shared tile component carries both tiers, the
shell meshes are not world-aligned, and HitResult fields are protected. This isolates
exactly one surface by disabling collision on every other roof component, then
bisecting a line trace to find that surface's Z at a series of points along its slope.
The result is the shell's own profile, and comparing it against the tile surface's
profile settles the slope direction beyond argument.

Spawns a transient copy, restores every collision mode, destroys it. Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-isolated-slope-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"

ROOF_KEYS = ("R1_RoofShellBack", "R1_RoofShellFront", "R1_RoofShellLeft", "R1_RoofShellRight",
             "R2_RoofShellBack", "R2_RoofShellFront", "R2_RoofShellLeft", "R2_RoofShellRight",
             "R1_MainRidge", "R2_MainRidge", "R1_T0")
# sample offsets along the slope axis, per shell family
SAMPLES_Y = (30, 130, 230, 330, 430)
SAMPLES_X = (100, 400, 700, 1000, 1300)


def _components(actor):
    return {c.get_name(): c for c in actor.get_components_by_class(
        unreal.HierarchicalInstancedStaticMeshComponent)}


def _surface_z(world, x, y, z_low, z_high, iterations=18):
    if not unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, z_high), unreal.Vector(x, y, z_low),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True):
        return None
    lo, hi = z_low, z_high
    for _ in range(iterations):
        mid = (lo + hi) / 2.0
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, hi), unreal.Vector(x, y, mid),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        if hit:
            lo = mid
        else:
            hi = mid
    return round((lo + hi) / 2.0, 2)


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    centre = unreal.Vector(100000.0, 100000.0, 0.0)
    spawned = actors.spawn_actor_from_class(blueprint.generated_class(), centre)
    assert spawned, "spawn failed"
    report = {"blueprint": BLUEPRINT, "surfaces": {}, "map_saved": False}
    try:
        components = _components(spawned)
        keys = {}
        for needle in ROOF_KEYS:
            for name in components:
                if needle in name:
                    keys[needle] = name
                    break
        assert len(keys) == len(ROOF_KEYS), (sorted(keys), sorted(components))
        z_low, z_high = 1100.0, 2300.0

        for needle, component_name in keys.items():
            saved = {}
            for other in components.values():
                saved[other.get_name()] = other.get_collision_enabled()
                other.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            components[component_name].set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            try:
                if "Left" in needle or "Right" in needle:
                    points = [(centre.x + x, centre.y) for x in SAMPLES_X]
                    axis = "x"
                else:
                    points = [(centre.x, centre.y + y) for y in SAMPLES_Y]
                    axis = "y"
                if needle == "R1_T0":
                    points = [(centre.x + x, centre.y + y)
                              for y in (30, 130, 230, 330, 430, 480)
                              for x in (0, 400, 800, 1200)]
                    axis = "x,y"
                values = []
                for px, py in points:
                    values.append({"x": round(px - centre.x, 1), "y": round(py - centre.y, 1),
                                   "z": _surface_z(world, px, py, z_low, z_high)})
                report["surfaces"][needle] = {"axis": axis, "samples": values}
            finally:
                for name, mode in saved.items():
                    components[name].set_collision_enabled(mode)
    finally:
        name = spawned.get_name()
        actors.destroy_actor(spawned)
        leaked = [a.get_name() for a in actors.get_all_level_actors() if a.get_name() == name]
        assert not leaked, leaked
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    for needle, entry in report["surfaces"].items():
        values = [(s["x"], s["y"], s["z"]) for s in entry["samples"]]
        print(f"ISOLATED {needle:22s} axis={entry['axis']:4s} " +
              " ".join(f"({x:.0f},{y:.0f})={z}" for x, y, z in values))
    print("ISOLATED_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
