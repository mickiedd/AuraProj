"""Capture a focused editor viewport image of the DabeiMen Great North Gate.

One view per run: the high-res screenshot is written by an async editor task that
only flushes once this script returns, so a second camera move in the same run
discards the first frame.

Angle: elevated 3/4 view from the front-right.  The gate's front faces +Y (the
plaque lands at +Y after the roll-90 import), which is also the sun side, so the
camera sits on +Y offset to +X.  The original GuangzhouLandmark_GreatNorthGate
(x -142200..-138600, y 104525..106275) is behind the subject at this angle, and
the GreatSouthGate (x -141680..-139120) stays outside the frustum edge.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_dabeimen_visual.png")
# The plaque (GNG_PLAQUE) lands at +Y after the roll-90 import, so the gate's
# front faces +Y - the same side the sun is on.  Camera sits on +Y, offset to +X
# and raised, so the lit facade and grey tile roof both read clearly.  At this
# angle the GreatSouthGate (x -141680..-139120) stays outside the frustum edge.
CAMERA = (-136200.0, 116200.0, 5000.0)
TARGET = (-140400.0, 110442.0, 1600.0)

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate DabeiMen visual check", 0.5, True)
print("GREAT_NORTH_GATE_DABEIMEN_VISUAL_CAPTURE", {
    "path": str(OUTPUT), "camera": list(CAMERA), "target": list(TARGET), "task": str(task)})
