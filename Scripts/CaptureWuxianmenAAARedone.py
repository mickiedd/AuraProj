"""Capture the Wuxianmen V4 preview after the AAA-redone material pass."""
import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[1] / "Saved/RawModelImport/V4"
LEVEL = "/Game/Assets/Environment/GuangzhouLandmarks/V4/Wuxianmen/L_Wuxianmen_V4_Preview"
BUILDING_LABEL = "Preview_Wuxianmen_V4"


def capture(view="front"):
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    building = next(a for a in all_actors if a.get_actor_label() == BUILDING_LABEL)
    origin, extent = building.get_actor_bounds(False)
    width = max(extent.x * 2.0, 1.0)
    height = max(extent.z * 2.0, 1.0)
    sign = 1.0 if view in ("front", "close") else -1.0
    scale = 0.72 if view == "close" else 1.0
    target = unreal.Vector(origin.x, origin.y, origin.z + height * 0.04)
    camera = target + unreal.Vector(width * 0.17 * sign * scale, width * 0.42 * sign * scale,
                                    height * 0.16 * scale)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "ShowFlag.Grid 0",
                    "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / ("Wuxianmen_V4-" + view + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(
        1600, 1000, str(output), None, False, False,
        unreal.ComparisonTolerance.LOW,
        "Wuxianmen AAA-redone visual validation " + view, 2.0, True,
    )
    print("WUXIANMEN_CAPTURE_REQUESTED", json.dumps({"view": view, "output": str(output)}))


if __name__ == "__main__":
    capture("front")
