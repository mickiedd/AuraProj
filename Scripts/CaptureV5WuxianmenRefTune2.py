"""Capture the post-repair Wuxianmen V5 Core review set from transient actors.

Spawns one transient copy plus preview lighting and ground in an isolated
location, walks the review views, and destroys everything. Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core"
PREFIX = "Wuxianmen_V5_4K_Core-reftune2-"
VIEWS = {
    "hero": (1.70, 1.70, 0.92),
    "front_doors": (0.00, 2.60, 0.40),
    "front_plaque": (0.00, -2.60, 0.40),
    "side_l": (-2.60, 0.00, 0.48),
    "side_r": (2.60, 0.00, 0.48),
    "low": (0.00, 2.00, 0.18),
    "top": (0.00, 0.00, 3.20),
    "arch_close": (0.00, 1.18, 0.30),
    "roof_close": (0.75, -1.35, 1.15),
    "roof_side": (2.30, 0.00, 0.85),
    "plaque_close": (0.00, -0.95, 0.62),
    "wall_close": (-1.10, -1.10, 0.10),
}
TARGET_LIFT = {"arch_close": 0.34, "plaque_close": 0.72, "roof_close": 0.86, "roof_side": 0.82,
               "wall_close": 0.14, "low": 0.30}


def _path(value):
    return value.get_path_name() if value else ""


def main():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    assert building
    origin, extent = building.get_actor_bounds(False)
    spawned = [building]

    ground = actors.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0)
    )
    ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    ground.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    material = unreal.EditorAssetLibrary.load_asset(
        "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround"
    )
    if material:
        ground.static_mesh_component.set_material(0, material)
    spawned.append(ground)

    key = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55)
    )
    key.light_component.set_intensity(7.0)
    spawned.append(key)
    fill = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125)
    )
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
    for name, value in (("override_auto_exposure_min_brightness", True),
                        ("override_auto_exposure_max_brightness", True),
                        ("auto_exposure_min_brightness", 1.0),
                        ("auto_exposure_max_brightness", 1.0)):
        settings.set_editor_property(name, value)
    post.set_editor_property("settings", settings)
    post.set_actor_location(center, False, False)
    spawned.append(post)

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10",
                    "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)

    radius = max(extent.x, extent.y, extent.z)
    captures = []
    for view_name, offset in VIEWS.items():
        lift = TARGET_LIFT.get(view_name, 0.50)
        target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * lift)
        camera = target + unreal.Vector(offset[0] * radius, offset[1] * radius, offset[2] * radius)
        editor.set_level_viewport_camera_info(
            camera, unreal.MathLibrary.find_look_at_rotation(camera, target)
        )
        output = ROOT / (PREFIX + view_name + ".png")
        unreal.AutomationLibrary.take_high_res_screenshot(
            1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW,
            "Wuxianmen reftune2 " + view_name, 2.0, True,
        )
        captures.append(str(output))
        print("REFTUNE2_CAPTURE_REQUESTED", view_name, str(output))

    for actor in spawned:
        actors.destroy_actor(actor)
    leaked = [
        _path(actor) for actor in actors.get_all_level_actors()
        if actor.get_name().startswith("BP_Wuxianmen_V5_4K_Core")
    ]
    assert not leaked, leaked
    (ROOT / "Wuxianmen_V5_4K_Core-reftune2-captures-20260917.json").write_text(
        json.dumps({
            "views": list(VIEWS),
            "captures": captures,
            "actor_bounds_origin": [origin.x, origin.y, origin.z],
            "actor_bounds_extent": [extent.x, extent.y, extent.z],
            "temporary_actors_destroyed": True,
            "map_saved": False,
        }, indent=2),
        encoding="utf-8",
    )
    print("REFTUNE2_CAPTURE_COMPLETE", len(captures))


if __name__ == "__main__":
    main()
