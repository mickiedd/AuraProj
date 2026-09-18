"""Viewport-independent capture for Guidemen, using a SceneCapture2D and a render-target readback.

The deferred high-res screenshot renders from the editor viewport, which in this environment
writes one stale frame per view — every capture since the harness broke has been identical.
This bypasses the viewport entirely:

  SceneCapture2D -> persistent TextureRenderTarget2D -> RenderingLibrary.read_render_target()
  -> PNG written with PIL

`read_render_target` flushes rendering commands internally and returns the whole target as
sRGB colours, so the image is the one the capture just rendered. `export_render_target`
writes nothing in this build, which is why the earlier attempt at this route failed.

Staged like the old harness so the transient actors persist between invocations:
  action "setup" | "view" | "teardown", with the view name, read from
  Saved/RawModelImport/V5/GuidemenRebuild/rt-stage.json.
"""
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT / "Saved/RawModelImport/V5/GuidemenRebuild"
SPEC = ROOT / "rt-stage.json"
STATE = ROOT / "rt-stage-state.json"
PREFIX = "Guidemen_V5_4K-rebuild-"

BLUEPRINT = "/Game/Assets/Environment/GuangzhouLandmarks/V5/Guidemen_4K/BP_Guidemen_V5_4K"
RT_DIR = "/Game/Assets/Environment/GuangzhouLandmarks/V5/GuidemenRebuild"
RT_NAME = "RT_GuidemenCapture"
WIDTH, HEIGHT = 1600, 1000

VIEWS = {
    "top": (0.00, 0.00, 1.60, 0.50),
    "hero": (0.95, 0.95, 0.55, 0.50),
    "front": (0.00, 1.45, 0.25, 0.45),
    "rear": (0.00, -1.45, 0.25, 0.45),
    "side": (1.45, 0.00, 0.30, 0.45),
    "roof_close": (0.45, -0.85, 0.70, 0.82),
    "low": (0.00, 1.20, 0.12, 0.28),
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
    key.light_component.set_intensity(12.0)
    spawned.append(key)
    fill = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-25, yaw=125))
    fill.light_component.set_intensity(2.0)
    fill.light_component.set_editor_property("cast_shadows", False)
    spawned.append(fill)
    front = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-30, yaw=-90))
    front.light_component.set_intensity(6.0)
    front.light_component.set_editor_property("cast_shadows", False)
    spawned.append(front)
    front = actors.spawn_actor_from_class(
        unreal.DirectionalLight, center + unreal.Vector(0, 0, 4000), unreal.Rotator(pitch=-30, yaw=-90))
    front.light_component.set_intensity(6.0)
    front.light_component.set_editor_property("cast_shadows", False)
    spawned.append(front)
    spawned.append(actors.spawn_actor_from_class(unreal.SkyAtmosphere, center))
    sky = actors.spawn_actor_from_class(unreal.SkyLight, center + unreal.Vector(0, 0, 3500))
    sky.light_component.set_intensity(1.4)
    sky.light_component.set_editor_property("real_time_capture", True)
    spawned.append(sky)

    unreal.EditorAssetLibrary.make_directory(RT_DIR)
    rt_path = RT_DIR + "/" + RT_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(rt_path):
        unreal.EditorAssetLibrary.delete_asset(rt_path)
    rt = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        RT_NAME, RT_DIR, unreal.TextureRenderTarget2D, unreal.TextureRenderTargetFactoryNew())
    rt.set_editor_property("size_x", WIDTH)
    rt.set_editor_property("size_y", HEIGHT)
    rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
    unreal.EditorAssetLibrary.save_loaded_asset(rt)

    capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, center)
    component = capture.capture_component2d
    component.set_editor_property("texture_target", rt)
    component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property("capture_every_frame", True)
    component.set_editor_property("fov_angle", 90.0)
    component.set_editor_property("post_process_blend_weight", 1.0)
    settings = component.get_editor_property("post_process_settings")
    settings.set_editor_property("override_auto_exposure_method", True)
    settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 11.0)
    component.set_editor_property("post_process_settings", settings)
    spawned.append(capture)

    STATE.write_text(json.dumps({
        "actors": [_path(a) for a in spawned],
        "capture": _path(capture),
        "render_target": _path(rt),
        "origin": [origin.x, origin.y, origin.z],
        "extent": [extent.x, extent.y, extent.z],
    }, indent=1), encoding="utf-8")
    print("RT_SETUP actors", len(spawned), "extent", [round(v, 1) for v in (extent.x, extent.y, extent.z)])


def view(view_name):
    state = json.loads(STATE.read_text(encoding="utf-8"))
    origin = unreal.Vector(*state["origin"])
    extent = unreal.Vector(*state["extent"])
    radius = max(extent.x, extent.y, extent.z)
    offset_x, offset_y, offset_z, lift = VIEWS[view_name]
    target = unreal.Vector(origin.x, origin.y, origin.z + extent.z * lift)
    camera = target + unreal.Vector(offset_x * radius, offset_y * radius, offset_z * radius)
    rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)

    capture = unreal.load_object(None, state["capture"])
    assert capture, state["capture"]
    capture.set_actor_location_and_rotation(camera, rotation, False, False)
    component = capture.capture_component2d
    component.capture_scene()

    rt = unreal.load_object(None, state["render_target"])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    colours = unreal.RenderingLibrary.read_render_target(world, rt, True)
    assert colours, "read_render_target returned nothing"

    # The editor's Python has no numpy and no PIL, so write a binary PPM (P6) here and
    # convert it to PNG outside the editor. FColor is BGRA.
    buffer = bytearray(WIDTH * HEIGHT * 3)
    index = 0
    for colour in colours:
        buffer[index] = colour.b
        buffer[index + 1] = colour.g
        buffer[index + 2] = colour.r
        index += 3
    output = ROOT / (PREFIX + view_name + ".ppm")
    with output.open("wb") as handle:
        handle.write(b"P6\n%d %d\n255\n" % (WIDTH, HEIGHT))
        handle.write(buffer)
    mean = sum(buffer) / len(buffer) / 255.0
    print("RT_VIEW", view_name, str(output), "mean", round(mean, 4),
          "camera", [round(v, 1) for v in (camera.x, camera.y, camera.z)])


def teardown():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    state = json.loads(STATE.read_text(encoding="utf-8"))
    destroyed = 0
    for path in state["actors"]:
        actor = unreal.load_object(None, path)
        if actor:
            actors.destroy_actor(actor)
            destroyed += 1
    leaked = [a.get_name() for a in actors.get_all_level_actors()
              if a.get_name().startswith("BP_Guidemen") or a.get_name().startswith("SceneCapture")]
    print("RT_TEARDOWN", destroyed, "leaked", json.dumps(leaked))


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
