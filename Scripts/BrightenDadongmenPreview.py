"""Add a soft interior fill to the Dadongmen preview map for visual QA."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'
report = json.loads((ROOT / 'Dadongmen_V4-import.json').read_text())
level = report['preview_level']
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(level)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actors.get_all_level_actors()
fill = next((a for a in all_actors if a.get_actor_label() == 'V4_Dadongmen_DoorFill'), None)
if not fill:
    fill = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 0, 340))
    fill.set_actor_label('V4_Dadongmen_DoorFill')
fill.light_component.set_intensity(260)
fill.light_component.set_attenuation_radius(1400)
fill.light_component.set_light_color(unreal.LinearColor(1.0, 0.78, 0.58, 1.0))
fill.light_component.set_editor_property('cast_shadows', False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world, level)
print('DADONGMEN_PREVIEW_FILL_READY', fill.get_actor_location())
