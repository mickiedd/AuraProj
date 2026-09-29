"""Fresh-editor route and floor-collision matrix for the provisional district.

Navigation and vertical floor traces are diagnostic. They do not simulate a pawn,
verify historic street locations, or establish runtime World Partition streaming.
"""
import json
import math
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
heights = memoryview(RAW.read_bytes()).cast("H")
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
phase = 0
started = time.monotonic()

# Each segment tests a distinct authored surface or transition. The courtyard is
# still a GIS reference, so the last route only tests an approach over open ground.
ROUTES = [
    ("principal_gate_side_to_interior", (180000, 123500), (180000, 138000), "principal"),
    ("mixed_lane_west_to_junction", (174500, 132000), (180000, 132000), "mixed"),
    ("mixed_lane_junction_to_east", (180000, 132000), (189000, 132000), "mixed"),
    ("principal_to_mixed_intersection", (180000, 130500), (184000, 132000), "intersection"),
    # Engineering approach reaches the actual closed-door landing via the source bridge.
    ("gate_threshold_approach", (180000, 125000), (180000, 121750), "gate_threshold"),
    ("covered_gutter_crossing", (180000, 134000), (180900, 134000), "gutter"),
    ("mixed_lane_to_courtyard_reference", (188000, 132000), (188000, 136000), "reference_only"),
]


def z_at(x, y):
    fx, fy = x / 200, y / 200
    col, row = math.floor(fx), math.floor(fy)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u)*(1-v)*sample(col,row) + u*(1-v)*sample(col+1,row) +
            (1-u)*v*sample(col,row+1) + u*v*sample(col+1,row+1))


def floor_trace(x, y):
    z = z_at(x, y)
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, z + 2500), unreal.Vector(x, y, z - 250),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE)
    # HitResult's reflected fields are not exposed as Python attributes or
    # editor properties in this UE build; to_tuple follows its struct order.
    parts = hit.to_tuple() if hit else ()
    assert not parts or len(parts) >= 10, parts
    blocked = bool(parts and parts[0])
    actor = parts[9] if blocked and hasattr(parts[9], "get_actor_label") else None
    impact = parts[5] if blocked else None
    return {"hit": blocked,
            "actor": actor.get_actor_label() if actor else None,
            "impact_z_cm": impact.z if impact else None,
            "z_error_cm": abs(impact.z-z) if impact else None}


def check_route(name, start_xy, end_xy, surface):
    ax, ay = start_xy
    bx, by = end_xy
    start = unreal.Vector(ax, ay, (floor_trace(ax, ay)["impact_z_cm"] or z_at(ax, ay)) + 20)
    end = unreal.Vector(bx, by, (floor_trace(bx, by)["impact_z_cm"] or z_at(bx, by)) + 20)
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, end)
    valid = bool(path and path.is_valid() and not path.is_partial())
    straight = math.hypot(bx-ax, by-ay)
    path_length = path.get_path_length() if valid else None
    # At most 2 m spacing, including both endpoints. These are floor support
    # checks, not a moving capsule/pawn traversal or clearance test.
    steps = max(1, math.ceil(straight / 200))
    samples = [floor_trace(ax+(bx-ax)*i/steps, ay+(by-ay)*i/steps)
               for i in range(steps+1)]
    support_hits = sum(sample["hit"] for sample in samples)
    labels = sorted({sample["actor"] for sample in samples if sample["actor"]})
    max_height_delta = max((sample["z_error_cm"] for sample in samples
                            if sample["z_error_cm"] is not None), default=None)
    detour_ratio = path_length/straight if path_length is not None and straight else None
    nav_corner_floor = []
    for point in path.path_points if path else []:
        support = floor_trace(point.x, point.y)
        nav_corner_floor.append({
            "xy_cm": [point.x, point.y], "nav_z_cm": point.z,
            "floor_z_cm": support["impact_z_cm"], "floor_actor": support["actor"],
            "nav_minus_floor_cm": point.z-support["impact_z_cm"]
            if support["impact_z_cm"] is not None else None,
        })
    nav_floor_gaps = [abs(sample["nav_minus_floor_cm"])
                      for sample in nav_corner_floor
                      if sample["nav_minus_floor_cm"] is not None]
    return {
        "route": name, "surface": surface,
        "endpoint_role": "engineered landing approximately 0.6 m outside the closed door; source bridge retained"
        if name == "gate_threshold_approach" else None,
        "start_xy_cm": list(start_xy), "end_xy_cm": list(end_xy),
        "floor_trace_hits": support_hits, "floor_trace_count": len(samples),
        "floor_support_pass": support_hits == len(samples),
        "max_floor_vs_r16_error_cm": max_height_delta,
        "floor_actor_labels": labels,
        "nav_path_valid": valid,
        "nav_path_partial": bool(path and path.is_partial()),
        "nav_point_count": len(path.path_points) if path else 0,
        "nav_points_cm": [[point.x, point.y, point.z] for point in path.path_points] if path else [],
        "nav_corner_floor_samples": nav_corner_floor,
        "max_abs_nav_minus_floor_cm": max(nav_floor_gaps, default=None),
        "nav_corner_on_authored_road_count": sum(
            bool(sample["floor_actor"] and sample["floor_actor"].startswith(
                ("District_Main_Stone_", "District_MixedLane_")))
            for sample in nav_corner_floor),
        "nav_path_length_cm": path_length,
        "nav_detour_ratio": detour_ratio,
        "pawn_traversal": "NOT_RUN",
        "route_check_pass": valid and support_hits == len(samples) and
                            detour_ratio is not None and detour_ratio <= 1.5 and
                            max(nav_floor_gaps, default=999) <= 6,
        "historical_route_claim": False,
    }


def tick(_delta):
    global phase, started
    if phase == 2:
        return
    if time.monotonic()-started < (30 if phase == 0 else 25):
        return
    try:
        if phase == 0:
            phase = 2  # BuildPaths can re-enter Slate callbacks.
            unreal.SystemLibrary.execute_console_command(world, "BuildPaths")
            phase = 1
            started = time.monotonic()
            return
        phase = 2
        rows = [check_route(*spec) for spec in ROUTES]
        closed_gate = check_route("closed_gate_transit_diagnostic",
                                  (180000, 120500), (180000, 123500), "closed_gate")
        closed_gate["interpretation"] = (
            "Known blocked through-gate query: south endpoint is inside the footprint "
            "and the timber doors are closed. Detour is expected, not a passage target.")
        unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
        result = {
            "map": MAP, "scope": "fresh-editor navigation plus vertical floor support",
            "historically_accepted": False,
            "pawn_traversal_performed": False,
            "world_partition_runtime_streaming_tested": False,
            "route_count": len(rows), "route_check_pass_count": sum(r["route_check_pass"] for r in rows),
            "all_route_checks_pass": all(r["route_check_pass"] for r in rows),
            "working_directness_screen_ratio": 1.5,
            "screen_status": "technical target approved 2026-09-29; 6 cm nav-floor screen remains working diagnostic",
            "working_nav_floor_gap_cm": 6,
            "closed_gate_transit_diagnostic": closed_gate,
            "routes": rows,
        }
        (OUT / "Traversal_Route_Matrix.json").write_text(json.dumps(result, indent=2)+"\n")
        (OUT / "Traversal_Error.txt").unlink(missing_ok=True)
    except Exception:
        (OUT / "Traversal_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        if phase == 1:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
