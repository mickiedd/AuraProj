"""Temporarily isolate one Production V3 mesh for visual diagnosis."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'
report = json.loads((ROOT / 'Xiaobeimen_Production_V3-import.json').read_text())
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'Preview_Xiaobeimen_Production_V3')
components = building.get_components_by_class(unreal.StaticMeshComponent)
keep = 'SM_XiaobeiMen_RoofTile_Green_LOD0'
for component in components:
    if component.static_mesh:
        component.set_editor_property('visible', component.static_mesh.get_name() == keep)
origin, extent = building.get_actor_bounds(False)
target = unreal.Vector(origin.x, origin.y, origin.z * .70)
radius = max(extent.x, extent.y, extent.z)
camera = target + unreal.Vector(radius * .25, radius * 1.35, radius * .35)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
world = editor.get_editor_world()
for command in ('r.TextureStreaming 0', 'r.ScreenPercentage 100', 'ShowFlag.Grid 0', 'ShowFlag.SelectionOutline 0'):
    unreal.SystemLibrary.execute_console_command(world, command)
output = ROOT / 'Xiaobeimen_Production_V3-roof-isolated.png'
unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False,
    unreal.ComparisonTolerance.LOW, 'Production V3 roof isolation', 2.0, True)
print('V3_COMPONENT_PROBE_REQUESTED', output)
