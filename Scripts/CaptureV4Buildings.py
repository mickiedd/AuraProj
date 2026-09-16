"""Light isolated V4 preview maps and capture the actual Blueprint instances.

Call capture(index, 'front' or 'rear') once per remote invocation, allowing the
editor to tick and write the PNG before changing maps or taking another capture.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1] / 'Saved/RawModelImport/V4'


def capture(index, view='front'):
    cfg = json.loads((ROOT / 'packages.json').read_text())[index]
    report = json.loads((ROOT / (cfg['name'] + '-import.json')).read_text())
    level = report['preview_level']
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(level)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    building = next(a for a in all_actors if a.get_actor_label() == 'Preview_' + cfg['name'])
    if not any(a.get_actor_label() == 'V4_Preview_Key' for a in all_actors):
        key = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-40, yaw=-60))
        key.set_actor_label('V4_Preview_Key')
        key.light_component.set_intensity(8)
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-30, yaw=120))
        fill.set_actor_label('V4_Preview_Fill')
        fill.light_component.set_intensity(2)
        fill.light_component.set_editor_property('cast_shadows', False)
        actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
        sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 3500))
        sky.light_component.set_intensity(1.5)
        sky.light_component.set_editor_property('real_time_capture', True)
        pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
        pp.set_editor_property('unbound', True)
        settings = pp.get_editor_property('settings')
        for name, value in [('override_auto_exposure_min_brightness', True),
                            ('override_auto_exposure_max_brightness', True),
                            ('auto_exposure_min_brightness', 1.0), ('auto_exposure_max_brightness', 1.0)]:
            settings.set_editor_property(name, value)
        pp.set_editor_property('settings', settings)
    all_actors = actors.get_all_level_actors()
    for label, pitch, yaw in [('V4_Preview_Key', -40, -60), ('V4_Preview_Fill', -30, 120)]:
        light = next(a for a in all_actors if a.get_actor_label() == label)
        light.set_actor_rotation(unreal.Rotator(pitch=pitch, yaw=yaw), False)
    if not any(a.get_actor_label() == 'V4_Preview_Ground' for a in all_actors):
        floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -20))
        floor.set_actor_label('V4_Preview_Ground')
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        floor.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    floor = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == 'V4_Preview_Ground')
    ground_path = '/Game/Assets/Environment/GuangzhouLandmarks/V4/M_V4PreviewGround'
    material = unreal.EditorAssetLibrary.load_asset(ground_path) if unreal.EditorAssetLibrary.does_asset_exist(ground_path) else None
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_V4PreviewGround',
            '/Game/Assets/Environment/GuangzhouLandmarks/V4', unreal.Material, unreal.MaterialFactoryNew())
        color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(.18, .20, .22, 1))
        unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material)
    floor.static_mesh_component.set_material(0, material)
    origin, extent = building.get_actor_bounds(False)
    radius = max(extent.x, extent.y, extent.z)
    target = unreal.Vector(origin.x, origin.y, origin.z * .9)
    sign = 1 if view in ('front', 'close') else -1
    distance = .72 if view == 'close' else 1.0
    camera = target + unreal.Vector(radius * 1.2 * sign * distance, radius * 2.15 * sign * distance, radius * .85 * distance)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    unreal.SystemLibrary.execute_console_command(world, 'r.ScreenPercentage 100')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.Grid 0')
    unreal.SystemLibrary.execute_console_command(world, 'ShowFlag.SelectionOutline 0')
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, level)
    # Saving a preview map can restore its persisted viewport.  Reapply the
    # requested camera after the save so the capture is deterministic.
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / (cfg['name'] + '-' + view + '.png')
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False,
        unreal.ComparisonTolerance.LOW, cfg['name'] + ' Blueprint visual validation ' + view, 2.0, True)
    print('V4_CAPTURE_REQUESTED', str(output))


if __name__ == '__main__':
    capture(0)
