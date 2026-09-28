"""Measure actual slab-to-slab top-corner steps in a fresh editor process."""
import json
import time
import traceback
from pathlib import Path

import unreal


root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
world = unreal.EditorLoadingAndSavingUtils.load_map(
    "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL")
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.monotonic()


def finish(_delta):
    if time.monotonic() - started < 20:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        result = json.loads(unreal.CantonTerrainLibrary.validate_district_world(
            world, str(root / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16")))
        assert result["passed"], result["errors"]
        (root / "QA/Canton_District/Seam_Probe.json").write_text(json.dumps({
            "measurement": "adjacent principal slab top-corner vertical step",
            "source": "fresh UE world; native ValidateDistrictWorld",
            "map": "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL",
            "road_joint_comparisons": result["road_joint_comparisons"],
            "max_road_joint_step_cm": result["max_road_joint_step_cm"],
            "historically_accepted": False,
            "scope": "technical road-joint geometry, not final visible paving art",
        }, indent=2) + "\n")
    except Exception:
        (root / "QA/Canton_District/Seam_Probe_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
