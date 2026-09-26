"""Capture the actual BP_Zhengdongmen from five views without saving a map.
Use RunZhengdongmenNativeReview.py for a fresh GUI process with shader warmup.
The imported facade is +Y. Stage name is read from Saved/Reports/Zhengdongmen.
"""

import json
import struct
import zlib
from pathlib import Path

import unreal

ROOT = "/Game/Assets/Environment/GuangzhouLandmarks/Zhengdongmen"
BLUEPRINT_PATH = ROOT + "/BP_Zhengdongmen"
MESH_ROOT = ROOT + "/Meshes"
MATERIAL_GROUPS = ["Stone", "Wood", "RoofTile", "Ridge", "Plaster", "Iron", "DoorWood", "Sign"]

PROJECT = Path(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
OUT_DIR = PROJECT / "Saved" / "Reports" / "Zhengdongmen"
WIDTH, HEIGHT = 1600, 1000

# (name, camera offset as a multiple of the building's extents)
STAGE = (PROJECT / "Saved/Reports/Zhengdongmen/capture-stage.txt").read_text().strip()
# offsets in cm from the building pivot, target in cm from its pivot, horizontal FOV
VIEWS = [
    ("front", (0,4600,1600), (0,0,930), 48),
    ("three-quarter", (3400,4400,2400), (0,0,960), 48),
    ("arch", (0,1750,310), (0,0,310), 48),
    ("timber", (2100,2500,1850), (0,0,1430), 52),
    ("rear", (-3200,-4300,2200), (0,0,960), 48),
    # Straight onto the plaque, to read the inscription, and onto the lower roof
    # slope, where course continuity is the thing being verified.
    ("plaque", (0,1090,1112), (0,590,1112), 40),
    ("roof", (0,1500,1950), (0,0,1290), 46),
]



def png_chunk(tag, payload):
    data = tag + payload
    return (struct.pack(">I", len(payload)) + data
            + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF))


def write_png(filename, pixels, width, height):
    """Write an RGBA->RGB PNG. Assumes top-down rows (macOS/Metal build)."""
    assert len(pixels) == width * height, len(pixels)
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        for x in range(width):
            pixel = pixels[y * width + x]
            raw.extend((pixel.r, pixel.g, pixel.b))
    payload = b"\x89PNG\r\n\x1a\n"
    payload += png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    payload += png_chunk(b"IDAT", zlib.compress(bytes(raw), 6))
    payload += png_chunk(b"IEND", b"")
    filename.write_bytes(payload)


def report_nanite():
    for group in MATERIAL_GROUPS:
        mesh = unreal.EditorAssetLibrary.load_asset(
            "{}/SM_ZDM_{}".format(MESH_ROOT, group))
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        settings = mesh.get_editor_property("nanite_settings")
        print("FALLBACK_SETTINGS", group,
              "enabled", settings.get_editor_property("enabled"),
              "target", settings.get_editor_property("fallback_target"),
              "percent", settings.get_editor_property("fallback_percent_triangles"),
              "error", settings.get_editor_property("fallback_relative_error"),
              "render_triangles", mesh.get_num_triangles(0))


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    library = unreal.EditorAssetLibrary
    before = len(actors.get_all_level_actors())

    blueprint = library.load_asset(BLUEPRINT_PATH)
    if not isinstance(blueprint, unreal.Blueprint) or not blueprint.generated_class():
        raise TypeError("Not a Blueprint with a generated class: " + BLUEPRINT_PATH)

    report_nanite()

    spawned = []
    results = []
    try:
        # Spawn far away so the surrounding level does not intrude on the capture.
        building = actors.spawn_actor_from_class(blueprint.generated_class(),
                                                 unreal.Vector(100000, 100000, 0))
        assert building, "Blueprint spawn failed"
        spawned.append(building)
        origin, extent = building.get_actor_bounds(False)
        print("CAPTURE_BOUNDS", [origin.x, origin.y, origin.z],
              [extent.x, extent.y, extent.z])

        # Camera and lights are shared across views; only the camera moves.
        target = origin + unreal.Vector(0, 0, extent.z * 0.05)
        key = actors.spawn_actor_from_class(
            unreal.DirectionalLight,
            target + unreal.Vector(extent.x * 0.4, extent.x * 1.4, extent.z * 1.6),
            unreal.MathLibrary.find_look_at_rotation(
                target + unreal.Vector(extent.x * 0.4, extent.x * 1.4, extent.z * 1.6),
                target))
        key.light_component.set_intensity(20.0)
        spawned.append(key)

        fill = actors.spawn_actor_from_class(
            unreal.DirectionalLight, origin + unreal.Vector(0, 0, 5000),
            unreal.Rotator(pitch=-30, yaw=100))
        fill.light_component.set_intensity(6.0)
        fill.light_component.set_editor_property("cast_shadows", False)
        spawned.append(fill)

        spawned.append(actors.spawn_actor_from_class(unreal.SkyAtmosphere, origin))
        skylight = actors.spawn_actor_from_class(
            unreal.SkyLight, origin + unreal.Vector(0, 0, 3500))
        skylight.light_component.set_intensity(1.4)
        skylight.light_component.set_editor_property("real_time_capture", True)
        spawned.append(skylight)

        render_target = unreal.RenderingLibrary.create_render_target2d(
            world, WIDTH, HEIGHT, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        assert render_target, "Transient render target creation failed"

        for name, offset, aim, fov in VIEWS:
            pivot=building.get_actor_location()
            camera=pivot+unreal.Vector(*offset)
            target=pivot+unreal.Vector(*aim)
            rotation = unreal.MathLibrary.find_look_at_rotation(camera, target)
            capture = actors.spawn_actor_from_class(unreal.SceneCapture2D, camera,
                                                    rotation)
            assert capture, "SceneCapture2D spawn failed"
            spawned.append(capture)
            component = capture.capture_component2d
            component.set_editor_property("texture_target", render_target)
            component.set_editor_property(
                "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            component.set_editor_property("capture_every_frame", True)
            component.set_editor_property("fov_angle", fov)
            component.set_editor_property("post_process_blend_weight", 1.0)
            settings = component.get_editor_property("post_process_settings")
            settings.set_editor_property("override_auto_exposure_method", True)
            settings.set_editor_property("auto_exposure_method",
                                         unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property("override_auto_exposure_bias", True)
            settings.set_editor_property("auto_exposure_bias", 8.0)
            component.set_editor_property("post_process_settings", settings)
            component.capture_scene()
            component.capture_scene()

            pixels = unreal.RenderingLibrary.read_render_target(world, render_target,
                                                                True)
            assert len(pixels) == WIDTH * HEIGHT, len(pixels)
            out = OUT_DIR / ("native-{}-{}.png".format(STAGE,name))
            write_png(out, pixels, WIDTH, HEIGHT)
            results.append({"view": name, "path": str(out),
                            "bytes": out.stat().st_size})
            print("NATIVE_CAPTURE", json.dumps(results[-1]))
            actors.destroy_actor(capture)
            spawned.remove(capture)
    finally:
        for actor in reversed(spawned):
            actors.destroy_actor(actor)
        after = len(actors.get_all_level_actors())
        print("CAPTURE_CLEANUP", before, after)
        assert before == after, "Transient actors leaked"

    (OUT_DIR / "native-capture.json").write_text(json.dumps({"views": results,"cleanup_before":before,"cleanup_after":after,"stage":STAGE},indent=2))
    print("ZHENGDONGMEN_CAPTURE_COMPLETE " + json.dumps({"views": results}))


if __name__ == "__main__":
    main()
