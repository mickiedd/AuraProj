"""Build a bounded navigation test for the local district route."""
import json
import math
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
heights = memoryview((ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16").read_bytes()).cast("H")
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
started = time.monotonic()
phase = 0


def z_at(x, y):
    fx, fy = x / 200, y / 200
    col, row = math.floor(fx), math.floor(fy)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u)*(1-v)*sample(col,row) + u*(1-v)*sample(col+1,row) +
            (1-u)*v*sample(col,row+1) + u*v*sample(col+1,row+1))


def tick(_delta):
    global phase, started
    if phase == 2:
        return
    if time.monotonic() - started < (25 if phase == 0 else 45):
        return
    try:
        if phase == 0:
            phase = 2  # SaveMap and BuildPaths can re-enter Slate callbacks.
            preflight = json.loads(unreal.CantonTerrainLibrary.validate_district_world(
                world, str(ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16")))
            assert preflight["passed"], preflight
            existing = actor_api.get_all_level_actors()
            if not any(isinstance(actor, unreal.NavMeshBoundsVolume) for actor in existing):
                volume = actor_api.spawn_actor_from_class(unreal.NavMeshBoundsVolume,
                    unreal.Vector(180000, 130000, 1000), unreal.Rotator())
                assert volume
                volume.set_actor_label("District_Navigation_Bounds_PROVISIONAL")
                # UE's default 100 cm brush scaled to 220 x 220 x 60 m.
                volume.set_actor_scale3d(unreal.Vector(220, 220, 60))
            if not any(isinstance(actor, unreal.PlayerStart) for actor in existing):
                spawn = actor_api.spawn_actor_from_class(unreal.PlayerStart,
                    unreal.Vector(180000, 124000, z_at(180000, 124000) + 100),
                    unreal.Rotator(yaw=90))
                assert spawn
                spawn.set_actor_label("District_Test_PlayerStart_PROVISIONAL")
            cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
            drain_material = unreal.EditorAssetLibrary.load_asset(
                "/Game/Canton/DistrictPrototype/Materials/M_District_Drain")
            for label, y in [("District_DrainOutlet_South", 121000),
                             ("District_DrainOutlet_Central", 133400)]:
                if any(actor.get_actor_label() == label for actor in existing):
                    continue
                outlet = actor_api.spawn_actor_from_class(unreal.StaticMeshActor,
                    unreal.Vector(180650, y, z_at(180650, y) + 8), unreal.Rotator())
                assert outlet
                outlet.set_actor_label(label)
                outlet.static_mesh_component.set_static_mesh(cube)
                outlet.static_mesh_component.set_material(0, drain_material)
                outlet.set_actor_scale3d(unreal.Vector(1.2, 1.2, .08))
                outlet.set_actor_enable_collision(False)
            assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
            unreal.SystemLibrary.execute_console_command(world, "BuildPaths")
            phase = 1
            started = time.monotonic()
            return
        start = unreal.Vector(180000, 123500, z_at(180000, 123500) + 90)
        end = unreal.Vector(180000, 138000, z_at(180000, 138000) + 90)
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, start, end)
        result = {
            "map": MAP, "start_cm": [start.x, start.y, start.z],
            "end_cm": [end.x, end.y, end.z],
            "path_valid": bool(path and path.is_valid()),
            "path_partial": bool(path and path.is_partial()),
            "point_count": len(path.path_points) if path else 0,
            "path_length_cm": path.get_path_length() if path and path.is_valid() else None,
            "historical_alignment_claim": False,
        }
        (OUT / "Navigation_Result.json").write_text(json.dumps(result, indent=2) + "\n")
        (OUT / "Navigation_Error.txt").unlink(missing_ok=True)
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        packages = [p for p in packages if "/Canton/DistrictPrototype/" in p.get_name()]
        if packages:
            assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
    except Exception:
        (OUT / "Navigation_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        if phase == 1 and time.monotonic() - started < 2:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
