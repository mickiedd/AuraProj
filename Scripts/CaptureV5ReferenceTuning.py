"""Capture isolated post-tuning views without saving the user's dirty map."""
from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
VIEWS = {
    "hero": (1.70, 1.70, 0.92),
    "front": (0.00, 2.60, 0.40),
    "rear": (0.00, -2.60, 0.40),
    "side_l": (-2.60, 0.00, 0.48),
    "side_r": (2.60, 0.00, 0.48),
    "low": (0.00, 2.00, 0.18),
    "top": (0.00, 0.00, 3.20),
    "arch_close": (0.00, 1.18, 0.30),
}


def _path(value):
    return value.get_path_name() if value else ""


def _spawn_preview_lights(center):
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
    post.set_actor_location(center, False, False)
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


def _capture(package, view_name, offset, output):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    import_report = json.loads((ROOT / (package["name"] + "-import.json")).read_text(encoding="utf-8"))
    blueprint = unreal.EditorAssetLibrary.load_asset(import_report["blueprint"])
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    assert building
    origin, extent = building.get_actor_bounds(False)
    ground = _spawn_ground(center, origin, extent)
    lights = _spawn_preview_lights(center)
    radius = max(extent.x, extent.y, extent.z)
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * (0.34 if view_name == "arch_close" else 0.50))
    camera = target + unreal.Vector(offset[0] * radius, offset[1] * radius, offset[2] * radius)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW, package["name"] + " reference tuning " + view_name, 2.0, True)
    for actor in [building, ground] + lights:
        actors.destroy_actor(actor)
    print("V5_REFERENCE_CAPTURE_REQUESTED", package["name"], view_name, str(output))


def main():
    packages = json.loads((ROOT / "packages.json").read_text(encoding="utf-8"))
    captures = []
    dirty = [_path(item) for item in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    for package in packages:
        for view_name, offset in VIEWS.items():
            output = ROOT / (package["name"] + "-reference-tuning-" + view_name + ".png")
            _capture(package, view_name, offset, output)
            captures.append(str(output))
    (ROOT / "reference-tuning-captures-20260917.json").write_text(json.dumps({"dirty_maps_recorded": dirty, "captures": captures, "saved_map": False, "temporary_actors_destroyed": True}, indent=2), encoding="utf-8")
    print("V5_REFERENCE_CAPTURE_COMPLETE", len(captures))


if __name__ == "__main__":
    main()
