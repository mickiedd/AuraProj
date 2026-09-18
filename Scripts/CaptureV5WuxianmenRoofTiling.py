"""Capture isolated after-tiling views for Wuxianmen V5 Core."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
VIEWS = {
    "hero": (1.70, 1.70, 0.92),
    "front": (0.00, 2.60, 0.40),
    "low": (0.00, 2.00, 0.18),
    "top": (0.00, 0.00, 3.20),
    "roof_close": (0.00, 1.30, 1.05),
}


def _spawn_lights(center):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    spawned = []
    key = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
    key.light_component.set_intensity(7.0)
    spawned.append(key)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
    fill.light_component.set_intensity(2.0)
    fill.light_component.set_editor_property("cast_shadows", False)
    spawned.append(fill)
    atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, center)
    spawned.append(atmosphere)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawned.append(sky)
    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, center)
    post.set_editor_property("unbound", False)
    post.set_editor_property("priority", 100.0)
    settings = post.get_editor_property("settings")
    for name, value in (("override_auto_exposure_min_brightness", True), ("override_auto_exposure_max_brightness", True), ("auto_exposure_min_brightness", 1.0), ("auto_exposure_max_brightness", 1.0)):
        settings.set_editor_property(name, value)
    post.set_editor_property("settings", settings)
    spawned.append(post)
    return spawned


def _spawn_ground(center, origin, extent):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    material = unreal.EditorAssetLibrary.load_asset("/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround")
    if material:
        floor.static_mesh_component.set_material(0, material)
    return floor


def _capture(view_name, offset):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    assert isinstance(blueprint, unreal.Blueprint), BLUEPRINT
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    assert building
    origin, extent = building.get_actor_bounds(False)
    ground = _spawn_ground(center, origin, extent)
    lights = _spawn_lights(center)
    radius = max(extent.x, extent.y, extent.z)
    target_height = 0.68 if view_name == "roof_close" else 0.50
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * target_height)
    camera = target + unreal.Vector(offset[0] * radius, offset[1] * radius, offset[2] * radius)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    output = ROOT / ("Wuxianmen_V5_4K_Core-roof-tiling-after-" + view_name + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, "Wuxianmen roof tiling after " + view_name, 2.0, True)
    for actor in [building, ground] + lights:
        actors.destroy_actor(actor)
    return str(output)


def main():
    dirty = [item.get_path_name() for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    captures = [_capture(name, offset) for name, offset in VIEWS.items()]
    report = {"blueprint": BLUEPRINT, "dirty_maps_recorded": dirty, "captures": captures, "saved_map": False, "temporary_actors_destroyed": True}
    (ROOT / "Wuxianmen_V5_4K_Core-roof-tiling-captures-20260917.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("V5_WUXIANMEN_ROOF_TILING_CAPTURE_COMPLETE", len(captures))


if __name__ == "__main__":
    main()
