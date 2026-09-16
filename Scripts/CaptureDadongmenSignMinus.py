import json
from pathlib import Path
import unreal

root = Path("C:/Git/AuraProj/Saved/RawModelImport/V4")
report = json.loads((root / "Dadongmen_V4-import.json").read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report["preview_level"])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == "Preview_Dadongmen_V4")
origin, extent = building.get_actor_bounds(False)
radius = max(extent.x, extent.y, extent.z)
target = unreal.Vector(origin.x, origin.y, origin.z * .9)
camera = target + unreal.Vector(-radius * 1.2, -radius * 2.15, radius * .85)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
editor.set_level_viewport_camera_info(camera, rotation)
print("CAM", camera, rotation, unreal.EditorLevelLibrary.get_level_viewport_camera_info())
out = str(root / "Dadongmen_V4-debug3.png")
print("TASK", unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, out, None, False, False, unreal.ComparisonTolerance.LOW, "debug3", 2.0, True))
