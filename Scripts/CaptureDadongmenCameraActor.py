import json
from pathlib import Path
import unreal

root = Path("C:/Git/AuraProj/Saved/RawModelImport/V4")
report = json.loads((root / "Dadongmen_V4-import.json").read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report["preview_level"])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actors.get_all_level_actors()
camera_actor = next((a for a in all_actors if a.get_actor_label() == "V4_Dadongmen_CaptureCamera"), None)
if not camera_actor:
    camera_actor = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector())
    camera_actor.set_actor_label("V4_Dadongmen_CaptureCamera")
target = unreal.Vector(0.0, 0.0, 900.0)
location = unreal.Vector(0.0, -5200.0, 2500.0)
rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
camera_actor.set_actor_location(location, False, True)
camera_actor.set_actor_rotation(rotation, False)
camera_actor.camera_component.set_field_of_view(55.0)
camera_actor.camera_component.set_aspect_ratio(1.6)
print("CAMERA_ACTOR", location, rotation)
print("PILOT", unreal.EditorLevelLibrary.pilot_level_actor(camera_actor))
out = str(root / "Dadongmen_V4-camera-actor.png")
print("TASK", unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, out, camera_actor, False, False, unreal.ComparisonTolerance.LOW, "camera actor", 2.0, True))
