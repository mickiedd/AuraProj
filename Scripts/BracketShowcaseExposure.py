"""Bracket the showcase exposure to find the value that renders it correctly.

A single capture_scene() call does not give auto-exposure time to converge, so
the level's auto-exposure cannot be judged from one frame. This pins exposure to
a range of EV100 values with a temporary post-process volume and captures the
same view at each, so the correct value can be chosen from the images rather
than guessed.

The temporary volume and capture actor are destroyed afterwards and the level is
not saved, so the level keeps engine-default auto-exposure for interactive use.
"""

import json
from pathlib import Path

import unreal

LEVEL_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase"
RT_PATH = "/Game/Assets/Environment/GuangzhouLandmarks/RT_ShowcaseCapture"
OUT_DIR = Path("C:/Git/AuraProj/Saved/RawModelImport/GuangzhouLandmarkShowcase")

WIDTH, HEIGHT = 1600, 900
CAMERA_LOCATION = unreal.Vector(6295, 953, 400)
CAMERA_ROTATION = unreal.Rotator(pitch=-2.0, yaw=8.6, roll=0.0)
FOV = 60.0
EV_VALUES = [2.0, 3.0, 4.0, 5.0]


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
    return {
        "mean": round(total / count, 4),
        "histogram_pct": [round(b / count * 100.0, 1) for b in buckets],
    }


def main():
    loaded = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL_PATH)
    assert loaded, LEVEL_PATH
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    render_target = unreal.EditorAssetLibrary.load_asset(RT_PATH)
    assert render_target, RT_PATH

    volume = actor_subsystem.spawn_actor_from_class(
        unreal.PostProcessVolume.static_class(), unreal.Vector(0, 0, 0),
        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), transient=False)
    volume.set_actor_label("TEMP_ExposureBracket")
    volume.set_editor_property("unbound", True)
    volume.set_editor_property("priority", 10.0)

    capture = actor_subsystem.spawn_actor_from_class(
        unreal.SceneCapture2D.static_class(), CAMERA_LOCATION, CAMERA_ROTATION,
        transient=False)
    capture.set_actor_label("TEMP_BracketCapture")
    component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
    component.set_editor_property("texture_target", render_target)
    component.set_editor_property("fov_angle", FOV)
    try:
        component.set_editor_property(
            "capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    except Exception:
        pass

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    results = []
    try:
        for ev in EV_VALUES:
            settings = volume.get_editor_property("settings")
            for attribute, value in (
                ("override_auto_exposure_min_brightness", True),
                ("auto_exposure_min_brightness", ev),
                ("override_auto_exposure_max_brightness", True),
                ("auto_exposure_max_brightness", ev),
            ):
                settings.set_editor_property(attribute, value)
            volume.set_editor_property("settings", settings)

            capture.set_actor_location(CAMERA_LOCATION, False, True)
            capture.set_actor_rotation(CAMERA_ROTATION, False)
            component.capture_scene()
            colors = unreal.RenderingLibrary.read_render_target(world, render_target, True)
            if len(colors) != WIDTH * HEIGHT:
                results.append({"ev100": ev, "error": "bad pixel count"})
                continue
            path = OUT_DIR / "ev-{:02d}.ppm".format(int(ev * 10))
            write_ppm(path, colors)
            entry = {"ev100": ev, "file": str(path)}
            entry.update(stats(colors))
            results.append(entry)
            print("EXPOSURE_BRACKET", json.dumps(entry))
    finally:
        actor_subsystem.destroy_actor(capture)
        actor_subsystem.destroy_actor(volume)

    leaked = [a.get_actor_label() for a in actor_subsystem.get_all_level_actors()
              if a.get_actor_label().startswith("TEMP_")]
    report = {"results": results, "leaked": leaked, "level_saved": False}
    (OUT_DIR / "exposure-bracket.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SHOWCASE_EXPOSURE_BRACKET " + json.dumps(report))
    print("SHOWCASE_EXPOSURE_BRACKET", json.dumps(report, indent=2))
    assert not leaked, leaked


if __name__ == "__main__":
    main()
