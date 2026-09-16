"""Final hero capture of the updated DabeiMen Great North Gate.

One view per run (the high-res screenshot task only flushes after this script
returns).  Moderate 3/4 elevation from the lit +Y side, framed so the wall, gate
arch, timber tower, plaque and the grey tile roof all read together.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_dabeimen_hero.png")
CAMERA = (-136800.0, 115500.0, 4200.0)
TARGET = (-140400.0, 110442.0, 1700.0)

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate DabeiMen hero", 0.5, True)
print("GREAT_NORTH_GATE_DABEIMEN_HERO", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
