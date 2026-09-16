"""Close-up capture of the DabeiMen Great North Gate for material inspection.

One view per run (the high-res screenshot task only flushes after this script
returns).  Camera sits on +Y - the lit facade side - close enough that the gate
fills most of the frame, so the stone wall, gate arch, timber tower, plaque and
roof read individually.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_dabeimen_closeup.png")
CAMERA = (-138500.0, 113500.0, 3500.0)
TARGET = (-140400.0, 110442.0, 1800.0)

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate DabeiMen close-up", 0.5, True)
print("GREAT_NORTH_GATE_DABEIMEN_CLOSEUP", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
