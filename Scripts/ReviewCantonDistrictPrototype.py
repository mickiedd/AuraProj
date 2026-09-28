"""Reload, validate and photograph the isolated provisional district map."""
import json
import struct
import time
import traceback
import zlib
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
OUT = ROOT / "QA/Canton_District"
OUT.mkdir(parents=True, exist_ok=True)
REVIEW_VIEWS = ROOT / "Review/M05_QA_Screenshots"
REVIEW_VIEWS.mkdir(parents=True, exist_ok=True)
MAP = "/Game/Canton/DistrictPrototype/Maps/L_Canton_District_PROVISIONAL"
RAW = ROOT / "Export/Provisional/Canton_Modern_Context_SouthFirst.r16"
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world and unreal.CantonTerrainLibrary.load_provisional_region(world)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.monotonic()


def write_png(path, pixels, width, height):
    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff))
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for pixel in pixels[y * width:(y + 1) * width]:
            raw.extend((pixel.r, pixel.g, pixel.b))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                     chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def finish(_delta):
    if time.monotonic() - started < 35:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        validation = json.loads(unreal.CantonTerrainLibrary.validate_district_world(world, str(RAW)))
        (OUT / "Reload_Validation.json").write_text(json.dumps(validation, indent=2) + "\n")
        unreal.SystemLibrary.execute_console_command(world, "MAP CHECK")
        actor_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        width, height = 1440, 1000
        target = unreal.RenderingLibrary.create_render_target2d(
            world, width, height, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        views = [
            ("district_overview", (151000, 102000, 17000), (180000, 130000, 900)),
            ("gate_approach", (180000, 109000, 3200), (180000, 121500, 1500)),
            ("main_street", (178000, 123000, 1550), (180000, 134000, 900)),
            ("mixed_lane", (190000, 132000, 2200), (180000, 132000, 900)),
            ("courtyard_edge", (189000, 138000, 2400), (185000, 136000, 1000)),
        ]
        records = []
        for name, eye, aim in views:
            camera = actor_api.spawn_actor_from_class(
                unreal.SceneCapture2D, unreal.Vector(*eye),
                unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*aim)))
            component = camera.capture_component2d
            component.set_editor_property("texture_target", target)
            component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            component.set_editor_property("fov_angle", 65)
            settings = component.get_editor_property("post_process_settings")
            settings.set_editor_property("override_auto_exposure_method", True)
            settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property("override_auto_exposure_bias", True)
            settings.set_editor_property("auto_exposure_bias", 11)
            component.set_editor_property("post_process_settings", settings)
            component.capture_scene()
            pixels = unreal.RenderingLibrary.read_render_target(world, target, True)
            path = OUT / (name + ".png")
            write_png(path, pixels, width, height)
            (REVIEW_VIEWS / path.name).write_bytes(path.read_bytes())
            records.append({"view": name, "camera_cm": eye,
                            "target_cm": aim, "path": str(path.relative_to(ROOT))})
            actor_api.destroy_actor(camera)
        damp_actors = [actor for actor in actor_api.get_all_level_actors()
                       if actor.get_actor_label().startswith("District_LocalDamp_")]
        assert len(damp_actors) == 14
        eye, aim = (181200, 127650, 1300), (180900, 127900, 870)
        for name, hidden in [("dry_ground", True), ("after_rain_ground", False)]:
            for actor in damp_actors:
                actor.set_is_temporarily_hidden_in_editor(hidden)
            camera = actor_api.spawn_actor_from_class(
                unreal.SceneCapture2D, unreal.Vector(*eye),
                unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*aim)))
            component = camera.capture_component2d
            component.set_editor_property("texture_target", target)
            component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            component.set_editor_property("fov_angle", 65)
            settings = component.get_editor_property("post_process_settings")
            settings.set_editor_property("override_auto_exposure_method", True)
            settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property("override_auto_exposure_bias", True)
            settings.set_editor_property("auto_exposure_bias", 11)
            component.set_editor_property("post_process_settings", settings)
            component.capture_scene()
            path = OUT / (name + ".png")
            write_png(path, unreal.RenderingLibrary.read_render_target(world, target, True), width, height)
            (REVIEW_VIEWS / path.name).write_bytes(path.read_bytes())
            records.append({"view": name, "camera_cm": eye, "target_cm": aim,
                            "path": str(path.relative_to(ROOT)), "damp_actors_hidden": hidden})
            actor_api.destroy_actor(camera)
        for actor in damp_actors:
            actor.set_is_temporarily_hidden_in_editor(False)
        (OUT / "Capture_Settings.json").write_text(json.dumps(records, indent=2) + "\n")
        assert validation["passed"], validation
        (OUT / "Review_Error.txt").unlink(missing_ok=True)
    except Exception:
        (OUT / "Review_Error.txt").write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(finish)
