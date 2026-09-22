"""Bracket exposure on the showcase level with the per-landmark lights ON and OFF.

Why this exists: at the level's pinned EV100 3.0 the showcase frames come back
almost entirely clipped (mean luminance 224-241 of 255), so a lights-on /
lights-off comparison at that exposure cannot show whether a building's own light
is shading it well or merely pushing already-blown pixels further into white.
This project's established answer to that is to bracket the exposure and look,
which is what this does - the same technique that resolved the earlier
"is this level lit at all?" question.

Both states are captured at every exposure, so each exposure step gives a real
side-by-side rather than a comparison against a frame from a different run.

Nothing is saved. Intensities are restored, temporary actors destroyed, and the
level reloaded from disk afterwards.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLights")
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-showcase.json")

WIDTH = 1600
HEIGHT = 900
CAPTURE_PPV_PRIORITY = 10.0
LIGHT_TAG = "GuangzhouLandmarkLight"

# EV100. Higher is darker; the level pins 3.0, which clips here.
EXPOSURES = [3.0, 4.0, 5.0, 6.0]

# (name, camera location cm, camera rotator, field of view deg)
VIEWS = [
    ("guidemen", unreal.Vector(-4965, 606, 400),
     unreal.Rotator(pitch=2.0, yaw=173.05, roll=0.0), 60.0),
    ("plaza", unreal.Vector(6295, 953, 400),
     unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0), 60.0),
]


def ensure_render_target():
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
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


def pin_exposure(volume, ev100):
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", ev100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", ev100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)


def light_components(actor_subsystem):
    components = {}
    for actor in actor_subsystem.get_all_level_actors():
        if LIGHT_TAG not in [str(tag) for tag in actor.tags]:
            continue
        component = actor.get_component_by_class(unreal.SpotLightComponent)
        if component is not None:
            components[actor.get_actor_label()] = component
    return components


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL_PATH), world
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    before = len(actor_subsystem.get_all_level_actors())
    lights = light_components(actor_subsystem)
    assert lights, "no landmark lights found"
    render_target = ensure_render_target()

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert volume, "capture post-process volume"
    volume.set_actor_label("TEMP_BracketExposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", CAPTURE_PPV_PRIORITY)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert capture, "SceneCapture2D"
    capture.set_actor_label("TEMP_BracketCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    assert component, "SceneCaptureComponent2D"
    component.set_editor_property("texture_target", render_target)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass
    # No capture_every_frame / persist-rendering-state flags here. The first run
    # of this script set them (copied from CaptureGuangzhouLandmarkShowcase.py,
    # where they are wrapped in try/except) and every exposure in the bracket came
    # back with identical statistics, because `capture_every_frame = False`
    # stops post-process changes - including the exposure pin - reaching the
    # capture. The two persist flags it also set do not exist on this build's
    # SceneCaptureComponent2D. See Scripts/ProbeShowcaseCapturePersistenceFlag.py.

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    originals = {}
    try:
        for ev100 in EXPOSURES:
            pin_exposure(volume, ev100)
            for state in ("lights-on", "lights-off"):
                if state == "lights-off":
                    for label, light_component in lights.items():
                        if label not in originals:
                            originals[label] = float(
                                light_component.get_editor_property("intensity"))
                        light_component.set_editor_property("intensity", 0.0)
                else:
                    for label, light_component in lights.items():
                        if label in originals:
                            light_component.set_editor_property(
                                "intensity", originals[label])
                for name, location, rotation, fov in VIEWS:
                    capture.set_actor_location(location, False, True)
                    capture.set_actor_rotation(rotation, False)
                    component.set_editor_property("fov_angle", fov)
                    # Warm-up frame discarded: the exposure and the light state
                    # both changed since the last capture.
                    component.capture_scene()
                    component.capture_scene()
                    colors = unreal.RenderingLibrary.read_render_target(
                        world, render_target, True)
                    if len(colors) != WIDTH * HEIGHT:
                        results.append({"view": name, "state": state, "ev100": ev100,
                                        "error": "pixel count {}".format(len(colors))})
                        continue
                    path = OUT_DIR / "ev{:02d}-{}-{}.ppm".format(
                        int(round(ev100)), name, state)
                    write_ppm(path, colors, WIDTH, HEIGHT)
                    results.append({"view": name, "state": state, "ev100": ev100,
                                    "file": str(path), "pixels": len(colors)})
                    print("LIGHT_BRACKET", name, state, "EV100", ev100, str(path))
    finally:
        for label, light_component in lights.items():
            if label in originals:
                light_component.set_editor_property("intensity", originals[label])
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    after = len(actor_subsystem.get_all_level_actors())
    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    report = {"level": LEVEL_PATH, "exposures": EXPOSURES, "views": results,
              "actor_count_before": before, "actor_count_after": after,
              "leaked_actors": leaked, "level_saved": False}
    assert not leaked, "capture actor leaked: " + ", ".join(leaked)
    assert before == after, "actor count changed: {} -> {}".format(before, after)

    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    stored = {}
    if MANIFEST.exists():
        stored = {entry["label"]: entry["intensity_cd"]
                  for entry in json.loads(MANIFEST.read_text(encoding="utf-8"))
                  .get("landmark_lights", [])}
    report["reload_check"] = []
    errors = []
    for label, light_component in sorted(light_components(actor_subsystem).items()):
        actual = float(light_component.get_editor_property("intensity"))
        expected = stored.get(label)
        report["reload_check"].append(
            {"label": label, "on_disk_intensity": round(actual, 4),
             "manifest_intensity": expected})
        if expected is None:
            errors.append("{}: no manifest entry".format(label))
        elif abs(actual - expected) > 0.01:
            errors.append("{}: saved intensity {} != manifest {}".format(
                label, actual, expected))
    report["errors"] = errors
    report["passed"] = not errors

    (OUT_DIR / "light-bracket-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_LIGHT_BRACKET " + json.dumps(report))
    print("SHOWCASE_LIGHT_BRACKET_PASSED", report["passed"])
    print("SHOWCASE_LIGHT_BRACKET_ERRORS", json.dumps(errors))
    print("SHOWCASE_LIGHT_BRACKET_REPORT", str(OUT_DIR / "light-bracket-report.json"))


if __name__ == "__main__":
    main()
