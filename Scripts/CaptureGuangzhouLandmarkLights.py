"""Capture the showcase level with the per-landmark lights ON and OFF.

A per-building light is only worth having if it visibly changes its building, so
this captures the same views twice: once with every landmark light as
configured, and once with every landmark light's intensity set to 0. Zeroing the
intensity is the only honest way to test a light's contribution - hiding a light
ACTOR does not disable the light, so a "hide the lights" comparison returns an
identical image whatever the lighting actually is.

THREE THINGS THIS SCRIPT DOES THAT THE EARLIER CAPTURE SCRIPTS DID NOT, all of
them forced by what the probes found:

1. It does not touch `capture_every_frame` or `always_persist_rendering_state`.
   CaptureGuangzhouLandmarkShowcase.py sets `capture_every_frame = False` plus two
   properties that DO NOT EXIST on this build's SceneCaptureComponent2D
   (`b_always_persist_rendering_state`, `b_capture_on_construction` - the real
   name has no `b_` prefix). Those writes are wrapped in try/except, so the script
   silently reported a configuration it never applied. The measured consequence
   (Scripts/ProbeShowcaseCapturePersistenceFlag.py) is that with those flags the
   capture ignores exposure entirely - EV100 3.0 and 7.0 came back with mean
   luminances differing by 5e-5 - while with default flags the same two exposures
   separate by a factor of sixty. That is why the showcase captures were reported
   as washed out and irreproducible while the standalone 2026-09-21 bracket was
   not: they used different flags.

2. It VERIFIES exposure control before trusting any frame. Two exposures are
   captured first and must separate; if they do not, the script fails loudly
   rather than emitting frames that look authoritative and are not.

3. It chooses the exposure by measurement instead of inheriting one. The level
   pins EV100 3.0, but this script does not assume that pin suits these cameras:
   it brackets, reports the clipping, and takes the brightest exposure that is not
   blown out.

Nothing is saved. Intensities are restored, temporary actors destroyed, and the
level reloaded from disk; every light's intensity is then re-read and compared
with the placement manifest, which is what proves the A/B did not persist.
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

# The self-check pair: far enough apart that any working exposure path separates them.
SELF_CHECK_EXPOSURES = [3.0, 6.0]
MIN_SELF_CHECK_SPREAD = 0.02          # mean luminance in 0..1
# Bracketed to choose the capture exposure. The level's own pin (3.0) is included
# so the result can be compared against it.
CANDIDATE_EXPOSURES = [3.0, 4.0, 5.0, 6.0]
TARGET_MAX_CLIP_PCT = 2.0
MIN_ACCEPTABLE_MEAN = 0.12            # 0..1, so the chosen frame is not simply dark
WARMUP_FRAMES = 3

# (name, camera location cm, camera rotator, field of view deg)
# Rotators are built with keyword arguments: positional order is (roll, pitch, yaw).
VIEWS = [
    ("hero", unreal.Vector(0, -42000, 26000),
     unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0), 60.0),
    ("plaza", unreal.Vector(6295, 953, 400),
     unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0), 60.0),
    ("guidemen", unreal.Vector(-4965, 606, 400),
     unreal.Rotator(pitch=2.0, yaw=173.05, roll=0.0), 60.0),
]


def ensure_render_target():
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    return render_target


def write_ppm(path, colors, width, height, flip_vertical=True):
    """Binary PPM (P6), values clamped. read_render_target returns rows bottom-up."""
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


def frame_stats(colors):
    """Clamped statistics, so they describe the same image the PPM holds.

    read_render_target does not always hand back normalised values - with default
    capture flags it can return values well above 1.0 - so everything is clamped
    to [0, 1] first. `mid_pct` is the share of the frame that is neither blown nor
    crushed, which is the figure the exposure is actually chosen on.
    """
    total = 0.0
    clipped = 0
    dark = 0
    for color in colors:
        value = min(1.0, max(0.0, (float(color.r) + float(color.g) + float(color.b)) / 3.0))
        total += value
        if value >= 0.98:
            clipped += 1
        elif value <= 0.02:
            dark += 1
    count = len(colors)
    return {"mean": round(total / count, 5),
            "clipped_pct": round(clipped / count * 100.0, 3),
            "dark_pct": round(dark / count * 100.0, 3),
            "mid_pct": round((count - clipped - dark) / count * 100.0, 3)}


def pin_exposure(volume, ev100):
    """Pin both ends of the auto-exposure range to the same EV100."""
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", ev100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", ev100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)


def spawn_capture(actor_subsystem, render_target):
    """A capture with DEFAULT flags - see note 1 in the module docstring.

    Only properties that exist are written, and each write is checked, because a
    set_editor_property inside try/except is how the previous scripts ended up
    believing they had configured a component they had not.
    """
    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), VIEWS[0][1], VIEWS[0][2],
        transient=False)
    assert capture, "SceneCapture2D"
    capture.set_actor_label("TEMP_LandmarkLightCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    assert component, "SceneCaptureComponent2D"
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", VIEWS[0][3])
    applied = {"texture_target": True, "fov_angle": True}
    for attribute, value in (("capture_source",
                              unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR),):
        assert hasattr(unreal.SceneCaptureComponent2D, attribute), attribute
        component.set_editor_property(attribute, value)
        applied[attribute] = True
    return capture, component, applied


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
    assert lights, "no landmark lights found - run CreateGuangzhouLandmarkShowcase.py first"
    render_target = ensure_render_target()

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    assert volume, "capture post-process volume"
    volume.set_actor_label("TEMP_CaptureExposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", CAPTURE_PPV_PRIORITY)

    capture, component, applied_flags = spawn_capture(actor_subsystem, render_target)

    report = {"level": LEVEL_PATH, "level_saved": False, "landmark_lights": len(lights),
              "capture_flags_applied": applied_flags,
              "self_check_exposures": SELF_CHECK_EXPOSURES,
              "candidate_exposures": CANDIDATE_EXPOSURES, "views": [], "bracket": []}
    originals = {}
    try:
        def shoot(ev100, location, rotation, fov, path):
            pin_exposure(volume, ev100)
            capture.set_actor_location(location, False, True)
            capture.set_actor_rotation(rotation, False)
            component.set_editor_property("fov_angle", fov)
            for _ in range(WARMUP_FRAMES):
                component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            assert len(colors) == WIDTH * HEIGHT, len(colors)
            if path is not None:
                write_ppm(path, colors, WIDTH, HEIGHT)
            return frame_stats(colors)

        # ---- 1. prove exposure control works before trusting anything --------
        name, location, rotation, fov = VIEWS[1]
        check = {}
        for ev100 in SELF_CHECK_EXPOSURES:
            check[ev100] = shoot(ev100, location, rotation, fov, None)
        spread = abs(check[SELF_CHECK_EXPOSURES[0]]["mean"]
                     - check[SELF_CHECK_EXPOSURES[1]]["mean"])
        report["self_check"] = {"stats": {str(key): value for key, value in check.items()},
                                "mean_spread": round(spread, 5),
                                "controls_exposure": spread > MIN_SELF_CHECK_SPREAD}
        print("LIGHT_CAPTURE_SELF_CHECK", json.dumps(report["self_check"]))
        assert spread > MIN_SELF_CHECK_SPREAD, (
            "exposure pinning does not reach the scene capture: EV100 {} and {} "
            "gave mean luminances {} and {} (spread {:.5f}). Frames from this "
            "harness would look authoritative and mean nothing.".format(
                SELF_CHECK_EXPOSURES[0], SELF_CHECK_EXPOSURES[1],
                check[SELF_CHECK_EXPOSURES[0]]["mean"],
                check[SELF_CHECK_EXPOSURES[1]]["mean"], spread))

        # ---- 2. choose the exposure by measurement ---------------------------
        for ev100 in CANDIDATE_EXPOSURES:
            entry = {"ev100": ev100, "view": name}
            entry.update(shoot(ev100, location, rotation, fov,
                               OUT_DIR / "bracket-ev{:02d}-{}.ppm".format(
                                   int(round(ev100)), name)))
            report["bracket"].append(entry)
            print("LIGHT_CAPTURE_BRACKET", json.dumps(entry))

        acceptable = [entry for entry in report["bracket"]
                      if entry["clipped_pct"] <= TARGET_MAX_CLIP_PCT
                      and entry["mean"] >= MIN_ACCEPTABLE_MEAN]
        if acceptable:
            # Brightest exposure that is not blown out.
            chosen = min(acceptable, key=lambda item: item["ev100"])
        else:
            # Nothing cleared the clipping target, so pick the exposure that puts
            # the MOST of the frame in the mid-tones. Minimising clipping instead
            # would just pick the darkest step, which is the opposite of useful.
            chosen = max(report["bracket"], key=lambda item: item["mid_pct"])
        report["exposure_target_met"] = bool(acceptable)
        report["chosen_exposure_ev100"] = chosen["ev100"]
        report["chosen_exposure_stats"] = {key: value for key, value in chosen.items()
                                          if key not in ("view",)}
        print("LIGHT_CAPTURE_CHOSEN_EXPOSURE", json.dumps(report["chosen_exposure_stats"]))
        ev100 = chosen["ev100"]

        # ---- 3. the A/B at the chosen exposure ------------------------------
        def shoot_views(state):
            for view_name, view_location, view_rotation, view_fov in VIEWS:
                path = OUT_DIR / "{}-{}-ev{:02d}.ppm".format(
                    view_name, state, int(round(ev100)))
                entry = {"view": view_name, "state": state, "ev100": ev100,
                         "file": str(path)}
                entry.update(shoot(ev100, view_location, view_rotation, view_fov, path))
                report["views"].append(entry)
                print("LIGHT_CAPTURE", json.dumps(entry))

        shoot_views("lights-on")

        for label, light_component in lights.items():
            originals[label] = float(light_component.get_editor_property("intensity"))
            light_component.set_editor_property("intensity", 0.0)
        for label, light_component in lights.items():
            assert float(light_component.get_editor_property("intensity")) == 0.0, \
                "{}: could not zero the intensity".format(label)
        shoot_views("lights-off")

        for label, light_component in lights.items():
            light_component.set_editor_property("intensity", originals[label])
        for label, light_component in lights.items():
            restored = float(light_component.get_editor_property("intensity"))
            assert abs(restored - originals[label]) < 1e-3, \
                "{}: intensity not restored ({})".format(label, restored)
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    report["restored_intensities"] = {label: round(value, 4)
                                      for label, value in originals.items()}
    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    after = len(actor_subsystem.get_all_level_actors())
    report["leaked_actors"] = leaked
    report["actor_count_before"] = before
    report["actor_count_after"] = after
    assert not leaked, "capture actor leaked: " + ", ".join(leaked)
    assert before == after, "actor count changed: {} -> {}".format(before, after)

    # Per-view light contribution, so the record states the effect numerically
    # rather than as an opinion about a picture.
    on = {(entry["view"]): entry for entry in report["views"] if entry["state"] == "lights-on"}
    off = {(entry["view"]): entry for entry in report["views"] if entry["state"] == "lights-off"}
    report["contribution"] = []
    for view_name in sorted(set(on) & set(off)):
        report["contribution"].append({
            "view": view_name,
            "mean_lights_on": on[view_name]["mean"],
            "mean_lights_off": off[view_name]["mean"],
            "mean_delta": round(on[view_name]["mean"] - off[view_name]["mean"], 5),
            "clipped_pct_on": on[view_name]["clipped_pct"],
            "clipped_pct_off": off[view_name]["clipped_pct"],
        })

    # Discard the in-memory A/B, then prove the saved level is untouched.
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    stored = {}
    if MANIFEST.exists():
        stored = {entry["label"]: entry["intensity_cd"]
                  for entry in json.loads(MANIFEST.read_text(encoding="utf-8"))
                  .get("landmark_lights", [])}
    errors = []
    report["reload_check"] = []
    for label, light_component in sorted(light_components(actor_subsystem).items()):
        actual = float(light_component.get_editor_property("intensity"))
        expected = stored.get(label)
        report["reload_check"].append({"label": label, "on_disk_intensity": round(actual, 4),
                                       "manifest_intensity": expected})
        if expected is None:
            errors.append("{}: no manifest entry".format(label))
        elif abs(actual - expected) > 0.01:
            errors.append("{}: saved intensity {} != manifest {}".format(
                label, actual, expected))
    report["errors"] = errors
    report["passed"] = not errors

    (OUT_DIR / "light-ab-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_LIGHT_AB " + json.dumps(report))
    print("SHOWCASE_LIGHT_AB_PASSED", report["passed"])
    print("SHOWCASE_LIGHT_AB_ERRORS", json.dumps(errors))
    print("SHOWCASE_LIGHT_AB_CONTRIBUTION", json.dumps(report["contribution"], indent=2))
    print("SHOWCASE_LIGHT_AB_REPORT", str(OUT_DIR / "light-ab-report.json"))


if __name__ == "__main__":
    main()
