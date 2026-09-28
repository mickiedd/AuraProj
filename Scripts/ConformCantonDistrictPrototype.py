"""Conform short road meshes to the raster and retire unsupported broad parcel pads."""
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
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
started = time.monotonic()


def z_at(x, y):
    fx = max(0.0, min(2016.0, x / 200))
    fy = max(0.0, min(2016.0, y / 200))
    col, row = math.floor(fx), math.floor(fy)
    col1, row1 = min(2016, col + 1), min(2016, row + 1)
    u, v = fx - col, fy - row
    sample = lambda a, b: (heights[b * 2017 + a] - 32768) * 50 / 128
    return ((1-u) * (1-v) * sample(col, row) + u * (1-v) * sample(col1, row) +
            (1-u) * v * sample(col, row1) + u * v * sample(col1, row1))


def finish(_delta):
    if time.monotonic() - started < 25:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        changed = 0
        removed = 0
        changed_packages = set()
        for actor in actor_api.get_all_level_actors():
            label = actor.get_actor_label()
            if label.startswith("District_Parcel_"):
                assert actor_api.destroy_actor(actor)
                removed += 1
                continue
            if not label.startswith("District_") or not isinstance(actor, unreal.StaticMeshActor):
                continue
            if label.startswith("District_Main_Stone_"):
                offset = 7
            elif label.startswith("District_Main_EarthShoulder_"):
                offset = 3
            elif label.startswith("District_MixedLane_"):
                offset = 5
            elif label.startswith("District_CoveredGutter_"):
                offset = 5
            elif label.startswith("District_Weed_"):
                offset = 14
            elif label.startswith("District_LocalDamp_"):
                offset = .5
            else:
                continue
            point = actor.get_actor_location()
            scale = actor.get_actor_scale3d()
            length, width, thickness = scale.x * 100, scale.y * 100, scale.z * 100
            z = z_at(point.x, point.y) + offset + thickness / 2
            dx = (z_at(point.x + length/2, point.y) - z_at(point.x - length/2, point.y)) / length
            dy = (z_at(point.x, point.y + width/2) - z_at(point.x, point.y - width/2)) / width
            actor.modify(True)
            actor.set_actor_location(unreal.Vector(point.x, point.y, z), False, True)
            assert abs(actor.get_actor_location().z - z) < .1
            assert actor.set_actor_rotation(unreal.Rotator(
                pitch=math.degrees(math.atan(dx)), roll=-math.degrees(math.atan(dy))), True)
            changed_packages.add(actor.get_package())
            changed += 1
        assert removed in (0, 16) and changed == 476, (removed, changed)
        labels = {actor.get_actor_label() for actor in actor_api.get_all_level_actors()}
        if "District_NonShadow_Front_Fill" not in labels:
            fill = actor_api.spawn_actor_from_class(unreal.DirectionalLight,
                unreal.Vector(0, 0, 30000), unreal.Rotator(pitch=-50, yaw=140))
            fill.set_actor_label("District_NonShadow_Front_Fill")
            fill.light_component.set_intensity(2)
            fill.light_component.set_editor_property("cast_shadows", False)
        if "District_Sky_Atmosphere" not in labels:
            sky = actor_api.spawn_actor_from_class(unreal.SkyAtmosphere,
                unreal.Vector(0, 0, 0), unreal.Rotator())
            sky.set_actor_label("District_Sky_Atmosphere")
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        assert unreal.EditorLoadingAndSavingUtils.save_packages(list(changed_packages), False)
        packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        packages = [p for p in packages if "/Canton/DistrictPrototype/" in p.get_name()]
        if packages:
            assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
        (OUT / "Surface_Conformance.json").write_text(json.dumps({
            "conformed_meshes": changed,
            "saved_actor_packages": len(changed_packages),
            "removed_unsupported_large_parcel_pads": 16,
            "removed_additional_parcel_pads": removed,
            "height_interpolation": "bilinear_from_locked_modern_R16",
            "rotation": "local_raster_gradient",
            "historical_terrain_claim": False,
        }, indent=2) + "\n")
    except Exception:
        (OUT / "Conform_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
