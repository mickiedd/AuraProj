"""Capture every landmark with its own light on and off, one frame pair each.

The aggregate A/B in CaptureGuangzhouLandmarkLights.py proves the per-building
lights as a group and measures three wide views. It does not show that each of the
nine lights frames and lights ITS OWN building well, which is the actual request -
only Guidemen and Zhengnanmen were ever captured close up. This does the other
seven as well, with the same on/off method.

Method notes carried over, because each of them was learned the hard way here:

  * A light is A/B'd by setting its intensity to 0 and restoring it. Hiding a light
    ACTOR does not disable the light, so a "hide the lights" comparison returns an
    identical image whatever the lighting is.
  * The capture keeps DEFAULT flags. `capture_every_frame = False` stops a
    SceneCapture2D honouring post-process, which silently makes the exposure pin a
    no-op; two of the flags the older scripts set do not exist on this build's
    component at all.
  * Exposure control is verified before any frame is trusted.
  * Camera placement is derived from the building's own geometry, so every building
    is framed the same way and the pairs are comparable.
  * Nothing is saved, and every intensity is verified back on disk afterwards.

Read-only with respect to the level.
"""

import json
import math
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
MANIFEST = Path("C:/Git/AuraProj/Saved/RawModelImport/guangzhou-landmark-showcase.json")
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkLightsPerBuilding")

WIDTH, HEIGHT = 1600, 900
GROUND_TOP_Z = 0.0
LIGHT_TAG = "GuangzhouLandmarkLight"

# From the exposure bracket: the brightest step that is not blown out.
EXPOSURE_EV100 = 4.0
SELF_CHECK_EXPOSURES = [3.0, 6.0]
MIN_SELF_CHECK_SPREAD = 0.02
CAPTURE_PPV_PRIORITY = 10.0
WARMUP_FRAMES = 3

# Camera framing, all as multiples of the building's own measured size.
CAM_DISTANCE_FACTOR = 1.6
CAM_DISTANCE_CLEARANCE_CM = 2500.0
CAM_HEIGHT_FACTOR = 0.55
AIM_HEIGHT_FACTOR = 0.45
FOV = 55.0


def ensure_render_target():
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH
    return render_target


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


def luminance(color):
    return min(1.0, max(0.0, (float(color.r) + float(color.g) + float(color.b)) / 3.0))


def camera_for(actor_location, radius, height):
    """Frame the building from the plaza side, at a fixed multiple of its size."""
    x, y = float(actor_location.x), float(actor_location.y)
    planar = math.hypot(x, y)
    front = (-x / planar, -y / planar)
    distance = CAM_DISTANCE_FACTOR * radius + CAM_DISTANCE_CLEARANCE_CM
    location = unreal.Vector(x + front[0] * distance, y + front[1] * distance,
                             GROUND_TOP_Z + height * CAM_HEIGHT_FACTOR)
    aim = (x, y, GROUND_TOP_Z + height * AIM_HEIGHT_FACTOR)
    dx, dy, dz = aim[0] - location.x, aim[1] - location.y, aim[2] - location.z
    planar_distance = math.hypot(dx, dy)
    rotation = unreal.Rotator(pitch=math.degrees(math.atan2(dz, planar_distance)),
                              yaw=math.degrees(math.atan2(dy, dx)), roll=0.0)
    return location, rotation, aim, distance


