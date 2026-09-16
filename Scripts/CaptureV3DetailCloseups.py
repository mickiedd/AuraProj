"""Capture centered gate close-ups for the three preview Blueprints."""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V3'


def capture(index):
    cfg = json.loads((ROOT / 'packages.json').read_text())[index]
    report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(report['preview_level'])
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    building = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'Preview_' + cfg['name'])
    origin, extent = building.get_actor_bounds(False)
    radius = max(extent.x, extent.y, extent.z)
    # The imported assets face the +Y side of their preview maps.
    target = unreal.Vector(origin.x, origin.y + extent.y * .10, origin.z * .62)
    camera = target + unreal.Vector(radius * .18, radius * 1.28, radius * .30)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    world = editor.get_editor_world()
    for command in ('r.TextureStreaming 0', 'r.ScreenPercentage 100', 'ShowFlag.Grid 0', 'ShowFlag.SelectionOutline 0'):
        unreal.SystemLibrary.execute_console_command(world, command)
    output = ROOT / (cfg['name'] + '-detail-closeup.png')
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False,
        unreal.ComparisonTolerance.LOW, cfg['name'] + ' detail closeup', 2.0, True)
    print('V3_DETAIL_CLOSEUP_REQUESTED', output)


if __name__ == '__main__':
    for index in range(3):
        capture(index)

