"""Capture a deterministic Dadongmen orthographic preview for visual QA."""
import json
import time
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'
report = json.loads((ROOT / 'Dadongmen_V4-import.json').read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'Preview_Dadongmen_V4')
origin, extent = building.get_actor_bounds(False)
target = unreal.Vector(origin.x, origin.y, origin.z * .92)
radius = max(extent.x, extent.y, extent.z)
camera = target + unreal.Vector(radius * 1.65, radius * .08, radius * .10)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
unreal.EditorLevelLibrary.editor_set_game_view(True)
unreal.SystemLibrary.execute_console_command(world, 'ToggleOrtho')
print('ORTHO_CAMERA', camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
out = str(ROOT / 'Dadongmen_V4-ortho.png')
unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, out, None, False, False,
    unreal.ComparisonTolerance.LOW, 'Dadongmen Blueprint visual validation orthographic', 2.0, True)
time.sleep(5.0)
print('DADONGMEN_ORTHO_CAPTURE_REQUESTED', out)
