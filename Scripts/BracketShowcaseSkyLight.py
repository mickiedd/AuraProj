"""Bracket the showcase SkyLight intensity to fix the blown-out ground.

Evidence so far: the ground plane clips to pure white at every exposure value,
while the sky renders black and the landmarks' vertical faces render dark. That
combination means the upward-facing plane is receiving far more light than the
7.0-lux key can supply, so the ambient sky light - not exposure - is the cause.
The project's preview levels use a SkyLight intensity of 1.4; this sweeps lower
values with exposure pinned at the preview value of EV100 = 1.0 so the sweep
isolates one variable.

The level is not saved and all temporary actors are destroyed.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(0, -42000, 26000)
CAMERA_ROTATION = unreal.Rotator(pitch=-31.7, yaw=90.0, roll=0.0)
FOV = 60.0
EV100 = 1.0
SKY_INTENSITIES = [0.05, 0.15, 0.4, 1.4]


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


def stats(colors):
    buckets = [0] * 8
    total = 0.0
    for color in colors:
        value = (float(color.r) + float(color.g) + float(color.b)) / 3.0
        total += value
        buckets[min(7, int(value * 8))] += 1
    count = len(colors)
    return {"mean": round(total / count, 4),
            "histogram_pct": [round(b / count * 100.0, 1) for b in buckets]}


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH

    sky = next((a for a in actor_subsystem.get_all_level_actors()
                if a.get_actor_label() == "Showcase_SkyLight"), None)
    assert sky, "Showcase_SkyLight"
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    original_intensity = float(sky_component.get_editor_property("intensity"))
    report = {"original_sky_intensity": original_intensity, "ev100": EV100, "results": []}

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_SkyBracket")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)
    settings = volume.get_editor_property("settings")
    for attribute, value in (("override_auto_exposure_min_brightness", True),
                             ("auto_exposure_min_brightness", EV100),
                             ("override_auto_exposure_max_brightness", True),
                             ("auto_exposure_max_brightness", EV100)):
        settings.set_editor_property(attribute, value)
    volume.set_editor_property("settings", settings)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_SkyCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        for intensity in SKY_INTENSITIES:
            sky_component.set_intensity(intensity)
            try:
                sky_component.recapture_sky()
            except Exception:
                pass
            capture.set_actor_location(CAMERA_LOCATION, False, True)
            capture.set_actor_rotation(CAMERA_ROTATION, False)
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            if len(colors) != WIDTH * HEIGHT:
                report["results"].append({"sky_intensity": intensity,
                                          "error": "bad pixel count"})
                continue
            path = OUT_DIR / "sky-{:03d}.ppm".format(int(round(intensity * 100)))
            write_ppm(path, colors)
            entry = {"sky_intensity": intensity, "file": str(path)}
            entry.update(stats(colors))
            report["results"].append(entry)
            print("SKY_BRACKET", json.dumps(entry))
    finally:
        sky_component.set_intensity(original_intensity)
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report["leaked"] = leaked
    report["level_saved"] = False
    (OUT_DIR / "sky-bracket.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_SKY_BRACKET " + json.dumps(report))
    print("SHOWCASE_SKY_BRACKET", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
