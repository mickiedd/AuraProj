"""Capture a wide editor viewport image of the landmark row after the new building.

One view per run: the high-res screenshot is written by an async editor task that
only flushes once this script returns.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_highdetail_row_overview.png")
CAMERA = (-146500.0, 96000.0, 6500.0)
TARGET = (-140400.0, 106000.0, 1200.0)

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate HighDetail landmark row overview", 0.5, True)
print("GREAT_NORTH_GATE_HIGHDETAIL_ROW_CAPTURE", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
