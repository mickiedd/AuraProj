"""Staged capture harness for Guidemen, so the rebuild can be checked visually.

The capture harness has been writing one stale frame per view for several passes, which is
why all recent work is geometry-only. The editor has since been restarted, so this re-tests
the deferred high-res screenshot with the same staged pattern that worked before it broke:
one `setup` invocation that leaves the transient actors in the world, then one capture per
invocation, then `teardown`.

The action and view come from Saved/RawModelImport/V5/GuidemenRebuild/stage.json.

Views are chosen for the open question: a top-down view and a side elevation show the ridge
line and each slope's direction directly.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild"
SPEC = ROOT / "stage.json"
SPAWNED = ROOT / "stage-spawned.json"
PREFIX = "Guidemen_V5_4K-"

BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"

# offset multipliers of the actor radius, plus the height fraction to look at
VIEWS = {
    "top": (0.00, 0.00, 3.20, 0.50),
    "hero": (1.70, 1.70, 0.92, 0.50),
    "front": (0.00, 2.60, 0.40, 0.50),
    "rear": (0.00, -2.60, 0.40, 0.50),
    "side": (2.60, 0.00, 0.48, 0.50),
    "roof_close": (0.75, -1.35, 1.15, 0.86),
    "low": (0.00, 2.00, 0.18, 0.30),
}


def _path(value):
    return value.get_path_name() if value else ""


def setup():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    blueprint = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    building = actors.spawn_actor_from_class(blueprint.generated_class(), center)
    assert building, "spawn failed"
    origin, extent = building.get_actor_bounds(False)
    spawned = [building]

    ground = actors.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(origin.x, origin.y, origin.z - extent.z - 8.0))
    ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    ground.set_actor_scale3d(unreal.Vector(1000, 1000, 1))
    material = unreal.EditorAssetLibrary.load_asset(
        "/Game/Assets/Environment/GuangzhouLandmarks/V5/M_V5PreviewGround")
    if material:
        ground.static_mesh_component.set_material(0, material)
    spawned.append(ground)

    key = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-38, yaw=-55))
    key.light_component.set_intensity(7.0)
    spawned.append(key)
    fill = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
    fill.light_component.set_intensity(2.0)
    fill.light_component.set_editor_property("cast_shadows", False)
    spawned.append(fill)
    spawned.append(actors.spawn_actor_from_class(unreal.SkyAtmosphere, center))
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawned.append(sky)
    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, center)
    post.set_editor_property("unbound", True)
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

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10",
                    "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)

    record = {"actors": [_path(a) for a in spawned],
              "origin": [origin.x, origin.y, origin.z],
              "extent": [extent.x, extent.y, extent.z]}
    SPAWNED.write_text(json.dumps(record, indent=1), encoding="utf-8")
    print("GUIDEMEN_STAGE_SETUP", len(spawned), json.dumps(record["extent"]))


def view(view_name):
    record = json.loads(SPAWNED.read_text(encoding="utf-8"))
    origin = unreal.Vector(*record["origin"])
    extent = unreal.Vector(*record["extent"])
    radius = max(extent.x, extent.y, extent.z)
    offset_x, offset_y, offset_z, lift = VIEWS[view_name]
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * lift)
    camera = target + unreal.Vector(offset_x * radius, offset_y * radius, offset_z * radius)
    rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera, rotation)
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
    except Exception as error:
        print("GUIDEMEN_INVALIDATE_FAILED", repr(error)[:100])
    output = ROOT / (PREFIX + "rebuild-" + view_name + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(
        1600, 1000, str(output), None, False, False, unreal.ComparisonTolerance.LOW,
        "Guidemen " + view_name, 2.0, True)
    print("GUIDEMEN_STAGE_VIEW", view_name, str(output),
          "camera", [round(v, 1) for v in (camera.x, camera.y, camera.z)])


def teardown():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    record = json.loads(SPAWNED.read_text(encoding="utf-8"))
    destroyed = 0
    for path in record["actors"]:
        actor = unreal.load_object(None, path)
        if actor:
            actors.destroy_actor(actor)
            destroyed += 1
    leaked = [a.get_name() for a in actors.get_all_level_actors()
              if a.get_name().startswith("BP_Guidemen") or a.get_name().startswith("SceneCapture")]
    print("GUIDEMEN_STAGE_TEARDOWN", destroyed, "leaked", json.dumps(leaked))


def main():
    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    action = spec["action"]
    if action == "setup":
        setup()
    elif action == "view":
        view(spec["view"])
    elif action == "teardown":
        teardown()
    else:
        raise AssertionError(action)


if __name__ == "__main__":
    main()
