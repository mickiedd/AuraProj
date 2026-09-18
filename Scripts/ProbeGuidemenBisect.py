"""Measure Guidemen's shell and tile surfaces by binary-searching a line trace.

`HitResult`'s fields are protected in this build, so the trace's hit location cannot
be read. But the trace's boolean can: to find a surface's Z at a given (x, y), trace
downward from a high point to a mid point and bisect. Fourteen iterations resolve the
surface to well under a centimetre, using nothing but the boolean.

The shell surface is isolated by hiding the tile component for the shell pass and vice
versa. That gives each surface's profile along the slope, which is the measurement
every indirect route failed to make.

Spawns a transient copy, restores visibility, destroys it. Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
OUTPUT = PROJECT / "Saved/RawModelImport/Guidemen_V5_4K-bisect-profile-20260918.json"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
TILE_COMPONENT = "HISM_029_R1_T0_GEN_VARIABLE"
SHELL_COMPONENTS = [
    "HISM_025_R1_RoofShellBack_GEN_VARIABLE", "HISM_026_R1_RoofShellFront_GEN_VARIABLE",
    "HISM_027_R1_RoofShellLeft_GEN_VARIABLE", "HISM_028_R1_RoofShellRight_GEN_VARIABLE",
    "HISM_031_R2_RoofShellBack_GEN_VARIABLE", "HISM_032_R2_RoofShellFront_GEN_VARIABLE",
    "HISM_033_R2_RoofShellLeft_GEN_VARIABLE", "HISM_034_R2_RoofShellRight_GEN_VARIABLE",
]
# (x, y) sample points: along the main slope in Y at the centre, and along the hip in X
SAMPLES = [(0, y) for y in (0, 100, 200, 300, 400, 480, 520, 560)] + \
          [(x, 0) for x in (0, 300, 600, 900, 1200, 1500)]


def _path(value):
    return value.get_path_name() if value else ""


def _components(actor):
    return {c.get_name(): c for c in actor.get_components_by_class(
        unreal.HierarchicalInstancedStaticMeshComponent)}


def _surface_z(world, x, y, z_low, z_high, iterations=16):
    """Highest surface Z at (x, y) between z_low and z_high, via bisection on the boolean."""
    lo, hi = z_low, z_high
    if not unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, hi), unreal.Vector(x, y, lo),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True):
        return None
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
    report = {"blueprint": BLUEPRINT, "passes": {}, "map_saved": False}
    try:
        components = _components(spawned)
        # spawned components carry runtime names, so match on the template suffix
        def resolve(needle):
            for name, component in components.items():
                if needle in name:
                    return name
            raise AssertionError((needle, sorted(components)[:8]))
        shell_keys = [resolve(k) for k in
                      ("R1_RoofShellBack", "R1_RoofShellFront", "R1_RoofShellLeft", "R1_RoofShellRight",
                       "R2_RoofShellBack", "R2_RoofShellFront", "R2_RoofShellLeft", "R2_RoofShellRight")]
        tile_key = resolve("R1_T0")
        r2_keys = [resolve(k) for k in ("R2_RoofShellBack", "R2_RoofShellFront", "R2_RoofShellLeft",
                                        "R2_RoofShellRight", "R2_MainRidge")]
        r1_keys = [resolve(k) for k in ("R1_RoofShellBack", "R1_RoofShellFront", "R1_RoofShellLeft",
                                        "R1_RoofShellRight", "R1_MainRidge", "R1_T0")]
        z_low, z_high = 1100.0, 2200.0

        for label, hide in (("R1_tier_only", r1_keys), ("R2_tier_only", r2_keys)):
            saved = {}
            for name in hide:
                # visibility does not affect traces; collision does
                saved[name] = components[name].get_collision_enabled()
                components[name].set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            try:
                rows = []
                for x, y in SAMPLES:
                    value = _surface_z(world, centre.x + x, centre.y + y, z_low, z_high)
                    rows.append({"x": x, "y": y, "z": value})
                report["passes"][label] = rows
            finally:
                for name, mode in saved.items():
                    components[name].set_collision_enabled(mode)
    finally:
        name = spawned.get_name()
        actors.destroy_actor(spawned)
        leaked = [a.get_name() for a in actors.get_all_level_actors() if a.get_name() == name]
        assert not leaked, leaked
    OUTPUT.write_text(json.dumps(report, indent=2), encoding="utf-8")

    for label, rows in report["passes"].items():
        print("BISECT", label)
        for row in rows:
            print(f"    x={row['x']:6.0f} y={row['y']:6.0f}  surface_z={row['z']}")
    print("BISECT_OUTPUT", OUTPUT)


if __name__ == "__main__":
    main()
