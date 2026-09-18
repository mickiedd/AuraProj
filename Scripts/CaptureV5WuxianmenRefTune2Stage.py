"""Staged capture harness for the Wuxianmen V5 reference-tuning passes.

take_high_res_screenshot writes the file about two seconds after the request, so
requesting several captures in one invocation and then destroying the transient
actors races the renderer and yields black or one-frame-lagged frames. This
harness splits the work:

  action "setup"     spawn the transient building, ground, lighting, post volume
                     and record their paths for the requested variant
  action "view"      set the viewport camera from the sidecar view spec and
                     request exactly one capture; nothing is destroyed, so the
                     deferred write lands while the editor is idle
  action "teardown"  destroy every recorded actor for the variant and verify

The action, view and variant come from Wuxianmen_V5-reftune2-stage.json, written
by the caller before each invocation. Never saves a map.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5"
SPEC = ROOT / "Wuxianmen_V5-reftune2-stage.json"

VARIANTS = {
    "Core": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_4K_Core/BP_Wuxianmen_V5_4K_Core",
        "prefix": "Wuxianmen_V5_4K_Core-reftune2-",
        "spawned": "Wuxianmen_V5_4K_Core-reftune2-spawned-20260917.json",
    },
    "FullPBR": {
        "blueprint": "/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR",
        "prefix": "Wuxianmen_V5_FullPBR-reftune2-",
        "spawned": "Wuxianmen_V5_FullPBR-reftune2-spawned-20260917.json",
    },
}

# Offset multipliers of the actor radius, plus the height fraction to look at.
VIEWS = {
    "hero": (1.70, 1.70, 0.92, 0.50),
    "front_doors": (0.00, 2.60, 0.40, 0.50),
    "front_plaque": (0.00, -2.60, 0.40, 0.50),
    "side_l": (-2.60, 0.00, 0.48, 0.50),
    "side_r": (2.60, 0.00, 0.48, 0.50),
    "low": (0.00, 2.00, 0.18, 0.30),
    "top": (0.00, 0.00, 3.20, 0.50),
    "arch_close": (0.00, 1.18, 0.30, 0.34),
    "roof_close": (0.75, -1.35, 1.15, 0.86),
    "roof_side": (2.30, 0.00, 0.85, 0.82),
    "plaque_close": (0.00, -0.95, 0.62, 0.72),
    "wall_close": (-1.10, -1.10, 0.10, 0.14),
    # Absolute-camera view: the plaque is a fixed 4.9 x 1.45 m panel at
    # component-local (0, -328, 1205) cm, so it is framed by distance, not radius.
    "plaque_tight": ("absolute", (0.0, -618.0, 1205.0), (0.0, -328.0, 1205.0)),
}


def _path(value):
    return value.get_path_name() if value else ""


def setup(variant):
    config = VARIANTS[variant]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    center = unreal.Vector(100000.0, 100000.0, 0.0)
    blueprint = unreal.EditorAssetLibrary.load_asset(config["blueprint"])
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
    spawned.append(actors.spawn_actor_from_class(unreal.SkyAtmosphere, center))
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawned.append(sky)
    post = actors.spawn_actor_from_class(unreal.PostProcessVolume, center)
    # Unbound so the exposure override applies to the whole frame, not just the
    # volume brush. With a bounded volume the background keeps the level's own
    # exposure and the two variants are not comparable.
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

    # An explicit camera actor drives the capture. Relying on the editor viewport
    # camera proved unreliable: the deferred high-res screenshot can ignore
    # set_level_viewport_camera_info and write the same frame for every view.
    camera_actor = actors.spawn_actor_from_class(unreal.CameraActor, center)
    camera_actor.set_actor_label("RefTune2CaptureCamera")
    spawned.append(camera_actor)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for command in ("r.TextureStreaming 0", "r.ScreenPercentage 100", "r.ViewDistanceScale 10",
                    "ShowFlag.Grid 0", "ShowFlag.SelectionOutline 0"):
        unreal.SystemLibrary.execute_console_command(world, command)

    record = {
        "variant": variant,
        "blueprint": config["blueprint"],
        "actors": [_path(actor) for actor in spawned],
        "camera_actor": _path(camera_actor),
        "location": [center.x, center.y, center.z],
        "origin": [origin.x, origin.y, origin.z],
        "extent": [extent.x, extent.y, extent.z],
    }
    (ROOT / config["spawned"]).write_text(json.dumps(record, indent=2), encoding="utf-8")
    print("REFTUNE2_STAGE_SETUP", variant, len(spawned), json.dumps(record["extent"]))


def view(variant, view_name, delay=2.0):
    config = VARIANTS[variant]
    record = json.loads((ROOT / config["spawned"]).read_text(encoding="utf-8"))
    origin = unreal.Vector(*record["origin"])
    extent = unreal.Vector(*record["extent"])
    spec = VIEWS[view_name]
    if spec[0] == "absolute":
        # The plaque is placed relative to the actor location, not the bounds
        # origin (the bounds origin sits at the building's vertical centre).
        _, camera_offset, target_offset = spec
        anchor = unreal.Vector(*record["location"])
        camera = anchor + unreal.Vector(*camera_offset)
        target = anchor + unreal.Vector(*target_offset)
    else:
        offset_x, offset_y, offset_z, lift = spec
        radius = max(extent.x, extent.y, extent.z)
        target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * lift)
        camera = target + unreal.Vector(offset_x * radius, offset_y * radius, offset_z * radius)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(camera, unreal.MathLibrary.find_look_at_rotation(camera, target))
    # Without an explicit invalidate the editor viewports do not repaint while the
    # editor window is in the background, so the deferred high-res screenshot
    # captures a stale frame from whatever camera was last drawn.
    try:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_invalidate_viewports()
    except Exception as error:
        print("REFTUNE2_INVALIDATE_FAILED", repr(error)[:120])
    camera_actor = unreal.load_object(None, record["camera_actor"])
    assert camera_actor, record["camera_actor"]
    camera_actor.set_actor_location_and_rotation(
        camera, unreal.MathLibrary.find_look_at_rotation(camera, target), False, False
    )
    output = ROOT / (config["prefix"] + view_name + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(
        1600, 1000, str(output), camera_actor, False, False, unreal.ComparisonTolerance.LOW,
        "Wuxianmen reftune2 " + variant + " " + view_name, float(delay), True,
    )
    print("REFTUNE2_STAGE_VIEW", variant, view_name, str(output), "delay", delay,
          "camera", [round(v, 1) for v in (camera.x, camera.y, camera.z)])


def teardown(variant):
    config = VARIANTS[variant]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    record = json.loads((ROOT / config["spawned"]).read_text(encoding="utf-8"))
    destroyed = 0
    names = []
    for path in record["actors"]:
        actor = unreal.load_object(None, path)
        if actor:
            names.append(actor.get_name())
            actors.destroy_actor(actor)
            destroyed += 1
    leaked = [actor.get_name() for actor in actors.get_all_level_actors() if actor.get_name() in names]
    print("REFTUNE2_STAGE_TEARDOWN", variant, destroyed, "leaked", json.dumps(leaked))


def main():
    spec = json.loads(SPEC.read_text(encoding="utf-8"))
    action = spec["action"]
    variant = spec.get("variant", "FullPBR")
    if action == "setup":
        setup(variant)
    elif action == "view":
        view(variant, spec["view"], spec.get("delay", 2.0))
    elif action == "teardown":
        teardown(variant)
    else:
        raise AssertionError(action)


if __name__ == "__main__":
    main()
