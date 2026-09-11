"""Capture a focused editor viewport image of the Great West Gate placement."""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_west_gate_visual_after.png")

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
target = unreal.Vector(-140400.0, 101366.0, 1050.0)
camera = unreal.Vector(-136000.0, 111000.0, 5200.0)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.set_level_viewport_camera_info(camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1280,
    720,
    str(OUTPUT),
    None,
    False,
    False,
    unreal.ComparisonTolerance.LOW,
    "Great West Gate visual placement check",
    0.5,
    True,
)
print("GREAT_WEST_GATE_VISUAL_CAPTURE", {"path": str(OUTPUT), "camera": [camera.x, camera.y, camera.z], "rotation": [rotation.pitch, rotation.yaw, rotation.roll], "task": str(task)})
