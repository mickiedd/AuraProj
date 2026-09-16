"""Capture Dadongmen through an explicit CameraActor for visual QA."""
import json, time
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'
report = json.loads((ROOT / 'Dadongmen_V4-import.json').read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actors.get_all_level_actors()
camera_actor = next((a for a in all_actors if a.get_actor_label() == 'V4_Dadongmen_CaptureCameraFront'), None)
if not camera_actor:
    camera_actor = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector())
    camera_actor.set_actor_label('V4_Dadongmen_CaptureCameraFront')
location = unreal.Vector(4800, 0, 1800)
target = unreal.Vector(0, 0, 900)
camera_actor.set_actor_location(location, False, True)
camera_actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(location, target), False)
camera_actor.camera_component.set_field_of_view(55.0)
camera_actor.camera_component.set_aspect_ratio(1.6)
print('CAMERA_ACTOR_FRONT', location, camera_actor.get_actor_rotation())
out = str(ROOT / 'Dadongmen_V4-camera-front.png')
unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, out, camera_actor, False, False,
    unreal.ComparisonTolerance.LOW, 'Dadongmen CameraActor front visual validation', 2.0, True)
time.sleep(8)
print('CAPTURE_REQUESTED', out)
