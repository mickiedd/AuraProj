"""Remove the west-end lane segment that fails the modern diagnostic slope test."""
import json
import time
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.monotonic()
actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def finish(_delta):
    if time.monotonic() - started < 25:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        removed = []
        for actor in actor_api.get_all_level_actors():
            label = actor.get_actor_label()
            if not label.startswith("District_MixedLane_"):
                continue
            x = int(label.split("_")[-1])
            if x < 174000:
                assert actor_api.destroy_actor(actor)
                removed.append(label)
        assert len(removed) == 20, (len(removed), removed)
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
        packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                    list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
        packages = [p for p in packages if "/Canton/DistrictPrototype/" in p.get_name()]
        if packages:
            assert unreal.EditorLoadingAndSavingUtils.save_packages(packages, True)
        (OUT / "Slope_Refinement.json").write_text(json.dumps({
            "removed_west_end_lane_slabs": len(removed),
            "reason": "modern raster diagnostic grade exceeded 12%; engineering route shortened; no historical inference",
            "remaining_lane_x_min_cm": 174000,
        }, indent=2) + "\n")
    except Exception:
        (OUT / "Refine_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
