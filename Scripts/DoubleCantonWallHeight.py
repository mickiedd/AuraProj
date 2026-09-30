"""Double the saved adaptive wall height while preserving every wall base."""
from __future__ import annotations

import json
import math
import traceback
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/Canton/Provisional/Maps/L_Canton_WalledCity_PROVISIONAL"
OUT = ROOT / "QA/Canton_Continuation/Walled_DoubleHeight.json"
OLD_HEIGHT_CM = 500.0
NEW_HEIGHT_CM = 1000.0


def run():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    modules = [a for a in actors.get_all_level_actors()
               if a.get_actor_label().startswith("PROVISIONAL_WallAdaptive_")]
    assert modules, "No adaptive wall modules found"
    before = []
    after = []
    for actor in modules:
        scale = actor.get_actor_scale3d()
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        old_height = abs(float(scale.z)) * 100.0
        assert abs(old_height - OLD_HEIGHT_CM) < 0.01, (actor.get_actor_label(), old_height)
        half_height = old_height * 0.5
        pitch = math.radians(float(rotation.pitch))
        # Keep XY and the terrain contact line unchanged.  The wall's local Z
        # is nearly vertical; using cos(pitch) preserves the world-space bottom
        # height exactly while the module grows upward.
        actor.modify(True)
        actor.set_actor_location(unreal.Vector(location.x, location.y,
                                               location.z + half_height * math.cos(pitch)),
                                 False, False)
        actor.set_actor_scale3d(unreal.Vector(scale.x, scale.y,
                                             math.copysign(NEW_HEIGHT_CM / 100.0, scale.z)))
        before.append(old_height)
        after.append(abs(float(actor.get_actor_scale3d().z)) * 100.0)
    assert all(abs(height - NEW_HEIGHT_CM) < 0.01 for height in after)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    packages = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) +
                list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()))
    packages = [p for p in set(packages) if "/Canton/Provisional/" in p.get_name()]
    if packages:
        unreal.EditorLoadingAndSavingUtils.save_packages(packages, False)
    report = {
        "passed": True,
        "map": MAP,
        "historically_accepted": False,
        "adaptive_module_count": len(modules),
        "old_height_cm": OLD_HEIGHT_CM,
        "new_height_cm": NEW_HEIGHT_CM,
        "base_preserved": True,
    }
    OUT.write_text(json.dumps(report, indent=2) + "\n")
    unreal.log("CANTON_WALL_DOUBLE_HEIGHT " + json.dumps(report))


try:
    run()
except Exception:
    OUT.write_text(json.dumps({"passed": False, "error": traceback.format_exc()}, indent=2) + "\n")
    unreal.log_error(traceback.format_exc())
    raise
finally:
    unreal.SystemLibrary.quit_editor()
