"""Render the showcase level to PPM images for visual verification.

Uses the capture path this project established for these assets:
  SceneCapture2D -> persistent TextureRenderTarget2D
                -> RenderingLibrary.read_render_target(world, rt, True)
                -> binary PPM (P6) written from inside the editor

Notes carried over from earlier passes on this project:
  * export_render_target writes nothing in this build.
  * The editor's bundled Python has no numpy and no PIL, so pixels leave as PPM
    and are converted to PNG by the local interpreter.
  * set_visibility does not affect captures; collision does.

The capture actor is destroyed and the actor count is checked before and after,
because a leaked capture setup silently poisons every later image. The level is
NOT saved, so the capture actor never persists.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")

WIDTH = 1600
HEIGHT = 900

# Exposure is pinned here as well as in the level. A single capture_scene() does
# not give auto-exposure time to converge, and the level's own unbound volume does
# not reproduce the value found by bracketing when the frame is captured - a
# runtime volume at higher priority does. Pinning both keeps the captures
# deterministic and representative.
EXPOSURE_EV100 = 3.0
CAPTURE_PPV_PRIORITY = 10.0

# (name, camera location cm, camera rotator, field of view deg)
# Rotators are built with keyword arguments: positional order is (roll, pitch, yaw).
VIEWS = [
    ("top", unreal.Vector(0, 0, 30000),
     unreal.Rotator(pitch=-90.0, yaw=0.0, roll=0.0), 62.0),
    ("hero", unreal.Vector(0, -42000, 26000),
     unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0), 60.0),
    # Eye level, standing in the plaza on the Zhengnanmen radius, looking out at
    # that gate's front. This is the view that shows whether the landmarks are
    # grounded and whether they face the plaza.
    ("plaza", unreal.Vector(6295, 953, 400),
     unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0), 60.0),
]


def ensure_render_target():
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    if render_target:
        return render_target
    folder = RT_PATH.rsplit("/", 1)[0]
    factory = unreal.TextureRenderTargetFactoryNew()
    render_target = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "RT_ShowcaseCapture", folder, unreal.TextureRenderTarget2D, factory)
    assert render_target, RT_PATH
    render_target.set_editor_property("size_x", WIDTH)
    render_target.set_editor_property("size_y", HEIGHT)
    try:
        render_target.set_editor_property(
            "render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
    except Exception:
        pass
    assert unreal.EditorAssetLibrary.save_asset(RT_PATH, only_if_is_dirty=False), RT_PATH
    return render_target


def write_ppm(path, colors, width, height, flip_vertical=True):
    """Binary PPM (P6). read_render_target returns rows bottom-up."""
    buffer = bytearray()
    rows = range(height - 1, -1, -1) if flip_vertical else range(height)
    for row in rows:
        base = row * width
        for column in range(width):
            color = colors[base + column]
            buffer.append(min(255, max(0, int(float(color.r) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.g) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.b) * 255.0))))
    with open(path, "wb") as handle:
        handle.write("P6\n{} {}\n255\n".format(width, height).encode("ascii"))
        handle.write(bytes(buffer))


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    before = len(actor_subsystem.get_all_level_actors())
    render_target = ensure_render_target()

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert volume, "capture post-process volume"
    volume.set_actor_label("TEMP_CaptureExposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", CAPTURE_PPV_PRIORITY)
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", EXPOSURE_EV100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", EXPOSURE_EV100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert capture, "SceneCapture2D"
    capture.set_actor_label("TEMP_ShowcaseCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    assert component, "SceneCaptureComponent2D"
    component.set_editor_property("texture_target", render_target)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    # This used to set three more flags here, inside try/except:
    #
    #   capture_every_frame = False
    #   b_always_persist_rendering_state = True
    #   b_capture_on_construction = False
    #
    # The last two DO NOT EXIST on this build's SceneCaptureComponent2D (the real
    # name is `always_persist_rendering_state`, with no `b_`), so the try/except
    # silently swallowed the failure and the script reported a configuration it
    # had never applied. `capture_every_frame = False` did take, and it breaks
    # exposure control: with it set, pinning the exposure to EV100 3.0 and 7.0
    # produced means differing by 5e-5, while with default flags the same two
    # exposures separate by a factor of sixty
    # (Scripts/ProbeShowcaseCapturePersistenceFlag.py). That is why this script's
    # captures came out washed out and irreproducible while the standalone
    # exposure bracket - which never set these flags - did not.
    #
    # The flags are gone. The capture now uses its defaults, which is the
    # configuration in which the level's own exposure pin actually applies.

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        # Warm-up: a freshly loaded world has not ticked, so the lights' rendering
        # state is not registered on the first capture and the scene renders as
        # flat base colour. Capturing once and discarding the result, then
        # invalidating the viewports, gives the lights a frame to come up.
        capture.set_actor_location(VIEWS[0][1], False, True)
        capture.set_actor_rotation(VIEWS[0][2], False)
        component.capture_scene()
        try:
            unreal.EditorLevelLibrary.editor_invalidate_viewports()
        except Exception:
            pass
        component.capture_scene()

        for name, location, rotation, fov in VIEWS:
            capture.set_actor_location(location, False, True)
            capture.set_actor_rotation(rotation, False)
            component.set_editor_property("fov_angle", fov)
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            count = len(colors)
            path = OUT_DIR / "showcase-{}.ppm".format(name)
            if count != WIDTH * HEIGHT:
                results.append({"view": name, "error": "expected {} pixels, got {}".format(
                    WIDTH * HEIGHT, count)})
                continue
            write_ppm(path, colors, WIDTH, HEIGHT)
            results.append({"view": name, "file": str(path), "pixels": count,
                            "camera_location": [round(location.x, 1), round(location.y, 1),
                                                round(location.z, 1)],
                            "fov": fov})
            print("SHOWCASE_CAPTURE", name, str(path), count)
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    after = len(actor_subsystem.get_all_level_actors())
    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    report = {"level": LEVEL_PATH, "actor_count_before": before, "actor_count_after": after,
              "leaked_actors": leaked, "views": results, "level_saved": False}
    (OUT_DIR / "capture-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_CAPTURE_REPORT " + json.dumps(report))
    print("SHOWCASE_CAPTURE_REPORT", json.dumps(report, indent=2))
    assert not leaked, "capture actor leaked: " + ", ".join(leaked)
    assert before == after, "actor count changed: {} -> {}".format(before, after)


if __name__ == "__main__":
    main()
