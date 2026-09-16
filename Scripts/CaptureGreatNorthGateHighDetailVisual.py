"""Capture a focused editor viewport image of the GreatNorthGate HighDetail building.

One view per run: the high-res screenshot is written by an async editor task that
only flushes once this script returns, so a second camera move in the same run
discards the first frame.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_highdetail_visual_after.png")
# Elevated 3/4 view from the north-east.  The row's neighbouring gates sit to the
# west and north, so this angle clears them (verified against their bounds).
CAMERA = (-136500.0, 115000.0, 4500.0)
TARGET = (-140400.0, 110442.0, 900.0)

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate HighDetail visual check", 0.5, True)
print("GREAT_NORTH_GATE_HIGHDETAIL_VISUAL_CAPTURE", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
