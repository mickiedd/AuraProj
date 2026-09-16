"""Render Dadongmen from the orthogonal world-Y side for visual QA."""
import json, time
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'
report = json.loads((ROOT / 'Dadongmen_V4-import.json').read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
capture_actor = actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 4800, 1800))
capture_actor.set_actor_label('V4_Dadongmen_SceneCaptureY')
target = unreal.Vector(0, 0, 900)
capture_actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture_actor.get_actor_location(), target), False)
component = capture_actor.capture_component2d
component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
component.set_editor_property('capture_every_frame', False)
component.set_editor_property('capture_on_movement', False)
component.set_editor_property('fov_angle', 55.0)
render_target = unreal.RenderingLibrary.create_render_target2d(world, 1600, 1000, unreal.TextureRenderTargetFormat.RTF_RGBA8)
component.set_editor_property('texture_target', render_target)
time.sleep(2)
component.capture_scene()
time.sleep(3)
unreal.RenderingLibrary.export_render_target(world, render_target, str(ROOT), 'Dadongmen_V4-scene-y')
time.sleep(2)
print('DADONGMEN_SCENE_Y_READY', str(ROOT / 'Dadongmen_V4-scene-y'))
actors.destroy_actor(capture_actor)
unreal.EditorLoadingAndSavingUtils.save_map(world, report['preview_level'])
