"""Test whether rendering the roof material two-sided restores a visible roof.

The source GNG_ROOF mesh has no upward-facing geometry (measured: 0.3% up,
83.7% down), so with backface culling on it vanishes from every above-horizon
view.  Making the material two-sided re-enables those faces when seen from
above, which is a workaround rather than a fix - the surface shown is still the
authored soffit, not a modelled tile layer.

The change is saved so the capture reflects it; run
Scripts/RevertGreatNorthGateDabeiMenRoofTwoSided.py afterwards to put the
material back.
"""
from pathlib import Path

import unreal


LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
ROOF_MATERIAL = "/Game/Assets/Environment/GuangzhouLandmarks/GreatNorthGate_DabeiMen/M_Roof_GrayClayTile"
OUTPUT = Path("C:/Git/AuraProj/Saved/RawModelImport/great_north_gate_dabeimen_roof_twosided.png")
CAMERA = (-137500.0, 113500.0, 9000.0)
TARGET = (-140400.0, 110442.0, 1500.0)

material = unreal.EditorAssetLibrary.load_asset(ROOF_MATERIAL)
assert material, ROOF_MATERIAL
try:
    before = bool(material.get_editor_property("two_sided"))
    material.set_editor_property("two_sided", True)
    after = bool(material.get_editor_property("two_sided"))
except Exception as exc:  # noqa: BLE001
    raise SystemExit("Could not set two_sided on {}: {}".format(ROOF_MATERIAL, exc))

assert unreal.EditorAssetLibrary.save_loaded_asset(material), "Failed to save roof material"

world = unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
assert world, LEVEL_PATH
camera = unreal.Vector(*CAMERA)
target = unreal.Vector(*TARGET)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    camera, rotation)
task = unreal.AutomationLibrary.take_high_res_screenshot(
    1600, 900, str(OUTPUT), None, False, False,
    unreal.ComparisonTolerance.LOW, "GreatNorthGate DabeiMen roof two-sided", 0.5, True)
print("GREAT_NORTH_GATE_DABEIMEN_ROOF_TWOSIDED", {
    "material": ROOF_MATERIAL, "two_sided_before": before, "two_sided_after": after,
    "path": str(OUTPUT), "task": str(task)})
