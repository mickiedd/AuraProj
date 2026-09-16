"""Capture the GreatNorthGate HighDetail building at the design sheet's angle.

Used to compare the placed model against the supplied design reference
(three-quarter perspective, ground level, front facade plus one side).
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_design_compare.png")
# Straight-on front view from the south, raised enough to clear the neighbouring
# GuangzhouLandmark_GreatNorthGate (top z = 2576) which sits in the sight line.
CAMERA = (-137500.0, 114500.0, 1800.0)
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
    unreal.ComparisonTolerance.LOW, "GreatNorthGate design comparison", 0.5, True)
print("GREAT_NORTH_GATE_DESIGN_COMPARE", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
