"""Try a primary viewport perspective capture without forcing game view."""
import json, time
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'
report = json.loads((ROOT / 'Dadongmen_V4-import.json').read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'Preview_Dadongmen_V4')
origin, extent = building.get_actor_bounds(False)
target = unreal.Vector(origin.x, origin.y, origin.z * .9)
camera = target + unreal.Vector(3300, 0, 1800)
rot = unreal.MathLibrary.find_look_at_rotation(camera, target)
level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_editor.editor_set_game_view(False)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.set_level_viewport_camera_info(camera, rot)
print('GAME_VIEW', level_editor.editor_get_game_view(), 'CAM', editor.get_level_viewport_camera_info())
out = str(ROOT / 'Dadongmen_V4-nogame.png')
unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, out, None, False, False,
    unreal.ComparisonTolerance.LOW, 'Dadongmen Blueprint no game view', 2.0, False)
time.sleep(8)
print('CAPTURE_REQUESTED', out)
