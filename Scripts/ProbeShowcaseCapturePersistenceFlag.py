"""Isolate which capture flag breaks exposure control on the SceneCapture2D path.

The trail so far:

  * Saved/RawModelImport/GuangzhouLandmarkShowcase/ev-{20,30,40,50}.png - the
    bracket taken on 2026-09-21 - DID respond to exposure (mean luminance
    215.13 / 199.96 / 85.32 / 24.80). So an unbound PostProcessVolume at
    priority 10 does control a scene capture on this build.
  * BracketShowcaseLandmarkLight.py used the same volume mechanism and got
    byte-comparable frames at EV100 3.0, 4.0, 5.0 and 6.0.
  * ProbeShowcaseCaptureExposureControl.py then measured both the volume and the
    capture component's own PostProcessSettings at EV100 3.0 and 7.0 and found
    neither moved the image (differences of 5e-5).

The one difference between the bracket that worked and the scripts that do not is
three flags copied from CaptureGuangzhouLandmarkShowcase.py:

    capture_every_frame = False
    b_always_persist_rendering_state = True
    b_capture_on_construction = False

The suspicion is b_always_persist_rendering_state: with the rendering state
persisted, the capture appears to reuse a state that no longer picks up
post-process changes, which would also explain why the showcase captures were
reported as irreproducible and washed out while the bracket was not.

This probe varies those flags one at a time, with a FRESH capture component per
variant so a flag change cannot be ignored, and a fixed number of warm-up frames
per variant so frame count is not a confound. A variant "controls exposure" only
if EV100 3.0 and 7.0 separate.

Read-only: nothing saved, temporary actors destroyed, level reloaded from disk.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLights")

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(6295, 953, 400)
CAMERA_ROTATION = unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0)
FOV = 60.0
EXPOSURES = [3.0, 7.0]
WARMUP_FRAMES = 3

# (name, component flags). "defaults" sets nothing at all.
# NOTE: the property is `always_persist_rendering_state`, NOT
# `b_always_persist_rendering_state` - the latter does not exist on
# SceneCaptureComponent2D in this build, and CaptureGuangzhouLandmarkShowcase.py
# sets it inside a try/except, so it has been silently reporting a configuration
# it never applied. Same for `b_capture_on_construction`.
VARIANTS = [
    ("persist_true", {"capture_every_frame": False,
                      "always_persist_rendering_state": True}),
    ("persist_false", {"capture_every_frame": False,
                       "always_persist_rendering_state": False}),
    ("defaults", {}),
]


def write_ppm(path, colors):
    buffer = bytearray()
    for row in range(HEIGHT - 1, -1, -1):
        base = row * WIDTH
        for column in range(WIDTH):
            color = colors[base + column]
            buffer.append(min(255, max(0, int(float(color.r) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.g) * 255.0))))
            buffer.append(min(255, max(0, int(float(color.b) * 255.0))))
    with open(path, "wb") as handle:
        handle.write("P6\n{} {}\n255\n".format(WIDTH, HEIGHT).encode("ascii"))
        handle.write(bytes(buffer))


def mean_luminance(colors):
    total = 0.0
    for color in colors:
        total += (float(color.r) + float(color.g) + float(color.b)) / 3.0
    return round(total / len(colors) * 255.0, 5)


def pin_exposure(volume, ev100):
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", ev100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", ev100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)


def spawn_capture(actor_subsystem, render_target, flags):
    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_FlagProbeCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    for attribute, value in flags.items():
        if not hasattr(unreal.SceneCaptureComponent2D, attribute):
            raise AssertionError(
                "flag '{}' does not exist on SceneCaptureComponent2D - the "
                "capture scripts set flags inside try/except, which is how a "
                "name that never existed goes unnoticed".format(attribute))
        component.set_editor_property(attribute, value)
    return capture, component


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    before = len(actor_subsystem.get_all_level_actors())

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_FlagProbeVolume")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for name, flags in VARIANTS:
            capture, component = spawn_capture(actor_subsystem, render_target, flags)
            try:
                for ev100 in EXPOSURES:
                    pin_exposure(volume, ev100)
                    for _ in range(WARMUP_FRAMES):
                        component.capture_scene()
                    colors = unreal.RenderingLibrary.read_render_target(
                        world, render_target, True)
                    assert len(colors) == WIDTH * HEIGHT, len(colors)
                    path = OUT_DIR / "flag-{}-ev{:02d}.ppm".format(
                        name, int(round(ev100)))
                    write_ppm(path, colors)
                    entry = {"variant": name, "ev100": ev100, "flags": flags,
                             "file": str(path), "mean_255": mean_luminance(colors)}
                    results.append(entry)
                    print("FLAG_PROBE", json.dumps(entry))
            finally:
                actor_subsystem.destroy_actor(capture)
    finally:
        actor_subsystem.destroy_actor(volume)

    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    after = len(actor_subsystem.get_all_level_actors())
    assert not leaked, leaked
    assert before == after, (before, after)

    by_variant = {}
    for entry in results:
        by_variant.setdefault(entry["variant"], {})[entry["ev100"]] = entry["mean_255"]
    verdict = {}
    for name, _flags in VARIANTS:
        low = by_variant.get(name, {}).get(EXPOSURES[0])
        high = by_variant.get(name, {}).get(EXPOSURES[1])
        if low is None or high is None:
            verdict[name] = "not measured"
            continue
        verdict[name] = {"mean_ev03": low, "mean_ev07": high,
                         "difference": round(low - high, 5),
                         "controls_exposure": abs(low - high) > 0.5}

    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    report = {"level": LEVEL_PATH, "exposures": EXPOSURES,
              "warmup_frames": WARMUP_FRAMES, "results": results,
              "verdict": verdict, "level_reloaded_from_disk": True}
    (OUT_DIR / "capture-flag-probe-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_CAPTURE_FLAG_PROBE " + json.dumps(report))
    print("SHOWCASE_CAPTURE_FLAG_PROBE_VERDICT", json.dumps(verdict, indent=2))


if __name__ == "__main__":
    main()