def pin_exposure(volume, ev100):
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", ev100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", ev100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().startswith(LEVEL_PATH), world
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    before = len(actor_subsystem.get_all_level_actors())
    stored = json.loads(MANIFEST.read_text(encoding="utf-8"))
    geometry = {entry["label"]: entry for entry in stored["landmarks"]}
    lights = {}
    for actor in actor_subsystem.get_all_level_actors():
        if LIGHT_TAG in [str(tag) for tag in actor.tags]:
            lights[actor.get_actor_label()] = (
                actor, actor.get_component_by_class(unreal.SpotLightComponent))
    assert len(lights) == len(geometry), (len(lights), len(geometry))
    render_target = ensure_render_target()

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_PerBuildingExposure")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", CAPTURE_PPV_PRIORITY)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    capture.set_actor_label("TEMP_PerBuildingCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    component.set_editor_property("capture_source",
                                  unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    report = {"level": LEVEL_PATH, "level_saved": False, "exposure_ev100": EXPOSURE_EV100,
              "warmup_frames": WARMUP_FRAMES, "buildings": []}
    originals = {}
    try:
        def shoot(location, rotation):
            pin_exposure(volume, EXPOSURE_EV100)
            capture.set_actor_location(location, False, True)
            capture.set_actor_rotation(rotation, False)
            for _ in range(WARMUP_FRAMES):
                component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            assert len(colors) == WIDTH * HEIGHT, len(colors)
            return colors

        # ---- prove the exposure pin reaches the capture ----------------------
        # The self-check uses a real building's camera: camera_for divides by the
        # landmark's planar radius, so a synthetic origin would divide by zero.
        first_label = sorted(geometry)[0]
        first_entry = geometry[first_label]
        first_actor = next(actor for actor in actor_subsystem.get_all_level_actors()
                           if actor.get_actor_label() == first_label)
        check_location, check_rotation, _a, _d = camera_for(
            first_actor.get_actor_location(), first_entry["geometry_radius_xy"],
            first_entry["height_cm"])
        check = {}
        for ev100 in SELF_CHECK_EXPOSURES:
            pin_exposure(volume, ev100)
            capture.set_actor_location(check_location, False, True)
            capture.set_actor_rotation(check_rotation, False)
            for _ in range(WARMUP_FRAMES):
                component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            check[ev100] = round(sum(luminance(c) for c in colors) / len(colors), 5)
        spread = abs(check[SELF_CHECK_EXPOSURES[0]] - check[SELF_CHECK_EXPOSURES[1]])
        report["self_check"] = {"means": {str(k): v for k, v in check.items()},
                                "spread": round(spread, 5),
                                "controls_exposure": spread > MIN_SELF_CHECK_SPREAD}
        print("PER_BUILDING_SELF_CHECK", json.dumps(report["self_check"]))
        assert spread > MIN_SELF_CHECK_SPREAD, \
            "exposure pin does not reach the capture; frames would be meaningless"

        # ---- one pair per building -------------------------------------------
        for label in sorted(geometry):
            entry = geometry[label]
            key = label[len("Landmark_"):]
            light_label = "Light_" + key
            light_actor, light_component = lights[light_label]
            landmark_actor = next(actor for actor in actor_subsystem.get_all_level_actors()
                                  if actor.get_actor_label() == label)
            location, rotation, aim, distance = camera_for(
                landmark_actor.get_actor_location(), entry["geometry_radius_xy"],
                entry["height_cm"])

            on = shoot(location, rotation)
            on_path = OUT_DIR / "{}-on.ppm".format(key)
            write_ppm(on_path, on)

            originals[light_label] = float(
                light_component.get_editor_property("intensity"))
            light_component.set_editor_property("intensity", 0.0)
            off = shoot(location, rotation)
            off_path = OUT_DIR / "{}-off.ppm".format(key)
            write_ppm(off_path, off)
            light_component.set_editor_property("intensity", originals[light_label])

            on_values = [luminance(c) for c in on]
            off_values = [luminance(c) for c in off]
            count = len(on_values)
            brighter = sum(1 for a, b in zip(on_values, off_values) if a - b > 8.0 / 255.0)
            record = {
                "key": key, "label": label, "light": light_label,
                "camera_distance_cm": round(distance, 1),
                "camera_location": [round(location.x, 1), round(location.y, 1),
                                    round(location.z, 1)],
                "aim": [round(value, 1) for value in aim],
                "mean_on": round(sum(on_values) / count, 5),
                "mean_off": round(sum(off_values) / count, 5),
                "mean_delta": round(sum(on_values) / count - sum(off_values) / count, 5),
                "pct_pixels_brighter": round(brighter / count * 100.0, 2),
                "on_file": str(on_path), "off_file": str(off_path),
            }
            report["buildings"].append(record)
            print("PER_BUILDING", json.dumps(record))

        for light_label, (actor, light_component) in lights.items():
            restored = float(light_component.get_editor_property("intensity"))
            assert abs(restored - originals[light_label]) < 1e-3, \
                "{}: intensity not restored".format(light_label)
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [actor.get_actor_label() for actor in actor_subsystem.get_all_level_actors()
              if actor.get_actor_label().startswith("TEMP_")]
    after = len(actor_subsystem.get_all_level_actors())
    assert not leaked, leaked
    assert before == after, (before, after)

    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    errors = []
    reloaded = {actor.get_actor_label(): actor for actor in
                actor_subsystem.get_all_level_actors()
                if LIGHT_TAG in [str(tag) for tag in actor.tags]}
    stored_intensity = {entry["label"]: entry["intensity_cd"]
                        for entry in stored["landmark_lights"]}
    for light_label, actor in sorted(reloaded.items()):
        actual = float(actor.get_component_by_class(
            unreal.SpotLightComponent).get_editor_property("intensity"))
        expected = stored_intensity.get(light_label)
        if expected is None or abs(actual - expected) > 0.01:
            errors.append("{}: on disk {} vs manifest {}".format(
                light_label, actual, expected))
    report["reload_errors"] = errors
    report["passed"] = not errors

    (OUT_DIR / "per-building-report.json").write_text(
        json.dumps(report, indent=2), encoding="utf-8")
    print("PER_BUILDING_PASSED", report["passed"])
    print("PER_BUILDING_ERRORS", json.dumps(errors))
    print("PER_BUILDING_REPORT", str(OUT_DIR / "per-building-report.json"))


if __name__ == "__main__":
    main()
